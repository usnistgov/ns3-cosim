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
*/

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "gateway.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Gateway");

/* ========== PUBLIC MEMBER FUNCTIONS ======================================= */

Gateway::Gateway(uint32_t dataSize, const std::string & delimiterField, const std::string & delimiterMessage):
    m_eventWait(),
    m_eventDestroy(),
    m_timeStart(Seconds(-1)),
    m_timePause(Seconds(0)),
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

    m_context = Simulator::GetContext();
    m_state   = STATE::CREATED;
}

void
Gateway::Connect(const std::string & serverAddress, uint16_t serverPort)
{
    NS_LOG_FUNCTION(this << serverAddress << serverPort);

    if (m_state != STATE::CREATED) // prevent duplicate calls
    {
        NS_FATAL_ERROR("ERROR: Gateway::Connect was called multiple times");
    }

    // create the client socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0)
    {
        NS_FATAL_ERROR("ERROR: Gateway::Connect failed to create a socket");
    }

    // set the server address
    struct sockaddr_in socketAddress;
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverAddress.c_str(), &socketAddress.sin_addr) <= 0)
    {
        NS_FATAL_ERROR("ERROR: Gateway::Connect failed to resolve the address " << serverAddress);
    }

    // connect to the server
    if (connect(m_socket, (struct sockaddr *)&socketAddress, sizeof(socketAddress)) < 0)
    {
        NS_FATAL_ERROR("ERROR: Gateway::Connect failed to connect to "
            << serverAddress << ":" << serverPort << " (check if the server is running)"
        );
    }

    m_state = STATE::CONNECTED; // this must be set before RunThread
    NS_LOG_INFO("Gateway connected to " << serverAddress << ":" << serverPort);

    // schedule a function to stop the socket thread when ns-3 ends
    m_eventDestroy = Simulator::ScheduleDestroy(&Gateway::Stop, this);

    // start a thread to handle the socket connection
    m_thread = std::thread(&Gateway::RunThread, this);

    // wait until the thread forwards the next received message
    NS_LOG_LOGIC("waiting for next update...");
    m_eventWait = Simulator::ScheduleNow(&Gateway::WaitForNextUpdate, this);
}

void
Gateway::SetValue(uint32_t index, const std::string & value)
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
Gateway::SendResponse()
{
    NS_LOG_FUNCTION(this);

    if (m_state != STATE::CONNECTED)
    {
        NS_FATAL_ERROR("ERROR: Gateway::SendResponse called without an active connection to the server");
    }

    std::string message = "";
    for (uint32_t i = 0; i < m_data.size(); i++)
    {
        if (i != 0)
        {
            message += m_delimiterField;
        }
        message += m_data[i];
    }
    NS_LOG_DEBUG("Gateway sending the message: " << message);
    message += m_delimiterMessage;

    if (send(m_socket, message.c_str(), message.size(), 0) == -1)
    {
        NS_LOG_WARN("WARNING: Gateway::SendResponse failed to send the message: " << message);
    }
}

/* ========== PRIVATE MEMBER FUNCTIONS ====================================== */

void
Gateway::Stop() // how does this interact with NS_FATAL_ERROR ?
{
    NS_LOG_FUNCTION(this);

    bool connected = (m_state == STATE::CONNECTED);

    m_state = STATE::STOPPING; // must set before m_thread.join() for the thread to exit

    if (connected)
    {
        if (m_thread.joinable())
        {
            NS_LOG_LOGIC("waiting for the gateway thread to stop...");
            m_thread.join(); // wait for the thread to stop
            NS_LOG_LOGIC("...gateway thread stopped.");
        }
        close(m_socket);
    }

    if (m_eventWait.IsPending())
    {
        m_eventWait.Cancel();
        NS_LOG_DEBUG("wait event cancelled");
    }

    if (m_eventDestroy.IsPending()) // if Gateway::Stop was called before Simulator::Stop
    {
        m_eventDestroy.Cancel();
        NS_LOG_DEBUG("destroy event cancelled");
    }

    NS_LOG_INFO("Gateway stopped");
}

