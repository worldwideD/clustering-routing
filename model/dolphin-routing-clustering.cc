#include "dolphin-routing-clustering.h"
#include "dolphin-clustering-header.h"
#include "aqua-sim-address.h"
#include "aqua-sim-header-routing.h"
#include "aqua-sim-header.h"
#include "ns3/log.h"
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DolphinRoutingClustering");
NS_OBJECT_ENSURE_REGISTERED(DolphinRoutingClustering);

#define LS_DELAY 0

// Clustering Routing Protocol  分簇路由协议
DolphinRoutingClustering::DolphinRoutingClustering()
    :helloTimer(Timer::CANCEL_ON_DESTROY),
    routingTimer(Timer::CANCEL_ON_DESTROY),
    m_ability(SHORT),
    m_identity(CM),
    waitingWindow(5)
{
    m_rand = CreateObject<UniformRandomVariable> ();
}

DolphinRoutingClustering::~DolphinRoutingClustering()
{
}

TypeId
DolphinRoutingClustering::GetTypeId()
{
    static TypeId tid = TypeId ("ns3::DolphinRoutingClustering")
        .SetParent<AquaSimRouting>()
        .AddConstructor<DolphinRoutingClustering>()
        .AddAttribute("HelloInterval",
                    "HELLO packets emission interval.",
                    DoubleValue(30.0),
                    MakeDoubleAccessor(&DolphinRoutingClustering::m_helloInterval),
                    MakeDoubleChecker<double>())
        .AddAttribute("HelloDelay",
                    "first Maintainance HELLO packets emission delay.",
                    DoubleValue(0),
                    MakeDoubleAccessor(&DolphinRoutingClustering::m_helloDelay),
                    MakeDoubleChecker<double>())
        .AddAttribute("Omega1",
                    "parameter omega1 for weight calculation",
                    DoubleValue(0.4),
                    MakeDoubleAccessor(&DolphinRoutingClustering::omega1),
                    MakeDoubleChecker<double>())
        .AddAttribute("Omega2",
                    "parameter omega2 for weight calculation",
                    DoubleValue(0.2),
                    MakeDoubleAccessor(&DolphinRoutingClustering::omega2),
                    MakeDoubleChecker<double>())
        .AddAttribute("Omega3",
                    "parameter omega3 for weight calculation",
                    DoubleValue(0.4),
                    MakeDoubleAccessor(&DolphinRoutingClustering::omega3),
                    MakeDoubleChecker<double>())
        .AddAttribute("Gamma1",
                    "parameter gamma1 for long-node's CH selection",
                    DoubleValue(0.5),
                    MakeDoubleAccessor(&DolphinRoutingClustering::gamma1),
                    MakeDoubleChecker<double>())
        .AddAttribute("Gamma2",
                    "parameter gamma2 for long-node's CH selection",
                    DoubleValue(0.5),
                    MakeDoubleAccessor(&DolphinRoutingClustering::gamma2),
                    MakeDoubleChecker<double>())
        .AddAttribute("Delta1",
                    "parameter delta1 for short-node's CH selection",
                    DoubleValue(0.5),
                    MakeDoubleAccessor(&DolphinRoutingClustering::delta1),
                    MakeDoubleChecker<double>())
        .AddAttribute("Delta2",
                    "parameter delta2 for short-node's CH selection",
                    DoubleValue(0.5),
                    MakeDoubleAccessor(&DolphinRoutingClustering::delta2),
                    MakeDoubleChecker<double>())
        .AddAttribute("Alpha",
                    "parameter alpha for node link-hold-time weight",
                    DoubleValue(100),
                    MakeDoubleAccessor(&DolphinRoutingClustering::alpha),
                    MakeDoubleChecker<double>())
        .AddAttribute("Mu",
                    "parameter mu for long-node's cluster switching tolerance",
                    DoubleValue(0.2),
                    MakeDoubleAccessor(&DolphinRoutingClustering::mu),
                    MakeDoubleChecker<double>())
        .AddAttribute("Lambda",
                    "parameter lambda for short-node's cluster switching tolerance",
                    DoubleValue(0.2),
                    MakeDoubleAccessor(&DolphinRoutingClustering::lambda),
                    MakeDoubleChecker<double>())
        .AddAttribute("Prob",
                    "parameter prob for link-hold probability",
                    DoubleValue(0.8),
                    MakeDoubleAccessor(&DolphinRoutingClustering::prob),
                    MakeDoubleChecker<double>())
        .AddAttribute("MaxSpeed",
                    "parameter max-speed for weight calculating",
                    DoubleValue(5),
                    MakeDoubleAccessor(&DolphinRoutingClustering::maxSpeed),
                    MakeDoubleChecker<double>())
        .AddAttribute("N_nodes",
                    "parameter num of nodes for weight calculating",
                    IntegerValue(1),
                    MakeIntegerAccessor(&DolphinRoutingClustering::n_nodes),
                    MakeIntegerChecker<int>())
        .AddAttribute("FormingTime",
                    "the time when network initialization end",
                    DoubleValue(100.),
                    MakeDoubleAccessor(&DolphinRoutingClustering::formingTime),
                    MakeDoubleChecker<double>())
        .AddAttribute("RoutingTime",
                    "the time when cluster forming end",
                    DoubleValue(200.),
                    MakeDoubleAccessor(&DolphinRoutingClustering::routingTime),
                    MakeDoubleChecker<double>())
        .AddAttribute("MaintenanceTime",
                    "the time when initial routing end",
                    DoubleValue(500.),
                    MakeDoubleAccessor(&DolphinRoutingClustering::maintenanceTime),
                    MakeDoubleChecker<double>())
        .AddAttribute("Thres",
                    "the thres for LT",
                    DoubleValue(50.),
                    MakeDoubleAccessor(&DolphinRoutingClustering::thres),
                    MakeDoubleChecker<double>())
        .AddAttribute("Ability",
                    "the node's transmission ability",
                    EnumValue(SHORT),
                    MakeEnumAccessor(&DolphinRoutingClustering::m_ability),
                    MakeEnumChecker(SHORT, "SHORT", LONG, "LONG"))
        .AddAttribute("Identity",
                    "the node's identity",
                    EnumValue(CM),
                    MakeEnumAccessor(&DolphinRoutingClustering::m_identity),
                    MakeEnumChecker(CM, "CM", CH, "CH", SINK, "SINK"))
        .AddAttribute("NMembers",
                    "num of CH's members",
                    IntegerValue(0),
                    MakeIntegerAccessor(&DolphinRoutingClustering::n_members),
                    MakeIntegerChecker<int>())
        .AddAttribute("Range",
                    "the node's transmission range",
                    DoubleValue(1000.),
                    MakeDoubleAccessor(&DolphinRoutingClustering::m_range),
                    MakeDoubleChecker<double>())
        .AddTraceSource("Tx",
                    "Net:A new packet is sent",
                    MakeTraceSourceAccessor(&DolphinRoutingClustering::m_txTrace),
                    "DolphinRoutingClustering::TxCallback")
        .AddTraceSource("Rx",
                    "Net:A new packet is received",
                    MakeTraceSourceAccessor(&DolphinRoutingClustering::m_rxTrace),
                    "DolphinRoutingClustering::RxCallback")
        .AddTraceSource("Counter",
                    "Net:Evaluation Counter",
                    MakeTraceSourceAccessor(&DolphinRoutingClustering::m_counterTrace),
                    "DolphinRoutingClustering::CounterCallBack")
        .AddTraceSource("Delay",
                    "Net:End2End Delay Counter",
                    MakeTraceSourceAccessor(&DolphinRoutingClustering::m_delayTrace),
                    "DolphinRoutingClustering::CounterCallBack")
    ;
    return tid;
}

