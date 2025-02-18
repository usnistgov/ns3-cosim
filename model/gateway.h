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

#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "ns3/core-module.h"

namespace ns3
{

/**
 * An abstract base class that maintains a socket connection with a server to exchange data during simulation runtime.
 * The pure virtual Gateway::DoInitialize and Gateway::DoUpdate functions must be implemented in a derived class to
 * specify how data received from the server is processed. The purpose of this class is to handle time management,
 * turning the ns-3 simulator into a discrete-time simulation that operates in lock-step with the server.
 *
 * All packets, sent and received, are strings with the format "v1_v2_.._vn|", where:
 *  1) _ is a user-specified delimiter that separates elements within the message (see the constructor)
 *  2) | is a user-specified delimiter that indicates the end of the message (see the constructor)
 *  3) v1 .. vn are string elements that contain any value excluding the delimiters from 1 and 2
 */
class Gateway
{
    public:
        /**
         * @brief Construct a new gateway instance.
         *
         * Exceptions:
         *  1) delimiterField and delimiterMessage must have non-empty values
         *  2) delimiterMessage must not be a substring of delimiterField
         *
         * @param dataSize the number of elements the gateway sends to its server
         * @param delimiterField the delimiter used between values within one message (default: " ")
         * @param delimiterMessage the delimiter used to indicate the end of a message (default: "\r\n")
         */
        Gateway(uint32_t dataSize,
                const std::string & delimiterField = " ",
                const std::string & delimiterMessage = "\r\n");
        
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
         * @brief Set the value of one element to be sent to the server.
         *
         * This function only buffers data and does not send anything to the server (see Gateway::SendResponse).
         *
         * Exceptions:
         *  1) the index must be less than the dataSize specified in the constructor.
         *  2) the value must not contain either delimiter specified in the constructor.
         *
         * @param index the index of the element to update
         * @param value the new value to assign to the element
         */
        void SetValue(uint32_t index, const std::string & value);

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
    private:
        enum STATE      // the gateway internal state
        {
            CREATED,    // constructed
            CONNECTED,  // Gateway::Connect called
            STOPPING    // Gateway::StopThread called
        };

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
         * @brief Read data from the socket until the connection closes.
         *
         * This function executes until either the socket terminates or Gateway::Stop is called from the main thread.
         * If the socket terminates, Gateway::Stop is scheduled before the function returns. When data is received from
         * the socket, Gateway::ForwardUp is scheduled to process the data.
         */
        void RunThread();

        /**
         * @brief Pause the simulation by scheduling events to execute now until cancelled.
         *
         * This function schedules itself to execute immediately forever. Interrupt it by cancelling m_eventWait.
         */
        void WaitForNextUpdate();

        /**
         * @brief Processes one received message.
         *
         * Dependent on the message timestamp, the following outcomes are possible:
         *  1) if the received timestamp is negative, Simulator::Stop is called (and the message it not processed).
         *  2) if this is the first message, Gateway::DoInitialize is scheduled to execute now.
         *  3) otherwise, Gateway::HandleUpdate is scheduled for the received timestamp.
         *
         * The timestamp is removed from the message before scheduling Gateway::DoInitialize and Gateway::HandleUpdate.
         *
         * Exceptions:
         *  1) m_messageQueue must contain at least one element.
         *  2) the message must begin with two integers that represent a (seconds, nanoseconds) timestamp.
         *  3) the received timestamps must be increasing between consecutive calls.
         */
        void ForwardUp();

        /**
         * @brief Handle processing a received message prior to execution of the callback functions.
         *
         * This function is responsible for pausing simulation time if there are no messages pending in the queue.
         *
         * @param receivedData the received message content excluding the header/timestamp
         */ 
        void HandleUpdate(const std::vector<std::string> & receivedData);

        /**
         * @brief Callback to process the first message received from the server.
         *
         * @param receivedData the received message content excluding the header/timestamp
         */        
        virtual void DoInitialize(const std::vector<std::string> & receivedData) = 0;

        /**
         * @brief Callback to process a message received from the server.
         *
         * @param receivedData the received message content excluding the header/timestamp
         */ 
        virtual void DoUpdate(const std::vector<std::string> & receivedData) = 0;

        uint32_t m_context;     //!< Simulator context when the gateway instance was created
        
        EventId m_eventWait;    //!< If IsPending, an event to call Gateway::WaitForNextUpdate in an infinite loop
        EventId m_eventDestroy; //!< If IsPending, an event to call Gateway::StopThread when the simulator stops

        STATE m_state;          //!< Current state of the gateway instance

        Time m_timeStart;       //!< Initial timestamp received from the server specified by Gateway::Connect
        Time m_timePause;       //!< Time at which Gateway::WaitForNextUpdate will pause ns-3 time progression

        int m_socket;           //!< Client UDP socket connection to the server specified by Gateway::Connect

        std::thread m_thread;   //!< Thread that receives messages from the client UDP socket connection

        std::queue<std::string> m_messageQueue; //!< Shared memory between the main thread and the read thread
        std::mutex m_messageQueueMutex;         //!< Mutex lock used to synchronize access to the shared memory
        
        std::string m_delimiterField;           //!< The character sequence that separates values within a message
        std::string m_delimiterMessage;         //!< The character sequence that indicates the end of a message
        std::string m_messageBuffer;            //!< A buffer for any data received after the message delimiter
        
        std::vector<std::string> m_data;        //!< The values that will be sent to the server next update
};

} // namespace ns3

#endif /* GATEWAY_H */
