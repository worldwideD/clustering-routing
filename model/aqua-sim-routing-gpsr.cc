#include "aqua-sim-routing-gpsr.h"

#include "aqua-sim-header.h"

#include "ns3/log.h"
#include "ns3/string.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <float.h>
#include <iostream>
#include <vector>

static int Finish_Initial_Network = 0;

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AquaSimGpsrRouting");
NS_OBJECT_ENSURE_REGISTERED(AquaSimGpsrRouting);

AquaSimGpsrRouting::AquaSimGpsrRouting()
    : has_set_neighbor(false)
{
    m_maxForward = 5;
    m_pkt_t1_out = 100;
    m_pkt_t2_out = 300;
    m_pkt_t_slot = 50;
    m_neighbor_t_out = 180;
    m_neighbor_t_slot = 50;
    m_NodeNum = 10;
}

AquaSimGpsrRouting::~AquaSimGpsrRouting()
{
}

TypeId
AquaSimGpsrRouting::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AquaSimGpsrRouting")
            .SetParent<AquaSimRouting>()
            .AddConstructor<AquaSimGpsrRouting>()
            .AddAttribute("max_forwards",
                          "the max num_forwards of broadcast packets.",
                          IntegerValue(5),
                          MakeIntegerAccessor(&AquaSimGpsrRouting::m_maxForward),
                          MakeIntegerChecker<int>())
            .AddAttribute("neighbor_time_out",
                          "Time out of neighbors",
                          DoubleValue(180),
                          MakeDoubleAccessor(&AquaSimGpsrRouting::m_neighbor_t_out),
                          MakeDoubleChecker<double>())
            .AddAttribute("neighbor_time_interval",
                          "Time slot of neighbors",
                          DoubleValue(50),
                          MakeDoubleAccessor(&AquaSimGpsrRouting::m_neighbor_t_slot),
                          MakeDoubleChecker<double>())
            .AddAttribute("packet_time_out",
                          "Time out of packets(T2)",
                          DoubleValue(500),
                          MakeDoubleAccessor(&AquaSimGpsrRouting::m_pkt_t2_out),
                          MakeDoubleChecker<double>())
            .AddAttribute("broad_timeout_T1_s",
                          "Time out of packets Aggregation",
                          DoubleValue(100),
                          MakeDoubleAccessor(&AquaSimGpsrRouting::m_pkt_t1_out),
                          MakeDoubleChecker<double>())
            .AddAttribute("packet_time_interval",
                          "Time slot of packets",
                          DoubleValue(50),
                          MakeDoubleAccessor(&AquaSimGpsrRouting::m_pkt_t_slot),
                          MakeDoubleChecker<double>())
            .AddAttribute("Node_num",
                          "the total number of nodes.",
                          IntegerValue(10),
                          MakeIntegerAccessor(&AquaSimGpsrRouting::m_NodeNum),
                          MakeIntegerChecker<int>())
            .AddTraceSource("optimizedTx",
                            "Net:A new packet is sent",
                            MakeTraceSourceAccessor(&AquaSimGpsrRouting::m_txTrace),
                            "AquaSimGpsrRouting::TxCallback")
            .AddTraceSource("optimizedRx",
                            "Net:A new packet is received",
                            MakeTraceSourceAccessor(&AquaSimGpsrRouting::m_rxTrace),
                            "AquaSimGpsrRouting::RxCallback")
            .AddTraceSource("optimizedInitialNet",
                            "Net:finish initial network",
                            MakeTraceSourceAccessor(&AquaSimGpsrRouting::m_InitialNetTrace),
                            "AquaSimGpsrRouting::InitialNetCallback");
    return tid;
}

int64_t
AquaSimGpsrRouting::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    return 0;
}

