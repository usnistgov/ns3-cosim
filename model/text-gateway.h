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

#ifndef TEXT_GATEWAY_H
#define TEXT_GATEWAY_H

#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "ns3/core-module.h"

#include "base-gateway.h"

namespace ns3
{

/** A subclass of BaseGateway that sends and receive messages formatted in delimiter-separated textual format. 
 * 
 */
class TextGateway : public BaseGateway
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
        TextGateway(uint32_t dataSize,
                const std::string & delimiterField = " ",
                const std::string & delimiterMessage = "\r\n");


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

    protected:
        /**
         * @brief Read data from the socket until the connection closes.
         *
         * This function executes until either the socket terminates or Gateway::Stop is called from the main thread.
         * If the socket terminates, Gateway::Stop is scheduled before the function returns. When data is received from
         * the socket, Gateway::ForwardUp is scheduled to process the data.
         */
        virtual void RunThread();

        virtual std::string PopulateResponseMessage() const;

        virtual void DoUpdate(const std::vector<std::string> & data) = 0;


    private:

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
         * @brief Callback to process the first message received from the server.
         *
         * @param receivedData the received message content excluding the header/timestamp
         */        
        virtual void DoInitialize(const std::vector<std::string> & receivedData) = 0;

        /**
         * @brief Handle processing a received message prior to execution of the callback functions.
         *
         * This function is responsible for pausing simulation time if there are no messages pending in the queue.
         *
         * @param receivedData the received message content excluding the header/timestamp
         */ 
        void HandleUpdate(const std::vector<std::string> & receivedData);


        std::string m_delimiterField;           //!< The character sequence that separates values within a message
        std::string m_delimiterMessage;         //!< The character sequence that indicates the end of a message
        std::string m_messageBuffer;            //!< A buffer for any data received after the message delimiter
        
        std::vector<std::string> m_data;        //!< The values that will be sent to the server next update
};

} // namespace ns3

#endif /* TEXT_GATEWAY_H */
