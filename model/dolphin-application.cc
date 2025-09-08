/*
 * dolphin applictaion
 *
 *  Created on: 9/2024
 *
 *
 */

#include "dolphin-application.h"

#include "aqua-sim-net-device.h"

#include "ns3/address.h"
#include "ns3/aqua-sim-header.h"
#include "ns3/data-rate.h"
#include "ns3/dolphin-application-header.h"
#include "ns3/dolphin-packet-header.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/packet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"
#include <ns3/double.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DolphinApplication");

NS_OBJECT_ENSURE_REGISTERED(DolphinApplication);

TypeId
DolphinApplication::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::DolphinApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<DolphinApplication>()
            .AddAttribute("DataRate",
                          "The data rate in on state.",
                          DataRateValue(DataRate("5b/s")),
                          MakeDataRateAccessor(&DolphinApplication::m_cbrRate),
                          MakeDataRateChecker())
            .AddAttribute("PacketSize",
                          "The size of packets sent in on state",
                          UintegerValue(512),
                          MakeUintegerAccessor(&DolphinApplication::SetDataSize,
                                               &DolphinApplication::GetDataSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("ContentType",
                          "The content type(TASK or DATA or ACK) of packet",
                          UintegerValue(DolphinApplication::PacketType::DATA),
                          MakeUintegerAccessor(&DolphinApplication::m_contentType),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("RemoteDestinationList",
                          "List of the destination addresses",
                          StringValue(""),
                          MakeStringAccessor(&DolphinApplication::m_peer_list_string),
                          MakeStringChecker())
            .AddAttribute("NumberOfDestinations",
                          "Number of destinations",
                          UintegerValue(),
                          MakeUintegerAccessor(&DolphinApplication::m_numberOfDestinations),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("OnTime",
                          "A RandomVariableStream used to pick the duration of the 'On' state.",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&DolphinApplication::m_onTime),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("OffTime",
                          "A RandomVariableStream used to pick the duration of the 'Off' state.",
                          StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
                          MakePointerAccessor(&DolphinApplication::m_offTime),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("MaxBytes",
                          "The total number of bytes to send. Once these bytes are sent, "
                          "no packet is sent again, even in on state. The value zero means "
                          "that there is no limit.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&DolphinApplication::m_maxBytes),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("Protocol",
                          "The type of protocol to use. This should be "
                          "a subclass of ns3::SocketFactory",
                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&DolphinApplication::m_tid),
                          // This should check for SocketFactory as a parent
                          MakeTypeIdChecker())
            .AddTraceSource("Tx",
                            "APP:A new packet is created and is sent",
                            MakeTraceSourceAccessor(&DolphinApplication::m_txTrace),
                            "DolphinApplication::TxCallback")
            .AddTraceSource("Rx",
                            "APP:A new packet is created and is received",
                            MakeTraceSourceAccessor(&DolphinApplication::m_rxTrace),
                            "DolphinApplication::RxCallback")
            .AddAttribute("SendIntervalS",
                          "interval to send data",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&DolphinApplication::m_sendIntervalS),
                          MakeDoubleChecker<double>())
            .AddAttribute("RxTxType",
                          "send(unicast\broadcast) or recv",
                          UintegerValue(DEFAULT),
                          MakeUintegerAccessor(&DolphinApplication::m_rxTxType),
                          MakeUintegerChecker<uint64_t>());
    return tid;
}

DolphinApplication::DolphinApplication()
    : m_socket_init(false),
      m_recv_all_ack(false),
      m_residualBits(0),
      m_lastStartTime(Seconds(0)),
      m_totBytes(0)
{
    NS_LOG_FUNCTION(this);
    m_payloadData = nullptr;
    m_payloadSize = 0;
}

DolphinApplication::~DolphinApplication()
{
    NS_LOG_FUNCTION(this);
    delete[] m_payloadData;
    m_payloadData = nullptr;
    m_payloadSize = 0;
}

void
DolphinApplication::SetDataSize(uint32_t dataSize)
{
    delete[] m_payloadData;
    m_payloadData = nullptr;
    m_payloadSize = dataSize;
}

uint32_t
DolphinApplication::GetDataSize() const
{
    NS_LOG_FUNCTION(this);
    return m_payloadSize;
}

void
DolphinApplication::SetFill(uint8_t fill, uint32_t dataSize)
{
    NS_LOG_FUNCTION(this << fill << dataSize);

    delete[] m_payloadData;
    m_payloadData = new uint8_t[dataSize];
    m_payloadSize = dataSize;

    memset(m_payloadData, fill, dataSize);
}

void
DolphinApplication::SetFill(uint8_t* fill, uint32_t fillSize, uint32_t dataSize)
{
    NS_LOG_FUNCTION(this << fill << fillSize << dataSize);

    delete[] m_payloadData;
    m_payloadData = new uint8_t[dataSize];
    m_payloadSize = dataSize;

    if (fillSize >= dataSize)
    {
        memcpy(m_payloadData, fill, dataSize);
        m_payloadSize = dataSize;
        return;
    }

    //
    // Do all but the final fill.
    //
    uint32_t filled = 0;
    while (filled + fillSize < dataSize)
    {
        memcpy(&m_payloadData[filled], fill, fillSize);
        filled += fillSize;
    }

    //
    // Last fill may be partial
    //
    memcpy(&m_payloadData[filled], fill, dataSize - filled);
}

std::vector<uint32_t>
DolphinApplication::ParseList(std::string input)
{
    std::vector<uint32_t> list;
    std::istringstream iss(input);
    std::string item;
    while (std::getline(iss, item, ','))
    {
        uint32_t peer = std::stoi(item);
        list.push_back(peer);
    }
    return list;
}

void
DolphinApplication::SetMaxBytes(uint64_t maxBytes)
{
    NS_LOG_FUNCTION(this << maxBytes);
    m_maxBytes = maxBytes;
}

Ptr<Socket>
DolphinApplication::GetSocket(void) const
{
    NS_LOG_FUNCTION(this);
    for (auto it = m_socket_list.begin(); it != m_socket_list.end(); ++it)
    {
        if ((*it) != NULL)
        {
            return (*it);
        }
    }
    return nullptr;
}

int64_t
DolphinApplication::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_onTime->SetStream(stream);
    m_offTime->SetStream(stream + 1);
    return 2;
}

void
DolphinApplication::DoDispose(void)
{
    NS_LOG_FUNCTION(this);

    m_recvsocket = nullptr;
    // chain up
    Application::DoDispose();
}

void
DolphinApplication::RecvPacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    while (packet = socket->Recv())
    {
        AquaSimHeader ash;
        packet->PeekHeader(ash);
        AquaSimAddress srcAddr = ash.GetSAddr();
        uint16_t srcId = srcAddr.GetAsInt();
        uint16_t n_forwards = ash.GetNumForwards();
        // for (int i = 0; i <= n_forwards; i++)
        {
            packet->RemoveHeader(ash);
        }

        // remove common header
        // DolphinPacketHeader commonHeader;
        // packet->RemoveHeader(commonHeader);

        // get application header
        DolphinApplicationHeader appHeader;
        uint32_t packetSize = packet->GetSize();
        packet->PeekHeader(appHeader);
        uint32_t packetId = packet->GetUid();

        switch (appHeader.GetContentType())
        {
        case PacketType::ACK: {
            if ((GetNode()->GetId() + 1) == srcId)
            {
                return;
            }
            Ptr<Packet> payloadPacket = packet->Copy();
            payloadPacket->RemoveHeader(appHeader);
            uint8_t payloadReading;
            payloadPacket->CopyData(&payloadReading, 1);
            if (payloadReading > m_numberOfDestinations)
            {
                break;
            }
            recv_ack[payloadReading] = 1;
            if (!m_recv_all_ack)
            {
                uint32_t s = 0;
                for (uint16_t i = 0; i < m_numberOfDestinations; i++)
                {
                    if (recv_ack[i] == 1)
                        s++;
                }
                if (s == (m_numberOfDestinations - 1))
                {
                    m_recv_all_ack = true;
                    NS_LOG_FUNCTION("recv all acks");
                }
            }
            m_rxTrace(packet,
                      payloadReading + 1,
                      GetNode()->GetId() + 1,
                      DolphinApplication::PacketType::ACK);

            NS_LOG_FUNCTION("Node"
                            << GetNode()->GetId() + 1 << "Recv an ACK" << "(UID=" << packetId << ")"
                            << "of size" << packetSize << "from Node " << payloadReading + 1);
        }
        break;
        case PacketType::DATA: {
            if ((GetNode()->GetId() + 1) == srcId)
            {
                return;
            }
            m_rxTrace(packet, srcId, GetNode()->GetId() + 1, appHeader.GetContentType());
            NS_LOG_FUNCTION("Node" << GetNode()->GetId() + 1 << "Recv a packet"
                                   << "(UID=" << packetId << ")"
                                   << "of size" << packetSize << ")");
        }
        break;
        case PacketType::TASK: {
            if ((GetNode()->GetId() + 1) == srcId)
            {
                return;
            }
            Ptr<Packet> payloadPacket = packet->Copy();
            payloadPacket->RemoveHeader(appHeader);
            uint8_t payloadReading; // send node id(leader id)
            payloadPacket->CopyData(&payloadReading, 1);

            m_rxTrace(packet,
                      payloadReading + 1,
                      GetNode()->GetId() + 1,
                      appHeader.GetContentType());

            uint32_t packetId = packet->GetUid();
            uint8_t payload = GetNode()->GetId();
            Ptr<Packet> ackPacket = Create<Packet>(&payload, 1);
            DolphinApplicationHeader ackAppHeader;
            ackAppHeader.SetContentType(PacketType::ACK);
            ackAppHeader.SetContentLength(1);
            ackPacket->AddHeader(ackAppHeader);
            m_txTrace(ackPacket,
                      GetNode()->GetId() + 1,
                      payloadReading + 1,
                      DolphinApplication::PacketType::ACK);
            NS_LOG_FUNCTION(this << "Node" << GetNode()->GetId() + 1 << "Recv a packet"
                                 << "(UID=" << packetId << ")"
                                 << "of size" << packetSize << "and return ACK(payload:" << payload
                                 << ")");

            SendTo(ackPacket, payloadReading + 1);
        }
        break;
        default:
            break;
        }
    }
}

