#include "dolphin-routing-LEACH.h"
#include "dolphin-clustering-header.h"
#include "aqua-sim-address.h"
#include "aqua-sim-header-routing.h"
#include "aqua-sim-header.h"
#include "ns3/log.h"
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DolphinRoutingLEACH");
NS_OBJECT_ENSURE_REGISTERED(DolphinRoutingLEACH);

// Clustering Routing Protocol  分簇路由协议
DolphinRoutingLEACH::DolphinRoutingLEACH()
    :m_Timer(Timer::CANCEL_ON_DESTROY),
    m_ability(SHORT),
    m_identity(CM),
    m_rounds(0),
    last_ch_round(-10)
{
}

DolphinRoutingLEACH::~DolphinRoutingLEACH()
{
}

TypeId
DolphinRoutingLEACH::GetTypeId()
{
    static TypeId tid = TypeId ("ns3::DolphinRoutingLEACH")
        .SetParent<AquaSimRouting>()
        .AddConstructor<DolphinRoutingLEACH>()
        .AddAttribute("Interval",
                    "cluster reforming interval.",
                    TimeValue(Seconds(30)),
                    MakeTimeAccessor(&DolphinRoutingLEACH::m_interval),
                    MakeTimeChecker())
        .AddAttribute("Prob",
                    "parameter prob for link-hold probability",
                    DoubleValue(0.2),
                    MakeDoubleAccessor(&DolphinRoutingLEACH::prob),
                    MakeDoubleChecker<double>())
        .AddAttribute("FormingTime",
                    "the time when network initialization end",
                    DoubleValue(2.),
                    MakeDoubleAccessor(&DolphinRoutingLEACH::formingTime),
                    MakeDoubleChecker<double>())
        .AddAttribute("RoutingTime",
                    "the time when cluster forming end",
                    DoubleValue(4.),
                    MakeDoubleAccessor(&DolphinRoutingLEACH::routingTime),
                    MakeDoubleChecker<double>())
        .AddAttribute("Ability",
                    "the node's transmission ability",
                    EnumValue(SHORT),
                    MakeEnumAccessor(&DolphinRoutingLEACH::m_ability),
                    MakeEnumChecker(SHORT, "SHORT", LONG, "LONG"))
        .AddAttribute("Identity",
                    "the node's identity",
                    EnumValue(CM),
                    MakeEnumAccessor(&DolphinRoutingLEACH::m_identity),
                    MakeEnumChecker(CM, "CM", CH, "CH", SINK, "SINK"))
        .AddAttribute("NMembers",
                    "num of CH's members",
                    IntegerValue(0),
                    MakeIntegerAccessor(&DolphinRoutingLEACH::n_members),
                    MakeIntegerChecker<int>())
        .AddAttribute("Range",
                    "the node's transmission range",
                    DoubleValue(1000.),
                    MakeDoubleAccessor(&DolphinRoutingLEACH::m_range),
                    MakeDoubleChecker<double>())
        .AddTraceSource("Tx",
                    "Net:A new packet is sent",
                    MakeTraceSourceAccessor(&DolphinRoutingLEACH::m_txTrace),
                    "DolphinRoutingLEACH::TxCallback")
        .AddTraceSource("Rx",
                    "Net:A new packet is received",
                    MakeTraceSourceAccessor(&DolphinRoutingLEACH::m_rxTrace),
                    "DolphinRoutingLEACH::RxCallback")
        .AddTraceSource("Counter",
                    "Net:Evaluation Counter",
                    MakeTraceSourceAccessor(&DolphinRoutingLEACH::m_counterTrace),
                    "DolphinRoutingLEACH::CounterCallBack")
        .AddTraceSource("Delay",
                    "Net:End2End Delay Counter",
                    MakeTraceSourceAccessor(&DolphinRoutingLEACH::m_delayTrace),
                    "DolphinRoutingLEACH::CounterCallBack")
    ;
    return tid;
}

int64_t
DolphinRoutingLEACH::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    return 0;
}

