#ifndef DOLPHIN_PACKET_HEADER_H
#define DOLPHIN_PACKET_HEADER_H

#include <ns3/header.h>
#include <ns3/nstime.h>

namespace ns3
{

class Packet;

class DolphinPacketHeader : public Header
{
  public:
    enum PacketType
    {
        DATA = 0,
        CONTROL = 1
    };

    /// Creates an empty instance.
    DolphinPacketHeader();

    /**
     * Returns the object TypeId.
     * \return The object TypeId.
     */
    static TypeId GetTypeId();

    // Inherited from ObjectBase base class.
    TypeId GetInstanceTypeId() const override;

    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(Buffer::Iterator start) const;
    virtual uint32_t Deserialize(Buffer::Iterator start);
    virtual void Print(std::ostream& os) const;
    /**
     * \return The string representation of the header.
     */
    std::string ToString() const;

    void SetType(uint8_t type);
    void SetNextHopAddr(uint16_t addr);
    void SetSrcAddr(uint16_t addr);
    void SetDestAddr(uint16_t addr);
    void SetLength(uint16_t length);
    void SetTimestamp(uint32_t timestamp);
    void SetTxTime(uint64_t txTime);
    void SetUid(uint16_t uid);
    void SetDirection(uint8_t direction);
    void SetNumForwards(uint16_t numForwards);
    void SetSeq(uint32_t seq);
    void SetErrorFlag(uint8_t errorFlag);

    uint8_t GetType() const;
    uint16_t GetNextHopAddr() const;
    uint16_t GetSrcAddr() const;
    uint16_t GetDestAddr() const;
    uint16_t GetLength() const;
    uint32_t GetTimestamp() const;
    uint64_t GetTxTime() const;
    uint16_t GetUid() const;
    uint8_t GetDirection() const;
    uint16_t GetNumForwards() const;
    uint32_t GetSeq() const;
    uint8_t GetErrorFlag() const;

  private:
    uint8_t m_type;
    uint16_t m_nextHopAddr;
    uint16_t m_srcAddr;
    uint16_t m_destAddr;
    uint16_t m_length;
    uint32_t m_timestamp;
    uint64_t m_txTime;
    uint16_t m_uid;
    uint8_t m_direction;
    uint16_t m_numForwards;
    uint32_t m_seq;
    uint8_t m_errorFlag;
};

} // namespace ns3

#endif /* THREE_GPP_HTTP_HEADER_H */