// Application Methods
void
DolphinApplication::StartApplication() // Called at time specified by Start
{
    NS_LOG_FUNCTION(this);
    m_peer_list = ParseList(m_peer_list_string);
    recv_ack = std::vector<int>(m_numberOfDestinations, 0);
    if (!m_socket_init)
    {
        uint8_t m_address[2];
        m_socket_init = true;
        memset(m_address, 0, 2);
        // Create a socket for each destination in list
        for (uint16_t i = 1; i <= m_numberOfDestinations; i++)
        {
            if (i != (GetNode()->GetId() + 1))
            {
                Ptr<Socket> socket = Socket::CreateSocket(GetNode(), m_tid);

                m_address[0] = (i >> 8) & 0xff;
                m_address[1] = (i >> 0) & 0xff;

                Address address = Address(2, m_address, 2);

                PacketSocketAddress packet_socket;
                packet_socket.SetAllDevices();
                packet_socket.SetPhysicalAddress(address);
                packet_socket.SetProtocol(0);

                if (Inet6SocketAddress::IsMatchingType(Address(packet_socket)))
                {
                    if (socket->Bind6() == -1)
                    {
                        NS_FATAL_ERROR("Failed to bind socket");
                    }
                }
                else if (InetSocketAddress::IsMatchingType(Address(packet_socket)) ||
                         PacketSocketAddress::IsMatchingType(Address(packet_socket)))
                {
                    if (socket->Bind() == -1)
                    {
                        NS_FATAL_ERROR("Failed to bind socket");
                    }
                }

                socket->Connect(Address(packet_socket));
                socket->SetAllowBroadcast(true);
                socket->ShutdownRecv();

                socket->SetConnectCallback(
                    MakeCallback(&DolphinApplication::ConnectionSucceeded, this),
                    MakeCallback(&DolphinApplication::ConnectionFailed, this));

                // socket -> SetRecvCallback (MakeCallback (&DolphinApplication::RecvPacket, this));

                m_socket_list.push_back(socket);
            }
            else if (m_rxTxType & RECV)
            {
                m_address[0] = (i >> 8) & 0xff;
                m_address[1] = (i >> 0) & 0xff;

                Address address = Address(2, m_address, 2);

                PacketSocketAddress packet_socket;
                packet_socket.SetAllDevices();
                packet_socket.SetPhysicalAddress(address);
                packet_socket.SetProtocol(0);

                TypeId psfid = TypeId::LookupByName("ns3::PacketSocketFactory");
                m_recvsocket = Socket::CreateSocket(GetNode(), psfid);
                m_recvsocket->Bind(packet_socket);
                m_recvsocket->SetRecvCallback(MakeCallback(&DolphinApplication::RecvPacket, this));
            }
        }
    }

    m_cbrRateFailSafe = m_cbrRate;

    // Insure no pending event
    CancelEvents();

    if (m_rxTxType & SEND_BROADCAST || m_rxTxType & SEND_UNICAST)
    {
        if (!m_peer_list.empty())
            ScheduleStartEvent();
    }
}