bool
AquaSimGpsrRouting::Recv(Ptr<Packet> p, const Address& dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this << p);
    if (!has_set_neighbor)
    {
        node_distribution_new();
        SetUp();
        has_set_neighbor = true;
        CheckNeighborTimeout();
        CheckBroadcastpktTimeout();
    }

    AquaSimAddress next_hop = FindNextHop(p);

    AquaSimHeader ash;
    p->PeekHeader(ash);

    NS_LOG_DEBUG("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()
                         << "): Received packet : " << ash.GetSize() << " bytes ; "
                         << ash.GetTxTime().GetSeconds() << " sec. ; Src: "
                         << ash.GetSAddr().GetAsInt() << "  ; Dest: " << ash.GetDAddr().GetAsInt()
                         << " ; Next Hop: " << next_hop.GetAsInt());
    NS_LOG_DEBUG("packet Uid = " << p->GetUid());

    if (next_hop.GetAsInt() == 65535)
    {
        NS_LOG_DEBUG("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()
                             << ") Dropping packet " << p << " due to cannot find next hop");
        m_rxTrace(p,
                  Simulator::Now().GetSeconds(),
                  AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                  5);
        return false;
    }
    // 处理位置广播包
    if (static_cast<int>(ash.GetNetDataType()))
    {
        NS_LOG_DEBUG("a location broadcast packet,put in hash");
        PutInHash(p);
        m_rxTrace(p,
                  Simulator::Now().GetSeconds(),
                  AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                  0);
        return true;
    }
    // NS_LOG_DEBUG("not broadcast packet");
    if (IsDeadLoop(p))
    {
        NS_LOG_DEBUG("Dropping packet " << p << " due to route loop");
        m_rxTrace(p,
                  Simulator::Now().GetSeconds(),
                  AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                  1);
        // drop(p, DROP_RTR_ROUTE_LOOP);
        p = 0;
        return false;
    }
    else if (AmISrc(p))
    {
        uint64_t sendBitString = ash.GetSendBitString();
        p->RemoveHeader(ash);
        NS_LOG_DEBUG("i am source,give new header");
        ash.SetDirection(AquaSimHeader::DOWN);
        ash.SetNumForwards(0);
        ash.SetNextHop(next_hop);
        ash.SetDAddr(AquaSimAddress::ConvertFrom(dest));
        ash.SetErrorFlag(false);
        ash.SetUId(p->GetUid());
        ash.SetSize(ash.GetSize() + SR_HDR_LEN); // add the overhead of static routing's header

        uint16_t nodeId = AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt();
        sendBitString |= (1 << (nodeId - 1));
        NS_LOG_FUNCTION("sendBitString:" << sendBitString << ",nodeId:" << nodeId);
        ash.SetSendBitString(sendBitString);
        p->AddHeader(ash);
    }
    else if (!((ash.GetNextHop().GetAsInt() ==
                AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()) ||
               (next_hop == AquaSimAddress::GetBroadcast())))
    {
        m_rxTrace(p,
                  Simulator::Now().GetSeconds(),
                  AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                  2);
        // NS_LOG_DEBUG("Next H.: " << ash.GetNextHop().GetAsInt());
        NS_LOG_DEBUG("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()
                             << ") Dropping packet " << p << " due to duplicate");
        // drop(p, DROP_MAC_DUPLICATE);
        p = 0;
        return false;
    }
    m_rxTrace(p,
              Simulator::Now().GetSeconds(),
              AquaSimAddress::ConvertFrom(m_device->GetAddress()),
              0);
    // increase the number of forwards
    p->RemoveHeader(ash);
    uint8_t numForwards = ash.GetNumForwards() + 1;
    ash.SetNextHop(next_hop);
    ash.SetNumForwards(numForwards);

    uint16_t sendBitString = ash.GetSendBitString();
    uint16_t nodeId = AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt();
    sendBitString |= (1 << (nodeId - 1));
    sendBitString = UpdateBitStringInList(p, sendBitString);
    NS_LOG_FUNCTION("sendBitString:" << sendBitString << ",nodeId:" << nodeId);
    ash.SetSendBitString(sendBitString);

    p->AddHeader(ash);

    if (AmIDst(p))
    {
        AquaSimHeader a;
        p->PeekHeader(a);
        // int HopNum = a.GetNumForwards() - 1;
        m_txTrace(p,
                  Simulator::Now().GetSeconds(),
                  AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                  0);
        NS_LOG_DEBUG("I am destination. Sending up.");
        SendUp(p);
        return true;
    }

    // find the next hop and forward

    if (next_hop != AquaSimAddress::GetBroadcast())
    {
        // send to mac
        NS_LOG_DEBUG("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()
                             << ") find the next hop: Node(" << next_hop.GetAsInt()
                             << "), send to mac");
        SendDown(p, next_hop, Seconds(0.0));
        m_txTrace(p,
                  Simulator::Now().GetSeconds(),
                  AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                  1);
        return true;
    }
    else
    {
        NS_LOG_FUNCTION("drop or broadcast?");
        int loss = PutInPkt(p, ash);
        if (loss == 0)
        {
            if (ash.GetNumForwards() > m_maxForward)
            {
                m_rxTrace(p,
                          Simulator::Now().GetSeconds(),
                          AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                          3);
                NS_LOG_FUNCTION("Dropping packet " << p << " due to too many numforwards");
                return false;
            }
            if (ash.GetSAddr() ==
                AquaSimAddress::ConvertFrom(m_device->GetAddress())) // 本节点产生的广播包不再sendup
            {
                NS_LOG_FUNCTION("a broadcast packet from myself. Send to mac.");
                m_txTrace(p,
                          Simulator::Now().GetSeconds(),
                          AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                          1);
                SendDown(p, next_hop, Seconds(0.0));
                return true;
            }
            // TODO：37节点邻居为33，存疑
            else if (nodeId != 37 && CheckNeighborSent(sendBitString))
            {
                /*
                uint64_t mask = 0;
                for (int i = 0; i < m_NodeNum; i++)
                {
                    if (hashTable[i].alive == 1)
                    {
                        mask |= (1 << i);
                    }
                }
                std::cout << "Dropping packet " << sendBitString << " all neighbors have sent "
                          << mask << "\n";*/
                NS_LOG_INFO("Send Up and drop packet " << p << " all neighbors have sent.");
                Ptr<Packet> cpkt = p->Copy();
                SendUp(cpkt);
                m_rxTrace(p,
                          Simulator::Now().GetSeconds(),
                          AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                          6);
                return false;
            }
            else
            {
                NS_LOG_FUNCTION("a broadcast packet. Sending up and down.");
                m_txTrace(p,
                          Simulator::Now().GetSeconds(),
                          AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                          0);
                m_txTrace(p,
                          Simulator::Now().GetSeconds(),
                          AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                          1);
                Ptr<Packet> cpkt = p->Copy();
                SendUp(cpkt);
                SendDown(p, next_hop, Seconds(0.0));
                return true;
            }
        }
        else
        {
            m_rxTrace(p,
                      Simulator::Now().GetSeconds(),
                      AquaSimAddress::ConvertFrom(m_device->GetAddress()),
                      loss);
            NS_LOG_FUNCTION("Dropping packet " << p << " due to Broadcast Storm");
            p = 0;
            return false;
        }
    }
    return false;
}