int64_t
DolphinRoutingClustering::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    return 0;
}

bool
DolphinRoutingClustering::Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this);
    AquaSimHeader ash;
    AquaSimAddress myAddr = AquaSimAddress::ConvertFrom(GetNetDevice()->GetAddress());
    packet->PeekHeader(ash);
    if (ash.GetNetDataType() != AquaSimHeader::DATA && ash.GetNetDataType() != AquaSimHeader::LOC)     // 控制包
    {
        switch (ash.GetNetDataType())
        {
        case AquaSimHeader::HELLO:
            ProcessHello(packet);
            break;
        case AquaSimHeader::ACK:
            ProcessAck(packet);
            break;
        case AquaSimHeader::CH:
            ProcessCHAnnouncement(packet);
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
                SendDown(packet, nxtAddr, Seconds(GetBackoff(0)));
                return true;
            }
            // 单播包
            // m_counterTrace(6, 0);
            // m_delayTrace(packet->GetUid(), 0, Simulator::Now().GetSeconds());
            // if (m_identity == CM)   // 转发给簇头
            // {
            //     if (m_ch == AquaSimAddress::GetBroadcast())
            //     {
            //         packet->AddHeader(ash);
            //         m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 5);
            //         NS_LOG_UNCOND("Can't find cluster head!  " << packet);
            //         packet = 0;
            //         m_counterTrace(6, 2);
            //         return false;
            //     }
            //     nxtAddr = m_ch;
            // }else                   // 根据路由表转发
            // {
            //     auto me = m_members.find(AquaSimAddress::ConvertFrom(dest));
            //     if (me != m_members.end())
            //     {
            //         nxtAddr = me->first;
            //     }else
            //     {
            //         auto re = m_RoutingTable.find(AquaSimAddress::ConvertFrom(dest));
            //         if (re == m_RoutingTable.end() || Simulator::Now() > re->second.exp)
            //         {
            //             if (ash.GetNextHop() != AquaSimAddress::GetBroadcast() &&
            //                 fa != AquaSimAddress::GetBroadcast() && 
            //                 (fa.GetAsInt() == 1 || m_chNeighbors.find(fa) != m_chNeighbors.end()))
            //                 nxtAddr = fa;
            //             else
            //             {
            //                 nxtAddr = AquaSimAddress::GetBroadcast();
            //                 ash.SetNetDataType(AquaSimHeader::LOC);
            //             }
            //         }else
            //         {
            //             nxtAddr = re->second.nxt;
            //         }
            //     }
            //     if (ash.GetNetDataType() == AquaSimHeader::LOC)
            //         m_forwarded_bc.insert(packet->GetUid());
            //     else
            //         m_forwarded.insert(packet->GetUid());
            // }
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
                SendDown(packet, AquaSimAddress::GetBroadcast(), Seconds(GetBackoff(0)));
                return true;
            }
            // 单播包
        //     if (dAddr == myAddr)        // 我是dst
        //     {
        //         m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
        //         m_counterTrace(6, 1);
        //         m_delayTrace(packet->GetUid(), 1, Simulator::Now().GetSeconds());
        //         SendUp(packet);
        //         return true;
        //     }
        //     if (IsDeadLoop(packet))
        //     {
        //         m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
        //         NS_LOG_UNCOND("Dropping pkt" << packet << " due to route loop");
        //         packet = 0;
        //         m_counterTrace(6, 2);
        //         return false;
        //     }        
        //     if (m_identity == CM)
        //     {
        //         m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 5);
        //         packet = 0;
        //         m_counterTrace(6, 2);
        //         return false;
        //     }
        //                                 // 根据路由表转发
        //     m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
        //     auto me = m_members.find(AquaSimAddress::ConvertFrom(dest));
        //     if (me != m_members.end())
        //     {
        //         nxtAddr = me->first;
        //     }else if (ash.GetNetDataType() == AquaSimHeader::DATA)
        //     {
        //         auto re = m_RoutingTable.find(dAddr);
        //         if (re == m_RoutingTable.end() || Simulator::Now() > re->second.exp)
        //         {
        //             if (ash.GetNextHop() != AquaSimAddress::GetBroadcast() &&
        //                 fa != AquaSimAddress::GetBroadcast() && 
        //                 (fa.GetAsInt() == 1 || m_chNeighbors.find(fa) != m_chNeighbors.end()))
        //                 nxtAddr = fa;
        //             else
        //             {
        //                 nxtAddr = AquaSimAddress::GetBroadcast();
        //                 ash.SetNetDataType(AquaSimHeader::LOC);
        //             }
        //         }else
        //         {
        //             nxtAddr = re->second.nxt;
        //         }
        //     }else
        //         nxtAddr = AquaSimAddress::GetBroadcast();
        //     packet->RemoveHeader(ash);
        //     ash.SetNumForwards(ash.GetNumForwards() + 1);
        }
        // ash.SetNextHop(nxtAddr);
        // packet->AddHeader(ash);
        // m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
        // m_counterTrace(7, ash.GetSize());
        // double delay = 0.75;
        // if (ash.GetNetDataType() == AquaSimHeader::LOC)
        //     SendDown(packet, nxtAddr, Seconds(m_rand->GetValue(0, delay)));
        // else
        //     SendDown(packet, nxtAddr, Seconds(0));
    }
    return true;
}