void
DolphinApplication::SendPacketByPayload(uint8_t* buffer, uint32_t size)
{
    Ptr<Socket> sendSocket = GetSocket();
    if (sendSocket != nullptr)
    {
        Ptr<Packet> packet = Create<Packet>(buffer, size);
        uint32_t packetId = packet->GetUid();
        int ret = sendSocket->Send(packet);
        NS_LOG_FUNCTION("Node" << GetNode()->GetId() + 1 << "Send a packet to start"
                               << "(UID=" << packetId << ")"
                               << "of size" << packet->GetSize() << ",ret:" << ret);
    }
}

void
DolphinApplication::SendPacket()
{
    NS_LOG_FUNCTION(this);

    if (m_payloadData == nullptr)
    {
        SetFill(GetNode()->GetId(), m_payloadSize);
    }
    Ptr<Packet> packet = Create<Packet>(m_payloadData, m_payloadSize);
    DolphinApplicationHeader appHeader;
    appHeader.SetContentType(m_contentType);
    appHeader.SetContentLength(m_payloadSize);
    packet->AddHeader(appHeader);

    for (auto it = m_peer_list.begin(); it != m_peer_list.end(); ++it)
    {
        uint32_t peer = *it;
        if (peer >= 0)
        {
            peer = ((peer + 1) >= 255) ? 255 : (peer + 1);
            NS_LOG_FUNCTION("At time " << Simulator::Now().GetSeconds() << ",node("
                                       << GetNode()->GetId() + 1 << ")s application sent "
                                       << packet->GetSize() << " bytes to " << peer << " total Tx "
                                       << m_totBytes << " bytes");
            m_txTrace(packet, GetNode()->GetId() + 1, peer, m_contentType);

            int ret = SendTo(packet, peer);
            if (ret < 0)
            {
                NS_LOG_ERROR("At time " << Simulator::Now().GetSeconds() << "s application sent "
                                        << packet->GetSize() << " bytes to " << peer << " total Tx "
                                        << m_totBytes << " bytes Error!");
            }
        }
    }

    m_totBytes += m_payloadSize;

    m_lastStartTime = Simulator::Now();
    m_residualBits = 0;
    ScheduleNextTx();
}

