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
 *  Raphael Barbau
*/

#ifndef BASE_GATEWAY_H
#define BASE_GATEWAY_H

#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/network-module.h"

namespace ns3
{

/**
 * An abstract base class that maintains a socket connection with a server to exchange data during simulation runtime.
 * The pure virtual BaseGateway::DoInitialize and BaseGateway::DoUpdate functions must be implemented in a derived class to
 * specify how data received from the server is processed. The purpose of this class is to handle time management,
 * turning the ns-3 simulator into a discrete-time simulation that operates in lock-step with the server.
 */
class BaseGateway
{
    public:
        /**
         * @brief Construct a new gateway instance.
         */
        BaseGateway();
        
        /**
         * @brief Connects the gateway to the server specified as arguments.
         *
         * Once connected, the remote server (through the gateway) will control time progression of the ns-3 simulator.
         * The gateway will continuously schedule an event for the current time (effectively, pausing time) until it
         * receives an explicit request from the remote server to advance. This function only attempts to connect to
         * the server once, so the remote server must be running before calling this function.
         *
         * Side Effects:
         *  1) this function will create a UDP socket connected to the remote server.
         *  2) this function will create a second thread to handle messages received from the remote server.
         *
         * Exceptions:
         *  1) this function can only be called once; a second call will cause a fatal error.
         *  2) an invalid or unresolved address will cause a fatal error. 
         *
         * @param serverAddress the IPv4 or IPv6 address of the remote server
         * @param serverPort the port number of the remote server
         */
        void Connect(const std::string & serverAddress, uint16_t serverPort);

        /**
         * @brief Send the buffered data values to the server.
         *
         * This function will send a message to the server containing the number of elements specified at construction.
         * Gateway::SetValue can be used to set the values of individual elements. If an element has not been updated
         * since a previous call to this function, it will retain its previous value. If an element has never been
         * assigned a value, the default value is the empty string.
         *
         * The sent message will be a string where the values are separated by the field delimiter specified in the
         * constructor, postpended with the message delimiter specified in the constructor. 
         *
         * If there is a send error, a warning will be output (this is not considered an exception).
         *
         * Exceptions:
         *  1) the function is called when the gateway is in a state other than CONNECTED.
         */
        void SendResponse();

        static NodeContainer GetNodes();

    protected:
        /**
         * @brief Read data from the socket until the connection closes.
         *
         * This function executes until either the socket terminates or BaseGateway::Stop is called from the main thread.
         * If the socket terminates, BaseGateway::Stop is scheduled before the function returns. When data is received from
         * the socket, BaseGateway::ForwardUp is scheduled to process the data.
         */
        virtual void RunThread() = 0;

        /**
         * @brief Create the response message sent back to the server.
         * 
         * This function allows subclasses of BaseGateway to create a custom response message.
         */
        virtual std::string PopulateResponseMessage() const = 0;
    
        enum STATE      // the gateway internal state
        {
            CREATED,    // constructed
            CONNECTED,  // Gateway::Connect called
            STOPPING    // Gateway::StopThread called
        };

        STATE GetState() const;
        int GetSocket() const;
        uint32_t GetContext() const;

        void SetTimeStart(Time timeStart);
        Time GetTimeStart() const;

        void SetTimePause(Time timePause);
        Time GetTimePause() const;

        EventId getEventWait() const;
        void SetEventWait(EventId eventId);

        /**
         * @brief Stop the gateway.
         *
         * Side Effects:
         *  1) a signal is sent for the thread to exit, and the thread is joined.
         *  2) if the client socket is connected to a server, the socket is closed.
         *  3) the gateway will no longer affect/prevent the Simulator time progression.
         *
         * This function is safe to call any number of times, and in any context within the main Simulator thread.
         */
        void Stop();

        /**
         * @brief Pause the simulation by scheduling events to execute now until cancelled.
         *
         * This function schedules itself to execute immediately forever. Interrupt it by cancelling m_eventWait.
         */
        virtual void WaitForNextUpdate();

        /**
         * @brief Returns the queue of messages to be processed by subclasses of BaseGateway.
         */
        std::queue<std::string>& GetMessageQueue();

        /**
         * @brief Returns the mutex to use when pushing a new message. 
         */
        std::mutex& GetMessageQueueMutex();

    private:

        uint32_t m_context;     //!< Simulator context when the gateway instance was created
        
        EventId m_eventWait;    //!< If IsPending, an event to call Gateway::WaitForNextUpdate in an infinite loop
        EventId m_eventDestroy; //!< If IsPending, an event to call Gateway::StopThread when the simulator stops

        STATE m_state;          //!< Current state of the gateway instance

        std::queue<std::string> m_messageQueue; //!< Shared memory between the main thread and the read thread
        std::mutex m_messageQueueMutex;         //!< Mutex lock used to synchronize access to the shared memory
        
        Time m_timeStart;       //!< Initial timestamp received from the server specified by Gateway::Connect
        Time m_timePause;       //!< Time at which Gateway::WaitForNextUpdate will pause ns-3 time progression

        int m_socket;           //!< Client UDP socket connection to the server specified by Gateway::Connect

        std::thread m_thread;   //!< Thread that receives messages from the client UDP socket connection
        
        // Rest moved to TextGateway
};

} // namespace ns3

#endif /* BASE_GATEWAY_H */
