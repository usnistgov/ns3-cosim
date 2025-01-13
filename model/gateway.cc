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


namespace ns3 {
    NS_LOG_COMPONENT_DEFINE("Gateway");

    Gateway::Gateway(): isInitialized(false),destroyEvent()  {
        NS_LOG_FUNCTION(this);
        memset(buffer, 0, sizeof(buffer));
        bufferIndex = 0;
        // for(int i = 0; i < 4; ++i)  { ???
        //     queueData.push("0.0");
        // }
        m_node = Simulator::GetContext();
        
    }

    int Gateway::initialize(const char *address, int portNum)    {
        NS_LOG_FUNCTION(this);
        if(isInitialized == false)  {
            //STILL NEED TO IMPLEMENT NS3 LOGGER
            NS_LOG_INFO("Gateway is initializing");

            connectToServer(address, portNum);
        
            if (!destroyEvent.IsRunning())
            {
                destroyEvent = Simulator::ScheduleDestroy(&Gateway::stop, this);
            }

            //Setup Listener Thread
            listenerThread = thread(&Gateway::listener, this);

            isInitialized = true;
        }
        else    {   NS_LOG_LOGIC("Gateway is already initialized");   }

        return 0;
    } 
    int Gateway::connectToServer(const char *address, int portNum) {
        NS_LOG_FUNCTION(this);
        //create the socket
        if ((clientFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            NS_LOG_ERROR("Socket Creation Error");
            return -2;
        }

        //setup the server address
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(portNum);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if (inet_pton(AF_INET, address, &serverAddress.sin_addr)
            <= 0) {
            NS_LOG_ERROR("Invalid address/ Address not supported");
            return -3;
        }
                
        //connect to the server
        if (connect(clientFD, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
            NS_LOG_ERROR("Connection Failed");
            return -4;
        }
        NS_LOG_INFO("Connected to Server");
        return 0;

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
        int bytesReceived = 0;

        for(;;) {
            bufferIndex = 0;

            while (bufferIndex < BUFFER_LENGTH && (bytesReceived = recv(clientFD, &buffer[bufferIndex], BUFFER_LENGTH - bufferIndex, 0)) > 0){
                bufferIndex += bytesReceived;
                
                if (bufferIndex > 1 && '\r' == buffer[bufferIndex-2] && '\n' == buffer[bufferIndex-1])    {
                    buffer[bufferIndex-1] = '\0';
                    buffer[bufferIndex-2] = '\0';
                    NS_LOG_INFO("Received Data: " << buffer);
                    receive_callback(buffer);
                    break;
                }
            }
            if(bytesReceived == 0)   {
                NS_LOG_INFO("Connection Closed");
                break;
            }
            if(bytesReceived == -1)   {
                NS_LOG_ERROR("Error Reading Data");
                break;
            }
            if(killListener)    {
                break;
            }            
        }
       
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
    
    void Gateway::receive_callback(char* received_data)   {
        NS_LOG_FUNCTION(this);
        if(received_data == NULL)   {
            NS_LOG_ERROR("Received Data is NULL");
            return;
        }
        string stringData = received_data;
        
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
};
