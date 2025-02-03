#ifndef GATEWAY_H
#define GATEWAY_H
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <sstream>

//My imports
#include <thread>
#include <mutex>
#include <fstream>
#include <iostream>
#include <queue>

#include <string>

using namespace std;

namespace ns3 {

    //My class
    class Gateway   {


        public:
            Gateway();

            ~Gateway();
            
            /*
            Connects to the "server" socket
            initiates a thread to run listen() 
            */
            int initialize(const char *address, int portNum);

            void sendData(int socketFD, const string& data);

            /*
            Listens for incoming packets indefinetly
            */
            void listener();
            
            /*
            Pops data from the datastructure
            Then sends data (x/y/z/t_brake) to ns-3 nodes/apps
            */
            void forward_up();

            /*
            closes all open resources encapsulated by the gateway
            */
            void stop();

            virtual void notify();

            bool isServerTermination();

            void Connect(const std::string & serverAddress, int serverPort);

        protected:
            int status;
            int valread;
            int clientFD;
            bool isInitialized;
            EventId destroyEvent;
            mutex queueLock;
            uint32_t m_node; 
            struct sockaddr_in serverAddress;
            thread listenerThread;
            
            static const size_t BUFFER_LENGTH = 1024;
            char buffer[BUFFER_LENGTH];
            size_t bufferIndex;
            queue<string> queueData;
            
            int connectToServer(const char *address, int portNum);      

            /*
            stores incoming data into a data structure
            */
            void receive_callback(std::string stringData);

            virtual void processData(string data) = 0;
    
            mutex m_lock;
            bool killListener = false;
            const double SIDELINKDURATION = 1.0;
            bool braking = false;
            bool serverTermination = false;
        private:
            enum STATE
            {
                CREATED,
                CONNECTED,
                STOPPING,
                STOPPED
            };

            bool CreateSocketConnection(const std::string & serverAddress, int serverPort);

            void RunThread();
            void StopThread();

            STATE m_state;
            bool m_isRunning;
            uint32_t m_nodeId;
            int m_clientSocket;
            std::thread m_thread;
            std::string m_message;
            EventId m_destroyEvent;
    };
}
#endif // GATEWAY_H