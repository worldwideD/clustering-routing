#include "routing-AEWCA.h"
#include "dolphin-clustering-header.h"
#include "aqua-sim-address.h"
#include "aqua-sim-header-routing.h"
#include "aqua-sim-header.h"
#include "ns3/log.h"
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoutingAEWCA");
NS_OBJECT_ENSURE_REGISTERED(RoutingAEWCA);

#define LS_DELAY 0

// Clustering Routing Protocol  分簇路由协议
RoutingAEWCA::RoutingAEWCA()
    :helloTimer(Timer::CANCEL_ON_DESTROY),
    m_ability(SHORT),
    m_identity(CM)
{
    m_rand = CreateObject<UniformRandomVariable> ();
}

RoutingAEWCA::~RoutingAEWCA()
{
}

TypeId
RoutingAEWCA::GetTypeId()
{
    static TypeId tid = TypeId ("ns3::RoutingAEWCA")
        .SetParent<AquaSimRouting>()
        .AddConstructor<RoutingAEWCA>()
        .AddAttribute("HelloInterval",
                    "HELLO packets emission interval.",
                    DoubleValue(30.0),
                    MakeDoubleAccessor(&RoutingAEWCA::m_helloInterval),
                    MakeDoubleChecker<double>())
        .AddAttribute("HelloDelay",
                    "first Maintainance HELLO packets emission delay.",
                    DoubleValue(0),
                    MakeDoubleAccessor(&RoutingAEWCA::m_helloDelay),
                    MakeDoubleChecker<double>())
        .AddAttribute("Omega1",
                    "parameter omega1 for weight calculation",
                    DoubleValue(0.4),
                    MakeDoubleAccessor(&RoutingAEWCA::omega1),
                    MakeDoubleChecker<double>())
        .AddAttribute("Omega2",
                    "parameter omega2 for weight calculation",
                    DoubleValue(0.2),
                    MakeDoubleAccessor(&RoutingAEWCA::omega2),
                    MakeDoubleChecker<double>())
        .AddAttribute("Omega3",
                    "parameter omega3 for weight calculation",
                    DoubleValue(0.4),
                    MakeDoubleAccessor(&RoutingAEWCA::omega3),
                    MakeDoubleChecker<double>())
        .AddAttribute("MaxSpeed",
                    "parameter max-speed for weight calculating",
                    DoubleValue(5),
                    MakeDoubleAccessor(&RoutingAEWCA::maxSpeed),
                    MakeDoubleChecker<double>())
        .AddAttribute("N_nodes",
                    "parameter num of nodes for weight calculating",
                    IntegerValue(1),
                    MakeIntegerAccessor(&RoutingAEWCA::n_nodes),
                    MakeIntegerChecker<int>())
        .AddAttribute("FormingTime",
                    "the time when network initialization end",
                    DoubleValue(100.),
                    MakeDoubleAccessor(&RoutingAEWCA::formingTime),
                    MakeDoubleChecker<double>())
        .AddAttribute("RoutingTime",
                    "the time when cluster forming end",
                    DoubleValue(200.),
                    MakeDoubleAccessor(&RoutingAEWCA::routingTime),
                    MakeDoubleChecker<double>())
        .AddAttribute("MaintenanceTime",
                    "the time when initial routing end",
                    DoubleValue(500.),
                    MakeDoubleAccessor(&RoutingAEWCA::maintenanceTime),
                    MakeDoubleChecker<double>())
        .AddAttribute("Ability",
                    "the node's transmission ability",
                    EnumValue(SHORT),
                    MakeEnumAccessor(&RoutingAEWCA::m_ability),
                    MakeEnumChecker(SHORT, "SHORT", LONG, "LONG"))
        .AddAttribute("Identity",
                    "the node's identity",
                    EnumValue(CM),
                    MakeEnumAccessor(&RoutingAEWCA::m_identity),
                    MakeEnumChecker(CM, "CM", CH, "CH", SINK, "SINK"))
        .AddAttribute("NMembers",
                    "num of CH's members",
                    IntegerValue(0),
                    MakeIntegerAccessor(&RoutingAEWCA::n_members),
                    MakeIntegerChecker<int>())
        .AddAttribute("Range",
                    "the node's transmission range",
                    DoubleValue(1000.),
                    MakeDoubleAccessor(&RoutingAEWCA::m_range),
                    MakeDoubleChecker<double>())
        .AddTraceSource("Tx",
                    "Net:A new packet is sent",
                    MakeTraceSourceAccessor(&RoutingAEWCA::m_txTrace),
                    "RoutingAEWCA::TxCallback")
        .AddTraceSource("Rx",
                    "Net:A new packet is received",
                    MakeTraceSourceAccessor(&RoutingAEWCA::m_rxTrace),
                    "RoutingAEWCA::RxCallback")
        .AddTraceSource("Counter",
                    "Net:Evaluation Counter",
                    MakeTraceSourceAccessor(&RoutingAEWCA::m_counterTrace),
                    "RoutingAEWCA::CounterCallBack")
        .AddTraceSource("Delay",
                    "Net:End2End Delay Counter",
                    MakeTraceSourceAccessor(&RoutingAEWCA::m_delayTrace),
                    "RoutingAEWCA::CounterCallBack")
    ;
    return tid;
}

