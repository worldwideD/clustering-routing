#include "routing-RCRP.h"
#include "dolphin-clustering-header.h"
#include "aqua-sim-address.h"
#include "aqua-sim-header-routing.h"
#include "aqua-sim-header.h"
#include "ns3/log.h"
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoutingRCRP");
NS_OBJECT_ENSURE_REGISTERED(RoutingRCRP);

// Clustering Routing Protocol  分簇路由协议
RoutingRCRP::RoutingRCRP()
    :m_Timer(Timer::CANCEL_ON_DESTROY),
    m_ability(SHORT),
    m_identity(CM),
    m_rounds(0)
{
    m_rand = CreateObject<UniformRandomVariable> ();
    sinkPos.clear();
}

RoutingRCRP::~RoutingRCRP()
{
}

TypeId
RoutingRCRP::GetTypeId()
{
    static TypeId tid = TypeId ("ns3::RoutingRCRP")
        .SetParent<AquaSimRouting>()
        .AddConstructor<RoutingRCRP>()
        .AddAttribute("Interval",
                    "cluster reforming interval.",
                    TimeValue(Seconds(100)),
                    MakeTimeAccessor(&RoutingRCRP::m_interval),
                    MakeTimeChecker())
        .AddAttribute("Prob",
                    "parameter prob for link-hold probability",
                    DoubleValue(0.2),
                    MakeDoubleAccessor(&RoutingRCRP::prob),
                    MakeDoubleChecker<double>())
        .AddAttribute("Ability",
                    "the node's transmission ability",
                    EnumValue(SHORT),
                    MakeEnumAccessor(&RoutingRCRP::m_ability),
                    MakeEnumChecker(SHORT, "SHORT", LONG, "LONG"))
        .AddAttribute("Range",
                    "the node's transmission range",
                    DoubleValue(1000.),
                    MakeDoubleAccessor(&RoutingRCRP::m_range),
                    MakeDoubleChecker<double>())
        .AddAttribute("Identity",
                    "the node's identity",
                    EnumValue(CM),
                    MakeEnumAccessor(&RoutingRCRP::m_identity),
                    MakeEnumChecker(CM, "CM", CH, "CH", SINK, "SINK"))
        .AddAttribute("Thres",
                    "the threshold for forwarding",
                    DoubleValue(0.55),
                    MakeDoubleAccessor(&RoutingRCRP::thres),
                    MakeDoubleChecker<double>())
        .AddTraceSource("Tx",
                    "Net:A new packet is sent",
                    MakeTraceSourceAccessor(&RoutingRCRP::m_txTrace),
                    "RoutingRCRP::TxCallback")
        .AddTraceSource("Rx",
                    "Net:A new packet is received",
                    MakeTraceSourceAccessor(&RoutingRCRP::m_rxTrace),
                    "RoutingRCRP::RxCallback")
        .AddTraceSource("Counter",
                    "Net:Evaluation Counter",
                    MakeTraceSourceAccessor(&RoutingRCRP::m_counterTrace),
                    "RoutingRCRP::CounterCallBack")
        .AddTraceSource("Delay",
                    "Net:End2End Delay Counter",
                    MakeTraceSourceAccessor(&RoutingRCRP::m_delayTrace),
                    "RoutingRCRP::CounterCallBack")
    ;
    return tid;
}

int64_t
RoutingRCRP::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    return 0;
}

