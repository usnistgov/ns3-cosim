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
 * Author: Thomas Roth <thomas.roth@nist.gov>
*/

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "ns3/core-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleGatewayServer");

int
main(int argc, char* argv[])
{
    bool verboseLogs        = false;
    uint32_t timeStart      = 0;    // s
    uint32_t timeDelta      = 1;    // s
    uint32_t iterations     = 20;
    uint16_t numberOfNodes  = 3;
    uint16_t positionDeltaX = 25;   // m
    uint16_t serverPort     = 8000;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Enable/disable detailed log output", verboseLogs);
    cmd.AddValue("timeStart", "Starting simulation time in seconds", timeStart);
    cmd.AddValue("timeDelta", "Simulation step size in seconds", timeDelta);
    cmd.AddValue("iterations", "Number of time steps to simulate", iterations);
    cmd.AddValue("numberOfNodes", "Number of vehicle nodes to simulate", numberOfNodes);
    cmd.AddValue("positionDeltaX", "Maximum increase per time step to a node's x-coordinate", positionDeltaX);
    cmd.AddValue("serverPort", "Port number of the UDP Server", serverPort);
    cmd.Parse(argc, argv);

    std::srand(std::time(NULL));

    if (verboseLogs)
    {
        LogComponentEnable("SimpleGatewayServer", LOG_LEVEL_ALL);
    }
    else
    {
        LogComponentEnable("SimpleGatewayServer", LOG_LEVEL_INFO);
    }

    // create the server socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        NS_FATAL_ERROR("ERROR: failed to create the socket");
    }
    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
        NS_FATAL_ERROR("ERROR: failed to set the socket options");
    }

    // set the server address
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // bind the socket to the server address
    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        NS_FATAL_ERROR("ERROR: failed to bind the socket to port " << serverPort);
    }
    
    // listen for client connections
    if (listen(serverSocket, 1) == -1)
    {
        NS_FATAL_ERROR("ERROR: failed to listen for client connections");
    }
    NS_LOG_INFO("Started server on Port " << serverPort);

    // accept a client connection
    int clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == -1)
    {
        NS_FATAL_ERROR("ERROR: failed to accept the client connection");
    }
    NS_LOG_INFO("Accepted a client connection");

    /* ========== START MESSAGE PROTOCOL =====================================*/

    const size_t BUFFER_SIZE = 4096;
    char recvBuffer[BUFFER_SIZE];

    std::vector<uint16_t> xVelocity(numberOfNodes, 0);
    std::vector<uint16_t> xPosition(numberOfNodes, 0);
    std::vector<uint16_t> broadcast(numberOfNodes, 0);
    
    for (uint32_t i = 0; i < iterations; i++)
    {
        uint32_t timeNow = timeStart + timeDelta * i;
        NS_LOG_INFO("t = " << timeNow);

        // create the next message
        std::string message = std::to_string(timeNow) + " 0";                               // timestamp header
        for (uint16_t n = 0; n < numberOfNodes; n++)
        {
            message += " " + std::to_string(xPosition[n]) + " " + std::to_string(n) + " 0"; // position vector
            message += " " + std::to_string(xVelocity[n]) + " 0 0";                         // velocity vector
            message += " " + std::to_string(broadcast[n]);                                  // broadcast bool
        }
        NS_LOG_DEBUG("next message: " << message);
        message += "\r\n";                                                                  // end of message

        // send the next message
        if (send(clientSocket, message.c_str(), message.size(), 0) == -1)
        {
            NS_FATAL_ERROR("ERROR: failed to send a message");
        }

        // receive client response
        int bytesReceived = recv(clientSocket, recvBuffer, BUFFER_SIZE - 1, 0);
        if (bytesReceived == -1)
        {
            NS_FATAL_ERROR("ERROR: failed to receive response");
        }
        else if (bytesReceived == 0)
        {
            NS_LOG_WARN("WARNING: client socket terminated connection");
            break;
        }
        else
        {
            recvBuffer[bytesReceived] = '\0'; // bytesReceived < BUFFER_SIZE
            NS_LOG_DEBUG("received message: " << recvBuffer);
        }

        if (i == iterations - 1) // last iteration
        {
            message = "-1 0\r\n"; // terminate message
            if (send(clientSocket, message.c_str(), message.size(), 0) == -1)
            {
                NS_FATAL_ERROR("ERROR: failed to send a message");
            }
            NS_LOG_INFO("Sent terminate message");
        }
        else
        {
            // simulate node movement
            for (uint16_t n = 0; n < numberOfNodes; n++)
            {
                xVelocity[n] = std::rand() % positionDeltaX + 1;
                xPosition[n] = xPosition[n] + xVelocity[n];
                broadcast[n] = (i % 5 == 0) && (std::rand() % 2 == 0); // on multiples of 5, 50 % chance
            }
        }
    }

    close(clientSocket);
    close(serverSocket);

    return 0;
}