void
DolphinRoutingClustering::ProcessHello(Ptr<Packet> packet)
{
    if (m_identity == SINK)
        return;
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringHelloHeader dchh;
    packet->RemoveHeader(dchh);
    AquaSimAddress src = ash.GetSAddr();
    ClusteringNeighborTableEntry entry;
    AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    // 更新邻居表条目
    // if (ab == SHORT)
    // {
    //     auto it = m_shortNeighbors.find(src);
    //     if (it == m_shortNeighbors.end())
    //     {
    //         entry = ClusteringNeighborTableEntry();
    //     }else
    //     {
    //         entry = it->second;
    //     }
    //     entry.range = dchh.GetRange();
    // }else
    // {
        auto it = m_longNeighbors.find(src);
        if (it == m_longNeighbors.end())
        {
            entry = ClusteringNeighborTableEntry();
        }else
        {
            entry = it->second;
        }
        entry.range = m_range;
        entry.weight = dchh.GetWeight();
    // }
    entry.lastT = dchh.GetTimeStamp();
    entry.position = dchh.GetPosition();
    entry.direction = dchh.GetDirection();
    entry.speed = dchh.GetSpeed();
    // 计算邻居权值
    CalcNeighbor(entry);
    // if (ab == SHORT)
    //     m_shortNeighbors[src] = entry;
    // else
    // {
        m_longNeighbors[src] = entry;
        if (dchh.GetCHFlag() == 2)      // sink
        {
        }
        else if (m_phase == MAINTENANCE)     //  判断是否发生邻居簇头增减
        {
            auto it = m_chNeighbors.find(src);
            if (dchh.GetCHFlag() == 1)
            {
                if (it == m_chNeighbors.end() && m_identity == CH)
                {
                    // 邻居新增一个CH
                    if (m_members.find(src) != m_members.end())
                        DeleteMember(src);
                }
                m_chNeighbors[src] = m_longNeighbors[src];
            }else
            {
                if (it != m_chNeighbors.end())
                {
                    m_chNeighbors.erase(it);
                    auto iter = waitingNeighbors.find(src);
                    if (iter != waitingNeighbors.end() && m_phase == ROUTING)
                    {
                        waitingNeighbors.erase(iter);
                        if (NoWaitingNodes())
                            SendAck();
                    }
                }
            }
        }
    // }

    UpdateNeighbor(src, entry.lastT + Seconds(entry.linkT));
    if (m_phase == MAINTENANCE && m_identity == CM && dchh.GetCHFlag() == 1)     // 更新选择权值
        UpdateSelectValue();
    
    if (m_phase == INITIALIZATION && Simulator::Now().GetSeconds() > formingTime / 2
        && (weight < entry.weight || weight == entry.weight && mAddr.GetAsInt() > src.GetAsInt()))     // 初始分簇形成阶段，等待通告
        waitingNeighbors.emplace(src);
    
    packet = 0;
}

void
DolphinRoutingClustering::ProcessAck(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    AquaSimAddress src = ash.GetSAddr();
    ClusteringUpLinkHeader culh;
    packet->RemoveHeader(culh);
    uint16_t sz = culh.GetSize();
    double t = culh.GetLt();
    Time exp = Simulator::Now() + Seconds(t);
    auto addrs = culh.GetAddrs();
    //  update routing table with expire time
    for (auto i = addrs.begin(); i != addrs.end(); ++i)
    {
        m_RoutingTable[*i] = ClusteringRoutingTableEntry(src, exp);
        sons.insert(*i);
    }
    // check waiting neightbors
    if (m_identity == SINK)
    {
        if (exp < nxtRound)
        {
            nxtRound = exp;
            routingTimer.Cancel();
            routingTimer.Schedule(exp - Simulator::Now());
        }
    }else
    {
        if (exp < nxtRound)
        {
            nxtRound = exp;
        }
        auto iter = waitingNeighbors.find(src);
        if (iter != waitingNeighbors.end())
        {
            waitingNeighbors.erase(iter);
            if (NoWaitingNodes())
                SendAck();
        }
    }
    packet = 0;
}

void
DolphinRoutingClustering::ProcessCHAnnouncement(Ptr<Packet> packet)
{
    if (m_identity == SINK)
        return;
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringAnnouncementHeader dcah;
    packet->RemoveHeader(dcah);
    AquaSimAddress src = dcah.GetCHID();
    ClusteringNeighborTableEntry entry;
    // 更新邻居表条目
    auto it = m_longNeighbors.find(src);
    entry = it->second;
    m_chNeighbors[src] = entry;
    // 初始分簇形成阶段等待的邻居
    if (NoWaitingNodes())
    {
        packet = 0;
        return;
    }
    waitingNeighbors.erase(src);
    if (NoWaitingNodes())       // 初始化等待的邻居都等待完
    {
        if (!CheckNesessity())
            SelectCH();
        else
            SendCHAnnouncement();
    }
    packet = 0;
}

void
DolphinRoutingClustering::ProcessJoinAnnouncement(Ptr<Packet> packet)
{
    if (m_identity == SINK)
        return;
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringAnnouncementHeader dcah;
    packet->RemoveHeader(dcah);
    AquaSimAddress joinCH = dcah.GetCHID();
    AquaSimAddress m_addr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    uint8_t ab = dcah.GetNodeTyp();
    AquaSimAddress src = ash.GetSAddr();
    if (m_addr == joinCH)
    {
        // if (ab == SHORT)
        //     AddMember(src, m_shortNeighbors.find(src)->second);
        // else
            AddMember(src, m_longNeighbors.find(src)->second);
    }else
    {
        if (m_phase == FORMING)             
        {
            if (NoWaitingNodes())
            {
                packet = 0;
                return;
            }
            waitingNeighbors.erase(src);
            if (NoWaitingNodes())       // 初始化等待的邻居都等待完
            {
                if (!CheckNesessity())
                    SelectCH();
                else
                    SendCHAnnouncement();
            }
        }
        else if (m_identity == CH)
        {
            auto it = m_members.find(src);
            if (it != m_members.end())      // 是我之前的成员，将其删除
            {
                m_members.erase(it);
                n_members--;
            }
        }
    }
    auto iter = waitingNeighbors.find(src);
    if (m_phase == ROUTING && iter != waitingNeighbors.end())
    {
        waitingNeighbors.erase(iter);
        if (NoWaitingNodes())
            SendAck();
    }
    packet = 0;
}

void
DolphinRoutingClustering::ProcessLS(Ptr<Packet> packet)
{
    if (m_identity != CH)   // 仅簇头处理
    {
        m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 2);
        packet = 0;
        return;
    }
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    ClusteringDownLinkHeader cdlh;
    packet->PeekHeader(cdlh);
    AquaSimAddress src = ash.GetSAddr();
    uint16_t r = cdlh.GetRound();
    uint8_t h = ash.GetNumForwards();
    double t = cdlh.GetLt();
    auto it = m_longNeighbors.find(src);
    // get LT of this neighbor
    if (it != m_longNeighbors.end())
    {
        it->second.linkT -= (Simulator::Now() - it->second.lastT).ToDouble(Time::S);
        it->second.lastT = Simulator::Now();
        t = std::min(t, it->second.linkT);
        if (it->second.linkT < thres)
        {
            packet = 0;
            return;
        }
    }else
    {
        packet = 0;
        return;
    }
    if (r < m_rounds)
    {
        packet = 0;
        return;
    }
    double bkf = 2 * thres / t;
    if (r > m_rounds)       // new maintenance round
    {
        m_rounds = r;
        m_phase = MAINTENANCE;
        lt = t;
        m_depth = h;
        fa = src;
        routingTimer.Cancel();
        routingTimer.SetFunction(&DolphinRoutingClustering::SendLS, this);
        routingTimer.Schedule(Seconds(bkf));
    }else
    {
        if (m_phase == MAINTENANCE)     // havenot sent Down Link
        {
            if (t > lt || t == lt && h < m_depth)   // update fa link
            {
                lt = t;
                m_depth = h;
                fa = src;
                routingTimer.Cancel();
                routingTimer.Schedule(Seconds(bkf));
            }
        }
        else                            // have sent Down Link 
        {
            AquaSimAddress f = cdlh.GetFa();
            if (f == AquaSimAddress::ConvertFrom(m_device->GetAddress()))   // new waiting son
            {
                waitingNeighbors.insert(src);
                routingTimer.Cancel();      // cancel current ack event
                routingTimer.Schedule(Seconds(5 * waitingWindow));
            }
        }
    }
    packet = 0;
}