bool
RoutingRCRP::Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this);
    AquaSimHeader ash;
    AquaSimAddress myAddr = AquaSimAddress::ConvertFrom(GetNetDevice()->GetAddress());
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    PAUVHeader ph;
    packet->PeekHeader(ash);
    if (ash.GetNetDataType() != AquaSimHeader::DATA)     // 控制包
    {
        switch (ash.GetNetDataType())
        {
        case AquaSimHeader::HELLO:
            ProcessHello(packet);
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
            ash.SetNumForwards(0);
            ash.SetDAddr(AquaSimAddress::ConvertFrom(dest));
            ash.SetErrorFlag(false);
            ash.SetUId(packet->GetUid());
            ash.SetSize(ash.GetSize() + 11);
            m_forwarded.insert(packet->GetUid());
            m_counterTrace(6, 0);
            m_delayTrace(packet->GetUid(), 0, Simulator::Now().GetSeconds());
            ph.SetLocSnd(m_pos);
            if (m_identity == CM)   // 转发给簇头
            {
                if (m_ch == AquaSimAddress::GetBroadcast())
                {
                    packet->AddHeader(ash);
                    m_rxTrace(packet, Simulator::Now().GetSeconds(), myAddr, 5);
                    NS_LOG_UNCOND("Can't find cluster head!  " << packet);
                    packet = 0;
                    return false;
                }
                ash.SetNextHop(m_ch);
                packet->AddHeader(ph);
                packet->AddHeader(ash);
                m_txTrace(packet, Simulator::Now().GetSeconds(), myAddr, 1);
                SendDown(packet, m_ch, Seconds(0));
            }
            else if (neighborSinks.find(AquaSimAddress::ConvertFrom(dest)) != neighborSinks.end()) // 一跳到sink
            {
                ash.SetNextHop(AquaSimAddress::ConvertFrom(dest));
                packet->AddHeader(ph);
                packet->AddHeader(ash);
                m_txTrace(packet, Simulator::Now().GetSeconds(), myAddr, 1);
                SendDown(packet, AquaSimAddress::ConvertFrom(dest), Seconds(0));
                return true;
            }
            else                   // 机会路由转发
            {
                ash.SetNumForwards(1);
                packet->AddHeader(ph);
                packet->AddHeader(ash);
                m_txTrace(packet, Simulator::Now().GetSeconds(), myAddr, 1);
                SendDown(packet, AquaSimAddress::ConvertFrom(dest), Seconds(0));
                // ForwardPacket(packet, ash.GetDAddr());
            }
        }else                           // forward packet
        {
            AquaSimAddress dAddr = ash.GetDAddr();
            uint16_t pid = ash.GetUId();
            packet->RemoveHeader(ash);
            packet->RemoveHeader(ph);
            Vector pos = ph.GetLocSnd();
            if (ash.GetNumForwards() > 0)
            {
                if (m_pos.z > pos.z)
                {
                    packet->AddHeader(ash);
                    m_rxTrace(packet, Simulator::Now().GetSeconds(), myAddr, 4);
                    packet = 0;
                    return false;
                }
            }
            auto it = m_forwarded.find(pid);
            if (it != m_forwarded.end())
            {
                packet->AddHeader(ash);
                m_rxTrace(packet, Simulator::Now().GetSeconds(), myAddr, 4);
                packet = 0;
                return false;
            }
            m_forwarded.insert(pid);
            if (dAddr == myAddr)        // 我是dst
            {
                packet->AddHeader(ash);
                m_txTrace(packet, Simulator::Now().GetSeconds(), myAddr, 0);
                m_counterTrace(6, 1);
                m_delayTrace(packet->GetUid(), 1, Simulator::Now().GetSeconds());
                SendUp(packet);
                return true;
            }
            if (m_identity == CM)             
            {
                packet->AddHeader(ash);
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 5);
                NS_LOG_UNCOND("Can't find routing table entry!  " << packet);
                packet = 0;
                return false;
            }
            ph.SetLocSnd(m_pos);
            if (neighborSinks.find(dAddr) != neighborSinks.end()) // 一跳到sink
            {
                m_rxTrace(packet, Simulator::Now().GetSeconds(), myAddr, 0);
                // packet->RemoveHeader(ash);
                ash.SetNumForwards(ash.GetNumForwards() + 1);
                ash.SetNextHop(dAddr);
                packet->AddHeader(ph);
                packet->AddHeader(ash);
                m_txTrace(packet, Simulator::Now().GetSeconds(), myAddr, 1);
                SendDown(packet, dAddr, Seconds(0));
                return true;
            }
            if (IsDeadLoop(packet))
            {
                packet->AddHeader(ash);
                m_rxTrace(packet, Simulator::Now().GetSeconds(), myAddr, 1);
                NS_LOG_UNCOND("Dropping pkt" << packet << " due to route loop");
                packet = 0;
                return false;
            }                     
            ash.SetNextHop(AquaSimAddress::GetBroadcast());
            ash.SetNumForwards(ash.GetNumForwards() + 1);
            packet->AddHeader(ph);
            packet->AddHeader(ash);
            m_rxTrace(packet, Simulator::Now().GetSeconds(), myAddr, 0);
            SendDown(packet, AquaSimAddress::GetBroadcast(), Seconds(0));
            // ForwardPacket(packet, dAddr);
        }
    }
    packet = 0;
    return true;
}

