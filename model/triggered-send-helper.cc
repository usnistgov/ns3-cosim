#include "triggered-send-helper.h"

#include "ns3/string.h"
#include "ns3/triggered-send-application.h"

namespace ns3
{

TriggeredSendHelper::TriggeredSendHelper(const std::string& protocol, const Address& address)
    : ApplicationHelper("ns3::TriggeredSendApplication")
{
    m_factory.Set("Protocol", StringValue(protocol));
    m_factory.Set("Remote", AddressValue(address));
}

} // namespace ns3
