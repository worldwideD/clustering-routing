

#include "aqua-sim-mac-tdma.h"

#include "aqua-sim-address.h"
#include "aqua-sim-header-mac.h"
#include "aqua-sim-header.h"
#include "aqua-sim-time-tag.h"

#include "ns3/aqua-sim-ng-module.h"
#include "ns3/integer.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>

// using namespace ns3;
namespace ns3
{
NS_LOG_COMPONENT_DEFINE("AquaSimTdmaMac");
NS_OBJECT_ENSURE_REGISTERED(AquaSimTdmaMac);
ATTRIBUTE_HELPER_CPP(IntergeVector);

// 重载输入流操作符>>
std::istream&
operator>>(std::istream& is, IntergeVector& sl)
{
    std::vector<int> values;
    int value;
    while (is >> value)
    {
        values.push_back(value);
        if (is.peek() == ',')
        { // 假设输入格式为逗号分隔
            is.ignore();
        }
    }
    sl.setValues(values);
    return is;
}

// 重载输出流操作符<<
std::ostream&
operator<<(std::ostream& os, const IntergeVector& sl)
{
    for (size_t i = 0; i < sl.data.size(); ++i)
    {
        os << sl.data[i];
        if (i < sl.data.size() - 1)
        {
            os << ", ";
        }
    }
    return os;
}

/* =========
TDMA-MAC
============ */

AquaSimTdmaMac::AquaSimTdmaMac()
{
    m_rand = CreateObject<UniformRandomVariable>();

    // Tdma
    m_tdma_state = IDLE;
    m_tdma_slot_period = 25;
    m_tdma_total_ms = MilliSeconds(1200);
    m_tdma_slot_ms = MilliSeconds(
        600); // TODO: adjust it to accomodate the max possible Tx range, pkt size and channel speed
    // m_tdma_dalay_ms = MilliSeconds(2000);
    m_tdma_guard_interval_ms = MilliSeconds(600);
    data_pac_length = 20;
    loc_pac_length = 9;
    m_tdma_slot_number = 0;
    m_tdma_data_length = 10000;
    // m_tdma_data_length
}

TypeId
AquaSimTdmaMac::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AquaSimTdmaMac")
            .SetParent<AquaSimMac>()
            .AddConstructor<AquaSimTdmaMac>()
            .AddAttribute("TdmaSlotPeriod",
                          "Number of slots in a single TDMA round",
                          UintegerValue(10),
                          MakeUintegerAccessor(&AquaSimTdmaMac::m_tdma_slot_period),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("TdmaSlotNumber",
                          "TDMA Tx slot number of the node",
                          UintegerValue(0),
                          MakeUintegerAccessor(&AquaSimTdmaMac::m_tdma_slot_number),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("TdmaSlotDuration",
                          "Duration of TDMA slot",
                          TimeValue(MilliSeconds(600)),
                          MakeTimeAccessor(&AquaSimTdmaMac::m_tdma_slot_ms),
                          MakeTimeChecker())
            .AddAttribute("TdmaGuardTime",
                          "Guard time in-between two slots",
                          TimeValue(MilliSeconds(600)),
                          MakeTimeAccessor(&AquaSimTdmaMac::m_tdma_guard_interval_ms),
                          MakeTimeChecker())
            .AddAttribute("tdmaDataLength",
                          "The maximum amount of data allowed to be transmitted in a time slot",
                          UintegerValue(1000),
                          MakeUintegerAccessor(&AquaSimTdmaMac::m_tdma_data_length),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("DataPacketLength",
                          "The maximum amount of data allowed to be transmitted in a time slot",
                          UintegerValue(20),
                          MakeUintegerAccessor(&AquaSimTdmaMac::data_pac_length),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("LocPacketLength",
                          "The maximum amount of data allowed to be transmitted in a time slot",
                          UintegerValue(9),
                          MakeUintegerAccessor(&AquaSimTdmaMac::loc_pac_length),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("TdmaSlotNumberList",
                          "TDMA Tx slot number list of the node",
                          IntergeVectorValue(AquaSimTdmaMac::GetDefaultLists()),
                          MakeIntergeVectorAccessor(&AquaSimTdmaMac::m_tdma_slot_list),
                          MakeIntergeVectorChecker())
            .AddTraceSource("MactdmaTx",
                            "Trace source indicating a packet has been delivered to the Phy layer "
                            "for transmitting.",
                            MakeTraceSourceAccessor(&AquaSimTdmaMac::tdma_mac_TxTrace),
                            "ns3::AquaSimTdmaMac::TxCallback")
            .AddTraceSource("MactdmaRx",
                            "Trace source indicating a packet has been delivered to the Phy layer "
                            "for transmitting.",
                            MakeTraceSourceAccessor(&AquaSimTdmaMac::tdma_mac_RxTrace),
                            "ns3::AquaSimTdmaMac::TxCallback")
            .AddTraceSource("MactdmaLOSS",
                            "Trace source indicating a packet has been delivered to the Phy layer "
                            "for transmitting.",
                            MakeTraceSourceAccessor(&AquaSimTdmaMac::tdma_mac_LOSSTrace),
                            "ns3::AquaSimTdmaMac::TxCallback")

        ;

    return tid;
}

int64_t
AquaSimTdmaMac::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_rand->SetStream(stream);
    return 1;
}

IntergeVector
AquaSimTdmaMac::GetDefaultLists()
{
    IntergeVector l(1);
    l.setValues({0});
    return l;
}

bool
AquaSimTdmaMac::RecvProcess(Ptr<Packet> pkt) // 收到来自物理层的包,并发送给网络层
{
    NS_LOG_FUNCTION(this);
    tdma_mac_RxTrace(pkt,
                     Simulator::Now().GetSeconds(),
                     AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    // pkt->Print(std::cout);
    AquaSimHeader ash;
    MacHeader mach;
    pkt->RemoveHeader(ash);
    pkt->RemoveHeader(mach);
    // uint8_t direct = ash.GetDirection();
    //  if(direct == 2)
    //  {NS_LOG_UNCOND("send time !!!!!!: " << ash.GetNumForwards());}
    //   if(ash.GetNumForwards()>4)
    // {NS_LOG_UNCOND("send time !!!!!!: " << ash.GetNumForwards());}
    // std::cout << "send time !!!!!!: " << ash.GetNumForwards() << std::endl;

    AquaSimAddress dst = mach.GetDA();
    AquaSimAddress src = mach.GetSA();
    AquaSimAddress dizhi = ash.GetNextHop();

    // 打印目的地址
    // std::cout << "Packet destination address: " <<
    // AquaSimAddress::ConvertFrom(m_device->GetAddress()) << std::endl;

    if (ash.GetErrorFlag())
    {
        NS_LOG_DEBUG("TdmaMac:RecvProcess: received corrupt packet.");
        pkt = 0;
        return false;
    }
    // read the next sender id
    if ((AquaSimAddress::ConvertFrom(m_device->GetAddress()) == dizhi) ||(AquaSimAddress::ConvertFrom(m_device->GetAddress()) == dst)
        ||(dst == AquaSimAddress(255)) || (dst == AquaSimAddress(0)) ||
        (dizhi == AquaSimAddress(255)) || (dizhi == AquaSimAddress(0)))
    {
        // trace the E2E delay
        AquaSimTimeTag timeTag;
        pkt->RemovePacketTag(timeTag);
        m_e2eDelayTrace((Simulator::Now() - timeTag.GetTime()).GetMilliSeconds());

        pkt->AddHeader(ash);

        tdma_mac_TxTrace(pkt,
                         Simulator::Now().GetSeconds(),
                         AquaSimAddress::ConvertFrom(m_device->GetAddress()));
        SendUp(pkt);
    }
    else
    {
        pkt->AddHeader(ash);
        tdma_mac_LOSSTrace(pkt,
                           Simulator::Now().GetSeconds(),
                           AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    }
    return false;
}

bool
AquaSimTdmaMac::TxProcess(Ptr<Packet> pkt) // 收到来自网络层的包
{
    NS_LOG_FUNCTION(this << pkt);
    tdma_mac_RxTrace(pkt,
                     Simulator::Now().GetSeconds(),
                     AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    // std::cout << m_device->GetNode()->GetId() << " " << m_tdma_slot_number << "\n";

    // Attach a timestamp tag to calculate E2E delay
    AquaSimTimeTag timeTag;
    // 测试用防止重复加标签
    if (pkt->RemovePacketTag(timeTag))
    {
        // 标签已经存在并被移除，现在可以重新添加
    }

    timeTag.SetTime(Simulator::Now());
    pkt->AddPacketTag(timeTag);

    // Put incoming data packets to send-queue
    m_send_queue.push_back(pkt);
    m_queueSizeTrace(m_send_queue.size());

    // Check at which state the GCSMA MAC is now (IDLE, IFS or backoffs)
    if ((m_tdma_state == IDLE))
    {
        // Start TDMA transmission
        StartTdma();
    }

    return true;
}

void
AquaSimTdmaMac::StartTdma()
{
    // If queue is empty, set TDMA state back to IDLE and return
    if (m_send_queue.size() == 0)
    {
        m_tdma_state = IDLE;
        return;
    }

    // Set TDMA state to TX
    m_tdma_state = TX;

    // Get packet from queue
    Ptr<Packet> p = m_send_queue.front();
    m_send_queue.pop_front();

    m_packet_start_ts = Simulator::Now(); // to calculate MAC send delay
    // Schedule packet for Tx within the own slot, according to TDMA schedule
    ScheduleNextSlotTx(p);
}

void
AquaSimTdmaMac::SendPacket(Ptr<Packet> packet) // 向物理层发包
{
    // tdma_mac_RxTrace(packet,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    AquaSimHeader ash;
    MacHeader mach;

    packet->RemoveHeader(ash);
    mach.SetSA(ash.GetSAddr());
    mach.SetDA(ash.GetDAddr());
    mach.SetDemuxPType(MacHeader::UWPTYPE_OTHER);
    uint64_t current_slot_now = Simulator::Now().GetMicroSeconds() /
                            (m_tdma_slot_ms + m_tdma_guard_interval_ms).GetMicroSeconds();
    ash.SetTimeSlot(current_slot_now);
    // uint8_t direct = ash.GetDirection();
    //  if(direct == 2)
    //  {NS_LOG_UNCOND("send time !!!!!!: " << ash.GetNumForwards());}
    // NS_LOG_UNCOND("send time !!!!!!: " << ash.GetNumForwards());
    packet->AddHeader(mach);
    packet->AddHeader(ash);
    // NS_LOG_UNCOND("Current simulation time: " << Simulator::Now().GetSeconds() << " seconds");
    // packet->Print(std::cout);
    //  Just send packet down to PHY
    //  uint32_t packetId = packet->GetUid();
    //  NS_LOG_UNCOND("Current packet: " << packetId << " uid");
    tdma_mac_TxTrace(packet,
                     Simulator::Now().GetSeconds(),
                     AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    SendDown(packet);
    // tdma_mac_TxTrace(packet,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    //  Go to next packet in the queue
    StartTdma();
}

void
AquaSimTdmaMac::SendPacketLOC() // 发送定位包
{
    Ptr<Packet> p = Create<Packet>();
    AquaSimHeader ash;
    MacHeader mach;
    LocalizationHeader loch;
    // packet->RemoveHeader(ash);
    ash.SetDirection(AquaSimHeader::DOWN);
    ash.SetNextHop(AquaSimAddress::GetBroadcast());
    ash.SetSAddr(AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    ash.SetDAddr(AquaSimAddress::GetBroadcast());
    ash.SetErrorFlag(false);
    ash.SetUId(p->GetUid());
    ash.SetTimeStamp(Simulator::Now());
    ash.SetNetDataType(AquaSimHeader::LOC);
    uint64_t current_slot_now = Simulator::Now().GetMicroSeconds() /
                            (m_tdma_slot_ms + m_tdma_guard_interval_ms).GetMicroSeconds();
    ash.SetTimeSlot(current_slot_now);
    mach.SetSA(ash.GetSAddr());
    mach.SetDA(ash.GetDAddr());
    mach.SetDemuxPType(MacHeader::UWPTYPE_OTHER);
    // NS_LOG_UNCOND(" packet: " << ash.GetNetDataType() << " uid");
    Ptr<DolphinMobilityModel> mobility = Device()->GetNode()->GetObject<DolphinMobilityModel>();
    if (mobility)
    {
        // 获取节点的位置
        Vector position = mobility->GetIdealPosition();
        // NS_LOG_UNCOND("Node position: " << position);
        // NS_LOG_UNCOND("Current  position simulation time: " << Simulator::Now().GetSeconds() << "
        // seconds");
        loch.SetNodePosition(position);
    }
    else
    {
        NS_LOG_WARN("Node does not have a mobility model.");
    }
    p->AddHeader(loch);
    p->AddHeader(mach); // strictly to set demux packet type
    p->AddHeader(ash);
    // NS_LOG_UNCOND("Current simulation time: " << Simulator::Now().GetSeconds() << " seconds");
    // NS_LOG_UNCOND("send log ");
    // std::cout << "sendnode address: " <<
    // (AquaSimAddress::ConvertFrom(m_device->GetAddress())).GetAsInt() << std::endl;
    // packet->Print(std::cout);
    //  Just send packet down to PHY
    //  uint32_t packetSize_now = p->GetSize();
    //  m_tdma_data_length_now += packetSize_now;
    //  NS_LOG_UNCOND("packetsize: " << packetSize_now << " btye");
    tdma_mac_TxTrace(p,
                     Simulator::Now().GetSeconds(),
                     AquaSimAddress::ConvertFrom(m_device->GetAddress()));
    // std::cout << "m_net_packet_type: " << static_cast<int>(ash.GetNetDataType()) << std::endl;
    // p->Print(std::cout);
    SendDown(p);
    // Go to next packet in the queue
    LocPacketSend();
}

void
AquaSimTdmaMac::ScheduleNextSlotTx(Ptr<Packet> packet)
{
    // Get current slot
    uint32_t current_slot = Simulator::Now().GetMicroSeconds() /
                            (m_tdma_slot_ms + m_tdma_guard_interval_ms).GetMicroSeconds();
    Time packet_interval = MilliSeconds(50);
    // Time packet_interval1 = MilliSeconds(0);
    //  Iterate over a single TDMA-round to find next-slot transmission
    uint32_t next_slot = 0;
    // NS_LOG_UNCOND("Current max_data_number: " << m_tdma_data_length << " btye");
    // NS_LOG_UNCOND("Current tdma_slot_ms: " << m_tdma_slot_ms << " ");
    // NS_LOG_UNCOND("tdma_guard_interval_ms: " << m_tdma_guard_interval_ms << " ");
    //  NS_LOG_UNCOND("tdma_slot_period: " << m_tdma_slot_period << " number");
    AquaSimHeader ash;
    packet->PeekHeader(ash);
    int PacketType = static_cast<int>(ash.GetNetDataType());
    uint32_t packetSize_now = packet->GetSize();
    uint8_t numForwards = ash.GetNumForwards();
    if (PacketType == 0)
    {
        // packetSize_now = packetSize_now - 27 - 31 * numForwards;
        packetSize_now = data_pac_length;
        NS_LOG_UNCOND("packetSize1: " << packetSize_now << " 字节");
    }
    else
    {
        // packetSize_now = 9;
        packetSize_now = loc_pac_length;
    }
    m_tdma_data_length_now += packetSize_now;
    // NS_LOG_UNCOND("packetsize: " << packetSize_now << " btye");
    Time current_slot_time =
        Simulator::Now() - current_slot * (m_tdma_slot_ms + m_tdma_guard_interval_ms);
    // std::cout<<m_tdma_slot_period<<std::endl;

    if ((m_tdma_slot_list.contains(current_slot % m_tdma_slot_period)) &&
        (current_slot_time < m_tdma_slot_ms) && (m_tdma_data_length_now <= m_tdma_data_length))
    {
        //  if(current_slot_time == (m_tdma_slot_ms - m_tdma_slot_ms))
        //  {
        //     Simulator::Schedule(packet_interval, &AquaSimTdmaMac::SendPacketLOC, this);
        //     ScheduleNextSlotTx(p);
        //  }
        // NS_LOG_UNCOND("Current max_data_number: " << m_tdma_data_length << " btye");
        // NS_LOG_UNCOND("Current simulation time: " << Simulator::Now().GetSeconds() << "
        // seconds"); NS_LOG_UNCOND("send packet " ); Simulator::Schedule(packet_interval,
        // &AquaSimTdmaMac::SendPacket, this, packet->Copy()); uint32_t packetId = packet->GetUid();
        // NS_LOG_UNCOND("UID: " << packetId );
        // std::cout << "sendnode address: " <<
        // (AquaSimAddress::ConvertFrom(m_device->GetAddress())).GetAsInt() << std::endl;
        Simulator::Schedule(packet_interval, &AquaSimTdmaMac::SendPacket, this, packet->Copy());
    }
    else
    {
        // if(current_slot_time >= m_tdma_slot_ms){NS_LOG_UNCOND("Time slot transmission time has
        // ended. Packet transmission for this time slot is complete.: " );}
        // if(m_tdma_data_length_now >= m_tdma_data_length){NS_LOG_UNCOND("Data volume has reached
        // the limit. Packet transmission for this time slot is complete. " );}
        m_tdma_data_length_now = 0;
        m_tdma_data_length_now += packetSize_now; // 52是位置包的大小（字节）
        m_tdma_data_length_now += loc_pac_length; // 加上时隙开始的定位包
        for (uint32_t i = (current_slot + 1); i < (current_slot + 1 + m_tdma_slot_period); i++)
        {
            if (m_tdma_slot_list.contains(i % m_tdma_slot_period))
            {
                next_slot = i;
                break;
                // if((i%m_tdma_slot_period) == (m_tdma_slot_number+15))
                // {
                //   NS_LOG_UNCOND("next slot: +15" );
                // }
            }
        }
        // if (next_slot == 0)
        // {
        //   NS_FATAL_ERROR ("Couldn't find next TDMA slot!");
        // }
        // if (next_slot < current_slot)
        // {
        //   NS_FATAL_ERROR ("Next slot is smaller than current slot!");
        // }
        // NS_LOG_UNCOND("Time slot has started, sending location packet" );
        // Simulator::Schedule(next_slot * (m_tdma_slot_ms + m_tdma_guard_interval_ms) -
        // Simulator::Now(), &AquaSimTdmaMac::SendPacketLOC, this); std::cout << "sendnode address:
        // " << (AquaSimAddress::ConvertFrom(m_device->GetAddress())).GetAsInt()-1 << std::endl;
        // Schedule the packet transmission to that slot
        Simulator::Schedule(next_slot * (m_tdma_slot_ms + m_tdma_guard_interval_ms) -
                                Simulator::Now() + packet_interval,
                            &AquaSimTdmaMac::SendPacket,
                            this,
                            packet->Copy());
    }
}

void
AquaSimTdmaMac::LocPacketSend(void)
{
    uint32_t current_slot = Simulator::Now().GetMicroSeconds() /
                            (m_tdma_slot_ms + m_tdma_guard_interval_ms).GetMicroSeconds();
    uint32_t next_slot = 0;
    if ((Simulator::Now().GetSeconds() == 0) && (m_tdma_slot_list.contains(0)))
    {
        Simulator::Schedule(MilliSeconds(50), &AquaSimTdmaMac::SendPacketLOC, this);
    }
    // NS_LOG_UNCOND("Current max_data_number: " << m_tdma_data_length << " btye");
    // NS_LOG_UNCOND("Current tdma_slot_ms: " << m_tdma_slot_ms << " ");
    // NS_LOG_UNCOND("tdma_guard_interval_ms: " << m_tdma_guard_interval_ms << " ");
    //  NS_LOG_UNCOND("tdma_slot_period: " << m_tdma_slot_period << " number");
    // NS_LOG_UNCOND("packetsize: " << packetSize_now << " btye");
    // Time current_slot_time = Simulator::Now()- current_slot*(m_tdma_slot_ms +
    // m_tdma_guard_interval_ms);
    // std::cout<<m_tdma_slot_period<<std::endl;

    // if(current_slot_time >= m_tdma_slot_ms){NS_LOG_UNCOND("Time slot transmission time has ended.
    // Packet transmission for this time slot is complete.: " );} if(m_tdma_data_length_now >=
    // m_tdma_data_length){NS_LOG_UNCOND("Data volume has reached the limit. Packet transmission for
    // this time slot is complete. " );}
    else
    {
        for (uint32_t i = (current_slot + 1); i < (current_slot + 1 + m_tdma_slot_period); i++)
        {
            if ((m_tdma_slot_list.contains(i % m_tdma_slot_period)))
            {
                next_slot = i;
                // NS_LOG_UNCOND("next slot:"<< next_slot);
                break;
                // if((i%m_tdma_slot_period) == (m_tdma_slot_number+15))
                // {
                // NS_LOG_UNCOND("next slot:"<< next_slot);
                // }
            }
        }
        // if (next_slot == 0)
        // {
        //   NS_FATAL_ERROR ("Couldn't find next TDMA slot!");
        // }
        // if (next_slot < current_slot)
        // {
        //   NS_FATAL_ERROR ("Next slot is smaller than current slot!");
        // }
        // NS_LOG_UNCOND("Time slot has started, sending location packet" );
        // Simulator::Schedule(next_slot * (m_tdma_slot_ms + m_tdma_guard_interval_ms) -
        // Simulator::Now(), &AquaSimTdmaMac::SendPacketLOC, this); std::cout << "sendnode address:
        // " << (AquaSimAddress::ConvertFrom(m_device->GetAddress())).GetAsInt()-1 << std::endl;
        // Schedule the packet transmission to that slot
        // if(next_slot!=0)
        Simulator::Schedule(next_slot * (m_tdma_slot_ms + m_tdma_guard_interval_ms) -
                                Simulator::Now(),
                            &AquaSimTdmaMac::SendPacketLOC,
                            this);
    }
}

void
AquaSimTdmaMac::DoDispose()
{
    NS_LOG_FUNCTION(this);
    AquaSimMac::DoDispose();
}
} // namespace ns3