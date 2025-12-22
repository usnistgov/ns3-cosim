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

#ifndef GATEWAY_JSON_H
#define GATEWAY_JSON_H

#include "base-gateway.h"
#include "json-mobility-object.h"
#include <ns3/json.hpp>

#include <map>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ns3
{

/** A subclass of BaseGateway that sends and receive messages formatted in JSON. 
 * 
 */
class JSONGateway : public BaseGateway
{
    public:
        /**
         * @brief Construct a new JSON gateway instance.
         *
         * @param delimiterMessage the delimiter used to indicate the end of a JSON message (default: "\4")
         */
        JSONGateway(const std::string & delimiterMessage = "\4");

    protected:
        static const std::string JSONGATEWAY_OBJECTS;

        const std::map<uint, Ptr<JSONMobilityObject>>& GetObjects() const;

        // TODO: store pointers? create/destroy

        Ptr<Node> AddObject(Ptr<JSONMobilityObject> object);

        void RemoveObject(uint id);

        virtual void RunThread();

        virtual std::string PopulateResponseMessage() const;

        virtual void DoUpdate(const json& data) = 0;

    private:
        static const std::string JSONGATEWAY_TIME_S;
        static const std::string JSONGATEWAY_TIME_NS;

        /**
         * @brief Processes one received message.
         *
         * Dependent on the message timestamp, the following outcomes are possible:
         *  1) if the received timestamp is negative, Simulator::Stop is called (and the message it not processed).
         *  2) if this is the first message, JSONGateway::DoInitialize is scheduled to execute now.
         *  3) otherwise, JSONGateway::HandleUpdate is scheduled for the received timestamp.
         *
         * The timestamp is removed from the message before scheduling JSONGateway::DoInitialize and Gateway::HandleUpdate.
         *
         * Exceptions:
         *  1) m_messageQueue must contain at least one element.
         *  2) the message must contain two integers that represent a (seconds, nanoseconds) timestamp.
         *  3) the received timestamps must be increasing between consecutive calls.
         */
        void ForwardUp();

        /**
         * @brief Callback to process the first message received from the server.
         *
         * @param receivedData the received message content excluding the header/timestamp
         */        
        virtual void DoInitialize(const json & data);

        /**
         * @brief Handle processing a received message prior to execution of the callback functions.
         *
         * This function is responsible for pausing simulation time if there are no messages pending in the queue.
         *
         * @param receivedData the received message content excluding the header/timestamp
         */ 
        void HandleUpdate(const json & data);

        std::string m_messageBuffer;            //!< A buffer for any data received after the message delimiter

        std::string m_delimiterMessage;         //!< The character sequence that indicates the end of a message
        
        std::map<uint, Ptr<JSONMobilityObject>> m_objects; //!< A map that retuns the json object with the given identifier

};

}

#endif