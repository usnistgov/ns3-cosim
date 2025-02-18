// An Application that sends a specified number of packets when a function is invoked
// This file is a modified version of onoff-application.cc from ns-3
// Modified by Thomas Roth <thomas.roth@nist.gov> on Jan 16 2025

////////////////////////////////////////////////////////////////////////////////
// Original License Statement for onoff-application.cc
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Georgia Tech Research Corporation
//
// SPDX-License-Identifier: GPL-2.0-only
//
// Author: George F. Riley<riley@ece.gatech.edu>
//
////////////////////////////////////////////////////////////////////////////////

#include "triggered-send-application.h"

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/packet-socket-address.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TriggeredSendApplication");
NS_OBJECT_ENSURE_REGISTERED(TriggeredSendApplication);

TypeId
TriggeredSendApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TriggeredSendApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<TriggeredSendApplication>()
            .AddAttribute(
                "LocalAddress",
                "The local endpoint to allocate to the application. If unset, it is generated automatically.",
                AddressValue(),
                MakeAddressAccessor(&TriggeredSendApplication::m_local),
                MakeAddressChecker())
            .AddAttribute(
                "RemoteAddress",
                "The Address of the remote host.",
                AddressValue(),
                MakeAddressAccessor(&TriggeredSendApplication::m_peer),
                MakeAddressChecker())
            .AddAttribute(
                "Protocol",
                "The TypeId of the application protocol. This must be a subclass of ns3::SocketFactory.",
                TypeIdValue(UdpSocketFactory::GetTypeId()),
                MakeTypeIdAccessor(&TriggeredSendApplication::m_socketTypeId),
                MakeTypeIdChecker()) // does not check if the type derives from ns3::SocketFactory
            .AddAttribute(
                "Tos",
                "The Type of Service used when sending IPv4 packets.",
                UintegerValue(0),
                MakeUintegerAccessor(&TriggeredSendApplication::m_tos),
                MakeUintegerChecker<uint8_t>())
            .AddAttribute(
                "PacketSize",
                "The size of packets sent by the application.",
                UintegerValue(512),
                MakeUintegerAccessor(&TriggeredSendApplication::m_packetSize),
                MakeUintegerChecker<uint32_t>(1))
            .AddAttribute(
                "PacketInterval",
                "The time interval between two sent packets.",
                TimeValue(MilliSeconds(100)),
                MakeTimeAccessor(&TriggeredSendApplication::m_packetInterval),
                MakeTimeChecker(FemtoSeconds(1)))
            .AddTraceSource(
                "Tx",
                "A new packet is created and is sent.",
                MakeTraceSourceAccessor(&TriggeredSendApplication::m_txTrace),
                "ns3::Packet::TracedCallback")
            .AddTraceSource(
                "TxWithAddresses",
                "A new packet is created and is sent.",
                MakeTraceSourceAccessor(&TriggeredSendApplication::m_txTraceWithAddresses),
                "ns3::Packet::TwoAddressTracedCallback");
    return tid;
}

TriggeredSendApplication::TriggeredSendApplication()
    : m_socket(nullptr),
      m_connected(false),
      m_packetCount(0)
{
    NS_LOG_FUNCTION(this);
}

TriggeredSendApplication::~TriggeredSendApplication()
{
    NS_LOG_FUNCTION(this);
}

void
TriggeredSendApplication::Send(uint32_t numberOfPackets)
{
    NS_LOG_FUNCTION(this << numberOfPackets);

    if (numberOfPackets == 0)
    {
        NS_LOG_WARN("Failed to send packet because numberOfPackets parameter = 0");
    }
    else
    {
        // This ScheduleNow call avoids a race condition assuming the ns-3 scheduler processes events FIFO.
        // The ProcessSendRequest call will be placed at the end of the ns-3 event queue, ensuring that any pending
        // m_sendPacketEvent scheduled for the current time step executes prior to processing this new send request.
        Simulator::ScheduleNow(&TriggeredSendApplication::ProcessSendRequest, this, numberOfPackets);
    }
}

void
TriggeredSendApplication::DoDispose()
{
    NS_LOG_FUNCTION(this);

    CancelEvents();
    m_socket = nullptr;

    Application::DoDispose();
}

