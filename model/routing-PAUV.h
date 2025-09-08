#ifndef DOLPHIN_ROUTING_PAUV
#define DOLPHIN_ROUTING_PAUV_H

#include "aqua-sim-routing.h"
#include "aqua-sim-header.h"

namespace ns3 {

// 路由协议
class RoutingPAUV: public AquaSimRouting {
public:
    enum NodeAbility                // node transmission ability (short or long)
    {
        SHORT = 1,
        LONG = 2,
    };
    RoutingPAUV();
    ~RoutingPAUV();
    static TypeId GetTypeId(void);
    int64_t AssignStreams (int64_t stream);

    virtual bool Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber);

    // Setters
    void SetBoundary(double minX, double maxX, double minY, double maxY, double minZ, double maxZ);

    // trace
    TracedCallback<Ptr<const Packet>, double, AquaSimAddress, int> m_txTrace;
    TracedCallback<Ptr<const Packet>, double, AquaSimAddress, int> m_rxTrace;
    TracedCallback<uint8_t, uint32_t> m_counterTrace;
    TracedCallback<uint32_t, uint8_t, double> m_delayTrace;

protected:
    void DoInitialize() override;
    void DoDispose() override;

private:
    // routing parameters
    std::set<uint16_t> m_forwarded;                                      // forwarded packets' id
    double minX, maxX, minY, maxY, minZ, maxZ;
    Vector sinkPos;
    Ptr<UniformRandomVariable> m_rand;
    NodeAbility m_ability;          // transmission ability
}; // class RoutingPAUV

} // namespace ns3

#endif /* ROUTING_PAUV_H */