void
DolphinApplication::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);

    if (m_rxTxType & RECV)
    {
        Ptr<NetDevice> device = GetNode()->GetDevice(0);
        if (device != nullptr)
        {
            Ptr<AquaSimNetDevice> asDevice = DynamicCast<AquaSimNetDevice>(device);
            asDevice->PowerOff();
            uint8_t payload = GetNode()->GetId() + 1;
            Ptr<Packet> packet = Create<Packet>(&payload, 1);
            m_txTrace(packet,
                      GetNode()->GetId() + 1,
                      GetNode()->GetId() + 1,
                      DolphinApplication::PacketType::NODEDOWEROFF);
        }
    }

    CancelEvents();
    if (m_socket_init != 0)
    {
        for (auto it = m_socket_list.begin(); it != m_socket_list.end(); ++it)
        {
            if ((*it))
                (*it)->Close();
        }
        m_socket_list.clear();
        if (m_recvsocket)
        {
            m_recvsocket->Close();
        }
    }
    else
    {
        NS_LOG_WARN("DolphinApplication found null socket to close in StopApplication");
    }
}

void
DolphinApplication::CancelEvents()
{
    NS_LOG_FUNCTION(this);

    if (m_sendEvent.IsRunning() && m_cbrRateFailSafe == m_cbrRate)
    { // Cancel the pending send packet event
        // Calculate residual bits since last packet sent
        Time delta(Simulator::Now() - m_lastStartTime);
        int64x64_t bits = delta.To(Time::S) * m_cbrRate.GetBitRate();
        m_residualBits += bits.GetHigh();
    }
    m_cbrRateFailSafe = m_cbrRate;
    Simulator::Cancel(m_sendEvent);
    Simulator::Cancel(m_startStopEvent);
}

