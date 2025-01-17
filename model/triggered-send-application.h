// An Application that sends a specified number of packets when a function is invoked
// This file is a modified version of onoff-application.h from ns-3
// Modified by Thomas Roth <thomas.roth@nist.gov> on Jan 16 2025

////////////////////////////////////////////////////////////////////////////////
// Original License Statement for onoff-application.h
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Georgia Tech Research Corporation
//
// SPDX-License-Identifier: GPL-2.0-only
//
// Author: George F. Riley<riley@ece.gatech.edu>
//
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIGGERED_SEND_APPLICATION_H
#define TRIGGERED_SEND_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "ns3/traced-callback.h"
#include "ns3/type-id.h"

namespace ns3
{

/**
 * An application that generates traffic in response to explicit calls to its Send method. By default, the application
 * will generate no traffic even after Application::StartApplication is called. Invoking TriggeredSendApplication:Send
 * will instead generate and send a fixed number of packets. The size and rate of these packets can be specified using
 * the packet size and packet interval attributes. The number of packets to generate is specified as a Send parameter.
 * The Send method can only be called after the application is started, and before the application is stopped.
 *
 * The Send method can be invoked any number of times during the simulation runtime. If invoked before a previous call
 * has finished processing, the previous call will be cancelled and its remaining packets will not be sent. Refer to
 * the triggered-send-example for concrete examples of how simultaneous calls to Send are resolved.
 */
class TriggeredSendApplication : public Application
{
    public:
        /**
         * @brief Get the type ID.
         * @return the object TypeId
         */
        static TypeId GetTypeId();

        TriggeredSendApplication();

        ~TriggeredSendApplication() override;

        /**
         * @brief Trigger the application to start sending packets.
         *
         * The time interval between consecutive sends is specified with the PacketInterval attribute.
         * The application can be triggered to send any number of times after the application has started.
         * If called while the application is already sending, the existing send operation will be cancelled.
         *
         * @param numberOfPackets the total number of packets to send
         */
        void Send(uint32_t numberOfPackets);
    protected:
        void DoDispose() override;
    private:
        void StartApplication() override;

        void StopApplication() override;

        /**
         * @brief Handle a Connection Succeed event.
         * @param socket the connected socket
         */
        void ConnectionSucceeded(Ptr<Socket> socket);

        /**
         * @brief Handle a Connection Failed event.
         * @param socket the socket that failed to connect
         */
        void ConnectionFailed(Ptr<Socket> socket);

        /**
         * @brief Cancel scheduled send packet events.
         */
        void CancelEvents();

        /**
         * @brief A helper method to process calls to TriggerSendApplication:Send.
         *
         * The Send method is split into two functions to avoid a race condition when TriggerSendApplication::Send
         * is called during the simulation time step when the send packet event is scheduled to execute.
         *
         * @param numberOfPackets the total number of packets to send
         */
        void ProcessSendRequest(uint32_t numberOfPackets);

        /**
         * @brief Send one packet and schedule the next send packet event.
         *
         * A packet will be generated with random data to send to the connected remote endpoint.
         * This is a recursive call that will re-schedule itself until the packet count reaches 0.
         */
        void SendPacket();

        Address m_local;            //!< Address of the local endpoint
        Address m_peer;             //!< Address of the remote host

        TypeId m_socketTypeId;      //!< Type ID of a ns3::SocketFactory
        Ptr<Socket> m_socket;       //!< Socket used to send packets
        bool m_connected;           //!< Flag for the socket connect status
        uint8_t m_tos;              //!< Type of Service for IPv4 connections

        Time m_packetInterval;      //!< Time interval between sending two packets
        uint32_t m_packetSize;      //!< Size in bytes of the generated packets
        uint32_t m_packetCount;     //!< Remaining number of packets to send

        EventId m_sendPacketEvent;  //!< Event ID for the next scheduled send packet event

        /// Callback for tracing when packets are sent
        TracedCallback<Ptr<const Packet>> m_txTrace;

        /// Callback for tracing when packets are sent that includes the source and destination addresses
        TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_txTraceWithAddresses;
};

} // namespace ns3

#endif /* TRIGGERED_SEND_APPLICATION_H */
