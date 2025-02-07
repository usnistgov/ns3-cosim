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

const char * Gateway::SEPARATOR_HEADER = "\r\n";
const char * Gateway::SEPARATOR_MESSAGE = "\r\n\r\n";

Gateway::Gateway():
    m_destroyEvent()
{
    NS_LOG_FUNCTION(this);

    m_nodeId = Simulator::GetContext();
    m_state = STATE::CREATED;
    m_startTime = Seconds(-1);
}

void
Gateway::Connect(const std::string & serverAddress, int serverPort)
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

    char recvBuffer[BUFFER_SIZE];

    std::string message = m_messageBuffer;
    size_t separatorIndex;

    m_messageBuffer.clear();

    while ((separatorIndex = message.find(SEPARATOR_MESSAGE)) == std::string::npos)
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

    m_messageBuffer = message.substr(separatorIndex + strlen(SEPARATOR_MESSAGE));
    message.erase(separatorIndex + strlen(SEPARATOR_MESSAGE));
        
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

    size_t index = message.find(SEPARATOR_HEADER);
    if (index == std::string::npos)
    {
        NS_LOG_ERROR("ERROR: bad message format - missing header");
        return;
    }

    std::string header  = message.substr(0, index);
    std::string data    = message.substr(index + strlen(SEPARATOR_HEADER));

    index = header.find(" ");
    if (index == std::string::npos)
    {
        NS_LOG_ERROR("ERROR: bad message format - invalid header");
        return;
    }

    Time receivedTime;
    try
    {
        std::string seconds = header.substr(0, index);      // int32 represented as string
        std::string nanoseconds = header.substr(index + 1); // int32 represented as string
        receivedTime = Seconds(std::stoi(seconds)) + NanoSeconds(std::stoi(nanoseconds));
        NS_LOG_DEBUG("interpreted '" << seconds << " " << nanoseconds << "' as " << receivedTime);
    }
    catch (std::exception & e)
    {
        NS_LOG_ERROR("ERROR: bad message format - invalid timestamp");
        return;
    }

    if (data.size() > strlen(SEPARATOR_MESSAGE))
    {
        data.erase(data.size() - strlen(SEPARATOR_MESSAGE));
    }
    else // data contains no content
    {
        data.clear();
    }

    if (m_waitEvent.IsPending()) // TODO: move
    {
        m_waitEvent.Cancel();
    }
    NS_LOG_INFO("...update received");

    if (receivedTime.IsStrictlyNegative()) // terminate
    {
        NS_LOG_INFO("Received terminate: " << receivedTime);
        // ??
    }
    else if (m_startTime.IsStrictlyNegative()) // initial condition
    {
        m_startTime = receivedTime;
        NS_LOG_INFO("Initialized Start Time as " << m_startTime);
        NS_LOG_INFO("waiting for next update...");
        m_waitEvent = Simulator::ScheduleNow(&Gateway::WaitForNextUpdate, this);
        Simulator::ScheduleNow(&Gateway::DoInitialize, this, data);
    }
    else
    {
        ns3::Time timeDelta = receivedTime - m_startTime; // TODO: check if negative
        NS_LOG_INFO("advancing time from " << Simulator::Now() << " to " << Simulator::Now() + timeDelta);
        Simulator::Schedule(timeDelta, &Gateway::HandleUpdate, this, data);    
    }
}

void
Gateway::HandleUpdate(std::string data)
{
    NS_LOG_FUNCTION(this << data);

    // TODO: skip wait if another event (stored in another time value) is pending
    NS_LOG_INFO("waiting for next update...");
    m_waitEvent = Simulator::ScheduleNow(&Gateway::WaitForNextUpdate, this);
    DoUpdate(data);
}

void
Gateway::SendData(const std::string & data)
{
    NS_LOG_FUNCTION(this << data);
    send(m_clientSocket, data.c_str(), data.size(), 0); // check for errors
}

} // namespace ns3
