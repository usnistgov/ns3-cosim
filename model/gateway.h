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

using namespace std;

namespace ns3 {

    //My class
    class Gateway   {


        public:
            Gateway();
            
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
            
            const size_t BUFFER_LENGTH = 1024;
            char buffer[1024];
            size_t bufferIndex;
            queue<string> queueData;
            
            int connectToServer(const char *address, int portNum);      

            /*
            stores incoming data into a data structure
            */
            void receive_callback(char* received_data);

            virtual void processData(string data) = 0;
    
            mutex m_lock;
            bool killListener = false;
            const double SIDELINKDURATION = 1.0;
            bool braking = false;
            bool serverTermination = false;
    };
}
#endif // GATEWAY_H