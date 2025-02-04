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

    const char END_TOKEN[] = "\r\n\r\n";
    const size_t BUFFER_LENGTH = 4096;
    char recvBuffer[BUFFER_LENGTH];

    std::string message = "";

    do
    {
        int bytesReceived = recv(m_clientSocket, &recvBuffer[0], BUFFER_LENGTH, 0);

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
    while(!StringEndsWith(message, END_TOKEN));

    NS_LOG_INFO("gateway received the message: " << message);

    return message; // message received
}

bool
Gateway::StringEndsWith(const std::string & str, const char * token)
{
    const size_t TOKEN_LENGTH = strlen(token);

    if (str.size() >= TOKEN_LENGTH)
    {
        if (str.compare(str.size() - TOKEN_LENGTH, TOKEN_LENGTH, token) == 0)
        {
            return true;
        }
    }
    return false;
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

    const char DATA_SEPERATOR[] = "\r\n";
    const char VALUE_SEPERATOR[] = " ";

    Time clockValue = Time(-1);
    std::map<std::string, std::string> content;

    // GIVEN ReceiveNextMessage returns a string that ends with \r\n\r\n
    // GIVEN m_messageQueue only contains values returned by ReceiveNextMessage
    // THEN  This while loop will eventually result in message == \r\n
    NS_LOG_DEBUG("Processing message...");
    while (message != DATA_SEPERATOR)
    {
        size_t index = message.find(DATA_SEPERATOR); // cannot be std::string::npos
        std::string data = message.substr(0, index);
        message.erase(0, index + strlen(DATA_SEPERATOR));

        index = data.find(VALUE_SEPERATOR);
        if (index == std::string::npos)
        {
            NS_LOG_ERROR("ERROR: bad message format, " << data);
            return;
        }

        std::string first = data.substr(0, index);
        
        std::string second;
        if (index == data.size() - strlen(VALUE_SEPERATOR)) // the last character
        {
            second = "";
        }
        else
        {
            second = data.substr(index + strlen(VALUE_SEPERATOR));
        }

        NS_LOG_DEBUG("\textracted: (" << first << "," << second << ")");

        if (clockValue.IsNegative()) // first iteration
        {
            try
            {
                // the two values will be signed int32
                clockValue = Seconds(std::stoi(first)) + NanoSeconds(std::stoi(second));
            }
            catch (std::exception & e)
            {
                NS_LOG_ERROR("ERROR: bad message timestamp");
                return;
            }
        }
        else
        {
            if (content.count(first) > 0)
            {
                NS_LOG_ERROR("ERROR: message contains duplicate values");
            }
            content[first] = second;
        }
    }
    NS_LOG_DEBUG("...message processed");

    // if clock negative then terminate
    // calculate the time_delta (if negative, error)
    // schedule processing content at time_delta
    // somewhere, cancel pending wait events
}

} // namespace ns3
