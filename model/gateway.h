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

#include <thread>
#include <mutex>
#include <fstream>
#include <iostream>
#include <queue>

#include <string>

namespace ns3
{

class Gateway
{
    public:
        Gateway();

        void Connect(const std::string & serverAddress, int serverPort);
    protected:
        void SendData(const std::string & data);
    private:
        static const char * SEPARATOR_HEADER;
        static const char * SEPARATOR_MESSAGE;
        static const size_t BUFFER_SIZE = 4096;

        enum STATE
        {
            CREATED,
            CONNECTED,
            STOPPING,
            STOPPED,
            ERROR
        };

        struct ClockValue
        {
            int64x64_t seconds;
            int64x64_t nanoseconds;
        };

        bool CreateSocketConnection(const std::string & serverAddress, int serverPort);

        void RunThread();
        void StopThread();
        void ForwardUp();
        void WaitForNextUpdate();

        std::string ReceiveNextMessage();
        bool StringEndsWith(const std::string & str, const char * token);

        void HandleUpdate(std::string data);

        virtual void DoInitialize(const std::string & data) = 0;
        virtual void DoUpdate(const std::string & data) = 0;

        STATE m_state;
        uint32_t m_nodeId;
        int m_clientSocket;
        std::thread m_thread;
        std::queue<std::string> m_messageQueue;
        std::mutex m_messageQueueMutex;
        EventId m_destroyEvent;
        EventId m_waitEvent;

        Time m_startTime;
        Time m_receivedTime;
};

} // namespace ns3

#endif /* GATEWAY_H */
