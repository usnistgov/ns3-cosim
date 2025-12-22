/*
 * NIST-developed software is provided by NIST as a public service. You may use,
 * copy, and distribute copies of the software in any medium, provided that you
 * keep intact this entire notice. You may improve, modify, and create
 * derivative works of the software or any portion of the software, and you may
 * copy and distribute such modifications or works. Modified works should carry
 * a notice stating that you changed the software and should note the date and
 * nature of any such change. Please explicitly acknowledge the National
 * Institute of Standards and Technology as the source of the software. 
 *
 * NIST-developed software is expressly provided "AS IS." NIST MAKES NO WARRANTY
 * OF ANY KIND, EXPRESS, IMPLIED, IN FACT, OR ARISING BY OPERATION OF LAW,
 * INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTY OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT, AND DATA ACCURACY. NIST
 * NEITHER REPRESENTS NOR WARRANTS THAT THE OPERATION OF THE SOFTWARE WILL BE
 * UNINTERRUPTED OR ERROR-FREE, OR THAT ANY DEFECTS WILL BE CORRECTED. NIST DOES
 * NOT WARRANT OR MAKE ANY REPRESENTATIONS REGARDING THE USE OF THE SOFTWARE OR
 * THE RESULTS THEREOF, INCLUDING BUT NOT LIMITED TO THE CORRECTNESS, ACCURACY,
 * RELIABILITY, OR USEFULNESS OF THE SOFTWARE.
 * 
 * You are solely responsible for determining the appropriateness of using and
 * distributing the software and you assume all risks associated with its use,
 * including but not limited to the risks and costs of program errors,
 * compliance with applicable laws, damage to or loss of data, programs or
 * equipment, and the unavailability or interruption of operation. This software 
 * is not intended to be used in any situation where a failure could cause risk
 * of injury or damage to property. The software developed by NIST employees is
 * not subject to copyright protection within the United States.
 *
 * Authors:
 *  Thomas Roth <thomas.roth@nist.gov>
 *  Benjamin Philipose
 *  Raphael Barbau
*/

#include "json-gateway.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("JSONGateway");

const std::string JSONGateway::JSONGATEWAY_TIME_S = "time_s";
const std::string JSONGateway::JSONGATEWAY_TIME_NS = "time_ns";
const std::string JSONGateway::JSONGATEWAY_OBJECTS = "objects";


/* ========== PUBLIC MEMBER FUNCTIONS ======================================= */

JSONGateway::JSONGateway(const std::string & delimiterMessage):
    m_delimiterMessage(delimiterMessage)
{
}


/* ========== PROTECTED MEMBER FUNCTIONS ======================================= */


void
JSONGateway::RunThread()
{
    NS_LOG_FUNCTION(this);

    const size_t BUFFER_SIZE = 4096;
    char recvBuffer[BUFFER_SIZE];   // buffer for recv call
    std::string receivedData;       // received message content
    size_t messageSize;             // received message size (excluding the message delimiter)

    while (GetState() == BaseGateway::STATE::CONNECTED)
    {
        receivedData = m_messageBuffer;  // recover any partially received message

        // this loop has the following possible outcomes:
        //  messageSize = receivedData.size() - m_delimiterMessage.size()   [received one message]
        //  messageSize < receivedData.size() - m_delimiterMessage.size()   [received more than one message]
        //  messageSize = std::string::npos                                 [unable to receive messages]
        while ((messageSize = receivedData.find(m_delimiterMessage)) == std::string::npos)
        {
            NS_LOG_LOGIC("\tWaiting to receive data...");
            int bytesReceived = recv(GetSocket(), &recvBuffer[0], BUFFER_SIZE, 0);

            if (bytesReceived > 0)
            {
                NS_LOG_LOGIC("\t...data received: " << bytesReceived << " bytes");
                receivedData.append(&recvBuffer[0], bytesReceived);
            }
            else if (bytesReceived == 0) // connection closed
            {
                NS_LOG_LOGIC("\t...connection closed");
                if (!receivedData.empty())
                {
                    NS_LOG_WARN("WARNING: dropped partial message " << receivedData);
                }
                break; // messageSize = std::string::npos  
            }
            else
            {
                NS_LOG_ERROR("ERROR: gateway socket connection error");
                break; // messageSize = std::string::npos  
            }
        }

        // RunThread needs the main thread to execute the next function (either Stop or ForwardUp)
        // it schedules the function on behalf of the main thread's m_context to execute now
        // Simulator::ScheduleWithContext is thread safe
        if (messageSize == std::string::npos)
        {
            NS_LOG_DEBUG("Stopping gateway");
            Simulator::ScheduleWithContext(GetContext(), Time(0), MakeEvent(&JSONGateway::Stop, this));
            break; // prevent additional receive attempts
        }

        std::string receivedMessage = receivedData.substr(0, messageSize);
        m_messageBuffer = receivedData.substr(messageSize + m_delimiterMessage.size());

        NS_LOG_DEBUG("Forwarding new message: " << receivedMessage);
        {   // critical section start
            std::unique_lock lock(GetMessageQueueMutex());
            GetMessageQueue().push(receivedMessage);    
        }   // critical section end
        Simulator::ScheduleWithContext(GetContext(), Time(0), MakeEvent(&JSONGateway::ForwardUp, this));
    }
}


