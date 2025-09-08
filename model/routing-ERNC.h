#ifndef ERNC_ROUTING
#define ERNC_ROUTING_H

#include "aqua-sim-routing.h"
#include "aqua-sim-header.h"

namespace ns3 {

// 路由协议
class RoutingERNC: public AquaSimRouting {
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
        INITIALIZATION = 1,
        FORMING = 2,
        ROUTING = 3,
    };
    RoutingERNC();
    ~RoutingERNC();
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
    void ProcessAck(Ptr<Packet> packet);
    void ProcessCHAnnouncement(Ptr<Packet> packet);
    void ProcessJoinAnnouncement(Ptr<Packet> packet);
    // send packets
    void SendHello();
    void SendAck();
    void SendCHAnnouncement();
    void SendJoinAnnouncement();
    double GetBackoff();

    double m_range;                 // transmission range
    double minX, maxX, minY, maxY, minZ, maxZ;
    NodeAbility m_ability;          // transmission ability
    NodeIdentity m_identity;        // node identity
    // routing parameters
    std::set<uint16_t> m_forwarded; // forwarded packets' id
    AquaSimAddress m_ch;
    Ptr<UniformRandomVariable> m_rand;
    // for reforming
    Timer m_Timer;
    Time m_interval;
    Time m_waiting;
    int n_members;
    uint16_t m_round;
    int m_hops;
    int m_degree;
    double m_dis;
    double m_cdis;
    double m_tc;
    double m_ctc;
    RoutingPhase m_phase;
    Vector sinkPos;
}; // class RoutingERNC

} // namespace ns3

#endif /* ROUTING_ERNC_H */