void
Gateway::RunThread()
{
    NS_LOG_FUNCTION(this);

    const size_t BUFFER_SIZE = 4096;
    char recvBuffer[BUFFER_SIZE];   // buffer for recv call
    std::string receivedData;       // received message content
    size_t messageSize;             // received message size (excluding the message delimiter)

    while (m_state == STATE::CONNECTED)
    {
        receivedData = m_messageBuffer;  // recover any partially received message

        // this loop has the following possible outcomes:
        //  messageSize = receivedData.size() - m_delimiterMessage.size()   [received one message]
        //  messageSize < receivedData.size() - m_delimiterMessage.size()   [received more than one message]
        //  messageSize = std::string::npos                                 [unable to receive messages]
        while ((messageSize = receivedData.find(m_delimiterMessage)) == std::string::npos)
        {
            NS_LOG_LOGIC("\twaiting to receive data...");
            int bytesReceived = recv(m_socket, &recvBuffer[0], BUFFER_SIZE, 0);

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
            Simulator::ScheduleWithContext(m_context, Time(0), MakeEvent(&Gateway::Stop, this));
            break; // prevent additional receive attempts
        }

        std::string receivedMessage = receivedData.substr(0, messageSize);
        m_messageBuffer = receivedData.substr(messageSize + m_delimiterMessage.size());

        NS_LOG_DEBUG("forwarding new message: " << receivedMessage);
        {   // critical section start
            std::unique_lock lock(m_messageQueueMutex);
            m_messageQueue.push(receivedMessage);    
        }   // critical section end
        Simulator::ScheduleWithContext(m_context, Time(0), MakeEvent(&Gateway::ForwardUp, this));
    }
}

void
Gateway::WaitForNextUpdate() // do not add log output to this function
{
    if (m_state != STATE::STOPPING) // this probably isn't necessary
    {
        if (m_eventWait.IsPending())
        {
            NS_LOG_WARN("WARNING: Gateway::WaitForNextUpdate scheduled multiple times"); // except this one!
            m_eventWait.Cancel();
        }
        // pause Simulator time progression until this event is cancelled
        m_eventWait = Simulator::ScheduleNow(&Gateway::WaitForNextUpdate, this);
    }
}

void
Gateway::ForwardUp()
{
    NS_LOG_FUNCTION(this);

    // get the message to process
    std::string message;
    {   // critical section start
        std::unique_lock lock(m_messageQueueMutex);
        if (m_messageQueue.empty())
        {
            NS_FATAL_ERROR("Gateway::ForwardUp called without any queued messages");
        }
        message = m_messageQueue.front();
        m_messageQueue.pop();
    }   // critical section end
    NS_LOG_DEBUG("processing message: " << message);

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
        NS_LOG_DEBUG("received time: " << timestamp);
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
    else if (m_timeStart.IsStrictlyNegative()) // first value received
    {
        m_timeStart = timestamp;
        NS_LOG_INFO("Gateway reference time set as " << timestamp);
        Simulator::ScheduleNow(&Gateway::DoInitialize, this, values);
    }
    else // normal message
    {
        if (m_eventWait.IsPending())
        {
            m_eventWait.Cancel();
        }
        NS_LOG_LOGIC("...update received for " << timestamp);

        // calculate the time difference
        m_timePause = timestamp - m_timeStart;
        ns3::Time timeDelta = m_timePause - Simulator::Now();
        if (timeDelta.IsStrictlyNegative()) // 0 allowed
        {
            NS_FATAL_ERROR("ERROR: received timestamps were not increasing values");
        }
        NS_LOG_INFO("advancing time from " << Simulator::Now() << " to " << m_timePause);
        Simulator::Schedule(timeDelta, &Gateway::HandleUpdate, this, values);
    }
}

void
Gateway::HandleUpdate(const std::vector<std::string> & data)
{
    NS_LOG_FUNCTION(this << data);

    if (Simulator::Now() == m_timePause)
    {
        NS_LOG_LOGIC("waiting for next update...");
        m_eventWait = Simulator::ScheduleNow(&Gateway::WaitForNextUpdate, this);
    }
    DoUpdate(data);
}

} // namespace ns3
