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
using namespace std;

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Gateway");

Gateway::Gateway():
    m_destroyEvent()
{
    NS_LOG_FUNCTION(this);

    m_nodeId = Simulator::GetContext();
    m_state = STATE::CREATED;
}

Gateway::~Gateway()
{
    NS_LOG_FUNCTION(this);
    StopThread();
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
        // schedule a function to stop the socket thread when ns-3 ends
        m_destroyEvent = Simulator::ScheduleDestroy(&Gateway::StopThread, this);

        // start a thread to handle the socket connection
        m_thread = thread(&Gateway::RunThread, this);

        // start time management (TODO)

        m_state = STATE::CONNECTED;
        NS_LOG_INFO("Connected the gateway to "
            << serverAddress << ":" << serverPort
        );
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
Gateway::RunThread()
{
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
}

    void Gateway::sendData(int socketFD, const string& data)    {
        NS_LOG_FUNCTION(this);
        NS_LOG_INFO("Sending Data");
        if(-1==send(socketFD, data.c_str(), data.length(), 0)) {
            NS_LOG_ERROR("Error Sending Data");
        }
    }

    void Gateway::listener() {
        NS_LOG_FUNCTION(this);
        NS_LOG_INFO("Listener Thread Started");

        while (!killListener)
        {
            bool socketError = false;
            bool messageReceived = false;
            m_message.clear();
            do
            {
                int bytesReceived = recv(clientFD, &buffer[0], BUFFER_LENGTH, 0);

                if (bytesReceived < 0)
                {
                    NS_LOG_ERROR("socket connection error");
                    socketError = true;
                }
                else if (bytesReceived == 0)
                {
                    NS_LOG_ERROR("socket terminated");
                    socketError = true;
                }
                else
                {
                    m_message.append(&buffer[0], bytesReceived); // size_t

                    if (m_message.size() >= 4 && m_message.compare(m_message.size() - 1, 1, "\n") == 0)
                    {
                        messageReceived = true; // need to look for termintaing
                    }
                }
            } while(!messageReceived && !socketError);

            if (messageReceived)
            {
                NS_LOG_INFO("Received Data: " << m_message);
                receive_callback(m_message);
            }
        }
        NS_LOG_INFO("listener stopped");
    }

    void Gateway::stop() {
        NS_LOG_FUNCTION(this);
        NS_LOG_INFO("Stopping Gateway");

        killListener = true;

        // wait for listenerThread to wrap up
        if (listenerThread.joinable())
        {
            listenerThread.join();
        }

        close(clientFD);
    }
    
    void Gateway::receive_callback(std::string stringData)   {
        NS_LOG_FUNCTION(this);
        
        queueLock.lock();
        queueData.push(stringData);
        queueLock.unlock();

        Simulator::ScheduleWithContext(m_node, Time(0), MakeEvent(&Gateway::forward_up, this));

    }

    void Gateway::forward_up()  {
        NS_LOG_FUNCTION(this);

        queueLock.lock();  
        if(queueData.empty())  { 
            queueLock.unlock();
            return; 
        }
        string stringData = queueData.front();
        NS_LOG_INFO("size: " << queueData.size());
        queueData.pop();
        queueLock.unlock();

        if(stringData == "-1")  {
            serverTermination = true; // maybe unnecessary with Stop() ?
            Simulator::Stop();
            return;
        }
        processData(stringData);
    }

    void Gateway::notify() {
        char continueMessage = '1';  
        send(clientFD, &continueMessage, sizeof(continueMessage), 0);//signal to server listener is ready
    }

    bool Gateway::isServerTermination()  {
        return serverTermination;
    }

} // namespace ns3
