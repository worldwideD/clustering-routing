#include "routing-ERNC.h"
#include "dolphin-clustering-header.h"
#include "aqua-sim-address.h"
#include "aqua-sim-header-routing.h"
#include "aqua-sim-header.h"
#include "ns3/log.h"
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoutingERNC");
NS_OBJECT_ENSURE_REGISTERED(RoutingERNC);

// ERNC Protocol ERNC分簇路由协议
RoutingERNC::RoutingERNC():
    m_ability(SHORT),
    m_identity(CM)
{
    m_rand = CreateObject<UniformRandomVariable> ();
}

RoutingERNC::~RoutingERNC()
{
}

TypeId
RoutingERNC::GetTypeId()
{
    static TypeId tid = TypeId ("ns3::RoutingERNC")
        .SetParent<AquaSimRouting>()
        .AddConstructor<RoutingERNC>()
        .AddAttribute("Interval",
                    "cluster reforming interval.",
                    TimeValue(Seconds(100)),
                    MakeTimeAccessor(&RoutingERNC::m_interval),
                    MakeTimeChecker())
        .AddAttribute("Ability",
                    "the node's transmission ability",
                    EnumValue(SHORT),
                    MakeEnumAccessor(&RoutingERNC::m_ability),
                    MakeEnumChecker(SHORT, "SHORT", LONG, "LONG"))
        .AddAttribute("Range",
                    "the node's transmission range",
                    DoubleValue(1000.),
                    MakeDoubleAccessor(&RoutingERNC::m_range),
                    MakeDoubleChecker<double>())
        .AddAttribute("Identity",
                    "the node's identity",
                    EnumValue(CM),
                    MakeEnumAccessor(&RoutingERNC::m_identity),
                    MakeEnumChecker(CM, "CM", CH, "CH", SINK, "SINK"))
        .AddAttribute("MinX",
                    "min X Boundary.",
                    DoubleValue(0),
                    MakeDoubleAccessor(&RoutingERNC::minX),
                    MakeDoubleChecker<double>())
        .AddAttribute("MaxX",
                    "max X Boundary.",
                    DoubleValue(1000),
                    MakeDoubleAccessor(&RoutingERNC::maxX),
                    MakeDoubleChecker<double>())
        .AddAttribute("MinY",
                    "min Y Boundary.",
                    DoubleValue(0),
                    MakeDoubleAccessor(&RoutingERNC::minY),
                    MakeDoubleChecker<double>())
        .AddAttribute("MaxY",
                    "max Y Boundary.",
                    DoubleValue(1000),
                    MakeDoubleAccessor(&RoutingERNC::maxY),
                    MakeDoubleChecker<double>())
        .AddAttribute("MinZ",
                    "min Z Boundary.",
                    DoubleValue(0),
                    MakeDoubleAccessor(&RoutingERNC::minZ),
                    MakeDoubleChecker<double>())
        .AddAttribute("MaxZ",
                    "max Z Boundary.",
                    DoubleValue(1000),
                    MakeDoubleAccessor(&RoutingERNC::maxZ),
                    MakeDoubleChecker<double>())
        .AddAttribute("NMembers",
                    "num of CH's members",
                    IntegerValue(0),
                    MakeIntegerAccessor(&RoutingERNC::n_members),
                    MakeIntegerChecker<int>())
        .AddTraceSource("Tx",
                    "Net:A new packet is sent",
                    MakeTraceSourceAccessor(&RoutingERNC::m_txTrace),
                    "RoutingERNC::TxCallback")
        .AddTraceSource("Rx",
                    "Net:A new packet is received",
                    MakeTraceSourceAccessor(&RoutingERNC::m_rxTrace),
                    "RoutingERNC::RxCallback")
        .AddTraceSource("Counter",
                    "Net:Evaluation Counter",
                    MakeTraceSourceAccessor(&RoutingERNC::m_counterTrace),
                    "RoutingERNC::CounterCallBack")
        .AddTraceSource("Delay",
                    "Net:End2End Delay Counter",
                    MakeTraceSourceAccessor(&RoutingERNC::m_delayTrace),
                    "RoutingERNC::CounterCallBack")
    ;
    return tid;
}