void
TriggeredSendApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), m_socketTypeId);

        int returnValue = -1;

        NS_ABORT_MSG_IF(m_peer.IsInvalid(), "'Remote' attribute not properly set");

        if (!m_local.IsInvalid()) // a local address was allocated for the socket
        {
            NS_ABORT_MSG_IF(
                (InetSocketAddress::IsMatchingType(m_peer) && Inet6SocketAddress::IsMatchingType(m_local)) ||
                (Inet6SocketAddress::IsMatchingType(m_peer) && InetSocketAddress::IsMatchingType(m_local)),
                "Incompatible peer and local address IP version");
            returnValue = m_socket->Bind(m_local);
        }
        else // a local address should be generated for the socket
        {
            if (Inet6SocketAddress::IsMatchingType(m_peer))
            {
                returnValue = m_socket->Bind6();
            }
            else if (InetSocketAddress::IsMatchingType(m_peer) || PacketSocketAddress::IsMatchingType(m_peer))
            {
                returnValue = m_socket->Bind();
            }
            // else returnValue was initialized as -1
        }

        if (returnValue == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket for " << m_peer);
        }

        m_socket->SetConnectCallback(
            MakeCallback(&TriggeredSendApplication::ConnectionSucceeded, this),
            MakeCallback(&TriggeredSendApplication::ConnectionFailed, this));

        if (InetSocketAddress::IsMatchingType(m_peer))
        {
            m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
        }
        m_socket->Connect(m_peer);
        m_socket->SetAllowBroadcast(true);
        m_socket->ShutdownRecv(); // disable receive
    }

    CancelEvents();
}

void
TriggeredSendApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);

    CancelEvents();

    if (m_socket)
    {
        m_socket->Close();
    }
    else
    {
        NS_LOG_WARN("TriggeredSendApplication found null socket to close in StopApplication");
    }
}

void
TriggeredSendApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    m_connected = true;
}

void
TriggeredSendApplication::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    NS_FATAL_ERROR("Socket failed to connect.");
}

void
TriggeredSendApplication::CancelEvents()
{
    NS_LOG_FUNCTION(this);

    if (m_sendPacketEvent.IsPending())
    {
        Simulator::Cancel(m_sendPacketEvent);
        NS_LOG_INFO("Cancelled pending SendPacket event.");
    }
}

void
TriggeredSendApplication::ProcessSendRequest(uint32_t numberOfPackets)
{
    NS_LOG_FUNCTION(this << numberOfPackets);

    if (m_socket && m_connected)
    {
        if (m_sendPacketEvent.IsPending())
        {
            NS_LOG_INFO("TriggeredSendApplication interrupted while sending packets. "
                << m_packetCount << " packets from a prior call to Send have been cancelled.");
            m_packetCount = numberOfPackets;
            // re-use the existing SendPacket event (to maintain packet interval)
        }
        else
        {
            m_packetCount = numberOfPackets;
            m_sendPacketEvent = Simulator::ScheduleNow(&TriggeredSendApplication::SendPacket, this);
        }
    }
    else
    {
        NS_LOG_WARN("Failed to send packet because TriggeredSendApplication Socket is not connected.");
    }
}

void
TriggeredSendApplication::SendPacket()
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT(m_sendPacketEvent.IsExpired());

    if (m_packetCount > 0)
    {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);

        int bytesSent = m_socket->Send(packet);
        if ((unsigned)bytesSent == m_packetSize)
        {
            Address localAddress;
            m_socket->GetSockName(localAddress);
            if (InetSocketAddress::IsMatchingType(m_peer))
            {
                NS_LOG_INFO("At time " << Simulator::Now().As(Time::S)
                    << " triggered send application sent " << packet->GetSize() << " bytes to "
                    << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << " port "
                    << InetSocketAddress::ConvertFrom(m_peer).GetPort());
                m_txTraceWithAddresses(packet, localAddress, InetSocketAddress::ConvertFrom(m_peer));
            }
            else if (Inet6SocketAddress::IsMatchingType(m_peer))
            {
                NS_LOG_INFO("At time " << Simulator::Now().As(Time::S)
                    << " triggered send application sent " << packet->GetSize() << " bytes to "
                    << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6() << " port "
                    << Inet6SocketAddress::ConvertFrom(m_peer).GetPort());
                m_txTraceWithAddresses(packet, localAddress, Inet6SocketAddress::ConvertFrom(m_peer));
            }
            m_txTrace(packet);
        }
        else
        {
            NS_LOG_DEBUG("Failed to send packet");
        }

        m_packetCount = m_packetCount - 1;
        m_sendPacketEvent = Simulator::Schedule(m_packetInterval, &TriggeredSendApplication::SendPacket, this);
    }
    else
    {
        // this m_packetCount == 0 event ensures the final SendPacket call isn't interrupted before PacketInterval
        NS_LOG_DEBUG("Finished sending all packets without interruption.");
    }
}

} // namespace ns3
