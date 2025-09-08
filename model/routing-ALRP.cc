#include "routing-ALRP.h"
#include "dolphin-clustering-header.h"
#include "aqua-sim-address.h"
#include "aqua-sim-header-routing.h"
#include "aqua-sim-header.h"
#include "ns3/log.h"
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoutingALRP");
NS_OBJECT_ENSURE_REGISTERED(RoutingALRP);

// Clustering Routing Protocol  分簇路由协议
RoutingALRP::RoutingALRP():
    m_ability(SHORT),
    m_identity(CM)
{
    m_rand = CreateObject<UniformRandomVariable> ();
    sinkPos.clear();
}

RoutingALRP::~RoutingALRP()
{
}

TypeId
RoutingALRP::GetTypeId()
{
    static TypeId tid = TypeId ("ns3::RoutingALRP")
        .SetParent<AquaSimRouting>()
        .AddConstructor<RoutingALRP>()
        .AddAttribute("K",
                    "parameter K for forward probability",
                    DoubleValue(7),
                    MakeDoubleAccessor(&RoutingALRP::k),
                    MakeDoubleChecker<double>())
        .AddAttribute("Ability",
                    "the node's transmission ability",
                    EnumValue(SHORT),
                    MakeEnumAccessor(&RoutingALRP::m_ability),
                    MakeEnumChecker(SHORT, "SHORT", LONG, "LONG"))
        .AddAttribute("Range",
                    "the node's transmission range",
                    DoubleValue(1000.),
                    MakeDoubleAccessor(&RoutingALRP::m_range),
                    MakeDoubleChecker<double>())
        .AddAttribute("Identity",
                    "the node's identity",
                    EnumValue(CM),
                    MakeEnumAccessor(&RoutingALRP::m_identity),
                    MakeEnumChecker(CM, "CM", CH, "CH", SINK, "SINK"))
        .AddAttribute("T",
                    "the delay for forwarding",
                    DoubleValue(1),
                    MakeDoubleAccessor(&RoutingALRP::delay),
                    MakeDoubleChecker<double>())
        .AddTraceSource("Tx",
                    "Net:A new packet is sent",
                    MakeTraceSourceAccessor(&RoutingALRP::m_txTrace),
                    "RoutingALRP::TxCallback")
        .AddTraceSource("Rx",
                    "Net:A new packet is received",
                    MakeTraceSourceAccessor(&RoutingALRP::m_rxTrace),
                    "RoutingALRP::RxCallback")
        .AddTraceSource("Counter",
                    "Net:Evaluation Counter",
                    MakeTraceSourceAccessor(&RoutingALRP::m_counterTrace),
                    "RoutingALRP::CounterCallBack")
        .AddTraceSource("Delay",
                    "Net:End2End Delay Counter",
                    MakeTraceSourceAccessor(&RoutingALRP::m_delayTrace),
                    "RoutingALRP::CounterCallBack")
    ;
    return tid;
}

int64_t
RoutingALRP::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    return 0;
}

