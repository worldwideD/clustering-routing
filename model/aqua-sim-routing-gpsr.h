#include "aqua-sim-address.h"
#include "aqua-sim-routing.h"
// 下面几个是后加的
#include "aqua-sim-channel.h"
#include "aqua-sim-datastructure.h"
#include "aqua-sim-mac-tdma.h"
#include "aqua-sim-routing-vbf.h"

#include "ns3/application.h"
#include "ns3/applications-module.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"
#include "ns3/vector.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <vector>

namespace ns3
{

/*header length of Static routing*/
#define SR_HDR_LEN (3 * sizeof(AquaSimAddress) + sizeof(int))

/**
 * \ingroup aqua-sim-ng
 *
 * \brief GPSR routing implementation
 */

struct gpsr_neighbor
{
    Vector neighbor_position;
    Time t_now;
    int alive;
};

struct gpsr_packet
{
    uint16_t pkt_uid;
    uint64_t pkt_bitstring;
    Time pkt_t;
    Time pkt_lastsend_t;
};

struct peripheral_pkt
{
    uint16_t pkt_uid;
    std::vector<int> node_forward;
};

class AquaSimGpsrRouting : public AquaSimRouting
{
  public:
    AquaSimGpsrRouting();
    virtual ~AquaSimGpsrRouting();
    static TypeId GetTypeId(void);
    int64_t AssignStreams(int64_t stream);

    virtual bool Recv(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);

    uint8_t MaxForwards;

    int m_maxForward;

    TracedCallback<Ptr<const Packet>, double, AquaSimAddress, int> m_txTrace;
    TracedCallback<Ptr<const Packet>, double, AquaSimAddress, int> m_rxTrace;
    TracedCallback<double> m_InitialNetTrace;

    void CheckNeighborTimeout();
    void PutInHash(Ptr<Packet> p);
    void CheckBroadcastpktTimeout();
    uint64_t UpdateBitStringInList(Ptr<Packet> p, uint64_t bitString);
    bool CheckNeighborSent(uint64_t bitstring);
    int PutInPkt(Ptr<Packet> p, AquaSimHeader ash);
    void SetUp();

    double Distance(int a, int b);
    Vector Vectorab(int a, int b);
    double VectorCalculate(Vector a, Vector dst);

  protected:
    int initial_num;
    int neighbor_num;
    std::vector<gpsr_neighbor> hashTable;
    std::vector<gpsr_packet> packet_table;
    std::vector<peripheral_pkt> peripheral_table;

    bool has_set_neighbor;
    bool m_hasSetNode;

    std::string m_routeFile_string;
    std::string m_initial_neighbor_string;

    AquaSimAddress FindNextHop(const Ptr<Packet> p);
    AquaSimAddress GreedyForwarding(const Ptr<Packet> p);
    AquaSimAddress PeripheralForwarding(const Ptr<Packet> p);

    void node_distribution_new();

    double m_neighbor_t_out;
    double m_neighbor_t_slot;
    double m_pkt_t1_out; // aggregation timeout
    double m_pkt_t2_out; // packet timeout
    double m_pkt_t_slot;
    int m_NodeNum;
    uint64_t m_bitstring;

    std::vector<Vector> nodes;
    std::vector<std::vector<int>> initial_neighbor;
};

} // namespace ns3