bool
DolphinRoutingLEACH::Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this);
    AquaSimHeader ash;
    AquaSimAddress myAddr = AquaSimAddress::ConvertFrom(GetNetDevice()->GetAddress());
    packet->PeekHeader(ash);
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
    if (ash.GetNetDataType() != AquaSimHeader::DATA)     // 控制包
    {
        switch (ash.GetNetDataType())
        {
        case AquaSimHeader::HELLO:
            ProcessHello(packet);
            break;
        case AquaSimHeader::JOIN:
            ProcessJoinAnnouncement(packet);
            break;
        case AquaSimHeader::LS:
            ProcessLS(packet);
            break;
        default:
            NS_FATAL_ERROR("Receiving Unknown Clustering Control Packet Type:" << ash.GetNetDataType());
            break;
        }
    }else                                                   // 数据包
    {
        AquaSimAddress nxtAddr;
        if (AmISrc(packet))               // new packet
        {
            packet->RemoveHeader(ash);
            ash.SetDirection(AquaSimHeader::DOWN);
            ash.SetNumForwards(1);
            ash.SetDAddr(AquaSimAddress::ConvertFrom(dest));
            ash.SetErrorFlag(false);
            ash.SetUId(packet->GetUid());
            ash.SetSize(1);
            ash.SetSAddr(myAddr);
            if (AquaSimAddress::ConvertFrom(dest) == AquaSimAddress::GetBroadcast()) // 广播包
            {
                m_counterTrace(6, 0);
                m_delayTrace(packet->GetUid(), 0, Simulator::Now().GetSeconds());
                if (m_identity == CM)
                {
                    if (m_ch == AquaSimAddress::GetBroadcast())
                    {
                        packet->AddHeader(ash);
                        m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 5);
                        NS_LOG_UNCOND("Can't find cluster head!  " << packet);
                        packet = 0;
                        m_counterTrace(6, 2);
                        return false;
                    }
                    nxtAddr = m_ch;
                }
                else
                    nxtAddr = AquaSimAddress::GetBroadcast();
                m_forwarded.insert(packet->GetUid());
                ash.SetNextHop(nxtAddr);
                packet->AddHeader(ash);
                m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
                SendDown(packet, nxtAddr, Seconds(0));
                return true;
            }
            // 单播包
            m_counterTrace(6, 0);
            m_delayTrace(packet->GetUid(), 0, Simulator::Now().GetSeconds());
            if (m_identity == CM)   // 转发给簇头
            {
                nxtAddr = m_ch;
            }else if (m_members.find(AquaSimAddress::ConvertFrom(dest)) != m_members.end())
                nxtAddr = AquaSimAddress::ConvertFrom(dest);
            else
                nxtAddr = AquaSimAddress::GetBroadcast();
        }else                           // forward packet
        {
            AquaSimAddress dAddr = ash.GetDAddr();
            uint16_t pid = ash.GetUId();
            if (m_identity == CM && ash.GetSAddr() != m_ch)
            {
                packet = 0;
                return false;
            }
            auto it = m_forwarded.find(pid);
            if (it != m_forwarded.end())
            {
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 4);
                if (dAddr != myAddr)
                    m_counterTrace(6, 2);
                packet = 0;
                return false;
            }
            m_forwarded.insert(pid);

            if (dAddr == AquaSimAddress::GetBroadcast()) // 广播包
            {
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
                Ptr<Packet> cpkt = packet->Copy();
                m_txTrace(cpkt, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
                m_counterTrace(6, 1);
                m_delayTrace(packet->GetUid(), 1, Simulator::Now().GetSeconds());
                SendUp(cpkt);
                if (m_identity == CM)
                {
                    packet = 0;
                    return true;
                }
                packet->RemoveHeader(ash);
                ash.SetNumForwards(ash.GetNumForwards() + 1);
                ash.SetSAddr(myAddr);
                packet->AddHeader(ash);
                m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
                SendDown(packet, AquaSimAddress::GetBroadcast(), Seconds(0));
                return true;
            }
            // 单播包
            if (dAddr == myAddr)        // 我是dst
            {
                m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
                m_counterTrace(6, 1);
                m_delayTrace(packet->GetUid(), 1, Simulator::Now().GetSeconds());
                SendUp(packet);
                return true;
            }
            if (IsDeadLoop(packet))
            {
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
                NS_LOG_UNCOND("Dropping pkt" << packet << " due to route loop");
                packet = 0;
                return false;
            }                
            if (m_identity == CM)             
            {
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 5);
                NS_LOG_UNCOND("Can't find routing table entry!  " << packet);
                packet = 0;
                return false;
            }     
                                        // 根据路由表转发
            m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
            packet->RemoveHeader(ash);
            if (m_members.find(dAddr) != m_members.end())
                nxtAddr = dAddr;
            else
                nxtAddr = AquaSimAddress::GetBroadcast();
            ash.SetNumForwards(ash.GetNumForwards() + 1);
        }
        ash.SetNextHop(nxtAddr);
        packet->AddHeader(ash);
        m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
        m_counterTrace(7, ash.GetSize());
        double delay = 0.75;
        SendDown(packet, nxtAddr, Seconds(rand->GetValue(0, delay)));
    }
    return true;
}

