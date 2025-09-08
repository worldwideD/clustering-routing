#include "routing-PAUV.h"
#include "dolphin-clustering-header.h"
#include "aqua-sim-address.h"
#include "aqua-sim-header-routing.h"
#include "aqua-sim-header.h"
#include "ns3/log.h"
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoutingPAUV");
NS_OBJECT_ENSURE_REGISTERED(RoutingPAUV);

// P-AUV Protocol  P-AUV路由协议
RoutingPAUV::RoutingPAUV()
{
    m_rand = CreateObject<UniformRandomVariable> ();
}

RoutingPAUV::~RoutingPAUV()
{
}

TypeId
RoutingPAUV::GetTypeId()
{
    static TypeId tid = TypeId ("ns3::RoutingPAUV")
        .SetParent<AquaSimRouting>()
        .AddConstructor<RoutingPAUV>()
        .AddAttribute("MinX",
                    "min X Boundary.",
                    DoubleValue(0),
                    MakeDoubleAccessor(&RoutingPAUV::minX),
                    MakeDoubleChecker<double>())
        .AddAttribute("MaxX",
                    "max X Boundary.",
                    DoubleValue(1000),
                    MakeDoubleAccessor(&RoutingPAUV::maxX),
                    MakeDoubleChecker<double>())
        .AddAttribute("MinY",
                    "min Y Boundary.",
                    DoubleValue(0),
                    MakeDoubleAccessor(&RoutingPAUV::minY),
                    MakeDoubleChecker<double>())
        .AddAttribute("MaxY",
                    "max Y Boundary.",
                    DoubleValue(1000),
                    MakeDoubleAccessor(&RoutingPAUV::maxY),
                    MakeDoubleChecker<double>())
        .AddAttribute("MinZ",
                    "min Z Boundary.",
                    DoubleValue(0),
                    MakeDoubleAccessor(&RoutingPAUV::minZ),
                    MakeDoubleChecker<double>())
        .AddAttribute("MaxZ",
                    "max Z Boundary.",
                    DoubleValue(1000),
                    MakeDoubleAccessor(&RoutingPAUV::maxZ),
                    MakeDoubleChecker<double>())
        .AddAttribute("Ability",
                    "the node's transmission ability",
                    EnumValue(SHORT),
                    MakeEnumAccessor(&RoutingPAUV::m_ability),
                    MakeEnumChecker(SHORT, "SHORT", LONG, "LONG"))                    
        .AddTraceSource("Tx",
                    "Net:A new packet is sent",
                    MakeTraceSourceAccessor(&RoutingPAUV::m_txTrace),
                    "DolphinRoutingLEACH::TxCallback")
        .AddTraceSource("Rx",
                    "Net:A new packet is received",
                    MakeTraceSourceAccessor(&RoutingPAUV::m_rxTrace),
                    "DolphinRoutingLEACH::RxCallback")
        .AddTraceSource("Counter",
                    "Net:Evaluation Counter",
                    MakeTraceSourceAccessor(&RoutingPAUV::m_counterTrace),
                    "DolphinRoutingLEACH::CounterCallBack")
        .AddTraceSource("Delay",
                    "Net:End2End Delay Counter",
                    MakeTraceSourceAccessor(&RoutingPAUV::m_delayTrace),
                    "DolphinRoutingLEACH::CounterCallBack")
    ;
    return tid;
}

int64_t
RoutingPAUV::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    return 0;
}

bool
RoutingPAUV::Recv(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this);
    AquaSimHeader ash;
    AquaSimAddress myAddr = AquaSimAddress::ConvertFrom(GetNetDevice()->GetAddress());
    Vector myLoc = m_device->GetNode()->GetObject<DolphinMobilityModel>()->GetLocatedPosition();
    PAUVHeader ph;
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
            ash.SetSize(ash.GetSize() + 8);
            m_forwarded.insert(packet->GetUid());
            ph.SetSrc(myAddr);
            ph.SetLocSrc(myLoc);
            ph.SetLocSnd(myLoc);
            if (AquaSimAddress::ConvertFrom(dest) != AquaSimAddress::GetBroadcast())
            {
                m_counterTrace(6, 0);
                m_delayTrace(packet->GetUid(), 0, Simulator::Now().GetSeconds());
            }  
            ash.SetNextHop(nxtAddr);
            packet->AddHeader(ph);
            packet->AddHeader(ash);
            m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
            SendDown(packet, nxtAddr, Seconds(0));
            return true;
        }else                           // forward packet
        {
            m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
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
            Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
            if (rand->GetValue() < 0.2)
            {
                m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 10);
                return false;
            }
            if (dAddr == AquaSimAddress::GetBroadcast()) // 广播包
            {
                packet->RemoveHeader(ash);
                ash.SetNumForwards(ash.GetNumForwards() + 1);
                packet->AddHeader(ash);
                Ptr<Packet> cpkt = packet->Copy();
                cpkt->RemoveHeader(ash);
                cpkt->RemoveHeader(ph);
                cpkt->AddHeader(ash);
                m_txTrace(cpkt, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
                SendUp(cpkt);
                m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
                SendDown(packet, AquaSimAddress::GetBroadcast(), Seconds(0));
                return true;
            }
            // 单播包
            if (dAddr == myAddr)        // 我是dst
            {
                packet->RemoveHeader(ash);
                packet->RemoveHeader(ph);
                packet->AddHeader(ash);
                m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 0);
                m_counterTrace(6, 1);
                m_delayTrace(packet->GetUid(), 1, Simulator::Now().GetSeconds());
                SendUp(packet);
                return true;
            }
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
            packet->RemoveHeader(ash);
            packet->RemoveHeader(ph);
            Vector locSnd = ph.GetLocSnd();
            Vector locSrc = ph.GetLocSrc();
            // if (CalculateDistance(locSrc, locSnd) >= CalculateDistance(locSrc, myLoc))
            // {
            //     packet->AddHeader(ash);
            //     m_rxTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 10);
            //     return false;
            // }                  
            ash.SetNumForwards(ash.GetNumForwards() + 1);
            ash.SetNextHop(nxtAddr);
            ph.SetLocSnd(myLoc);
            packet->AddHeader(ph);
            packet->AddHeader(ash);
            m_txTrace(packet, Simulator::Now().GetSeconds(), AquaSimAddress::ConvertFrom(m_device->GetAddress()), 1);
            m_counterTrace(7, ash.GetSize());
            Vector diff = Vector(std::max(locSrc.x - minX, maxX - locSrc.x),
                                 std::max(locSrc.y - minY, maxY - locSrc.y),
                                 std::max(locSrc.z - minZ, maxZ - locSrc.z));
            double delay = diff.GetLength() / CalculateDistance(locSrc, myLoc);
            delay = 1.5;
            SendDown(packet, nxtAddr, Seconds(m_rand->GetValue(0, delay)));
        }
    }
    return true;
}

void
RoutingPAUV::SetBoundary(double a, double b, double c, double d, double e, double f)
{
    minX = a; maxX = b;
    minY = c; maxY = d;
    minZ = e; maxZ = f;
}

void
RoutingPAUV::DoInitialize()
{
    m_forwarded.clear();
}

void
RoutingPAUV::DoDispose()
{
    m_rand = 0;
    m_forwarded.clear();
}