int64_t
RoutingERNC::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    return 0;
}

bool
RoutingERNC::Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
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
        default:
            NS_FATAL_ERROR("Receiving Unknown Clustering Control Packet Type:" << ash.GetNetDataType());
            break;
        }
    }else
    {
        AquaSimAddress nxtAddr;
        AquaSimAddress dstAddr = AquaSimAddress::ConvertFrom(dest);
        if (AmISrc(packet))               // new packet
        {
            packet->RemoveHeader(ash);
            ash.SetDirection(AquaSimHeader::DOWN);
            ash.SetNumForwards(1);
            ash.SetDAddr(dstAddr);
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
            // m_forwarded.insert(packet->GetUid());
            // m_counterTrace(6, 0);
            // m_delayTrace(packet->GetUid(), 0, Simulator::Now().GetSeconds());
            // if (m_table.find(dstAddr) == m_table.end())
            // {
            //     NS_LOG_UNCOND("Can't find routing table entry!  " << packet);
            //     packet = 0;
            //     return false;
            // }
            // nxtAddr = m_table[dstAddr].nxt;
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
                                        // 根据路由表转发
            // if (m_table.find(dstAddr) == m_table.end())
            // {
            //     m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 5);
            //     NS_LOG_UNCOND("Can't find routing table entry!  " << packet);
            //     packet = 0;
            //     return false;
            // }
            // m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
            // packet->RemoveHeader(ash);
            // nxtAddr = m_table[dstAddr].nxt;
            // ash.SetNumForwards(ash.GetNumForwards() + 1);
        }
        ash.SetNextHop(nxtAddr);
        packet->AddHeader(ash);
        m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
        m_counterTrace(7, ash.GetSize());
        SendDown(packet, nxtAddr, Seconds(0));
    }
    return true;
}

void
RoutingERNC::ProcessHello(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    if (m_identity == SINK)
    {
        packet = 0;
        return;
    }
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringHelloHeader dchh;
    packet->RemoveHeader(dchh);
    Vector pos = dchh.GetPosition();
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    double distance = CalculateDistance(pos, m_pos);
    uint16_t pid = ash.GetUId();
    int h = ash.GetNumForwards();

    if (pid > m_round)
    {
        m_phase = INITIALIZATION;
        m_Timer.Cancel();
        m_Timer.SetFunction(&RoutingERNC::SendAck, this);
        m_Timer.Schedule(Seconds(2));
        // Simulator::Schedule(Seconds(2), &RoutingERNC::SendAck, this);
        Simulator::ScheduleNow(&RoutingERNC::SendHello, this);
        m_hops = h;
        m_dis = distance;
        m_degree = 1;
        m_ctc = 0.;
        m_cdis = m_range + 1;
        m_round = pid;
    }
    else if (pid == m_round && m_phase == INITIALIZATION)
    {
        m_dis = std::max(m_dis, distance);
        m_degree++;
    }

    packet->AddHeader(ash);
    packet = 0;
}

void
RoutingERNC::ProcessAck(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    if (m_phase == ROUTING)
    {
        packet = 0;
        return;
    }
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringHelloHeader dchh;
    packet->RemoveHeader(dchh);

    Vector pos = dchh.GetPosition();
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    double distance = CalculateDistance(pos, m_pos);
    double tc = dchh.GetWeight();
    double rr = dchh.GetRange();

    if (distance > rr)
    {
        packet = 0;
        return;
    }
    m_ctc = std::max(m_ctc, tc);
    packet->AddHeader(ash);
    packet = 0;
}