/*
 * @param p   a packet
 * @return    the next hop to route packet p
 * */
AquaSimAddress
AquaSimGpsrRouting::FindNextHop(const Ptr<Packet> p)
{
    AquaSimHeader ash;
    p->PeekHeader(ash);
    AquaSimAddress dst = ash.GetDAddr();
    AquaSimAddress nxt_hop1, nxt_hop2, nxt_hop_fail;
    nxt_hop_fail = AquaSimAddress(-1);

    if (dst == AquaSimAddress::GetBroadcast())
        return AquaSimAddress::GetBroadcast();
    else if (dst == m_device->GetAddress())
        return dst;

    nxt_hop1 = GreedyForwarding(p);
    NS_LOG_INFO("nxt_hop1:" << nxt_hop1.GetAsInt());
    if (nxt_hop1.GetAsInt() == 65535)
    {
        nxt_hop2 = PeripheralForwarding(p);
        if (nxt_hop2.GetAsInt() == 65535)
        {
            return nxt_hop_fail;
        }
        else
            return nxt_hop2;
    }
    else
        return nxt_hop1;
}

double
AquaSimGpsrRouting::Distance(int a, int b)
{
    // 注意！b只能是dst
    double dis;
    dis = sqrt((nodes[a].x - nodes[b].x) * (nodes[a].x - nodes[b].x) +
               (nodes[a].y - nodes[b].y) * (nodes[a].y - nodes[b].y) +
               (nodes[a].z - nodes[b].z) * (nodes[a].z - nodes[b].z));
    return dis;
}

Vector
AquaSimGpsrRouting::Vectorab(int a, int b)
{
    Vector ans;
    ans = nodes[b] - nodes[a];
    if (Distance(a, b) != 0)
    {
        ans.x = ans.x / Distance(a, b);
        ans.y = ans.y / Distance(a, b);
        ans.z = ans.z / Distance(a, b);
    }
    return ans;
}

double
AquaSimGpsrRouting::VectorCalculate(Vector a, Vector dst)
{
    double ans = a.x * dst.x + a.y * dst.y + a.z * dst.z;
    return ans;
}

