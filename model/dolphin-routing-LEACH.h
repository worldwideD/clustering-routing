#ifndef DOLPHIN_ROUTING_LEACH
#define DOLPHIN_ROUTING_LEACH_H

#include "aqua-sim-routing.h"
#include "aqua-sim-header.h"

namespace ns3 {
// 路由表条目
struct LEACHRoutingTableEntry
{
    AquaSimAddress nxt;
    uint16_t dist;
    LEACHRoutingTableEntry ():nxt(), dist(0)
    {
    }
    LEACHRoutingTableEntry (AquaSimAddress n, uint16_t d):nxt(n), dist(d)
    {
    }
};

// 路由协议
class DolphinRoutingLEACH: public AquaSimRouting {
public:
    enum NodeAbility                // node transmission ability (short or long)
    {
        SHORT = 1,
        LONG = 2,
        SINK = 3,
    };
    enum NodeIdentity               // node identity (cluster head; cluster member)
    {
        CH = 1,
        CM = 2,
    };
    enum RoutingPhase
    {
        INITIALIZATION = 1,
        FORMING = 2,
        ROUTING = 3,
        MAINTENANCE = 4,
    };
    DolphinRoutingLEACH();
    ~DolphinRoutingLEACH();
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
    void ProcessLS(Ptr<Packet> packet);
    // send packets
    void SendHello();
    void SendJoinAnnouncement();
    void SendLS();

    // maintain cluster
    void UpdatePosition();
    bool SelectCH();

    // routing
    void CalcRoutingTable();

    // parameters
    double prob;                    // parameter for ch election
    // time intervals
    Time m_interval;           // clustering reforming interval
    // node parameters
    double m_range;                 // transmission range
    NodeAbility m_ability;          // transmission ability
    NodeIdentity m_identity;        // node identity
    Vector m_position;
    // neighbor parameters
    std::vector<AquaSimAddress> m_chNeighbors;   // CH neighbors
    std::set<AquaSimAddress> m_members;       // member (not empty when I'm CH)
    
    int m_rounds;                   // current round
    int last_ch_round;
    double min_dis;
    AquaSimAddress m_ch;                                                    // CH of current node
    int n_members;

    // routing parameters
    std::map<AquaSimAddress, Time> m_LSTimeStamp;
    std::map<AquaSimAddress, std::vector<AquaSimAddress>> m_CHMemberTable;
    std::map<AquaSimAddress, std::vector<AquaSimAddress>> m_CHLinksTable;
    std::map<AquaSimAddress, LEACHRoutingTableEntry> m_RoutingTable;      // routing table
    std::set<uint16_t> m_forwarded;                                      // forwarded packets' id

    // for reforming
    Timer m_Timer;
    uint32_t lsSeqNum;
    void TimerExpire();

    // different phases
    uint8_t m_phase;
    double formingTime, routingTime;
}; // class DolphinRoutingLEACH

} // namespace ns3

#endif /* DOLPHIN_ROUTING_LEACH_H */