void
DolphinRoutingLEACH::ProcessHello(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringHelloHeader dchh;
    packet->RemoveHeader(dchh);
    AquaSimAddress src = ash.GetSAddr();
    Vector pos = dchh.GetPosition();

    if (m_identity == CH)
        m_chNeighbors.emplace_back(src);
    else
    {
        double dis = CalculateDistance(m_position, pos);
        if (dis < min_dis)
        {
            min_dis = dis;
            m_ch = src;
        }
    }

    packet = 0;
}

void
DolphinRoutingLEACH::ProcessJoinAnnouncement(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringAnnouncementHeader dcah;
    packet->RemoveHeader(dcah);
    AquaSimAddress src = ash.GetSAddr();
    m_members.insert(src);
    n_members++;
    packet = 0;
}

void
DolphinRoutingLEACH::ProcessLS(Ptr<Packet> packet)
{
    if (m_identity == CM)   // 仅簇头处理LS
    {
        m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 2);
        packet = 0;
        return;
    }
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringLinkStateHeader dclsh;
    packet->PeekHeader(dclsh);
    Time ts = dclsh.GetTimeStamp();
    AquaSimAddress src = dclsh.GetSrc();
    auto it = m_LSTimeStamp.find(src);
    if (it != m_LSTimeStamp.end() && it->second >= ts)      // timestamp不更新，丢弃这个包
    {
        packet->AddHeader(ash);
        m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 3);
        packet = 0;
        return;
    }
    uint16_t uid = ash.GetUId();
    std::vector<AquaSimAddress> links = dclsh.GetLinks();
    std::vector<AquaSimAddress> members = dclsh.GetMembers();
    m_LSTimeStamp[src] = ts;
    m_CHLinksTable[src] = links;
    m_CHMemberTable[src] = members;
    CalcRoutingTable();

    // 转发此包
    ash.SetDirection(AquaSimHeader::DOWN);
    AquaSimAddress asBroadcast = AquaSimAddress::GetBroadcast();
    ash.SetSAddr(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    ash.SetTimeStamp(Simulator::Now());
    packet->AddHeader(ash);
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(2, ash.GetSize());
    SendDown(packet, asBroadcast, Seconds(0.0));
}

