#ifndef TRIGGERED_SEND_HELPER_H
#define TRIGGERED_SEND_HELPER_H

#include "ns3/address.h"
#include "ns3/application-helper.h"

namespace ns3
{

class TriggeredSendHelper : public ApplicationHelper
{
    public:
        TriggeredSendHelper(const std::string& protocol, const Address& address);
};

} // namespace ns3

#endif /* TRIGGERED_SEND_HELPER_H */