std::string JSONGateway::PopulateResponseMessage() const
{
    json data;
    data.emplace(JSONGATEWAY_TIME_S, Simulator::Now().GetSeconds());
    data.emplace(JSONGATEWAY_TIME_NS, Simulator::Now().GetNanoSeconds());
    std::vector<json> objects;

    std::map<uint, Ptr<JSONMobilityObject>>::const_iterator it;
    for (it = m_objects.begin(); it != m_objects.end(); it++)
    {
        json object;
        it->second->Serialize(object);
        objects.emplace_back(object);
    }
    data.emplace(JSONGATEWAY_OBJECTS, objects);
    return data.dump() + m_delimiterMessage;
}


/* ========== PROTECTED MEMBER FUNCTIONS ====================================== */

const std::map<uint, Ptr<JSONMobilityObject>>& JSONGateway::GetObjects() const
{
    return m_objects;
}

Ptr<Node> JSONGateway::AddObject(Ptr<JSONMobilityObject> object)
{
    uint id = object->GetId();
    if (m_objects.find(id) != m_objects.end())
        NS_LOG_WARN("There is another object  with id " << id);
    m_objects[id] = object;
    Ptr<Node> node = CreateObject<Node>();
    NodeList::Add(node);
    node->AggregateObject(object);
    NS_LOG_DEBUG("Adding object " << node);
    return node;
}

void JSONGateway::RemoveObject(uint id)
{
    // cannot remove from NodeList!!
    m_objects.erase(id);
}

/* ========== PRIVATE MEMBER FUNCTIONS ====================================== */

void
JSONGateway::ForwardUp()
{
    NS_LOG_FUNCTION(this);

    // get the message to process
    std::string message;
    {   // critical section start
        std::unique_lock lock(GetMessageQueueMutex());
        if (GetMessageQueue().empty())
        {
            NS_FATAL_ERROR("Gateway::ForwardUp called without any queued messages");
        }
        message = GetMessageQueue().front();
        GetMessageQueue().pop();
    }   // critical section end

    // split the message into values
    try
    {
        json data = json::parse(message);
        auto it_time_s = data.find(JSONGATEWAY_TIME_S);
        // what if simulation time is negative?
        Time timestamp = Seconds(0);
        if (it_time_s != data.end())
        {
            json value = *it_time_s;
            if (JSONObject::IsInt(JSONGATEWAY_TIME_S, value))
                timestamp += Seconds(JSONObject::GetInt(value));
            else
            {
                NS_LOG_DEBUG("Timestamp seconds not an integer");
            }
        }
        else
        {
            NS_LOG_DEBUG("No timestamp seconds in message");
        }
        auto it_time_ns = data.find(JSONGATEWAY_TIME_NS);
        if (it_time_ns != data.end())
        {
            json value = *it_time_ns;
            if (JSONObject::IsInt(JSONGATEWAY_TIME_NS, value))
                timestamp += NanoSeconds(JSONObject::GetInt(value));
            else
            {
                NS_LOG_DEBUG("Timestamp nanoseconds not an integer");
            }
        }
        else
        {
            NS_LOG_DEBUG("No timestamp nanoseconds in message");
        }

        NS_LOG_DEBUG("Received time: " << timestamp);

        
        // process based on timestamp content
        if (timestamp.IsStrictlyNegative()) // signal to terminate
        {
            NS_LOG_INFO("Gateway received the terminate message");
            Simulator::Stop();
        }
        else if (GetTimeStart().IsStrictlyNegative()) // first value received
        {
            SetTimeStart(timestamp);
            NS_LOG_INFO("Gateway reference time set as " << timestamp);
            Simulator::ScheduleNow(&JSONGateway::DoInitialize, this, data);
        }
        else // normal message
        {
            if (getEventWait().IsPending())
            {
                getEventWait().Cancel();
            }
            NS_LOG_LOGIC("...update received for " << timestamp);

            // calculate the time difference
            SetTimePause(timestamp - GetTimeStart());
            ns3::Time timeDelta = GetTimePause() - Simulator::Now();
            if (timeDelta.IsStrictlyNegative()) // 0 allowed
            {
                NS_FATAL_ERROR("ERROR: received timestamps were not increasing values");
            }
            NS_LOG_INFO("Scheduling next update in " << timeDelta);
            Simulator::Schedule(timeDelta, &JSONGateway::HandleUpdate, this, data);
        }
    }
    catch (const json::parse_error& e)
    {
        NS_LOG_ERROR("JSON parsong error: " << e.what());
    }

}

void JSONGateway::DoInitialize(const json & data)
{

}

void
JSONGateway::HandleUpdate(const json & data)
{
    NS_LOG_FUNCTION(this << data);

    if (Simulator::Now() == GetTimePause())
    {
        NS_LOG_LOGIC("Waiting for next update...");
        SetEventWait(Simulator::ScheduleNow(&JSONGateway::WaitForNextUpdate, this));
    }
    DoUpdate(data);
}


}