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

#include "ns3/applications-module.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>

#include <iostream>

#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

#include "gateway.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Gateway");

Gateway::Gateway():
    m_timeStart(Seconds(-1)),
    m_timePause(Seconds(0)),
    m_messageEndToken("\r\n"),
    m_messageDelimiter(" "),
    m_destroyEvent()
{
    NS_LOG_FUNCTION(this);

    m_nodeId = Simulator::GetContext();
    m_state = STATE::CREATED;
}

void
Gateway::Connect(const std::string & serverAddress, int serverPort, size_t dataSize)
{
    NS_LOG_FUNCTION(this << serverAddress << serverPort);

    if (m_state != STATE::CREATED) // prevent duplicate calls
    {
        NS_LOG_ERROR("ERROR: failed to connect the gateway to "
            << serverAddress << ":" << serverPort
            << " due to a previous connection attempt"
        );
        return;
    }

    m_data.resize(dataSize + 2); // + header size

    if (CreateSocketConnection(serverAddress, serverPort))
    {
        m_state = STATE::CONNECTED; // set before RunThread
        NS_LOG_INFO("Connected the gateway to "
            << serverAddress << ":" << serverPort
        );

        // schedule a function to stop the socket thread when ns-3 ends
        m_destroyEvent = Simulator::ScheduleDestroy(&Gateway::StopThread, this);

        // start a thread to handle the socket connection
        m_thread = std::thread(&Gateway::RunThread, this);

        // start time management
        NS_LOG_INFO("waiting for next update...");
        m_waitEvent = Simulator::ScheduleNow(&Gateway::WaitForNextUpdate, this);
    }
}

void
Gateway::WaitForNextUpdate()
{
    // pause ns-3 time progression until this event is cancelled
    m_waitEvent = Simulator::ScheduleNow(&Gateway::WaitForNextUpdate, this);
}

void
Gateway::SetDelimiter(const std::string & delimiter)
{
    NS_LOG_FUNCTION(this);
    m_messageDelimiter = delimiter;
}

void
Gateway::SetMessageEnd(const std::string & endToken)
{
    NS_LOG_FUNCTION(this);
    m_messageEndToken = endToken;
}

void
Gateway::SetValue(size_t index, std::string value)
{
    NS_LOG_FUNCTION(this << index << value);

    if (index <= m_data.size())
    {
        NS_LOG_DEBUG("DATA UPDATE: " << m_data[index] << " updated to " << value);
        m_data[index] = value;
    }
    else
    {
        NS_LOG_ERROR("ERROR: SetValue index of " << index << " exceeds message size of " << m_data.size());
    }
}

bool
Gateway::CreateSocketConnection(const std::string & serverAddress, int serverPort)
{
    NS_LOG_FUNCTION(this << serverAddress << serverPort);

    // create the client socket
    m_clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_clientSocket < 0)
    {
        NS_LOG_ERROR("ERROR: failed to create socket");
        return false;
    }

    // set the server address
    struct sockaddr_in socketAddress;
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverAddress.c_str(), &socketAddress.sin_addr) <= 0)
    {
        NS_LOG_ERROR("ERROR: failed to resolve the address " << serverAddress);
        return false;
    }

    // connect to the server
    if (connect(m_clientSocket, (struct sockaddr *)&socketAddress, sizeof(socketAddress)) < 0)
    {
        NS_LOG_ERROR("ERROR: failed to connect to "
            << serverAddress << ":" << serverPort
            << " (check if the server is online)"
        );
        return false;
    }

    return true;
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

        close(m_clientSocket);
    }
    else
    {
        NS_LOG_LOGIC("Gateway::StopThread skipped - the gateway is not connected");
    }

    if (m_waitEvent.IsPending())
    {
        m_waitEvent.Cancel();
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
        // it schedules the function for this time step on behalf of the main thread's m_nodeId
        // then, Simulator::Run from the main thread will process the scheduled function
        if (message.empty()) // TODO: test whether this works as intended
        {
            NS_LOG_ERROR("Stopping the gateway thread due to a receive error");
            Simulator::ScheduleWithContext(m_nodeId, Time(0), MakeEvent(&Gateway::StopThread, this));
            break; // prevent additional receive attempts
        }
        else
        {   // critical section start
            std::unique_lock lock(m_messageQueueMutex);
            NS_LOG_LOGIC("Pushing a new message to the gateway message queue");
            m_messageQueue.push(message);
            Simulator::ScheduleWithContext(m_nodeId, Time(0), MakeEvent(&Gateway::ForwardUp, this));
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

    while ((separatorIndex = message.find(m_messageEndToken)) == std::string::npos)
    {
        int bytesReceived = recv(m_clientSocket, &recvBuffer[0], BUFFER_SIZE, 0);

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

    m_messageBuffer = message.substr(separatorIndex + m_messageEndToken.size());
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
    while ((index = message.find(m_messageDelimiter)) != std::string::npos)
    {
        receivedData.push_back(message.substr(0, index));
        message.erase(0, index + m_messageDelimiter.size());
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
        Simulator::ScheduleNow(&Gateway::HandleInitialize, this, receivedData);
    }
    else
    {
        if (m_waitEvent.IsPending()) // TODO: move
        {
            m_waitEvent.Cancel();
        }
        NS_LOG_INFO("...update received for " << receivedTime);

        ns3::Time timeDelta = receivedTime - m_timeStart; // TODO: check if negative
        m_timePause = Simulator::Now() + timeDelta;
        NS_LOG_INFO("advancing time from " << Simulator::Now() << " to " << m_timePause);
        Simulator::Schedule(timeDelta, &Gateway::HandleUpdate, this, receivedData);
    }
}

void
Gateway::HandleUpdate(std::vector<std::string> data)
{
    NS_LOG_FUNCTION(this << data);

    DoUpdate(data);
    SendResponse();

    if (Simulator::Now() == m_timePause)
    {
        NS_LOG_INFO("waiting for next update...");
        m_waitEvent = Simulator::ScheduleNow(&Gateway::WaitForNextUpdate, this);
    }
}

void
Gateway::HandleInitialize(std::vector<std::string> data)
{
    NS_LOG_FUNCTION(this << data);
    DoInitialize(data);
    SendResponse();
}

void
Gateway::SendResponse() // TODO: add time stamp
{
    NS_LOG_FUNCTION(this);

    std::string message = "";
    for (size_t i = 0; i < m_data.size(); i++)
    {
        if (i != 0)
        {
            message += m_messageDelimiter;
        }
        message += m_data[i];
    }
    message += m_messageEndToken;

    send(m_clientSocket, message.c_str(), message.size(), 0); // check for errors
}

} // namespace ns3
