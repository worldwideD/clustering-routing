#ifndef SOAM_ROUTING
#define SOAM_ROUTING_H

#include "aqua-sim-routing.h"
#include "aqua-sim-header.h"

namespace ns3 {

// 路由协议
class RoutingSOAM: public AquaSimRouting {
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
        FORMING = 2,
        ROUTING = 3,
    };
    struct SOAMRoutingTableEntry
    {
        AquaSimAddress nxt;
        uint8_t hops;
        uint16_t dist;
        RoutingPhase phase;
        uint16_t rounds;
        SOAMRoutingTableEntry ():nxt(), dist(0), hops(0), phase(ROUTING), rounds(-1)
        {
        }
        SOAMRoutingTableEntry (AquaSimAddress n, uint8_t h, uint16_t d, RoutingPhase p, uint16_t r)
                :nxt(n), dist(d), hops(h), phase(p), rounds(r)
        {
        }
    };
    RoutingSOAM();
    ~RoutingSOAM();
    static TypeId GetTypeId(void);
    int64_t AssignStreams (int64_t stream);

    virtual bool Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber);

    // Setters
    void SetIdentity(NodeIdentity identity);
    
    // trace
    TracedCallback<Ptr<const Packet>, double, AquaSimAddress, int> m_txTrace;
    TracedCallback<Ptr<const Packet>, double, AquaSimAddress, int> m_rxTrace;
    TracedCallback<uint8_t, uint32_t> m_counterTrace;
    TracedCallback<uint32_t, uint8_t, double> m_delayTrace;

protected:
    void DoInitialize() override;
    void DoDispose() override;

private:
    // process packets
    void ProcessHello(Ptr<Packet> packet);
    void ProcessJoinAnnouncement(Ptr<Packet> packet);
    // send packets
    void SendHello(AquaSimAddress src);
    void SendJoinAnnouncement(AquaSimAddress src);
    double GetBackoff();

    double m_range;                 // transmission range
    NodeAbility m_ability;          // transmission ability
    NodeIdentity m_identity;        // node identity
    // routing parameters
    std::set<uint16_t> m_forwarded; // forwarded packets' id
    AquaSimAddress m_ch;
    std::map<AquaSimAddress, SOAMRoutingTableEntry> m_table;
    Ptr<UniformRandomVariable> m_rand;
    // for reforming
    std::map<AquaSimAddress, Timer> m_Timer;
    Time m_interval;
    Time m_waiting;
    int n_members;
}; // class RoutingSOAM

} // namespace ns3

#endif /* ROUTING_SOAM_H */