AquaSimAddress
AquaSimGpsrRouting::GreedyForwarding(const Ptr<Packet> p)
{
    AquaSimHeader ash;
    p->PeekHeader(ash);
    int nexthop = -1;
    int dst = AquaSimAddress::ConvertFrom(ash.GetDAddr()).GetAsInt();
    double mindistance = DBL_MAX;

    if (hashTable[dst - 1].alive == 1)
    {
        nexthop = dst;
    }
    else
    {
        for (int i = 0; i < m_NodeNum; i++)
        {
            if (hashTable[i].alive == 1)
            {
                NS_LOG_DEBUG("caculate:"
                             << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()
                             << " " << i + 1 << " " << dst);
                if ((Distance(i + 1, dst) <
                     Distance(AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt(),
                              dst)) &&
                    (Distance(i + 1, dst) < mindistance) &&
                    ((i + 1) != AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()))
                {
                    mindistance = Distance(i + 1, dst);
                    nexthop = i + 1;
                }
            }
        }
    }

    return AquaSimAddress(nexthop);
}

AquaSimAddress
AquaSimGpsrRouting::PeripheralForwarding(const Ptr<Packet> p)
{
    AquaSimHeader ash;
    p->PeekHeader(ash);
    int dst = AquaSimAddress::ConvertFrom(ash.GetDAddr()).GetAsInt();

    uint16_t puid = p->GetUid();
    bool find = 0;
    int table_place = -1;
    int next_choose = -1;
    double next_choose_ans = -DBL_MAX;
    std::vector<int> nexthops;
    Vector to_dst = Vectorab(AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt(), dst);

    for (int k = 0; k < static_cast<int>(peripheral_table.size()); k++)
    {
        if (puid == peripheral_table[k].pkt_uid)
        {
            find = 1;
            table_place = k;
        }
    }

    if (find == 1)
    {
        for (int i = 0; i < m_NodeNum; i++)
        { // 是邻居且没有被周边转发过该数据包
            if ((hashTable[i].alive == 1) &&
                (std::find(peripheral_table[table_place].node_forward.begin(),
                           peripheral_table[table_place].node_forward.end(),
                           i + 1) == peripheral_table[table_place].node_forward.end()))
            {
                nexthops.push_back(i + 1);
            }
        }
    }
    else
    {
        for (int i = 0; i < m_NodeNum; i++)
        {
            if (hashTable[i].alive == 1)
                nexthops.push_back(i + 1);
        }
    }

    for (int j = 0; j < static_cast<int>(nexthops.size()); j++)
    {
        Vector a =
            Vectorab(AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt(), nexthops[j]);
        double vector_ans = VectorCalculate(a, to_dst);
        if (vector_ans > next_choose_ans)
        {
            next_choose_ans = vector_ans;
            next_choose = j;
        }
    }

    if (next_choose < 0)
        return AquaSimAddress(-1);

    if (find == 0)
    {
        peripheral_pkt newp;
        newp.pkt_uid = puid;
        newp.node_forward.push_back(nexthops[next_choose]);
        peripheral_table.push_back(newp);
    }
    else
    {
        peripheral_table[table_place].node_forward.push_back(nexthops[next_choose]);
    }
    return AquaSimAddress(nexthops[next_choose]);
}

// 下面是有关地理位置包的部分

void
AquaSimGpsrRouting::SetUp()
{
    // 这个是网络层获取本节点位置的方法
    /*
     Ptr<DolphinMobilityModel> mobility = m_device->GetNode()->GetObject<DolphinMobilityModel>();
     if (mobility)
   {
       // 获取节点的位置
       Vector position = mobility->GetIdealPosition();
       //loch.SetNodePosition(position);
   }
   */

    initial_num = 0;
    hashTable.resize(m_NodeNum);
    neighbor_num = 0;
    for (int j = 0; j < m_NodeNum; j++)
    {
        hashTable[j].neighbor_position = {0, 0, 0};
        hashTable[j].t_now = Seconds(INT_MAX);
        if (initial_neighbor[AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()]
                            [j + 1] == 1)
        {
            hashTable[j].alive = -1;
            initial_num++;
        }
        else
            hashTable[j].alive = 0;
    }
    if(initial_num<=0)
    {
        Finish_Initial_Network-=1000000;
    }
}