int64_t
RoutingAEWCA::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    return 0;
}

bool
RoutingAEWCA::Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
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
        case AquaSimHeader::CH:
            ProcessCHAnnouncement(packet);
            break;
        case AquaSimHeader::JOIN:
            ProcessJoinAnnouncement(packet);
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
RoutingAEWCA::ProcessHello(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringHelloHeader dchh;
    packet->RemoveHeader(dchh);
    AquaSimAddress src = ash.GetSAddr();
    AEWCANeighborTableEntry entry;
    AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    // 更新邻居表条目
    // if (ab == SHORT)
    // {
    //     auto it = m_shortNeighbors.find(src);
    //     if (it == m_shortNeighbors.end())
    //     {
    //         entry = AEWCANeighborTableEntry();
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
            entry = AEWCANeighborTableEntry();
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
                    }
                }
            }
        }
    // }

    UpdateNeighbor(src, entry.lastT + Seconds(entry.linkT));
    
    if (m_phase == INITIALIZATION && Simulator::Now().GetSeconds() > formingTime / 2)
    {
        if ((weight > entry.weight || weight == entry.weight && mAddr.GetAsInt() > src.GetAsInt()))     // 初始分簇形成阶段，等待通告
        waitingNeighbors.emplace(src);
    }
    
    
    packet = 0;
}

void
RoutingAEWCA::ProcessCHAnnouncement(Ptr<Packet> packet)
{
    if (m_identity == SINK)
        return;
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringAnnouncementHeader dcah;
    packet->RemoveHeader(dcah);
    AquaSimAddress src = dcah.GetCHID();
    AEWCANeighborTableEntry entry;
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
RoutingAEWCA::ProcessJoinAnnouncement(Ptr<Packet> packet)
{
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
    packet = 0;
}

void
RoutingAEWCA::SendHello()
{
    UpdateMobility();
    for (auto it = m_longNeighbors.begin(); it != m_longNeighbors.end(); it++)
    {
        AEWCANeighborTableEntry entry = it->second;
        CalcNeighbor(entry);
        m_longNeighbors[it->first] = entry;
    }
    UpdateWeight();
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
}

void
RoutingAEWCA::SendCHAnnouncement()
{
    m_counterTrace(4, 0);
    m_identity = CH;
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringAnnouncementHeader dcah;
    // 配置announcement头
    dcah.SetCHID(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
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
RoutingAEWCA::SendJoinAnnouncement(AquaSimAddress head)
{
    if (head == AquaSimAddress::GetBroadcast())
        return;
    m_identity = CM;
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringAnnouncementHeader dcah;
    // 配置announcement头
    dcah.SetCHID(head);
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
RoutingAEWCA::SetIdentity(NodeIdentity identity)
{
    m_identity = identity;
}

void
RoutingAEWCA::UpdateMobility()
{
    Ptr<Node> m_node = m_device->GetNode();
    Ptr<DolphinMobilityModel> mobility = m_node->GetObject<DolphinMobilityModel>();
    m_position = mobility->GetLocatedPosition();
    m_speed = mobility->GetIdealSpeed().GetLength();
    m_direction = mobility->GetDirection();
    m_currentTime = Simulator::Now();
}

void
RoutingAEWCA::CalcNeighbor(AEWCANeighborTableEntry &n)
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
    linkT = 1 - std::exp(-0.01 * T);
    distV = std::sqrt(m_currentTime.GetSeconds() * m_currentTime.GetSeconds() * f2 + m_currentTime.GetSeconds() * f1 + f0);
    n.linkT = linkT;
    n.distV = distV;
}

void
RoutingAEWCA::UpdateNeighbor(AquaSimAddress addr, Time t)
{
    auto it = m_event.find(addr);
    if (it != m_event.end())
        it->second.Cancel();
    Time tt = Seconds(m_helloInterval + 5);
    m_event[addr] = Simulator::Schedule(tt, &RoutingAEWCA::NeighborExpire, this, addr);
}

void
RoutingAEWCA::NeighborExpire(AquaSimAddress addr)
{
    std::map<AquaSimAddress, AEWCANeighborTableEntry>::iterator it;
    AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());

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
        }
    }
}

