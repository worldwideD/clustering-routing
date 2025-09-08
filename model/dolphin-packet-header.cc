/*
 * dolphin packet header
 *
 *  Created on: 9/2024
 *
 *
 */

#include "dolphin-packet-header.h"

#include <ns3/log.h>
#include <ns3/packet.h>

#include <sstream>

NS_LOG_COMPONENT_DEFINE("DolphinPacketHeader");

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(DolphinPacketHeader);

DolphinPacketHeader::DolphinPacketHeader()
    : m_type(DATA),
      m_nextHopAddr(255),
      m_srcAddr(0),
      m_destAddr(0),
      m_length(0),
      m_timestamp(0),
      m_txTime(0),
      m_uid(0),
      m_direction(0),
      m_numForwards(0),
      m_seq(0),
      m_errorFlag(0)
{
}

// static
TypeId
DolphinPacketHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DolphinPacketHeader")
                            .SetParent<Header>()
                            .AddConstructor<DolphinPacketHeader>();
    return tid;
}

TypeId
DolphinPacketHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

std::string
DolphinPacketHeader::ToString() const
{
    NS_LOG_FUNCTION(this);
    std::ostringstream oss;
    Print(oss);
    return oss.str();
}

void
DolphinPacketHeader::SetType(uint8_t type)
{
    m_type = type;
}

void
DolphinPacketHeader::SetNextHopAddr(uint16_t addr)
{
    m_nextHopAddr = addr;
}

void
DolphinPacketHeader::SetSrcAddr(uint16_t addr)
{
    m_srcAddr = addr;
}

void
DolphinPacketHeader::SetDestAddr(uint16_t addr)
{
    m_destAddr = addr;
}

void
DolphinPacketHeader::SetLength(uint16_t length)
{
    m_length = length;
}

void
DolphinPacketHeader::SetTimestamp(uint32_t timestamp)
{
    m_timestamp = timestamp;
}

void
DolphinPacketHeader::SetTxTime(uint64_t txTime)
{
    m_txTime = txTime;
}

void
DolphinPacketHeader::SetUid(uint16_t uid)
{
    m_uid = uid;
}

void
DolphinPacketHeader::SetDirection(uint8_t direction)
{
    m_direction = direction;
}

void
DolphinPacketHeader::SetNumForwards(uint16_t numForwards)
{
    m_numForwards = numForwards;
}

void
DolphinPacketHeader::SetSeq(uint32_t seq)
{
    m_seq = seq;
}

void
DolphinPacketHeader::SetErrorFlag(uint8_t errorFlag)
{
    m_errorFlag = errorFlag;
}

uint8_t
DolphinPacketHeader::GetType() const
{
    return m_type;
}

uint16_t
DolphinPacketHeader::GetNextHopAddr() const
{
    return m_nextHopAddr;
}

uint16_t
DolphinPacketHeader::GetSrcAddr() const
{
    return m_srcAddr;
}

uint16_t
DolphinPacketHeader::GetDestAddr() const
{
    return m_destAddr;
}

uint16_t
DolphinPacketHeader::GetLength() const
{
    return m_length;
}

uint32_t
DolphinPacketHeader::GetTimestamp() const
{
    return m_timestamp;
}

uint64_t
DolphinPacketHeader::GetTxTime() const
{
    return m_txTime;
}

uint16_t
DolphinPacketHeader::GetUid() const
{
    return m_uid;
}

uint8_t
DolphinPacketHeader::GetDirection() const
{
    return m_direction;
}

uint16_t
DolphinPacketHeader::GetNumForwards() const
{
    return m_numForwards;
}

uint32_t
DolphinPacketHeader::GetSeq() const
{
    return m_seq;
}

uint8_t
DolphinPacketHeader::GetErrorFlag() const
{
    return m_errorFlag;
}

uint32_t
DolphinPacketHeader::GetSerializedSize(void) const
{
    return 1 + 2 + 2 + 2 + 2 + 4 + 8 + 2 + 1 + 2 + 4 + 1;
}

void
DolphinPacketHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteU8(m_type);
    start.WriteHtonU16(m_nextHopAddr);
    start.WriteHtonU16(m_srcAddr);
    start.WriteHtonU16(m_destAddr);
    start.WriteHtonU16(m_length);
    start.WriteHtonU32(m_timestamp);
    start.WriteHtonU64(m_txTime);
    start.WriteHtonU16(m_uid);
    start.WriteU8(m_direction);
    start.WriteHtonU16(m_numForwards);
    start.WriteHtonU32(m_seq);
    start.WriteU8(m_errorFlag);
}

uint32_t
DolphinPacketHeader::Deserialize(Buffer::Iterator start)
{
    m_type = start.ReadU8();
    m_nextHopAddr = start.ReadNtohU16();
    m_srcAddr = start.ReadNtohU16();
    m_destAddr = start.ReadNtohU16();
    m_length = start.ReadNtohU16();
    m_timestamp = start.ReadNtohU32();
    m_txTime = start.ReadNtohU64();
    m_uid = start.ReadNtohU16();
    m_direction = start.ReadU8();
    m_numForwards = start.ReadNtohU16();
    m_seq = start.ReadNtohU32();
    m_errorFlag = start.ReadU8();
    return GetSerializedSize();
}

void
DolphinPacketHeader::Print(std::ostream& os) const
{
    os << "Type=" << (uint32_t)m_type << ", NextHopAddr=" << m_nextHopAddr
       << ", SrcAddr=" << m_srcAddr << ", DestAddr=" << m_destAddr << ", Length=" << m_length
       << ", Timestamp=" << m_timestamp << ", TxTime=" << m_txTime << ", Uid=" << m_uid
       << ", Direction=" << (uint32_t)m_direction << ", NumForwards=" << m_numForwards
       << ", Seq=" << m_seq << ", ErrorFlag=" << (uint32_t)m_errorFlag;
}

} // namespace ns3