void
RoutingERNC::ProcessCHAnnouncement(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    if (m_phase != FORMING)
    {
        packet = 0;
        return;
    }
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringHelloHeader dchh;
    packet->RemoveHeader(dchh);

    Vector pos = dchh.GetPosition();
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    double distance = CalculateDistance(pos, m_pos);

    if (distance < m_cdis)
    {
        m_cdis = distance;
        m_ch = ash.GetSAddr();
    }
    packet = 0;
}

void
RoutingERNC::ProcessJoinAnnouncement(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    n_members++;
    packet = 0;
}

void
RoutingERNC::SendHello()    // only sink create HELLO;  others forward HELLO
{
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringHelloHeader dchh;
    AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    AquaSimAddress asBroadcast = AquaSimAddress::GetBroadcast();
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    if (m_identity == SINK)
    {
        // 配置hello头
        dchh.SetPosition(m_pos);
        dchh.SetCHFlag(2);
        // 配置aqua-sim头
        ash.SetDirection(AquaSimHeader::DOWN);
        ash.SetNextHop(asBroadcast);
        ash.SetSAddr(mAddr);
        ash.SetDAddr(asBroadcast);
        ash.SetErrorFlag(false);
        ash.SetNumForwards(1);
        ash.SetUId(packet->GetUid());
        ash.SetNetDataType(AquaSimHeader::HELLO);
        n_members = 0;
    }else
    {
        m_identity = CH;
        // 配置hello头
        dchh.SetPosition(m_pos);
        dchh.SetCHFlag(1);
        // 配置aqua-sim头
        ash.SetDirection(AquaSimHeader::DOWN);
        ash.SetNextHop(asBroadcast);
        ash.SetSAddr(mAddr);
        ash.SetDAddr(asBroadcast);
        ash.SetErrorFlag(false);
        ash.SetNumForwards(m_hops + 1);
        ash.SetUId(m_round);
        ash.SetNetDataType(AquaSimHeader::HELLO);
        n_members = 0;
    }
    ash.SetSize(12);
    
    // 添加headers
    packet->AddHeader(dchh);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(1, ash.GetSize());
    if (m_identity == SINK)
    {
        SendDown(packet, asBroadcast, Seconds(0));
        // m_Timer.Schedule(m_interval);
        Simulator::Schedule(m_interval, &RoutingERNC::SendHello, this);
    }else
        SendDown(packet, asBroadcast, Seconds(0));
    
    NS_LOG_UNCOND("Node " << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() 
                    << " send hello");
}

void
RoutingERNC::SendAck()
{
    AquaSimAddress asBroadcast = AquaSimAddress::GetBroadcast();
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    int m_layer = std::ceil((m_pos.z + 0.1) / m_range);
    m_tc = std::pow(2. / (m_layer + m_hops), 1. / (1 + m_degree / 10));
    m_phase = FORMING;
    m_ch = asBroadcast;
    if (m_rand->GetValue() > m_tc)        // tuixuan
    {
        m_identity = CM;
        m_Timer.Cancel();
        m_Timer.SetFunction(&RoutingERNC::SendJoinAnnouncement, this);
        m_Timer.Schedule(Seconds(5));
        // Simulator::Schedule(Seconds(5), &RoutingERNC::SendJoinAnnouncement, this);
        return;
    }
    m_identity = CH;
    double tw = 0.9 * m_dis / 1500.;
    m_Timer.Cancel();
    m_Timer.SetFunction(&RoutingERNC::SendCHAnnouncement, this);
    m_Timer.Schedule(Seconds(tw));
    // Simulator::Schedule(Seconds(tw), &RoutingERNC::SendCHAnnouncement, this);

    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringHelloHeader dchh;
    AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    
    // 配置aqua-sim头
    ash.SetDirection(AquaSimHeader::DOWN);
    m_ch = asBroadcast;
    ash.SetNextHop(asBroadcast);
    ash.SetSAddr(mAddr);
    ash.SetDAddr(asBroadcast);
    ash.SetErrorFlag(false);
    ash.SetNumForwards(1);
    ash.SetNetDataType(AquaSimHeader::ACK);
    n_members = 0;
    dchh.SetWeight(m_tc);
    double dmax = Vector(std::max(m_pos.x - minX, maxX - m_pos.x),
                                 std::max(m_pos.y - minY, maxY - m_pos.y),
                                 std::max(m_pos.z - minZ, maxZ - m_pos.z)).GetLength();
    double dsink = (m_pos - sinkPos).GetLength();
    dchh.SetRange((1 - 0.5 * (dmax - dsink) / dmax) * m_range);
    dchh.SetPosition(m_pos);
    ash.SetSize(7);
    
    // 添加headers
    packet->AddHeader(dchh);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(1, ash.GetSize());
    SendDown(packet, asBroadcast, Seconds(0));
    
    NS_LOG_UNCOND("Node " << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() 
                    << " send race ");
}