void
AquaSimGpsrRouting::PutInHash(Ptr<Packet> p)
{
    AquaSimHeader ash;
    LocalizationHeader loch;
    p->PeekHeader(ash);
    p->PeekHeader(loch);
    AquaSimAddress src = ash.GetSAddr();
    Time tstamp = ash.GetTimeStamp();
    Vector nowposition;
    nowposition = loch.GetNodePosition();

    int i = src.GetAsInt() - 1;
    if (hashTable[i].alive == 1)
    {
        // 已经存在的邻居，更新其位置和时间
        hashTable[i].neighbor_position = nowposition;
        hashTable[i].t_now = Simulator::Now();
    }
    else if ((hashTable[i].alive == -1))
    {
        // 在初始邻居表里的邻居
        NS_LOG_DEBUG("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()
                             << ") find initial neighbor node(" << src.GetAsInt() << ") at "
                             << Simulator::Now().GetSeconds());
        hashTable[i].neighbor_position = nowposition;
        hashTable[i].t_now = Simulator::Now();
        hashTable[i].alive = 1;
        neighbor_num++;
        initial_num--;
        if (initial_num == 0)
        {
            Finish_Initial_Network++;
            NS_LOG_DEBUG("!!!Node("
                         << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()
                         << ") find all initial neighbors at " << Simulator::Now().GetSeconds());
            if (m_NodeNum == 10)
            {
                if (Finish_Initial_Network == m_NodeNum)
                {
                    NS_LOG_DEBUG("!!!!!!all nodes find their initial neighbors at "
                                 << Simulator::Now().GetSeconds());
                    m_InitialNetTrace(Simulator::Now().GetSeconds());
                }
            }
            else
            {
                if (Finish_Initial_Network == m_NodeNum - 2)
                {
                    NS_LOG_DEBUG("!!!!!!all nodes find their initial neighbors at "
                                 << Simulator::Now().GetSeconds());
                    m_InitialNetTrace(Simulator::Now().GetSeconds());
                }
            }
        }
    }
    else // 有新邻居
    {
        NS_LOG_DEBUG("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()
                             << ") find new neighbor node(" << src.GetAsInt() << ") at "
                             << Simulator::Now().GetSeconds());
        hashTable[i].neighbor_position = nowposition;
        hashTable[i].t_now = Simulator::Now();
        hashTable[i].alive = 1;
        neighbor_num++;
    }
}

bool
AquaSimGpsrRouting::CheckNeighborSent(uint64_t bitstring)
{
    uint64_t mask = 0;
    for (int i = 0; i < m_NodeNum; i++)
    {
        if (hashTable[i].alive == 1)
        {
            mask |= (1 << i);
        }
    }
    if (mask != 0 && (bitstring & mask) == mask)
    {
        return true;
    }
    return false;
}

void
AquaSimGpsrRouting::CheckNeighborTimeout()
{
    Time time_out = Time::FromDouble(m_neighbor_t_out, Time::S);
    Time time_slot = Time::FromDouble(m_neighbor_t_slot, Time::S);

    int update = 0;
    for (int i = 0; i < m_NodeNum; i++)
    {
        if ((hashTable[i].alive == 1) && (Simulator::Now() - hashTable[i].t_now > time_out))
        {
            NS_LOG_DEBUG("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()
                                 << ")'s Neighbor " << i + 1
                                 << " is deleted from AquaSimGPSRHashTable because of time out");
            hashTable[i].alive = 0;
            neighbor_num--;
            update = 1;
        }
    }
    if (update == 1)
        NS_LOG_DEBUG("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()
                             << ") has " << neighbor_num << " neighbors after updating");

    // Simulator::Schedule(time_slot,this, &AquaSimGpsrRouting::CheckNeighborTimeout);
    Simulator::Schedule(time_slot, MakeEvent(&AquaSimGpsrRouting::CheckNeighborTimeout, this));
}

// 获取存的包的bitstring
uint64_t
AquaSimGpsrRouting::UpdateBitStringInList(Ptr<Packet> p, uint64_t bitString)
{
    uint16_t pktuid = p->GetUid(); // uint16_t

    int pktnum = static_cast<int>(packet_table.size());
    for (int i = 0; i < pktnum; i++)
    {
        if (pktuid == packet_table[i].pkt_uid)
        {
            packet_table[i].pkt_bitstring |= bitString;
            return packet_table[i].pkt_bitstring;
        }
    }
    return bitString;
}

