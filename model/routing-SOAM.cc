#include "routing-SOAM.h"
#include "dolphin-clustering-header.h"
#include "aqua-sim-address.h"
#include "aqua-sim-header-routing.h"
#include "aqua-sim-header.h"
#include "ns3/log.h"
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoutingSOAM");
NS_OBJECT_ENSURE_REGISTERED(RoutingSOAM);

// SOAM Protocol  SOAM分簇路由协议
RoutingSOAM::RoutingSOAM():
    m_ability(SHORT),
    m_identity(CM)
{
    m_rand = CreateObject<UniformRandomVariable> ();
}

RoutingSOAM::~RoutingSOAM()
{
}

TypeId
RoutingSOAM::GetTypeId()
{
    static TypeId tid = TypeId ("ns3::RoutingSOAM")
        .SetParent<AquaSimRouting>()
        .AddConstructor<RoutingSOAM>()
        .AddAttribute("Interval",
                    "cluster reforming interval.",
                    TimeValue(Seconds(100)),
                    MakeTimeAccessor(&RoutingSOAM::m_interval),
                    MakeTimeChecker())
        .AddAttribute("WaitingInterval",
                    "cluster reforming waiting interval for CHs.",
                    TimeValue(Seconds(10)),
                    MakeTimeAccessor(&RoutingSOAM::m_waiting),
                    MakeTimeChecker())
        .AddAttribute("Ability",
                    "the node's transmission ability",
                    EnumValue(SHORT),
                    MakeEnumAccessor(&RoutingSOAM::m_ability),
                    MakeEnumChecker(SHORT, "SHORT", LONG, "LONG"))
        .AddAttribute("Range",
                    "the node's transmission range",
                    DoubleValue(1000.),
                    MakeDoubleAccessor(&RoutingSOAM::m_range),
                    MakeDoubleChecker<double>())
        .AddAttribute("Identity",
                    "the node's identity",
                    EnumValue(CM),
                    MakeEnumAccessor(&RoutingSOAM::m_identity),
                    MakeEnumChecker(CM, "CM", CH, "CH", SINK, "SINK"))
        .AddAttribute("NMembers",
                    "num of CH's members",
                    IntegerValue(0),
                    MakeIntegerAccessor(&RoutingSOAM::n_members),
                    MakeIntegerChecker<int>())
        .AddTraceSource("Tx",
                    "Net:A new packet is sent",
                    MakeTraceSourceAccessor(&RoutingSOAM::m_txTrace),
                    "RoutingSOAM::TxCallback")
        .AddTraceSource("Rx",
                    "Net:A new packet is received",
                    MakeTraceSourceAccessor(&RoutingSOAM::m_rxTrace),
                    "RoutingSOAM::RxCallback")
        .AddTraceSource("Counter",
                    "Net:Evaluation Counter",
                    MakeTraceSourceAccessor(&RoutingSOAM::m_counterTrace),
                    "RoutingSOAM::CounterCallBack")
        .AddTraceSource("Delay",
                    "Net:End2End Delay Counter",
                    MakeTraceSourceAccessor(&RoutingSOAM::m_delayTrace),
                    "RoutingSOAM::CounterCallBack")
    ;
    return tid;
}

int64_t
RoutingSOAM::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    return 0;
}

bool
RoutingSOAM::Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this);
    AquaSimHeader ash;
    AquaSimAddress myAddr = AquaSimAddress::ConvertFrom(GetNetDevice()->GetAddress());
    packet->PeekHeader(ash);
    if (ash.GetNetDataType() == AquaSimHeader::HELLO)
    {
        ProcessHello(packet);
    }
    else if (ash.GetNetDataType() == AquaSimHeader::JOIN)
    {
        ProcessJoinAnnouncement(packet);
    }
    else
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
            m_forwarded.insert(packet->GetUid());
            m_counterTrace(6, 0);
            m_delayTrace(packet->GetUid(), 0, Simulator::Now().GetSeconds());
            if (m_table.find(dstAddr) == m_table.end())
            {
                NS_LOG_UNCOND("Can't find routing table entry!  " << packet);
                packet = 0;
                return false;
            }
            nxtAddr = m_table[dstAddr].nxt;
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
            if (m_table.find(dstAddr) == m_table.end())
            {
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 5);
                NS_LOG_UNCOND("Can't find routing table entry!  " << packet);
                packet = 0;
                return false;
            }
            m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
            packet->RemoveHeader(ash);
            nxtAddr = m_table[dstAddr].nxt;
            ash.SetNumForwards(ash.GetNumForwards() + 1);
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
RoutingSOAM::ProcessHello(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    if (m_identity == SINK)
        return;
    AquaSimHeader ash;
    packet->RemoveHeader(ash);
    DolphinClusteringHelloHeader dchh;
    packet->RemoveHeader(dchh);
    DolphinClusteringAnnouncementHeader dcah;
    packet->PeekHeader(dcah);
    AquaSimAddress lst = ash.GetSAddr();
    AquaSimAddress src = dcah.GetCHID();
    Vector pos = dchh.GetPosition();
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    double distance = CalculateDistance(pos, m_pos);
    uint16_t pid = ash.GetUId();
    int h = ash.GetNumForwards();
    SOAMRoutingTableEntry entry = SOAMRoutingTableEntry(lst, h, distance, FORMING, pid);

    if (m_table.find(src) == m_table.end() || pid > m_table[src].rounds)
    {
        if (m_Timer.find(src) == m_Timer.end())
        {
            m_Timer[src] = Timer(Timer::CANCEL_ON_DESTROY);
            if (m_ability == LONG)
                m_Timer[src].SetFunction(&RoutingSOAM::SendHello, this);
            else
                m_Timer[src].SetFunction(&RoutingSOAM::SendJoinAnnouncement, this);
            m_Timer[src].SetArguments(src);
        }
        if (dchh.GetCHFlag() == 2)      // from sink
        {
            m_table[src] = entry;
            m_ch = entry.nxt;
            // if (m_ability == LONG)
            {
                m_Timer[src].Cancel();
                m_Timer[src].Schedule(Seconds(0.01));
            }
        }else
        {
            // if (m_ability == LONG)
            {
                m_Timer[src].Cancel();
                m_Timer[src].Schedule(m_waiting);
            }
            m_ch = entry.nxt;
            m_table[src] = entry;
        }
    }
    else if (pid == m_table[src].rounds && m_table[src].phase == FORMING)
    {
        if (distance < m_table[src].dist || distance == m_table[src].dist && h < m_table[src].hops)
        {
            m_table[src] = entry;
        }
    }
    else
        return;

    packet->AddHeader(ash);
    packet = 0;
}

