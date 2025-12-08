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

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "text-gateway.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TextGateway");

/* ========== PUBLIC MEMBER FUNCTIONS ======================================= */

TextGateway::TextGateway(uint32_t dataSize, const std::string & delimiterField, const std::string & delimiterMessage):
    m_delimiterField(delimiterField),
    m_delimiterMessage(delimiterMessage),
    m_data(dataSize, "")
{
    NS_LOG_FUNCTION(this << dataSize);

    if (delimiterField.empty())
    {
        NS_FATAL_ERROR("ERROR: gateway field delimiter cannot be empty");
    }
    if (delimiterMessage.empty())
    {
        NS_FATAL_ERROR("ERROR: gateway message delimiter cannot be empty");
    }
    if (delimiterField.find(delimiterMessage) != std::string::npos)
    {
        NS_FATAL_ERROR("ERROR: gateway message delimiter cannot be a substring of the field delimiter");
    }
}

void
TextGateway::SetValue(uint32_t index, const std::string & value)
{
    NS_LOG_FUNCTION(this << index << value);

    if (index >= m_data.size())
    {
        NS_FATAL_ERROR("ERROR: Gateway::SetValue called with i=" << index << " for a max size of " << m_data.size());
    }
    if (value.find(m_delimiterField) != std::string::npos)
    {
        NS_FATAL_ERROR("ERROR: Gateway::SetValue called with a value containing the protocol field delimiter");
    }
    if (value.find(m_delimiterMessage) != std::string::npos)
    {
        NS_FATAL_ERROR("ERROR: Gateway::SetValue called with a value containing the protocol message delimiter");
    }
    m_data[index] = value;
}


void
TextGateway::RunThread()
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
                NS_LOG_LOGIC("\t...data received");
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
            Simulator::ScheduleWithContext(GetContext(), Time(0), MakeEvent(&TextGateway::Stop, this));
            break; // prevent additional receive attempts
        }

        std::string receivedMessage = receivedData.substr(0, messageSize);
        m_messageBuffer = receivedData.substr(messageSize + m_delimiterMessage.size());

        NS_LOG_DEBUG("Forwarding new message: " << receivedMessage);
        {   // critical section start
            std::unique_lock lock(GetMessageQueueMutex());
            GetMessageQueue().push(receivedMessage);    
        }   // critical section end
        Simulator::ScheduleWithContext(GetContext(), Time(0), MakeEvent(&TextGateway::ForwardUp, this));
    }
}


std::string TextGateway::PopulateResponseMessage() const
{
    std::string message;
    for (uint32_t i = 0; i < m_data.size(); i++)
    {
        if (i != 0)
        {
            message += m_delimiterField;
        }
        message += m_data[i];
    }
    message += m_delimiterMessage;
    return message;
}

/* ========== PRIVATE MEMBER FUNCTIONS ====================================== */

void
TextGateway::ForwardUp()
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
    NS_LOG_DEBUG("Processing message: " << message);

    // split the message into values
    size_t index;
    std::vector<std::string> values;
    while ((index = message.find(m_delimiterField)) != std::string::npos)
    {
        values.push_back(message.substr(0, index));
        message.erase(0, index + m_delimiterField.size());
    }
    values.push_back(message);

    // remove the timestamp header
    Time timestamp;
    try
    {
        const std::string & seconds = values.at(0);     // int32 represented as string
        const std::string & nanoseconds = values.at(1); // uint32 represented as string
        timestamp = Seconds(std::stoi(seconds)) + NanoSeconds(std::stol(nanoseconds));
        NS_LOG_DEBUG("Received time: " << timestamp);
    }
    catch (std::exception & e)
    {
        NS_FATAL_ERROR("ERROR: received invalid message header");
    }
    values.erase(values.begin(), values.begin()+2);

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
        Simulator::ScheduleNow(&TextGateway::DoInitialize, this, values);
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
        Simulator::Schedule(timeDelta, &TextGateway::HandleUpdate, this, values);
    }
}

void
TextGateway::HandleUpdate(const std::vector<std::string> & data)
{
    NS_LOG_FUNCTION(this << data);

    if (Simulator::Now() == GetTimePause())
    {
        NS_LOG_LOGIC("Waiting for next update...");
        SetEventWait(Simulator::ScheduleNow(&TextGateway::WaitForNextUpdate, this));
    }
    DoUpdate(data);
}

} // namespace ns3