void
DolphinRoutingLEACH::SendHello()
{
    m_identity = CH;
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringHelloHeader dchh;
    // 配置hello头
    dchh.SetPosition(m_position);
    // 配置aqua-sim头
    ash.SetDirection(AquaSimHeader::DOWN);
    AquaSimAddress asBroadcast = AquaSimAddress::GetBroadcast();
    ash.SetNextHop(asBroadcast);
    ash.SetSAddr(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    ash.SetDAddr(asBroadcast);
    ash.SetErrorFlag(false);
    ash.SetUId(packet->GetUid());
    ash.SetTimeStamp(Simulator::Now());
    ash.SetNetDataType(AquaSimHeader::HELLO);
    ash.SetSize(9);
    // 添加headers
    packet->AddHeader(dchh);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(1, ash.GetSize());
    SendDown(packet, asBroadcast, Seconds(0.0));
    NS_LOG_UNCOND("Node " << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() 
                    << " send ch announcement");
}

void
DolphinRoutingLEACH::SendJoinAnnouncement()
{
    if (m_ch == AquaSimAddress::GetBroadcast())
        return;
    m_identity = CM;
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringAnnouncementHeader dcah;
    // 配置announcement头
    dcah.SetCHID(m_ch);
    // 配置aqua-sim头
    ash.SetDirection(AquaSimHeader::DOWN);
    ash.SetNextHop(m_ch);
    ash.SetSAddr(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    ash.SetDAddr(m_ch);
    ash.SetErrorFlag(false);
    ash.SetUId(packet->GetUid());
    ash.SetTimeStamp(Simulator::Now());
    ash.SetNetDataType(AquaSimHeader::JOIN);
    ash.SetSize(5);
    // 添加headers
    packet->AddHeader(dcah);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(3, ash.GetSize());
    SendDown(packet, m_ch, Seconds(0.0));
    // NS_LOG_UNCOND("Node " << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() 
    //                 << " send join announcement; join in CH " << AquaSimAddress::ConvertFrom(head).GetAsInt());
}

void
DolphinRoutingLEACH::SendLS()
{
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringLinkStateHeader dclsh;
    // 配置LS头
    dclsh.SetSrc(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    dclsh.SetTimeStamp(Simulator::Now());
    AquaSimAddress myAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    m_LSTimeStamp[myAddr] = Simulator::Now();
    dclsh.SetLinks(m_chNeighbors.size(), m_chNeighbors);  
    // 配置aqua-sim头
    ash.SetDirection(AquaSimHeader::DOWN);
    AquaSimAddress asBroadcast = AquaSimAddress::GetBroadcast();
    ash.SetNextHop(asBroadcast);
    ash.SetSAddr(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    ash.SetDAddr(asBroadcast);
    ash.SetErrorFlag(false);
    ash.SetUId(packet->GetUid());
    ash.SetTimeStamp(Simulator::Now());
    ash.SetNetDataType(AquaSimHeader::LS);
    ash.SetSize(13 + 2 * (m_chNeighbors.size() + m_members.size()));
    // 添加headers
    packet->AddHeader(dclsh);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(2, ash.GetSize());
    SendDown(packet, asBroadcast, Seconds(0.0));

    // NS_LOG_UNCOND("LS packet id = " << ash.GetUId() << " links:");
    // auto links = dclsh.GetLinks();
    // for (auto i = links.begin(); i != links.end(); i++)
    //     NS_LOG_UNCOND(AquaSimAddress::ConvertFrom(*i));
    // NS_LOG_UNCOND("members:");
    // auto members = dclsh.GetMembers();
    // for (auto i = members.begin(); i != members.end(); i++)
    //     NS_LOG_UNCOND(AquaSimAddress::ConvertFrom(*i));
}

void
DolphinRoutingLEACH::SetIdentity(NodeIdentity identity)
{
    m_identity = identity;
}

void
DolphinRoutingLEACH::UpdatePosition()
{
    Ptr<Node> m_node = m_device->GetNode();
    Ptr<DolphinMobilityModel> mobility = m_node->GetObject<DolphinMobilityModel>();
    m_position = mobility->GetLocatedPosition();
}

void
DolphinRoutingLEACH::CalcRoutingTable()
{
    std::set<AquaSimAddress> vis;
    vis.insert(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    std::queue<AquaSimAddress> q;
    //     确定各节点身份及各自最新的CH-CM链接时间
    AquaSimAddress myAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    std::map<AquaSimAddress, uint8_t> identity;
    std::map<AquaSimAddress, Time> latest;
    identity[myAddr] = CH;
    for (auto i = m_LSTimeStamp.begin(); i != m_LSTimeStamp.end(); i++)
        identity[i->first] = CH;
    for (auto i = m_CHMemberTable.begin(); i != m_CHMemberTable.end(); i++)
        for (auto j = i->second.begin(); j != i->second.end(); j++)
        {
            if (latest.find(*j) == latest.end() || m_LSTimeStamp[i->first] > latest[*j])
            {
                identity[*j] = CM;
                latest[*j] = m_LSTimeStamp[i->first];
            }
        }
    //     bfs计算路由
    for (auto i = m_members.begin(); i != m_members.end(); i++)
        if (m_LSTimeStamp[myAddr] == latest[*i])
            m_RoutingTable[*i] = LEACHRoutingTableEntry(*i, 1);
    for (auto i = m_chNeighbors.begin(); i != m_chNeighbors.end(); i++)
        if (identity[*i] == CH)
        {
            q.push(*i);
            vis.insert(*i);
            m_RoutingTable[*i] = LEACHRoutingTableEntry(*i, 1);
        }
    AquaSimAddress addr, nxthop;
    uint16_t dis;
    for (; !q.empty(); q.pop())
    {
        addr = q.front();
        nxthop = m_RoutingTable[addr].nxt;
        dis = m_RoutingTable[addr].dist;
        auto members = m_CHMemberTable[addr];
        Time ts = m_LSTimeStamp[addr];
        for (auto i = members.begin(); i != members.end(); i++)
            if (ts == latest[*i])
                m_RoutingTable[*i] = LEACHRoutingTableEntry(nxthop, dis + 1);
        auto links = m_CHLinksTable[addr];
        for (auto i = links.begin(); i != links.end(); i++)
            if (identity[*i] == CH && vis.find(*i) == vis.end())
            {
                q.push(*i);
                vis.insert(*i);
                m_RoutingTable[*i] = LEACHRoutingTableEntry(nxthop, dis + 1);
            }
    }
}

bool
DolphinRoutingLEACH::SelectCH()
{
    if (m_rounds - last_ch_round < int(1. / prob))
        return false;
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    double p = prob / (1 - prob * (m_rounds % int(1. / prob)));
    if (rand->GetValue() <= p)
        return true;
    return false;
}

void
DolphinRoutingLEACH::TimerExpire()
{
    UpdatePosition();
    m_RoutingTable.clear();
    m_chNeighbors.clear();
    m_members.clear();
    n_members = 0;
    m_rounds++;
    m_ch = AquaSimAddress::GetBroadcast();
    if (m_ability == LONG && m_identity != SINK && SelectCH())
    {
        m_identity = CH;
        last_ch_round = m_rounds;
        Simulator::ScheduleNow(&DolphinRoutingLEACH::SendHello, this);
        // Simulator::Schedule(Seconds(routingTime), &DolphinRoutingLEACH::SendLS, this);
    }else if (m_identity != SINK)
    {
        m_identity = CM;
        min_dis = 1000000;
        Simulator::Schedule(Seconds(formingTime), &DolphinRoutingLEACH::SendJoinAnnouncement, this);
    }

    m_Timer.Schedule(m_interval);
}

void
DolphinRoutingLEACH::DoInitialize()
{
    lsSeqNum = m_rounds = 0;
    last_ch_round = -int(1. / prob);
    m_chNeighbors.clear();
    m_members.clear();
    m_RoutingTable.clear();
    m_LSTimeStamp.clear();
    m_CHLinksTable.clear();
    m_CHLinksTable.clear();
    m_forwarded.clear();
    m_ch = AquaSimAddress::GetBroadcast();
    AquaSimRouting::DoInitialize();
    m_Timer.SetFunction(&DolphinRoutingLEACH::TimerExpire, this);
    Simulator::ScheduleNow(&DolphinRoutingLEACH::TimerExpire, this);
}

void
DolphinRoutingLEACH::DoDispose()
{
    m_chNeighbors.clear();
    m_members.clear();
    m_RoutingTable.clear();
    m_LSTimeStamp.clear();
    m_CHLinksTable.clear();
    m_CHLinksTable.clear();
    m_forwarded.clear();
    AquaSimRouting::DoDispose();
}