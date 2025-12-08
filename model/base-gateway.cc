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

#include "base-gateway.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BaseGateway");

/* ========== PUBLIC MEMBER FUNCTIONS ======================================= */

BaseGateway::BaseGateway():
    m_eventWait(),
    m_eventDestroy(),
    m_timeStart(Seconds(-1)),
    m_timePause(Seconds(0))
{
    NS_LOG_FUNCTION(this);

    m_context = Simulator::GetContext();
    m_state   = STATE::CREATED;
}

void
BaseGateway::Connect(const std::string & serverAddress, uint16_t serverPort)
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
    m_eventDestroy = Simulator::ScheduleDestroy(&BaseGateway::Stop, this);

    // start a thread to handle the socket connection
    m_thread = std::thread(&BaseGateway::RunThread, this);

    // wait until the thread forwards the next received message
    NS_LOG_LOGIC("Waiting for next update...");
    m_eventWait = Simulator::ScheduleNow(&BaseGateway::WaitForNextUpdate, this);
}


void
BaseGateway::SendResponse()
{
    NS_LOG_FUNCTION(this);

    if (m_state != STATE::CONNECTED)
    {
        NS_FATAL_ERROR("ERROR: Gateway::SendResponse called without an active connection to the server");
    }

    std::string message = PopulateResponseMessage();
    NS_LOG_DEBUG("Gateway sending the message: " << message);

    if (send(m_socket, message.c_str(), message.size(), 0) == -1)
    {
        NS_LOG_WARN("WARNING: Gateway::SendResponse failed to send the message: " << message);
    }
}

NodeContainer BaseGateway::GetNodes()
{
    return NodeContainer::GetGlobal();
}

/* ========== PROTECTED MEMBER FUNCTIONS ====================================== */

BaseGateway::STATE BaseGateway::GetState() const
{
    return m_state;
}

int BaseGateway::GetSocket() const
{
    return m_socket;
}

uint32_t BaseGateway::GetContext() const
{
    return m_context;
}

Time BaseGateway::GetTimeStart() const
{
    return m_timeStart;
}

void BaseGateway::SetTimeStart(Time timeStart)
{
    m_timeStart = timeStart;
}

Time BaseGateway::GetTimePause() const
{
    return m_timePause;
}

void BaseGateway::SetTimePause(Time timePause)
{
    m_timePause = timePause;
}

EventId BaseGateway::getEventWait() const
{
    return m_eventWait;
}

void BaseGateway::SetEventWait(EventId eventId)
{
    m_eventWait = eventId;
}

std::queue<std::string>& BaseGateway::GetMessageQueue()
{
    return m_messageQueue;
}

std::mutex& BaseGateway::GetMessageQueueMutex()
{
    return m_messageQueueMutex;
}

/* ========== PRIVATE MEMBER FUNCTIONS ====================================== */

void
BaseGateway::Stop() // how does this interact with NS_FATAL_ERROR ?
{
    NS_LOG_FUNCTION(this);

    bool connected = (m_state == STATE::CONNECTED);

    m_state = STATE::STOPPING; // must set before m_thread.join() for the thread to exit

    if (connected)
    {
        if (m_thread.joinable())
        {
            NS_LOG_LOGIC("Waiting for the gateway thread to stop...");
            m_thread.join(); // wait for the thread to stop
            NS_LOG_LOGIC("...gateway thread stopped.");
        }
        close(m_socket);
    }

    if (m_eventWait.IsPending())
    {
        m_eventWait.Cancel();
        NS_LOG_DEBUG("Wait event cancelled");
    }

    if (m_eventDestroy.IsPending()) // if Gateway::Stop was called before Simulator::Stop
    {
        m_eventDestroy.Cancel();
        NS_LOG_DEBUG("Destroy event cancelled");
    }

    NS_LOG_INFO("Gateway stopped");
}

void
BaseGateway::WaitForNextUpdate() // do not add log output to this function
{
    if (m_state != STATE::STOPPING) // this probably isn't necessary
    {
        if (m_eventWait.IsPending())
        {
            NS_LOG_WARN("WARNING: Gateway::WaitForNextUpdate scheduled multiple times"); // except this one!
            m_eventWait.Cancel();
        }
        // pause Simulator time progression until this event is cancelled
        m_eventWait = Simulator::ScheduleNow(&BaseGateway::WaitForNextUpdate, this);
    }
}

} // namespace ns3