void
RoutingAEWCA::UpdateWeight()
{
    int m_degree = 0;
    double linkTCnt = 0., distVCnt = 0.;
    std::map<AquaSimAddress, AEWCANeighborTableEntry>::iterator cur;
    double curT = m_currentTime.ToDouble(Time::S);
    AquaSimAddress addr;

    for (cur = m_longNeighbors.begin(); cur != m_longNeighbors.end(); cur++)
    {
        m_degree++;
        cur->second.linkT -= curT - cur->second.lastT.ToDouble(Time::S);
        cur->second.lastT = m_currentTime;
        linkTCnt += cur->second.linkT;
        distVCnt += cur->second.distV;
    }

    double avgd = n_nodes * 0.1;
    if (!m_degree)
        weight = omega2 * std::exp(-std::fabs(avgd));
    else
    {
        double linkTAvg = linkTCnt / m_degree,
                distVAvg = distVCnt / m_degree;
        double t1 = (linkTAvg < 1e-5)? 1 / linkTAvg: 1e5,
                t2 = std::exp(-std::fabs(avgd - m_degree)),
                t3 = distVAvg;
        weight = omega1 * t1 + omega2 * t2 + omega3 * t3;
    }
}

void
RoutingAEWCA::AddMember(AquaSimAddress addr, AEWCANeighborTableEntry n)
{
    m_members[addr] = n;
    n_members++;
}

void
RoutingAEWCA::DeleteMember(AquaSimAddress addr)
{
    auto m = m_members.find(addr);
    m_members.erase(m);
    n_members--;
}

bool
RoutingAEWCA::CheckNesessity()
{
    bool flag;
    return (m_chNeighbors.size() == 0);
}

void
RoutingAEWCA::SelectCH()
{
    std::map<AquaSimAddress, AEWCANeighborTableEntry>::iterator cur;
    AquaSimAddress addr;
    double curT = m_currentTime.ToDouble(Time::S);
    m_currentTime = Simulator::Now();
    double mV = 10000., curV;
    for (cur = m_chNeighbors.begin(); cur != m_chNeighbors.end(); cur++)
    {
        addr = cur->first;
        curV = cur->second.weight;
        if (mV > curV)
        {
            mV = curV;
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
RoutingAEWCA::HelloTimerExpire()
{
    // 检查是否满足簇头增减条件
    if (m_phase == MAINTENANCE && m_identity != SINK)
    {
        if (m_identity == CH)
        {
            if (m_members.size() == 0 && CheckNesessity() == false)
            {
                m_identity = CM;
                m_counterTrace(5, 0);
                NS_LOG_UNCOND("Node " << m_device->GetNode()->GetId() + 1 << " become CM");
                SelectCH();
            }
        }else
        {
            if ((m_ch == AquaSimAddress::GetBroadcast() && CheckNesessity()) || weight < m_chNeighbors[m_ch].weight)
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
RoutingAEWCA::SwitchPhase(RoutingPhase p)
{
    m_phase = p;
    if (p == FORMING && waitingNeighbors.empty())
        SendCHAnnouncement();
    else if (p == MAINTENANCE)  // 长节点定期发送hello
    {
        helloTimer.SetFunction(&RoutingAEWCA::HelloTimerExpire, this);
        helloTimer.Schedule(Seconds(m_helloDelay));
    }
}

double
RoutingAEWCA::GetBackoff(uint8_t pktType)
{
    // return ((m_rand->GetValue() <= 0.5)? 0: 2);
    return 0;
}

void
RoutingAEWCA::DoInitialize()
{
    weight = 0;
    m_currentTime = Seconds(0.);
    m_longNeighbors.clear();
    m_chNeighbors.clear();
    m_members.clear();
    m_event.clear();
    waitingNeighbors.clear();
    m_forwarded.clear();
    Simulator::Schedule(Seconds(0.5), &RoutingAEWCA::SendHello, this);
    if (m_identity != SINK)
    {
        Simulator::Schedule(Seconds(formingTime / 2), &RoutingAEWCA::SendHello, this);
        Simulator::Schedule(Seconds(formingTime), &RoutingAEWCA::SwitchPhase, this, FORMING);
    }
    Simulator::Schedule(Seconds(maintenanceTime), &RoutingAEWCA::SwitchPhase, this, MAINTENANCE);

    m_phase = INITIALIZATION;
    m_ch = AquaSimAddress::GetBroadcast();
    AquaSimRouting::DoInitialize();
}

void
RoutingAEWCA::DoDispose()
{
    m_longNeighbors.clear();
    m_chNeighbors.clear();
    m_members.clear();
    m_event.clear();
    waitingNeighbors.clear();
    m_forwarded.clear();
    m_rand = 0;
    n_members = 0;
    AquaSimRouting::DoDispose();
}