void
RoutingSOAM::ProcessJoinAnnouncement(Ptr<Packet> packet)
{
    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
    n_members++;
    packet = 0;
}

void
RoutingSOAM::SendHello(AquaSimAddress src)    // only sink create HELLO;  CH forward HELLO
{
    m_table[src].phase = ROUTING;
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    DolphinClusteringHelloHeader dchh;
    DolphinClusteringAnnouncementHeader dcah;
    AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    AquaSimAddress asBroadcast = AquaSimAddress::GetBroadcast();
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    if (m_identity == SINK)
    {
        // 配置hello头
        dcah.SetCHID(mAddr);
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
        dcah.SetCHID(src);
        dchh.SetPosition(m_pos);
        dchh.SetCHFlag(1);
        // 配置aqua-sim头
        ash.SetDirection(AquaSimHeader::DOWN);
        ash.SetNextHop(asBroadcast);
        ash.SetSAddr(mAddr);
        ash.SetDAddr(asBroadcast);
        ash.SetErrorFlag(false);
        ash.SetNumForwards(m_table[src].hops + 1);
        ash.SetUId(m_table[src].rounds);
        ash.SetNetDataType(AquaSimHeader::HELLO);
        n_members = 0;
    }
    ash.SetSize(12);
    
    // 添加headers
    packet->AddHeader(dcah);
    packet->AddHeader(dchh);
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(1, ash.GetSize());
    if (m_identity == SINK)
    {
        SendDown(packet, asBroadcast, Seconds(0.0));
        m_Timer[src].Schedule(m_interval);
    }else
        SendDown(packet, asBroadcast, Seconds(GetBackoff()));
    
    NS_LOG_UNCOND("Node " << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() 
                    << " send ch announcement");
}

void
RoutingSOAM::SendJoinAnnouncement(AquaSimAddress src)
{
    m_table[src].phase = ROUTING;
    Ptr<Packet> packet = Create<Packet>();
    AquaSimHeader ash;
    AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    
    // 配置aqua-sim头
    ash.SetDirection(AquaSimHeader::DOWN);
    m_ch = m_table[src].nxt;
    ash.SetNextHop(m_ch);
    ash.SetSAddr(mAddr);
    ash.SetDAddr(m_table[src].nxt);
    ash.SetErrorFlag(false);
    ash.SetNumForwards(1);
    ash.SetUId(m_table[src].rounds);
    ash.SetNetDataType(AquaSimHeader::JOIN);
    n_members = 0;
    
    ash.SetSize(12);
    
    // 添加headers
    packet->AddHeader(ash);
    // 发包
    m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
    m_counterTrace(1, ash.GetSize());
    SendDown(packet, m_table[src].nxt, Seconds(GetBackoff()));
    
    NS_LOG_UNCOND("Node " << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() 
                    << " send join announcement to " << m_table[src].nxt.GetAsInt());
}

void
RoutingSOAM::SetIdentity(NodeIdentity identity)
{
    m_identity = identity;
}

double
RoutingSOAM::GetBackoff()
{
    return 0;
    // return m_rand->GetValue() * 1.5;
}

void
RoutingSOAM::DoInitialize()
{
    m_forwarded.clear();
    m_table.clear();
    m_Timer.clear();
    // m_Timer.SetFunction(&RoutingSOAM::SendHello, this);
    if (m_identity == SINK)
    {
        AquaSimAddress mAddr = AquaSimAddress::ConvertFrom(m_device->GetAddress());
        m_table[mAddr] = SOAMRoutingTableEntry(mAddr, 0, 0, ROUTING, -1);
        m_Timer[mAddr] = Timer(Timer::CANCEL_ON_DESTROY);
        m_Timer[mAddr].SetFunction(&RoutingSOAM::SendHello, this);
        m_Timer[mAddr].SetArguments(mAddr);
        m_Timer[mAddr].Schedule(Seconds(0.01));
    }
}

void
RoutingSOAM::DoDispose()
{
    m_rand = 0;
    m_table.clear();
    m_Timer.clear();
    m_forwarded.clear();
}