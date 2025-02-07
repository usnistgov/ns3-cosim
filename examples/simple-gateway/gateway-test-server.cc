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

#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <thread>
#include <vector>
#include <sstream>

#include "ns3/core-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleGatewayServer");

int
main(int argc, char* argv[])
{
    bool enableLogging      = true;
    uint64_t timeStart      = 0;
    uint32_t timeDelta      = 1;
    uint16_t positionDeltaX = 10;
    uint16_t serverPort     = 1111;

    uint64_t iterations     = 30;
    uint16_t numberOfNodes  = 3;

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable/disable detailed output logs (default=true)", enableLogging);
    cmd.AddValue("timeStart", "Starting simulation time in seconds", timeStart);
    cmd.AddValue("timeDelta", "Step size of simulation time in seconds", timeDelta);
    cmd.AddValue("positionDeltaX", "Maximum increase per time step to the x-coordinate of each node", positionDeltaX);
    cmd.AddValue("serverPort", "Port number for the UDP Server", serverPort);
    cmd.AddValue("iterations", "Number of time steps to simulate", iterations);
    cmd.AddValue("numberOfNodes", "Number of vehicle nodes to simulate", numberOfNodes);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("SimpleGatewayServer", LOG_LEVEL_ALL);
    }
    NS_LOG_INFO("started");

    std::srand(std::time(NULL));

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        NS_LOG_ERROR("Failed to create the socket");
        return 1;
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
        NS_LOG_ERROR("Failed to set socket options");
        return 1;
    }

    // Bind the socket to the server address
    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        NS_LOG_ERROR("Failed to bind socket");
        return 1;
    }

    if (listen(serverSocket, 1) == -1)
    {
        NS_LOG_ERROR("Failed to listen for connections");
        return 1;
    }

    int clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == -1)
    {
        NS_LOG_ERROR("Failed to accept connection");
        return 1;
    }
    NS_LOG_INFO("client connected");
    
    std::vector<uint16_t> xPosition(numberOfNodes, 0);
    std::string message = std::to_string(timeStart) + " 0\r\n";
    for (uint16_t i = 0; i < numberOfNodes; i++)
    {
        message += std::to_string(xPosition[i]) + " " + std::to_string(i) + " 0 0\r\n"; // x y z signal
    }
    message += "\r\n";
    
    if (send(clientSocket, message.c_str(), message.size(), 0) == -1)
    {
        NS_LOG_ERROR("Failed to send message");
    }
    NS_LOG_INFO("sent message " << 0);

    const size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    for (uint64_t i = 1; i < iterations; i++)
    {
        message = std::to_string(timeStart + timeDelta * i) + " 0\r\n";
        for (uint16_t n = 0; n < numberOfNodes; n++)
        {
            xPosition[n] += std::rand() % positionDeltaX + 1;

            int transmit = 0;
            if (i % 5 == 0)
            {
                transmit = std::rand() % 2;
            }

            message += std::to_string(xPosition[n]) + " " + std::to_string(n) + " 0 " + std::to_string(transmit) + "\r\n";
        }
        message += "\r\n";
    
        if (send(clientSocket, message.c_str(), message.size(), 0) == -1)
        {
            NS_LOG_ERROR("Failed to send message");
        }
        NS_LOG_INFO("sent message " << i);

        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived == -1)
        {
            NS_LOG_INFO("received response");
        }
    }

    message = "-1 0\r\n\r\n";
    if (send(clientSocket, message.c_str(), message.size(), 0) == -1)
    {
        NS_LOG_ERROR("Failed to send message");
    }

    close(clientSocket);
    close(serverSocket);

    return 0;
}