void
RoutingERNC::SendCHAnnouncement()
{
    if (m_ctc > m_tc)
    {
        m_identity = CM;
        m_Timer.Cancel();
        m_Timer.SetFunction(&RoutingERNC::SendJoinAnnouncement, this);
        m_Timer.Schedule(Seconds(4));
        // Simulator::Schedule(Seconds(4), &RoutingERNC::SendJoinAnnouncement, this);
        return;
    }
    m_phase = ROUTING;
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringHelloHeader dchh;
    AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    AquaSimAddress asBroadcast = AquaSimAddress::GetBroadcast();
    
    // 配置aqua-sim头
    ash.SetDirection(AquaSimHeader::DOWN);
    ash.SetNextHop(asBroadcast);
    ash.SetSAddr(mAddr);
    ash.SetDAddr(asBroadcast);
    ash.SetErrorFlag(false);
    ash.SetNumForwards(1);
    ash.SetNetDataType(AquaSimHeader::CH);
    dchh.SetPosition(m_pos);
    n_members = 0;
    
    ash.SetSize(3);
    
    // 添加headers
    packet->AddHeader(dchh);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(1, ash.GetSize());
    SendDown(packet, asBroadcast, Seconds(0));
    
    NS_LOG_UNCOND("Node " << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() 
                    << " send ch announcement");
}

void
RoutingERNC::SendJoinAnnouncement()
{
    if (m_ch == AquaSimAddress::GetBroadcast())
        return;
    m_phase = ROUTING;
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    
    // 配置aqua-sim头
    ash.SetDirection(AquaSimHeader::DOWN);
    ash.SetNextHop(m_ch);
    ash.SetSAddr(mAddr);
    ash.SetDAddr(m_ch);
    ash.SetErrorFlag(false);
    ash.SetNumForwards(1);
    ash.SetUId(m_round);
    ash.SetNetDataType(AquaSimHeader::JOIN);
    n_members = 0;
    
    ash.SetSize(5);
    
    // 添加headers
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(3, ash.GetSize());
    SendDown(packet, m_ch, Seconds(0));
    
    NS_LOG_UNCOND("Node " << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() 
                    << " send join announcement to " << m_ch.GetAsInt());
}

void
RoutingERNC::SetIdentity(NodeIdentity identity)
{
    m_identity = identity;
}

double
RoutingERNC::GetBackoff()
{
    return 0;
}

void
RoutingERNC::DoInitialize()
{
    m_forwarded.clear();
    m_Timer = Timer(Timer::CANCEL_ON_DESTROY);
    m_round = 0;
    if (m_identity == SINK)
    {
        m_hops = 0;
        AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
        // m_Timer.SetFunction(&RoutingERNC::SendHello, this);
        // m_Timer.Schedule(Seconds(0.01));
        Simulator::Schedule(Seconds(0.01), &RoutingERNC::SendHello, this);
    }
}

void
RoutingERNC::DoDispose()
{
    m_rand = 0;
    m_forwarded.clear();
}