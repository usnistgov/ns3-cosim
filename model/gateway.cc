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

#include "ns3/log.h"
#include "ns3/simulator.h"

#include "gateway.h"




#include "ns3/applications-module.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cstdlib>

#include <iostream>

#include "ns3/node.h"

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
    if (delimiterField == delimiterMessage)
    {
        NS_FATAL_ERROR("ERROR: gateway message and field delimiters must have different values");
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
    m_eventDestroy = Simulator::ScheduleDestroy(&Gateway::StopThread, this);

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
    message += m_delimiterMessage;

    if (send(m_socket, message.c_str(), message.size(), 0) > 0)
    {
        NS_LOG_DEBUG("Gateway sent the message: " << message);
    }
    else
    {
        NS_LOG_WARN("WARNING: Gateway::SendResponse failed to send the message: " << message);
    }
}

/* ========== PRIVATE MEMBER FUNCTIONS ====================================== */



void
Gateway::WaitForNextUpdate()
{
    // pause ns-3 time progression until this event is cancelled
    m_eventWait = Simulator::ScheduleNow(&Gateway::WaitForNextUpdate, this);
}

void
Gateway::StopThread()
{
    NS_LOG_FUNCTION(this);

    if (m_state == STATE::CONNECTED)
    {
        m_state = STATE::STOPPING; // trigger the thread to stop

        if (m_thread.joinable())
        {
            NS_LOG_INFO("Waiting for the gateway thread to stop...");
            m_thread.join(); // wait for the thread to stop
            NS_LOG_INFO("...gateway thread stopped.");
        }

        close(m_socket);
    }
    else
    {
        NS_LOG_LOGIC("Gateway::StopThread skipped - the gateway is not connected");
    }

    if (m_eventWait.IsPending())
    {
        m_eventWait.Cancel();
        NS_LOG_INFO("Cancelled a pending WaitForNextUpdate");
    }
}

void
Gateway::RunThread()
{
    NS_LOG_FUNCTION(this);

    while (m_state == STATE::CONNECTED)
    {
        std::string message = ReceiveNextMessage();

        // the RunThread needs the main thread to execute a function (either StopThread or ForwardUp)
        // it schedules the function for this time step on behalf of the main thread's m_context
        // then, Simulator::Run from the main thread will process the scheduled function
        if (message.empty()) // TODO: test whether this works as intended
        {
            NS_LOG_ERROR("Stopping the gateway thread due to a receive error");
            Simulator::ScheduleWithContext(m_context, Time(0), MakeEvent(&Gateway::StopThread, this));
            break; // prevent additional receive attempts
        }
        else
        {   // critical section start
            std::unique_lock lock(m_messageQueueMutex);
            NS_LOG_LOGIC("Pushing a new message to the gateway message queue");
            m_messageQueue.push(message);
            Simulator::ScheduleWithContext(m_context, Time(0), MakeEvent(&Gateway::ForwardUp, this));
        }   // critical section end
    }
}

std::string
Gateway::ReceiveNextMessage()
{
    NS_LOG_FUNCTION(this);
    
    const size_t BUFFER_SIZE = 4096;
    char recvBuffer[BUFFER_SIZE];

    std::string message = m_messageBuffer;
    size_t separatorIndex;

    m_messageBuffer.clear();

    while ((separatorIndex = message.find(m_delimiterMessage)) == std::string::npos)
    {
        int bytesReceived = recv(m_socket, &recvBuffer[0], BUFFER_SIZE, 0);

        if (bytesReceived > 0)
        {
            message.append(&recvBuffer[0], bytesReceived);
        }
        else if (bytesReceived == 0)
        {
            NS_LOG_ERROR("ERROR: gateway socket terminated ");
            return ""; // message not received
        }
        else
        {
            NS_LOG_ERROR("ERROR: gateway socket connection error");
            return ""; // message not received
        }
    }

    m_messageBuffer = message.substr(separatorIndex + m_delimiterMessage.size());
    message.erase(separatorIndex);
        
    NS_LOG_INFO("gateway received the message: " << message);

    return message; // message received
}

void
Gateway::ForwardUp()
{
    NS_LOG_FUNCTION(this);

    std::string message;
    {   // critical section start
        std::unique_lock lock(m_messageQueueMutex);
        message = m_messageQueue.front();
        m_messageQueue.pop();
    }   // critical section end

    std::vector<std::string> receivedData;

    size_t index;
    while ((index = message.find(m_delimiterField)) != std::string::npos)
    {
        receivedData.push_back(message.substr(0, index));
        message.erase(0, index + m_delimiterField.size());
    }
    receivedData.push_back(message);

    if (receivedData.size() < 2)
    {
        NS_LOG_ERROR("ERROR: bad message format - missing header");
        return;
    }

    Time receivedTime;
    try
    {
        const std::string & seconds = receivedData[0];      // int32 represented as string
        const std::string & nanoseconds = receivedData[1];  // int32 represented as string
        receivedTime = Seconds(std::stoi(seconds)) + NanoSeconds(std::stoi(nanoseconds));
    }
    catch (std::exception & e)
    {
        NS_LOG_ERROR("ERROR: bad message format - invalid timestamp");
        return;
    }
    receivedData.erase(receivedData.begin(), receivedData.begin()+2);

    if (receivedTime.IsStrictlyNegative()) // terminate
    {
        NS_LOG_INFO("Received terminate: " << receivedTime);
        Simulator::Stop(); // TODO: check if working
    }
    else if (m_timeStart.IsStrictlyNegative()) // initial condition
    {
        m_timeStart = receivedTime;
        NS_LOG_INFO("Initialized Start Time as " << m_timeStart);
        Simulator::ScheduleNow(&Gateway::DoInitialize, this, receivedData);
    }
    else
    {
        if (m_eventWait.IsPending()) // TODO: move
        {
            m_eventWait.Cancel();
        }
        NS_LOG_INFO("...update received for " << receivedTime);

        m_timePause = receivedTime - m_timeStart; // TODO: check if negative
        ns3::Time timeDelta = m_timePause - Simulator::Now();
        NS_LOG_INFO("advancing time from " << Simulator::Now() << " to " << m_timePause);
        Simulator::Schedule(timeDelta, &Gateway::HandleUpdate, this, receivedData);
    }
}

void
Gateway::HandleUpdate(const std::vector<std::string> & data)
{
    NS_LOG_FUNCTION(this << data);

    if (Simulator::Now() == m_timePause)
    {
        NS_LOG_INFO("waiting for next update...");
        m_eventWait = Simulator::ScheduleNow(&Gateway::WaitForNextUpdate, this);
    }
    DoUpdate(data);
}

} // namespace ns3