void
RoutingRCRP::ForwardPacket(Ptr<Packet> packet, AquaSimAddress dst)
{
    AquaSimAddress m_addr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    Vector dstPos = sinkPos[dst];
    AquaSimHeader ash;
    for (auto it = m_chNeighbors.begin(); it != m_chNeighbors.end(); it++)
    {
        double psc = 1 - 1. / (1 + m_range / CalculateDistance(it->second, m_pos));
        if (psc >= thres && it->second.z < m_pos.z)
        {
            Ptr<Packet> cpkt = packet->Copy();
            cpkt->RemoveHeader(ash);
            ash.SetNextHop(it->first);
            ash.SetNumForwards(ash.GetNumForwards() + 1);
            cpkt->AddHeader(ash);
            m_txTrace(cpkt, Simulator::Now().GetSeconds(), m_addr, 1);
            m_counterTrace(7, ash.GetSize());
            double delay = m_rand->GetValue(0, 4. / (1 + std::exp(psc)));
            SendDown(cpkt, it->first, Seconds(delay));
            cpkt = 0;
        }
    }
    packet = 0;
}

void
RoutingRCRP::ProcessHello(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringHelloHeader dchh;
    packet->RemoveHeader(dchh);
    AquaSimAddress src = ash.GetSAddr();
    if (dchh.GetCHFlag() == 0)
    {
        neighborSinks.insert(src);
        packet = 0;
        return;
    }
    Vector pos = dchh.GetPosition();
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    if (m_identity == CH)
        m_chNeighbors[src] = pos;
    else
    {
        double dis = CalculateDistance(m_pos, pos);
        if (dis < min_dis)
        {
            min_dis = dis;
            m_ch = src;
        }
    }

    packet = 0;
}

void
RoutingRCRP::SendHello()
{
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringHelloHeader dchh;
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    // 配置hello头
    dchh.SetPosition(m_pos);
    dchh.SetCHFlag((m_identity == CH)? 1: 0);
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
    ash.SetSize(3);
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
RoutingRCRP::SetIdentity(NodeIdentity identity)
{
    m_identity = identity;
}

void
RoutingRCRP::AddSinkPos(AquaSimAddress addr, Vector pos)
{
    sinkPos[addr] = pos;
}

bool
RoutingRCRP::SelectCH()
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
RoutingRCRP::TimerExpire()
{
    m_chNeighbors.clear();
    m_rounds++;
    neighborSinks.clear();
    if (m_identity == SINK)
    {
        Simulator::ScheduleNow(&RoutingRCRP::SendHello, this);
    }
    else if (m_ability == LONG && SelectCH())
    {
        m_identity = CH;
        last_ch_round = m_rounds;
        Simulator::ScheduleNow(&RoutingRCRP::SendHello, this);
    }else
    {
        m_identity = CM;
        m_ch = AquaSimAddress::GetBroadcast();
        min_dis = 1000000;
    }

    m_Timer.Schedule(m_interval);
}

void
RoutingRCRP::DoInitialize()
{
    m_rounds = 0;
    last_ch_round = -int(1. / prob);
    m_chNeighbors.clear();
    m_forwarded.clear();
    m_ch = AquaSimAddress::GetBroadcast();
    neighborSinks.clear();
    AquaSimRouting::DoInitialize();
    m_Timer.SetFunction(&RoutingRCRP::TimerExpire, this);
    Simulator::ScheduleNow(&RoutingRCRP::TimerExpire, this);
}

void
RoutingRCRP::DoDispose()
{
    m_chNeighbors.clear();
    m_forwarded.clear();
    neighborSinks.clear();
    sinkPos.clear();
    AquaSimRouting::DoDispose();
    m_rand = 0;
}