void
DolphinRoutingClustering::SendHello()
{
    UpdateMobility();
    for (auto it = m_longNeighbors.begin(); it != m_longNeighbors.end(); it++)
    {
        ClusteringNeighborTableEntry entry = it->second;
        CalcNeighbor(entry);
        m_longNeighbors[it->first] = entry;
        UpdateNeighbor(it->first, entry.lastT + Seconds(entry.linkT));
    }
    UpdateWeight();
    for (auto it = m_shortNeighbors.begin(); it != m_shortNeighbors.end(); it++)
    {
        ClusteringNeighborTableEntry entry = it->second;
        CalcNeighbor(entry);
        m_shortNeighbors[it->first] = entry;
        UpdateNeighbor(it->first, entry.lastT + Seconds(entry.linkT));
    }
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringHelloHeader dchh;
    // 配置hello头
    dchh.SetPosition(m_position);
    dchh.SetDirection(m_direction);
    dchh.SetSpeed(m_speed);
    dchh.SetTimeStamp(m_currentTime);
    dchh.SetWeight(weight);
    if (m_identity == CM)
        dchh.SetCHFlag(0);
    else
        dchh.SetCHFlag((m_identity == CH)? 1: 2);
    // dchh.SetNodeTyp(m_ability);
    dchh.SetRange(m_range);
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
    ash.SetSize(23);
    // 添加headers
    packet->AddHeader(dchh);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(1, ash.GetSize());
    SendDown(packet, asBroadcast, Seconds(GetBackoff(1)));
    // NS_LOG_UNCOND("Node " << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() 
    //                 << " send hello; weight = " << weight << "  dir=" << m_direction);
}