int // 返回大于0为丢包原因，说明pkt表里有未超时的相同包或因为其它原因丢弃该包，因此不继续广播；返回0说明是新的包，进行广播。
AquaSimGpsrRouting::PutInPkt(Ptr<Packet> p, AquaSimHeader ash)
{
    uint16_t pktuid = p->GetUid(); // uint16_t

    int pktnum = static_cast<int>(packet_table.size());
    int find = 0;
    for (int i = 0; i < pktnum; i++)
    {
        if (pktuid == packet_table[i].pkt_uid)
        {
            find = 1;
            // return false;
            Time t1_time_out = Time::FromDouble(m_pkt_t1_out, Time::S);
            Time t2_time_out = Time::FromDouble(m_pkt_t2_out, Time::S);

            // 失效包，不广播
            if (Simulator::Now() - packet_table[i].pkt_t > t2_time_out)
            {
                NS_LOG_DEBUG("there is a timeout packet, its uid is "
                             << pktuid << ", so this packet will not be forwarded");
                return 7;
            }
            else if (Simulator::Now() - packet_table[i].pkt_lastsend_t < t1_time_out)
            { // 聚合时间内的相同包，只是更新bitstring，不广播
                NS_LOG_DEBUG("there is a same packet, its uid is "
                             << pktuid << ", so this packet will not be forwarded");
                return 4;
            }
            else
            { // 超过聚合时间的包，更新bitstring，并广播
                packet_table[i].pkt_lastsend_t = Simulator::Now();
                return 0;
            }
        }
    }
    if (find == 0)
    {
        NS_LOG_DEBUG("it is a new packet, its uid is " << pktuid
                                                       << ", so this packet be forwarded");
        gpsr_packet newpkt;
        newpkt.pkt_uid = pktuid;
        newpkt.pkt_t = Simulator::Now();
        newpkt.pkt_lastsend_t = Simulator::Now();
        newpkt.pkt_bitstring = ash.GetSendBitString();
        packet_table.push_back(newpkt);
        return 0;
    }
    return 0;
}

void
AquaSimGpsrRouting::CheckBroadcastpktTimeout()
{
    Time t2_time_out = Time::FromDouble(m_pkt_t2_out, Time::S);
    Time time_slot = Time::FromDouble(m_pkt_t_slot, Time::S);

    if (static_cast<int>(packet_table.size()) > 0)
    { // check for nullptr
        for (int i = 0; i < static_cast<int>(packet_table.size());)
        {
            if (Simulator::Now() - packet_table[i].pkt_t > t2_time_out)
            {
                // 交换当前超时的元素与末尾元素
                std::swap(packet_table[i], packet_table.back());
                // 删除末尾元素
                packet_table.pop_back();
            }
            else
            {
                ++i;
            }
        }

        // if (updated)  NS_LOG_DEBUG("Node(" << nodeid << ") has " << packet->pkt_num << "
        // packets after updating ");
    }
    // 重新调度自己
    // Simulator::Schedule(time_slot, this,&AquaSimGpsrRouting::CheckBroadcastpktTimeout);
    Simulator::Schedule(time_slot, MakeEvent(&AquaSimGpsrRouting::CheckBroadcastpktTimeout, this));
}

