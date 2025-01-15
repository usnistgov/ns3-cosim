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

namespace ns3
{

class TriggeredSendApplication : public Application
{
    public:
        static TypeId GetTypeId();

        TriggeredSendApplication();

        ~TriggeredSendApplication() override;

        void Send(uint32_t numberOfPackets);
    protected:
        void DoDispose() override;
    private:
        void StartApplication() override;

        void ConnectionSucceeded(Ptr<Socket> socket);

        void ConnectionFailed(Ptr<Socket> socket);

        void StopApplication() override;

        void CancelEvents();

        void SendPacket();

        Address m_local;
        Address m_peer;
        TypeId m_socketTypeId;
        uint8_t m_tos;
        uint32_t m_packetSize;
        Time m_packetInterval;

        Ptr<Socket> m_socket;
        bool m_connected;
        uint32_t m_packetCount;
        EventId m_sendPacketEvent;

        TracedCallback<Ptr<const Packet>> m_txTrace;
        TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_txTraceWithAddresses;
};

} // namespace ns3

#endif /* TRIGGERED_SEND_APPLICATION_H */
