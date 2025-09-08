#ifndef ALRP_ROUTING
#define ALRP_ROUTING_H

#include "aqua-sim-routing.h"
#include "aqua-sim-header.h"

namespace ns3 {

// 路由协议
class RoutingALRP: public AquaSimRouting {
public:
    enum NodeAbility                // node transmission ability (short or long)
    {
        SHORT = 1,
        LONG = 2,
    };
    enum NodeIdentity               // node identity (cluster head; cluster member)
    {
        CH = 1,
        CM = 2,
        SINK = 3,
    };
    enum RoutingPhase
    {
        FORMING = 1,
        ROUTING = 2,
    };
    RoutingALRP();
    ~RoutingALRP();
    static TypeId GetTypeId(void);
    int64_t AssignStreams (int64_t stream);

    virtual bool Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber);

    // Setters
    void SetIdentity(NodeIdentity identity);
    void AddSinkPos(AquaSimAddress addr, Vector pos);
    
    // trace
    TracedCallback<Ptr<const Packet>, double, AquaSimAddress, int> m_txTrace;
    TracedCallback<Ptr<const Packet>, double, AquaSimAddress, int> m_rxTrace;
    TracedCallback<uint8_t, uint32_t> m_counterTrace;
    TracedCallback<uint32_t, uint8_t, double> m_delayTrace;

protected:
    void DoInitialize() override;
    void DoDispose() override;

private:
    void SwitchPhase()
    {
        m_phase = ROUTING;
    }

    double m_range;                 // transmission range
    NodeAbility m_ability;          // transmission ability
    NodeIdentity m_identity;        // node identity
    // routing parameters
    std::set<uint16_t> m_forwarded; // forwarded packets' id
    std::map<AquaSimAddress, Vector> sinkPos;                 // position of SINKs
    double k, delay;                   // forward thres

    void ForwardPacket(Ptr<Packet> Packet, AquaSimAddress dst);
    
    Ptr<UniformRandomVariable> m_rand;
   
    uint8_t m_phase;
}; // class RoutingALRP

} // namespace ns3

#endif /* ROUTING_ALRP_H */