// Event handlers
void
DolphinApplication::StartSending()
{
    NS_LOG_FUNCTION(this);
    m_lastStartTime = Simulator::Now();
    SendPacket();
    // ScheduleNextTx(); // Schedule the send packet event
    //     ScheduleStopEvent();
}

void
DolphinApplication::StopSending()
{
    NS_LOG_FUNCTION(this);
    CancelEvents();

    ScheduleStartEvent();
}

// Private helpers
void
DolphinApplication::ScheduleNextTx()
{
    NS_LOG_FUNCTION(this);
    if (m_contentType == TASK && m_recv_all_ack)
    {
        NS_LOG_FUNCTION("Node Has recevied all acks,stop to send broadcast packet");
        return;
    }

    if (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
        if (m_peer_list.empty())
            return;
        uint32_t bits = m_payloadSize * 8 - m_residualBits;
        NS_LOG_LOGIC("bits = " << bits);
        double bitRate = static_cast<double>(m_cbrRate.GetBitRate());
        double nextTimeS;
        if (bitRate == 0)
        {
            nextTimeS = m_sendIntervalS;
        }
        else
        {
            nextTimeS = bits / bitRate;
        }
        Time nextTime(Seconds(nextTimeS)); // Time till next packet
        NS_LOG_LOGIC("nextTime = " << nextTime);
        m_sendEvent = Simulator::Schedule(nextTime, &DolphinApplication::SendPacket, this);
    }
    else
    { // All done, cancel any pending events
        if (m_rxTxType == SEND)
            StopApplication();
        // CancelEvents();
    }
}

void
DolphinApplication::ScheduleStartEvent()
{ // Schedules the event to start sending data (switch to the "On" state)
    NS_LOG_FUNCTION(this);

    Time offInterval = Seconds(m_offTime->GetValue());
    NS_LOG_LOGIC("start at " << offInterval);
    m_startStopEvent = Simulator::Schedule(offInterval, &DolphinApplication::StartSending, this);
}

void
DolphinApplication::ScheduleStopEvent()
{ // Schedules the event to stop sending data (switch to "Off" state)
    NS_LOG_FUNCTION(this);

    Time onInterval = Seconds(m_onTime->GetValue());
    NS_LOG_LOGIC("stop at " << onInterval);
    m_startStopEvent = Simulator::Schedule(onInterval, &DolphinApplication::StopSending, this);
}

void
DolphinApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
DolphinApplication::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    auto it = std::find(m_socket_list.begin(), m_socket_list.end(), socket);
    if (it != m_socket_list.end())
    {
        m_socket_list.erase(it);
    }
}

int
DolphinApplication::SendTo(Ptr<Packet> packet, uint32_t dst)
{
    // set common header
    // DolphinPacketHeader commonHeader;
    // packet->AddHeader(commonHeader);
    Ptr<NetDevice> device = GetNode()->GetDevice(0);
    if (device != nullptr)
    {
        Address device_address;
        device_address = device->GetAddress();

        uint8_t dstAddr[2];
        dstAddr[0] = (dst >> 8) & 0xff;
        dstAddr[1] = (dst >> 0) & 0xff;
        Address sendto_address = Address(device_address); // Address(2, dstAddr, 2);
        sendto_address.CopyFrom(dstAddr, 2);

        PacketSocketAddress packet_socket;
        packet_socket.SetAllDevices();
        packet_socket.SetPhysicalAddress(sendto_address);
        packet_socket.SetProtocol(0);
        Ptr<Socket> sendSocket = GetSocket();
        return sendSocket->SendTo(packet, 0, Address(packet_socket));
    }
    return 0;
}

} // Namespace ns3