void
AquaSimGpsrRouting::node_distribution_new()
{
    if (m_NodeNum == 10)
    {
        nodes.resize(11);
        initial_neighbor.resize(11);
        for (int k = 0; k < 11; k++)
        {
            nodes[k].z = 0;
            initial_neighbor[k].resize(11, 0);
        }
        nodes[1].x = 0;
        nodes[1].y = -3000;
        nodes[2].x = 1300;
        nodes[2].y = 750;
        nodes[3].x = 1300;
        nodes[3].y = -2250;
        nodes[4].x = 1300;
        nodes[4].y = -5250;
        nodes[5].x = 0;
        nodes[5].y = 3000;
        nodes[6].x = 0;
        nodes[6].y = -6000;
        nodes[7].x = -1300;
        nodes[7].y = 2250;
        nodes[8].x = -1300;
        nodes[8].y = -3750;
        nodes[9].x = -1300;
        nodes[9].y = -750;
        nodes[10].x = 0;
        nodes[10].y = 0;

        initial_neighbor = {
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1},
            {0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1},
            {0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1},
            {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0},
            {0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1},
            {0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0},
            {0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1},
            {0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0},
            {0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1},
            {0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0},
        };
    }
    else
    {
        nodes.resize(39);
        initial_neighbor.resize(39);
        for (int k = 0; k < 39; k++)
        {
            nodes[k].z = 0;
            initial_neighbor[k].resize(39, 0);
        }
        nodes[37].x = -42;
        nodes[0].y = 30;
        nodes[38].x = 42;
        nodes[0].y = -30;

        nodes[1].x = -30;
        nodes[1].y = 30;
        nodes[2].x = -18;
        nodes[2].y = 30;
        nodes[3].x = -6;
        nodes[3].y = 30;
        nodes[4].x = -6;
        nodes[4].y = 18;
        nodes[5].x = -18;
        nodes[5].y = 18;
        nodes[6].x = -30;
        nodes[6].y = 18;
        nodes[7].x = -30;
        nodes[7].y = 6;
        nodes[8].x = -18;
        nodes[8].y = 6;
        nodes[9].x = -6;
        nodes[9].y = 6;
        nodes[10].x = 6;
        nodes[10].y = 30;
        nodes[11].x = 18;
        nodes[11].y = 30;
        nodes[12].x = 30;
        nodes[12].y = 30;
        nodes[13].x = 30;
        nodes[13].y = 18;
        nodes[14].x = 18;
        nodes[14].y = 18;
        nodes[15].x = 6;
        nodes[15].y = 18;
        nodes[16].x = 6;
        nodes[16].y = 6;
        nodes[17].x = 18;
        nodes[17].y = 6;
        nodes[18].x = 30;
        nodes[18].y = 6;
        nodes[19].x = -30;
        nodes[19].y = -6;
        nodes[20].x = -18;
        nodes[20].y = -6;
        nodes[21].x = -6;
        nodes[21].y = -6;
        nodes[22].x = -6;
        nodes[22].y = -18;
        nodes[23].x = -18;
        nodes[23].y = -18;
        nodes[24].x = -30;
        nodes[24].y = -18;
        nodes[25].x = -30;
        nodes[25].y = -30;
        nodes[26].x = -18;
        nodes[26].y = -30;
        nodes[27].x = -6;
        nodes[27].y = -30;
        nodes[28].x = 6;
        nodes[28].y = -6;
        nodes[29].x = 18;
        nodes[29].y = -6;
        nodes[30].x = 30;
        nodes[30].y = -6;
        nodes[31].x = 30;
        nodes[31].y = -18;
        nodes[32].x = 18;
        nodes[32].y = -18;
        nodes[33].x = 6;
        nodes[33].y = -18;
        nodes[34].x = 6;
        nodes[34].y = -30;
        nodes[35].x = 18;
        nodes[35].y = -30;
        nodes[36].x = 30;
        nodes[36].y = -30;

        for (int j = 1; j < 39; j++)
        { // 调整节点位置布局
            nodes[j].x = nodes[j].x * 1000 * 11 / (16.97056328444);
            nodes[j].y = nodes[j].y * 1000 * 11 / (16.97056328444);
        }

        for (int i = 0; i < 4; i++)
        {
            initial_neighbor[1 + i * 9][2 + i * 9] = 1;
            initial_neighbor[1 + i * 9][5 + i * 9] = 1;
            initial_neighbor[1 + i * 9][6 + i * 9] = 1;
            initial_neighbor[10][3] = 1;
            initial_neighbor[10][4] = 1;
            initial_neighbor[19][7] = 1;
            initial_neighbor[19][8] = 1;
            initial_neighbor[28][9] = 1;
            initial_neighbor[28][16] = 1;
            initial_neighbor[28][17] = 1;
            initial_neighbor[28][21] = 1;
            initial_neighbor[28][22] = 1;

            initial_neighbor[2 + i * 9][1 + i * 9] = 1;
            initial_neighbor[2 + i * 9][3 + i * 9] = 1;
            initial_neighbor[2 + i * 9][4 + i * 9] = 1;
            initial_neighbor[2 + i * 9][5 + i * 9] = 1;
            initial_neighbor[2 + i * 9][6 + i * 9] = 1;
            initial_neighbor[20][7] = 1;
            initial_neighbor[20][8] = 1;
            initial_neighbor[20][9] = 1;
            initial_neighbor[29][16] = 1;
            initial_neighbor[29][17] = 1;
            initial_neighbor[29][18] = 1;

            initial_neighbor[3 + i * 9][2 + i * 9] = 1;
            initial_neighbor[3 + i * 9][4 + i * 9] = 1;
            initial_neighbor[3 + i * 9][5 + i * 9] = 1;
            initial_neighbor[3][10] = 1;
            initial_neighbor[3][15] = 1;
            initial_neighbor[21][8] = 1;
            initial_neighbor[21][9] = 1;
            initial_neighbor[21][16] = 1;
            initial_neighbor[21][28] = 1;
            initial_neighbor[21][33] = 1;
            initial_neighbor[30][17] = 1;
            initial_neighbor[30][18] = 1;

            initial_neighbor[4 + i * 9][2 + i * 9] = 1;
            initial_neighbor[4 + i * 9][3 + i * 9] = 1;
            initial_neighbor[4 + i * 9][5 + i * 9] = 1;
            initial_neighbor[4 + i * 9][8 + i * 9] = 1;
            initial_neighbor[4 + i * 9][9 + i * 9] = 1;
            initial_neighbor[4][10] = 1;
            initial_neighbor[4][15] = 1;
            initial_neighbor[4][16] = 1;
            initial_neighbor[22][28] = 1;
            initial_neighbor[22][33] = 1;
            initial_neighbor[22][34] = 1;

            initial_neighbor[5 + i * 9][1 + i * 9] = 1;
            initial_neighbor[5 + i * 9][2 + i * 9] = 1;
            initial_neighbor[5 + i * 9][3 + i * 9] = 1;
            initial_neighbor[5 + i * 9][4 + i * 9] = 1;
            initial_neighbor[5 + i * 9][6 + i * 9] = 1;
            initial_neighbor[5 + i * 9][7 + i * 9] = 1;
            initial_neighbor[5 + i * 9][8 + i * 9] = 1;
            initial_neighbor[5 + i * 9][9 + i * 9] = 1;

            initial_neighbor[6 + i * 9][2 + i * 9] = 1;
            initial_neighbor[6 + i * 9][1 + i * 9] = 1;
            initial_neighbor[6 + i * 9][5 + i * 9] = 1;
            initial_neighbor[6 + i * 9][8 + i * 9] = 1;
            initial_neighbor[6 + i * 9][7 + i * 9] = 1;
            initial_neighbor[15][3] = 1;
            initial_neighbor[15][4] = 1;
            initial_neighbor[15][9] = 1;
            initial_neighbor[33][21] = 1;
            initial_neighbor[33][22] = 1;
            initial_neighbor[33][27] = 1;

            initial_neighbor[7 + i * 9][5 + i * 9] = 1;
            initial_neighbor[7 + i * 9][6 + i * 9] = 1;
            initial_neighbor[7 + i * 9][8 + i * 9] = 1;
            initial_neighbor[7][19] = 1;
            initial_neighbor[7][20] = 1;
            initial_neighbor[16][4] = 1;
            initial_neighbor[16][9] = 1;
            initial_neighbor[16][21] = 1;
            initial_neighbor[16][28] = 1;
            initial_neighbor[16][29] = 1;
            initial_neighbor[34][22] = 1;
            initial_neighbor[34][27] = 1;

            initial_neighbor[8 + i * 9][7 + i * 9] = 1;
            initial_neighbor[8 + i * 9][9 + i * 9] = 1;
            initial_neighbor[8 + i * 9][4 + i * 9] = 1;
            initial_neighbor[8 + i * 9][5 + i * 9] = 1;
            initial_neighbor[8 + i * 9][6 + i * 9] = 1;
            initial_neighbor[8][19] = 1;
            initial_neighbor[8][20] = 1;
            initial_neighbor[8][21] = 1;
            initial_neighbor[17][28] = 1;
            initial_neighbor[17][29] = 1;
            initial_neighbor[17][30] = 1;

            initial_neighbor[9 + i * 9][8 + i * 9] = 1;
            initial_neighbor[9 + i * 9][4 + i * 9] = 1;
            initial_neighbor[9 + i * 9][5 + i * 9] = 1;
            initial_neighbor[9][15] = 1;
            initial_neighbor[9][16] = 1;
            initial_neighbor[9][20] = 1;
            initial_neighbor[9][21] = 1;
            initial_neighbor[9][28] = 1;
            initial_neighbor[18][29] = 1;
            initial_neighbor[18][30] = 1;
            initial_neighbor[27][33] = 1;
            initial_neighbor[27][34] = 1;
        }
    }
}

} // namespace ns3