void
DolphinRoutingClustering::SendAck()
{
    routingTimer.Cancel();
    UpdateMobility();
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    ClusteringUpLinkHeader culh;
    // 配置ACK头
    culh.SetDst(fa);
    culh.SetLt((nxtRound - Simulator::Now()).ToDouble(Time::S));
    culh.SetLinks(sons.size() + m_members.size() + 1, GetASAddr());
    // 配置aqua-sim头;
    ash.SetDirection(AquaSimHeader::DOWN);
    ash.SetNextHop(fa);
    ash.SetSAddr(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    ash.SetDAddr(fa);
    ash.SetErrorFlag(false);
    ash.SetUId(packet->GetUid());
    ash.SetTimeStamp(Simulator::Now());
    ash.SetSize(9 + std::ceil(n_nodes / 8.));
    ash.SetNetDataType(AquaSimHeader::ACK);
    // 添加headers
    packet->AddHeader(culh);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(1, ash.GetSize());
    SendDown(packet, fa, Seconds(GetBackoff(0)));
}

void
DolphinRoutingClustering::SendCHAnnouncement()
{
    m_counterTrace(4, 0);
    m_identity = CH;
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringAnnouncementHeader dcah;
    // 配置announcement头
    dcah.SetCHID(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    // dcah.SetNodeTyp(m_ability);
    // 配置aqua-sim头
    ash.SetDirection(AquaSimHeader::DOWN);
    AquaSimAddress asBroadcast = AquaSimAddress::GetBroadcast();
    ash.SetNextHop(asBroadcast);
    ash.SetSAddr(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    ash.SetDAddr(asBroadcast);
    ash.SetErrorFlag(false);
    ash.SetUId(packet->GetUid());
    ash.SetTimeStamp(Simulator::Now());
    ash.SetNetDataType(AquaSimHeader::CH);
    ash.SetSize(3);
    // 添加headers
    packet->AddHeader(dcah);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    SendDown(packet, asBroadcast, Seconds(GetBackoff(1)));
    NS_LOG_UNCOND("Node " << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() 
                    << " send ch announcement");
}

void
DolphinRoutingClustering::SendJoinAnnouncement(AquaSimAddress head)
{
    if (head == AquaSimAddress::GetBroadcast())
        return;
    m_identity = CM;
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringAnnouncementHeader dcah;
    // 配置announcement头
    dcah.SetCHID(head);
    // dcah.SetNodeTyp(m_ability);
    // 配置aqua-sim头
    ash.SetDirection(AquaSimHeader::DOWN);
    AquaSimAddress asBroadcast = AquaSimAddress::GetBroadcast();
    ash.SetNextHop(asBroadcast);
    ash.SetSAddr(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    ash.SetDAddr(asBroadcast);
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
    SendDown(packet, asBroadcast, Seconds(GetBackoff(0)));
}

void
DolphinRoutingClustering::SendLS()
{
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    ClusteringDownLinkHeader cdlh;
    sons.clear();
    // 配置LS头
    if (m_identity == SINK)
    {
        cdlh.SetRound(++m_rounds);
        cdlh.SetLt(200.);
        ash.SetNumForwards(1);
        routingTimer.SetFunction(&DolphinRoutingClustering::SendLS, this);
        nxtRound = Simulator::Now() + Seconds(200);
        routingTimer.Schedule(Seconds(200));
    }else
    {
        cdlh.SetRound(m_rounds);
        ash.SetNumForwards(m_depth + 1);
        cdlh.SetLt(lt);
        cdlh.SetFa(fa);
        waitingNeighbors.clear();
        routingTimer.SetFunction(&DolphinRoutingClustering::SendAck, this);
        routingTimer.Schedule(Seconds(waitingWindow));
        nxtRound = Simulator::Now() + Seconds(lt);
        m_phase = ROUTING;
    }
    AquaSimAddress myAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    NS_LOG_UNCOND("fa of " << myAddr.GetAsInt() << " is " << fa.GetAsInt());
    // 配置aqua-sim头
    ash.SetDirection(AquaSimHeader::DOWN);
    AquaSimAddress asBroadcast = AquaSimAddress::GetBroadcast();
    ash.SetNextHop(asBroadcast);
    ash.SetSAddr(myAddr);
    ash.SetDAddr(asBroadcast);
    ash.SetErrorFlag(false);
    ash.SetUId(packet->GetUid());
    ash.SetTimeStamp(Simulator::Now());
    ash.SetNetDataType(AquaSimHeader::LS);
    ash.SetSize(11);
    // 添加headers
    packet->AddHeader(cdlh);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(2, ash.GetSize());
    SendDown(packet, asBroadcast, Seconds(0));
}

void
DolphinRoutingClustering::SetIdentity(NodeIdentity identity)
{
    m_identity = identity;
}

void
DolphinRoutingClustering::UpdateMobility()
{
    Ptr<Node> m_node = m_device->GetNode();
    Ptr<DolphinMobilityModel> mobility = m_node->GetObject<DolphinMobilityModel>();
    m_position = mobility->GetLocatedPosition();
    m_speed = mobility->GetIdealSpeed().GetLength();
    m_direction = mobility->GetDirection();
    m_currentTime = Simulator::Now();
}

double 
DolphinRoutingClustering::LHProbAllCases(double zeta, double eta, double theta, double t, double dt, double r)
{
    double f = zeta * t * t + eta * t + theta, d = sqrt(f), rr = 2 * maxSpeed * dt;
    if (dt < 1e-9 || d + rr <= r)                 // case 1
        return 1;
    if (d + r <= rr)                              // case 5
        return r * r * r / (rr * rr * rr);
    if (d < 1e-9)
        return 0;
    double alpha;
    double beta;
    if (f + rr * rr < r * r)   // case 2
    {
        alpha = (r * r - rr * rr + f) / (2 * d);
        beta = (r * r - rr * rr - f) / (2 * d);
        return ((r - alpha) * (r - alpha) * (2 * r + alpha) + 
                (rr + beta) * (rr + beta) * (2 * rr - beta)) / 
                (4 * rr * rr * rr);
    }
    if (f + r * r >= rr * rr)                    // case 3
    {
        alpha = (r * r - rr * rr + f) / (2 * d);
        beta = (rr * rr - r * r + f) / (2 * d);
        return ((r - alpha) * (r - alpha) * (2 * r + alpha) + 
                (rr - beta) * (rr - beta) * (2 * rr + beta)) /
                (4 * rr * rr * rr);
    }    
    if (d + r > rr)                 // case 4                                  
    {
        alpha = (rr * rr - r * r - f) / (2 * d);
        beta = (rr * rr - r * r + f) / (2 * d);
        return ((r + alpha) * (r + alpha) * (2 * r - alpha) + 
                (rr - beta) * (rr - beta) * (2 * rr + beta)) /
                (4 * rr * rr * rr);
    }
    return 0;
}

double
DolphinRoutingClustering::LHProbKnownCase(double zeta, double eta, double theta, double t, double dt, double r, int c)
{
    double f = zeta * t * t + eta * t + theta, d = sqrt(f), rr = 2 * maxSpeed * dt;
    if (d < 1e-9)
        return 0;
    double alpha, beta;
    switch (c)
    {
        case 1:
            return 1;
        case 2:
            alpha = (r * r - rr * rr + f) / (2 * d);
            beta = (r * r - rr * rr - f) / (2 * d);
            return ((r - alpha) * (r - alpha) * (2 * r + alpha) + 
                    (rr + beta) * (rr + beta) * (2 * rr - beta)) / 
                    (4 * rr * rr * rr);
        case 3:
            alpha = (r * r - rr * rr + f) / (2 * d);
            beta = (rr * rr - r * r + f) / (2 * d);
            return ((r - alpha) * (r - alpha) * (2 * r + alpha) + 
                    (rr - beta) * (rr - beta) * (2 * rr + beta)) /
                    (4 * rr * rr * rr);
        case 4:
            alpha = (rr * rr - r * r - f) / (2 * d);
            beta = (rr * rr - r * r + f) / (2 * d);
            return ((r + alpha) * (r + alpha) * (2 * r - alpha) + 
                    (rr - beta) * (rr - beta) * (2 * rr + beta)) /
                    (4 * rr * rr * rr);
        default:
            return r * r * r / (rr * rr * rr);
    }
}

void gauss_legendre_nodes_weights(int n, std::vector<double>& nodes, std::vector<double>& weights)
{
    nodes.resize(n);
    weights.resize(n);
    // 预定义的节点和权重（以 n=16 为例，实际需动态生成或查表）
    // 注：此处仅为示例，实际应用需使用库或算法生成（如 Boost.Math 或 GNU Scientific Library）
    const std::vector<double> x_n16 =
    {
        -0.989400934991650, -0.944575023073233, -0.865631202387832,
        -0.755404408355003, -0.617876244402644, -0.458016777657227,
        -0.281603550779259, -0.095012509837637, 0.095012509837637,
        0.281603550779259, 0.458016777657227, 0.617876244402644,
        0.755404408355003, 0.865631202387832, 0.944575023073233,
        0.989400934991650
    };
    const std::vector<double> w_n16 =
    {
        0.027152459411754, 0.062253523938648, 0.095158511682493,
        0.124628971255534, 0.149595988816577, 0.169156519395003,
        0.182603415044924, 0.189450610455069, 0.189450610455069,
        0.182603415044924, 0.169156519395003, 0.149595988816577,
        0.124628971255534, 0.095158511682493, 0.062253523938648,
        0.027152459411754
    };
    if (n == 16)
    {
        for (int i = 0; i < n; ++i)
        {
            nodes[i] = x_n16[i];
            weights[i] = w_n16[i];
        }
    }else
    {
        // 实际项目中应使用库函数生成（如下方说明）
        std::cerr << "Error: Only n=16 nodes/weights are pre-defined.\n";
        exit(1);
    }
}

// 高斯-勒让德积分计算
double
DolphinRoutingClustering::gauss_legendre_integrate(double f2, double f1, double f0, double t, double R, double a, double b, int n, int flag)
{
    if (b - a < 1e-9)
        return 0;
    std::vector<double> nodes, weights;
    gauss_legendre_nodes_weights(n, nodes, weights);

    double sum = 0.0, f;
    for (int i = 0; i < n; ++i)
    {
        // 将节点从 [-1,1] 映射到 [a,b]
        double x_mapped = 0.5 * (b - a) * nodes[i] + 0.5 * (a + b);
        switch (flag)
        {
        case 2:
            f = LHProbKnownCase(f2, f1, f0, x_mapped, t - x_mapped, R, 2) -
                LHProbKnownCase(f2, f1, f0, x_mapped, t - x_mapped, R, 3);
            break;
        case 3:
            f = LHProbKnownCase(f2, f1, f0, x_mapped, t - x_mapped, R, 3);
            break;
        case 4:
            f = LHProbKnownCase(f2, f1, f0, x_mapped, t - x_mapped, R, 4) - 
                LHProbKnownCase(f2, f1, f0, x_mapped, t - x_mapped, R, 3);
            break;
        default:
            f = LHProbKnownCase(f2, f1, f0, x_mapped, t - x_mapped, R, 5);
            break;
        }
        sum += weights[i] * f;
    }
    return 0.5 * (b - a) * sum; // 乘以区间缩放因子
}

double
DolphinRoutingClustering::LinkHoldProb(double f2, double f1, double f0, double t, double R)
{
#if 0
    // simple brute-force integrate
    double sum = 0;
    for (double i = t; i >= 0; i -= eps)
    {
        sum += LHProbAllCases(f2, f1, f0, i, t - i, R) * min(eps, i);
    }
    sum /= t;
    return sum;
#endif
    // gauss-legendre integrate
    double l, r, mid, t1, t5, g1a, g1b, g2a, g2b, v2 = maxSpeed * maxSpeed;
    if (std::sqrt(f0) + 2 * maxSpeed * t - R < 0)
    {
        t1 = 0;
    }else
    {
        for (l = 0, r = t, mid = r / 2; r - l >= 1e-6; mid = (l + r) / 2)
            if (std::sqrt(f2 * mid * mid + f1 * mid + f0) + 2 * maxSpeed * (t - mid) - R > 0)
                l = mid;
            else
                r = mid;
        t1 = mid;
    }
    if (std::sqrt(f0) + R - 2 * maxSpeed * t > 0)
        t5 = 0;
    else
    {
        for (l = 0, r = t, mid = r / 2; r - l >= 1e-6; mid = (l + r) / 2)
            if (std::sqrt(f2 * mid * mid + f1 * mid + f0) + R - 2 * maxSpeed * (t - mid) > 0)
                r = mid;
            else
                l = mid;
        t5 = mid;
    }
    double sum = gauss_legendre_integrate(f2, f1, f0, t, R, 0, t5, 16, 5) + (t - t1);
    if (t1 - t5 < 1e-9)
    {
        return sum / t;
    }

    g1a = (8 * v2 * t - f1 -
          std::sqrt((f1 - 8 * v2 * t) * (f1 - 8 * v2 * t) -
          4 * (f2 + 4 * v2) * (f0 + 4 * v2 * t * t - R * R))) /
          (2 * (f2 + 4 * v2));
    
    g1b = (8 * v2 * t - f1 +
          std::sqrt((f1 - 8 * v2 * t) * (f1 - 8 * v2 * t) -
          4 * (f2 + 4 * v2) * (f0 + 4 * v2 * t * t - R * R))) /
          (2 * (f2 + 4 * v2));
    g1a = std::max(t5, std::min(t1, g1a));
    g1b = std::max(g1a, std::min(t1, g1b));
    sum += gauss_legendre_integrate(f2, f1, f0, t, R, g1a, g1b, 16, 2);


    if (4 * v2 - f2 < 1e9)
    {
        if (fabs(f1 + 8 * v2 * t) < 1e-9)
        {
            if (f0 - 4 * v2 * t * t + R * R >= 0)
            {
                g2a = t5;
                g2b = t1;
            }else
            {
                g2a = g2b = t1;
            }
        }else
        {
            if (f1 + 8 * v2 * t > 0)
            {
                g2a = (4 * v2 * t * t - R * R - f0) / (f1 + 8 * v2 * t);
                g2b = t1;
            }else
            {
                g2a = t5;
                g2b = (4 * v2 * t * t - R * R - f0) / (f1 + 8 * v2 * t);
            }
        }
    }else
    {
        g2a = (- 8 * v2 * t - f1 -
            std::sqrt((f1 + 8 * v2 * t) * (f1 + 8 * v2 * t) -
            4 * (f2 - 4 * v2) * (f0 - 4 * v2 * t * t + R * R))) /
            (2 * (f2 - 4 * v2));
        
        g2b = (- 8 * v2 * t - f1 +
            std::sqrt((f1 + 8 * v2 * t) * (f1 + 8 * v2 * t) -
            4 * (f2 - 4 * v2) * (f0 - 4 * v2 * t * t + R * R))) /
            (2 * (f2 - 4 * v2));
    }
    g2a = std::max(t5, std::min(t1, g2a));
    g2b = std::max(g2a, std::min(t1, g2b));
    sum += gauss_legendre_integrate(f2, f1, f0, t, R, g1a, g1b, 16, 2) + 
            gauss_legendre_integrate(f2, f1, f0, t, R, t5, g2a, 16, 4) +
            gauss_legendre_integrate(f2, f1, f0, t, R, g2b, t1, 16, 4) +
            gauss_legendre_integrate(f2, f1, f0, t, R, t5, t1, 16, 3);
    return (sum / t) * (1 - std::exp(- 0.001 * t)) + std::exp(- 0.001 * t);
}

void
DolphinRoutingClustering::CalcNeighbor(ClusteringNeighborTableEntry &n)
{
    UpdateMobility();
    // 计算相对坐标和速度
    double timeDiff = (m_currentTime - n.lastT).ToDouble(Time::S);
    Vector m_pos = Vector(m_position.x - m_speed * m_direction.x * timeDiff,
                          m_position.y - m_speed * m_direction.y * timeDiff,
                          m_position.z - m_speed * m_direction.z * timeDiff);
    Vector relV = Vector(n.speed * n.direction.x - m_speed * m_direction.x,
                         n.speed * n.direction.y - m_speed * m_direction.y,
                         n.speed * n.direction.z - m_speed * m_direction.z);
    Vector relP = n.position - m_pos;
    // 计算运动不变下的距离函数和保持时间
    double f2 = relV.GetLengthSquared(), f1 = 2 * (relV.x * relP.x + relV.y * relP.y + relV.z * relP.z), f0 = relP.GetLengthSquared();
    double T = (!f2)? 500000.: (- f1 + std::sqrt(f1 * f1 - 4 * f2 * (f0 - n.range * n.range))) / (2 * f2);
    double linkT, distV;
    // 计算运动误差情况下
    // double l, r, mid;
    // for (l = 0, r = T, mid = r / 2; r - l >= 1e-2; mid = (l + r) / 2)
    // {
    //     if (LinkHoldProb(f2, f1, f0, mid, n.range) >= prob)
    //         l = mid;
    //     else
    //         r = mid;
    // }
    // linkT = mid;
    // distV = (- 4 / 15. * maxSpeed * maxSpeed - f2 / 9.) * (linkT * linkT) 
    //         + (- 0.25 * f1) * linkT 
    //         + n.range * n.range - f0;
    linkT = T * 0.9;
    distV = n.range * n.range - m_currentTime.GetSeconds() * m_currentTime.GetSeconds() * f2 - m_currentTime.GetSeconds() * f1 - f0;
    n.linkT = linkT;
    n.distV = (std::exp(-1)) / (std::exp(- distV / (n.range * n.range)));
}

void
DolphinRoutingClustering::UpdateNeighbor(AquaSimAddress addr, Time t)
{
    auto it = m_event.find(addr);
    if (it != m_event.end())
        it->second.Cancel();
    if (t <= Simulator::Now())
        m_event[addr] = Simulator::ScheduleNow(&DolphinRoutingClustering::NeighborExpire, this, addr);
    else
    {
        Time tt = std::min(t - Simulator::Now(), Seconds(m_helloInterval + 5));
        m_event[addr] = Simulator::Schedule(tt, &DolphinRoutingClustering::NeighborExpire, this, addr);
    }
}

void
DolphinRoutingClustering::NeighborExpire(AquaSimAddress addr)
{
    std::map<AquaSimAddress, ClusteringNeighborTableEntry>::iterator it;
    AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    // it = m_shortNeighbors.find(addr);
    // if (it != m_shortNeighbors.end())
    //     m_shortNeighbors.erase(it);
    
    bool flag = (m_identity == CH);
    it = m_longNeighbors.find(addr);
    if (it != m_longNeighbors.end())
        m_longNeighbors.erase(it);
    
    it = m_chNeighbors.find(addr);
    if (it != m_chNeighbors.end())
    {
        m_chNeighbors.erase(it);
        if (m_phase == MAINTENANCE && m_identity == CH && addr < mAddr) // 簇头邻居变化
        {
            flag = true;
        }
    }

    it = m_members.find(addr);
    if (it != m_members.end())  // 与成员断开连接
    {
        m_members.erase(it);
        flag = true;
        n_members--;
    }
    
    if (addr == m_ch)           // 与簇头断开连接
    {
        m_ch = AquaSimAddress::GetBroadcast();
        SelectCH();
    }

    auto i = waitingNeighbors.find(addr);
    if (i != waitingNeighbors.end())
    {
        waitingNeighbors.erase(i);
        if (NoWaitingNodes())
        {
            if (m_phase == FORMING)
            {
                if (!CheckNesessity())
                    SelectCH();
                else
                    SendCHAnnouncement();
            }
            else if (m_phase == ROUTING)
            {
                SendAck();
            }
        }
    }
}

void
DolphinRoutingClustering::UpdateWeight()
{
    int m_degree = 0;
    double linkTCnt = 0., distVCnt = 0.;
    std::map<AquaSimAddress, ClusteringNeighborTableEntry>::iterator cur;
    double curT = m_currentTime.ToDouble(Time::S);
    AquaSimAddress addr;
    // for (cur = m_shortNeighbors.begin(); cur != m_shortNeighbors.end(); cur++)
    // {
    //     m_degree++;
    //     cur->second.linkT -= curT - cur->second.lastT.ToDouble(Time::S);
    //     cur->second.lastT = m_currentTime;
    //     linkTCnt += cur->second.linkT;
    //     distVCnt += cur->second.distV;
    // }

    for (cur = m_longNeighbors.begin(); cur != m_longNeighbors.end(); cur++)
    {
        m_degree++;
        cur->second.linkT -= curT - cur->second.lastT.ToDouble(Time::S);
        cur->second.lastT = m_currentTime;
        linkTCnt += cur->second.linkT;
        distVCnt += cur->second.distV;
    }

    if (!m_degree)
        weight = 0;
    else
    {
        double linkTAvg = linkTCnt / m_degree,
                distVAvg = distVCnt / m_degree;
        double t1 = 1 - std::exp(- linkTAvg / alpha),
                t2 = m_degree / (n_nodes - 1),
                t3 = distVAvg;
        weight = omega1 * t1 + omega2 * t2 + omega3 * t3;
        // 保留3位小数
        weight = std::floor((weight * 1000 + 0.5)) / 1000.;
    }
}

void
DolphinRoutingClustering::AddMember(AquaSimAddress addr, ClusteringNeighborTableEntry n)
{
    m_members[addr] = n;
    n_members++;
}

void
DolphinRoutingClustering::DeleteMember(AquaSimAddress addr)
{
    auto m = m_members.find(addr);
    m_members.erase(m);
    n_members--;
}

bool
DolphinRoutingClustering::CheckCoverage(ClusteringNeighborTableEntry n, ClusteringNeighborTableEntry ch)
{
    double dn = (Simulator::Now() - n.lastT).GetSeconds();
    Vector posn = Vector(
        n.position.x + n.speed * n.direction.x * dn,
        n.position.y + n.speed * n.direction.y * dn,
        n.position.z + n.speed * n.direction.z * dn
    );
    double dc = (Simulator::Now() - ch.lastT).GetSeconds();
    Vector posch = Vector(
        ch.position.x + ch.speed * ch.direction.x * dc,
        ch.position.y + ch.speed * ch.direction.y * dc,
        ch.position.z + ch.speed * ch.direction.z * dc
    );

    return CalculateDistance(posn, posch) <= n.range;
}

bool
DolphinRoutingClustering::CheckNesessity()
{
    bool flag;
    return (m_chNeighbors.size() == 0);
    // 维护阶段，可假设其他成员节点都已经有簇头，只需保证簇头间的连通性
    // if (m_phase == MAINTENANCE)
    // {
    //     if (m_chNeighbors.size() == 0)
    //         return true;
    //     if (m_chNeighbors.size() > 1)
    //     {
    //         for (auto it = m_chNeighbors.begin(); it != m_chNeighbors.end(); it++)
    //         {
    //             flag = false;
    //             for (auto jt = m_chNeighbors.begin(); jt != m_chNeighbors.end(); jt++)
    //                 if (it != jt && CheckCoverage(it->second, jt->second))
    //                 {
    //                     flag = true;
    //                     break;
    //                 }
    //             if (!flag)
    //                 return true;
    //         }
    //     }
    //     if (m_identity == CH)
    //     {
    //         for (auto it = m_chNeighbors.begin(); it != m_chNeighbors.end(); it++)
    //             if (it->second.weight <= (1 + lambda) * weight)
    //                 return true;
    //         return false;
    //     }
    //     for (auto it = m_chNeighbors.begin(); it != m_chNeighbors.end(); it++)
    //         if (it->second.weight > (1 + lambda) * weight)
    //             return false;
    //     return true;
    // }
    // // 初始化阶段，检查远距离邻居能否被簇头覆盖
    // for (auto it = m_longNeighbors.begin(); it != m_longNeighbors.end(); it++)
    // {
    //     flag = false;
    //     for (auto jt = m_chNeighbors.begin(); jt != m_chNeighbors.end(); jt++)
    //         if (CheckCoverage(it->second, jt->second))
    //         {
    //             flag = true;
    //             break;
    //         }
    //     if (!flag)
    //         return true;
    // }
    // // 检查近距离邻居能否被簇头以及远距离邻居覆盖
    // for (auto it = m_shortNeighbors.begin(); it != m_shortNeighbors.end(); it++)
    // {
    //     flag = false;
    //     for (auto jt = m_chNeighbors.begin(); jt != m_chNeighbors.end(); jt++)
    //         if (CheckCoverage(it->second, jt->second))
    //         {
    //             flag = true;
    //             break;
    //         }
    //     if (!flag && m_phase == FORMING)
    //     {
    //         for (auto jt = m_longNeighbors.begin(); jt != m_longNeighbors.end(); jt++)
    //             if (jt->second.weight < weight &&
    //                CheckCoverage(it->second, jt->second))
    //             {
    //                 flag = true;
    //                 break;
    //             }
    //         if (!flag)
    //             return true;
    //     }
    // }
    // return false;
}

void
DolphinRoutingClustering::SelectCH()
{
    std::map<AquaSimAddress, ClusteringNeighborTableEntry>::iterator cur;
    AquaSimAddress addr;
    double curT = m_currentTime.ToDouble(Time::S);
    m_currentTime = Simulator::Now();
    double mxV = -10000., curV, lt, dv;
    for (cur = m_chNeighbors.begin(); cur != m_chNeighbors.end(); cur++)
    {
        addr = cur->first;
        cur->second.linkT -= curT - cur->second.lastT.ToDouble(Time::S);
        cur->second.lastT = m_currentTime;
        lt = cur->second.linkT;
        dv = cur->second.distV;
        // if (m_ability == LONG)
        //     curV = gamma1 * 1 / (1 + std::exp(- lt / alpha)) + gamma2 * dv;
        // else
            curV = delta1 * 1 / (1 + std::exp(- lt / alpha)) + delta2 * dv;
        if (mxV < curV)
        {
            mxV = curV;
            m_ch = addr;
        }
    }
    if (m_ch != AquaSimAddress::GetBroadcast())
    {
        SendJoinAnnouncement(m_ch);
        NS_LOG_UNCOND("Node " << m_device->GetNode()->GetId()+1
                    << " select CH " << AquaSimAddress::ConvertFrom(m_ch).GetAsInt());
    }else
    {
        m_identity = CH;
        m_ch = AquaSimAddress::GetBroadcast();
        // 新簇头产生
        m_counterTrace(4, 0);
        NS_LOG_UNCOND("Node " << m_device->GetNode()->GetId() + 1 << " become CH");
        m_members.clear();
        n_members = 0;
    }
}

void
DolphinRoutingClustering::UpdateSelectValue()
{
    std::map<AquaSimAddress, ClusteringNeighborTableEntry>::iterator cur;
    AquaSimAddress addr, mxAddr;
    double curT = m_currentTime.ToDouble(Time::S);
    bool chFlag = false;
    double chV = -10000., mxV = -10000., curV, lt, dv;
    for (cur = m_chNeighbors.begin(); cur != m_chNeighbors.end(); cur++)
    {
        addr = cur->first;
        cur->second.linkT -= curT - cur->second.lastT.ToDouble(Time::S);
        cur->second.lastT = m_currentTime;
        lt = cur->second.linkT;
        dv = cur->second.distV;
        // if (m_ability == LONG)
        //     curV = gamma1 * 1 / (1 + std::exp(- lt / alpha)) + gamma2 * dv;
        // else
            curV = delta1 * 1 / (1 + std::exp(- lt / alpha)) + delta2 * dv;
        if (addr == m_ch)
        {
            chFlag = true;
            chV = curV;
        }else if (mxV < curV)
        {
            mxV = curV;
            mxAddr = addr;
        }
    }
    if (chFlag)     // 主动切换分簇
    {
        if (chV * (1 + mu) <= mxV)
            chFlag = false;
    }
    if (!chFlag && mxV >= -100)    // 切换分簇
    {
        m_ch = mxAddr;
        SendJoinAnnouncement(m_ch);
    }
}

std::vector<AquaSimAddress>
DolphinRoutingClustering::GetASAddr()
{
    std::vector<AquaSimAddress> addrList;
    for (auto iter = sons.begin(); iter != sons.end(); iter++)
    {
        addrList.emplace_back(*iter);
    }
    for (auto iter = m_members.begin(); iter != m_members.end(); iter++)
    {
        addrList.emplace_back(iter->first);
    }
    addrList.emplace_back(AquaSimAddress::ConvertFrom(GetNetDevice()->GetAddress()));
    return addrList;
}

void
DolphinRoutingClustering::HelloTimerExpire()
{
    // 检查是否满足簇头增减条件
    if (m_phase == MAINTENANCE && m_identity != SINK)
    {
        if (m_identity == CH)
        {
            if (m_members.size() == 0 && CheckNesessity() == false)
            {
                m_identity = CM;
                m_RoutingTable.clear();
                m_counterTrace(5, 0);
                NS_LOG_UNCOND("Node " << m_device->GetNode()->GetId() + 1 << " become CM");
                SelectCH();
            }
        }else
        {
            if ((m_ch == AquaSimAddress::GetBroadcast() && CheckNesessity()) || weight > m_chNeighbors[m_ch].weight * (1 + lambda))
            {
                m_identity = CH;
                m_ch = AquaSimAddress::GetBroadcast();
                // 新簇头产生
                m_counterTrace(4, 0);
                NS_LOG_UNCOND("Node " << m_device->GetNode()->GetId() + 1 << " become CH");
                m_members.clear();
                n_members = 0;
            }
        }
    }
    // 发送定期hello
    SendHello();
    helloTimer.Schedule(Seconds(m_helloInterval));
}

void
DolphinRoutingClustering::SwitchPhase(RoutingPhase p)
{
    m_phase = p;
    if (p == FORMING && waitingNeighbors.empty())
        SendCHAnnouncement();
    else if (p == MAINTENANCE)  // 长节点定期发送hello
    {
        helloTimer.SetFunction(&DolphinRoutingClustering::HelloTimerExpire, this);
        helloTimer.Schedule(Seconds(m_helloDelay));
    }
}

double
DolphinRoutingClustering::GetBackoff(uint8_t pktType)
{
    // return ((m_rand->GetValue() <= 0.5)? 0: 2);
    if (m_phase == MAINTENANCE || pktType == 0)
        return 0;
    return m_rand->GetValue() * 2;
}

void
DolphinRoutingClustering::DoInitialize()
{
    weight = 0;
    m_rounds = 0;
    fa = AquaSimAddress::GetBroadcast();
    m_currentTime = Seconds(0.);
    // m_shortNeighbors.clear();
    m_longNeighbors.clear();
    m_chNeighbors.clear();
    m_members.clear();
    m_event.clear();
    m_RoutingTable.clear();
    waitingNeighbors.clear();
    m_forwarded_bc.clear();
    sons.clear();
    m_forwarded.clear();
    Simulator::Schedule(Seconds(0.5), &DolphinRoutingClustering::SendHello, this);
    if (m_identity != SINK)
    {
        Simulator::Schedule(Seconds(formingTime / 2), &DolphinRoutingClustering::SendHello, this);
        
        Simulator::Schedule(Seconds(formingTime), &DolphinRoutingClustering::SwitchPhase, this, FORMING);
    
        // Simulator::Schedule(Seconds(routingTime), &DolphinRoutingClustering::SwitchPhase, this, ROUTING);
    }else
    {
        // routingTimer.SetFunction(&DolphinRoutingClustering::SendLS, this);
        // routingTimer.Schedule(Seconds(routingTime));
    }
    Simulator::Schedule(Seconds(maintenanceTime), &DolphinRoutingClustering::SwitchPhase, this, MAINTENANCE);

    m_phase = INITIALIZATION;
    m_ch = AquaSimAddress::GetBroadcast();
    AquaSimRouting::DoInitialize();
}

void
DolphinRoutingClustering::DoDispose()
{
    // m_shortNeighbors.clear();
    m_longNeighbors.clear();
    m_chNeighbors.clear();
    m_members.clear();
    m_RoutingTable.clear();
    m_event.clear();
    waitingNeighbors.clear();
    m_forwarded.clear();
    m_forwarded_bc.clear();
    sons.clear();
    m_rand = 0;
    n_members = 0;
    AquaSimRouting::DoDispose();
}