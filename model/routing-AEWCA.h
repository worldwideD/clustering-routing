#ifndef ROUTING_AEWCA
#define ROUTING_AEWCA_H

#include "aqua-sim-routing.h"
#include "aqua-sim-header.h"

namespace ns3 {

// 邻居表条目
struct AEWCANeighborTableEntry
{
    Time lastT;                      // last interaction time
    Vector position;                 // position at lastT
    Vector direction;                // direction at lastT
    double speed;                    // speed at lastT
    double range;                    // minimum of two nodes' transmission range
    double linkT;                    // link holding time
    double distV;                    // distance value
    double weight;                   // weight of long neighbor
    AEWCANeighborTableEntry ()
        :lastT(Time(0)), position(), direction(), speed(0), linkT(0), distV(-1), weight(0)
    {
    }
};

// 路由协议
class RoutingAEWCA: public AquaSimRouting {
public:
    enum NodeAbility                // node transmission ability (short or long)
    {
        SHORT = 1,
        LONG = 2,
    };
    enum NodeIdentity               // node identity (cluster head; cluster member; sink node)
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
        MAINTENANCE = 4,
    };
    RoutingAEWCA();
    ~RoutingAEWCA();
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
    void ProcessCHAnnouncement(Ptr<Packet> packet);
    void ProcessJoinAnnouncement(Ptr<Packet> packet);
    // send packets
    void SendHello();
    void SendCHAnnouncement();
    void SendJoinAnnouncement(AquaSimAddress head);
    // calculate weight
    void UpdateMobility();
    void CalcNeighbor(AEWCANeighborTableEntry &n);
    void UpdateWeight();

    // maintain cluster
    void AddMember(AquaSimAddress addr, AEWCANeighborTableEntry n);
    void DeleteMember(AquaSimAddress addr);
    bool CheckNesessity();
    void SelectCH();

    // parameters
    double omega1, omega2, omega3;  // parameters for node weight calculation
    // time intervals
    double m_helloInterval;         // hello messages' emission interval
    double m_helloDelay;            // first maintainance hello message's delay
    // node parameters
    double m_range;                 // transmission range
    NodeAbility m_ability;          // transmission ability
    NodeIdentity m_identity;        // node identity
    double weight;                  // node weight
    Vector m_position, m_direction;
    double m_speed, maxSpeed;
    Time m_currentTime;             // pos, dir, speed of current time
    // neighbor parameters
    std::map<AquaSimAddress, AEWCANeighborTableEntry> m_shortNeighbors;// short neighbors
    std::map<AquaSimAddress, AEWCANeighborTableEntry> m_longNeighbors; // long neighbors
    std::map<AquaSimAddress, AEWCANeighborTableEntry> m_chNeighbors;   // CH neighbors
    std::map<AquaSimAddress, AEWCANeighborTableEntry> m_members;       // member (not empty when I'm CH)
    int n_members;
    std::map<AquaSimAddress, EventId> m_event;                              // neighbors' expire event
    std::set<AquaSimAddress> waitingNeighbors;             // waiting neighbors at forming & routing
    void UpdateNeighbor(AquaSimAddress addr, Time t);
    void NeighborExpire(AquaSimAddress addr);

    int n_nodes;
    AquaSimAddress m_ch;                                                    // CH of current node
    bool NoWaitingNodes()         // checking if there's any waiting-announcement long neighbor
    {
        return waitingNeighbors.empty();
    }

    // routing parameters
    std::set<uint16_t> m_forwarded;                                      // forwarded packets' id

    // for control packets
    Timer helloTimer;
    void HelloTimerExpire();
    Ptr<UniformRandomVariable> m_rand;
    double GetBackoff(uint8_t pktType);

    // different phases
    uint8_t m_phase;
    double formingTime, routingTime, maintenanceTime;
    void SwitchPhase(RoutingPhase p);
}; // class RoutingAEWCA

} // namespace ns3

#endif /* ROUTING_AEWCA_H */