bool
RoutingALRP::Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this);
    AquaSimHeader ash;
    AquaSimAddress myAddr = AquaSimAddress::ConvertFrom(GetNetDevice()->GetAddress());
    Vector m_pos = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    DolphinClusteringHelloHeader dchh;
    packet->PeekHeader(ash);
    if (ash.GetNetDataType() == AquaSimHeader::DATA)
    {
        AquaSimAddress nxtAddr = AquaSimAddress::GetBroadcast();
        if (AmISrc(packet))               // new packet
        {
            packet->RemoveHeader(ash);
            ash.SetDirection(AquaSimHeader::DOWN);
            ash.SetNumForwards(1);
            ash.SetDAddr(AquaSimAddress::ConvertFrom(dest));
            ash.SetErrorFlag(false);
            ash.SetUId(packet->GetUid());
            ash.SetSize(ash.GetSize() + 16);
            m_forwarded.insert(packet->GetUid());
            dchh.SetPosition(m_pos);
            dchh.SetRange(m_range);
            if (AquaSimAddress::ConvertFrom(dest) != AquaSimAddress::GetBroadcast())
            {
                m_counterTrace(6, 0);
                m_delayTrace(packet->GetUid(), 0, Simulator::Now().GetSeconds());
            }  
            ash.SetNextHop(nxtAddr);
            packet->AddHeader(dchh);
            packet->AddHeader(ash);
            m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
            SendDown(packet, nxtAddr, Seconds(0));
            return true;
        }else                           // forward packet
        {
            m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
            AquaSimAddress dAddr = ash.GetDAddr();
            if (dAddr == myAddr)        // 我是dst
            {
                AquaSimAddress dAddr = ash.GetDAddr();
                uint16_t pid = ash.GetUId();
                auto it = m_forwarded.find(pid);
                if (it != m_forwarded.end())
                {
                    packet->AddHeader(ash);
                    m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 4);
                    packet = 0;
                    return false;
                }
                m_forwarded.insert(pid);
                packet->RemoveHeader(ash);
                packet->RemoveHeader(dchh);
                packet->AddHeader(ash);
                m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
                m_counterTrace(6, 1);
                m_delayTrace(packet->GetUid(), 1, Simulator::Now().GetSeconds());
                SendUp(packet);
                return true;
            }
            packet->RemoveHeader(ash);
            packet->RemoveHeader(dchh);
            Vector lst_pos = dchh.GetPosition();
            Vector dst_pos = sinkPos[dAddr];
            double r = dchh.GetRange();
            if ((dst_pos.x - lst_pos.x) * (m_pos.x - lst_pos.x) +
                (dst_pos.y - lst_pos.y) * (m_pos.y - lst_pos.y) +
                (dst_pos.z - lst_pos.z) * (m_pos.z - lst_pos.z) <= 0) // if not in forward area
            {
                packet->AddHeader(ash);
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 5);
                packet = 0;
                return false;
            }

            double d2plane = ((dst_pos.x - lst_pos.x) * (m_pos.x - lst_pos.x) +
                        (dst_pos.y - lst_pos.y) * (m_pos.y - lst_pos.y) +
                        (dst_pos.z - lst_pos.z) * (m_pos.z - lst_pos.z)) / CalculateDistance(dst_pos, lst_pos);
            double d2lst = CalculateDistance(dst_pos, m_pos);
            double prob = (k * d2lst + d2plane - d2lst) / (k * d2lst);

            if (m_rand->GetValue() > prob)
            {
                packet->AddHeader(ash);
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 6);
                packet = 0;
                return false;
            }

            uint16_t pid = ash.GetUId();
            auto it = m_forwarded.find(pid);
            if (it != m_forwarded.end())
            {
                packet->AddHeader(ash);
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 4);
                packet = 0;
                return false;
            }
            m_forwarded.insert(pid);

            // 单播包
            if (IsDeadLoop(packet))
            {
                packet->AddHeader(ash);
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
                NS_LOG_UNCOND("Dropping pkt" << packet << " due to route loop");
                packet = 0;
                return false;
            }   
            // if (m_ability == SHORT)             
            // {
            //     m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 5);
            //     NS_LOG_UNCOND("Can't find routing table entry!  " << packet);
            //     packet = 0;
            //     return false;
            // }
            ash.SetNumForwards(ash.GetNumForwards() + 1);
            ash.SetNextHop(nxtAddr);
            dchh.SetPosition(m_pos);
            dchh.SetRange(m_range);
            packet->AddHeader(dchh);
            packet->AddHeader(ash);
            m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
            m_counterTrace(7, ash.GetSize());
            double delay = std::sqrt((1 - d2plane / d2lst) + (r - d2plane) / r) * delay + 2 * (r - d2lst) / 1500;
            SendDown(packet, nxtAddr, Seconds(m_rand->GetValue(0, delay)));
        }
    }
    packet = 0;
    return true;
}

void
RoutingALRP::SetIdentity(NodeIdentity identity)
{
    m_identity = identity;
}

void
RoutingALRP::AddSinkPos(AquaSimAddress addr, Vector pos)
{
    sinkPos[addr] = pos;
}

void
RoutingALRP::DoInitialize()
{
    m_forwarded.clear();
    AquaSimRouting::DoInitialize();
}

void
RoutingALRP::DoDispose()
{
    m_forwarded.clear();
    sinkPos.clear();
    AquaSimRouting::DoDispose();
    m_rand = 0;
}