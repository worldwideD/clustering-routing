/*
 * dolphin applictaion helper
 *
 *  Created on: 9/2024
 *
 *
 */

#include "dolphin-application-header.h"

#include <ns3/log.h>
#include <ns3/packet.h>

#include <sstream>

NS_LOG_COMPONENT_DEFINE("DolphinApplicationHeader");

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(DolphinApplicationHeader);

DolphinApplicationHeader::DolphinApplicationHeader()
    : Header(),
      m_code(code),
      m_contentType(0),
      m_contentLength(0)
{
    NS_LOG_FUNCTION(this);
}

// static
TypeId
DolphinApplicationHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DolphinApplicationHeader")
                            .SetParent<Header>()
                            .AddConstructor<DolphinApplicationHeader>();
    return tid;
}

TypeId
DolphinApplicationHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
DolphinApplicationHeader::GetSerializedSize() const
{
    return 1 + 1 + 4;
}

void
DolphinApplicationHeader::Serialize(Buffer::Iterator start) const
{
    NS_LOG_FUNCTION(this << &start);
    start.WriteU8(m_code);
    start.WriteU8(m_contentType);
    start.WriteU32(m_contentLength);
}

uint32_t
DolphinApplicationHeader::Deserialize(Buffer::Iterator start)
{
    NS_LOG_FUNCTION(this << &start);
    uint32_t bytesRead = 0;

    // First block of 2 bytes (code and content type)
    m_code = start.ReadU8();
    bytesRead += 1;
    m_contentType = start.ReadU8();
    bytesRead += 1;

    // Second block of 4 bytes (content length)
    m_contentLength = start.ReadU32();
    bytesRead += 4;

    return bytesRead;
}

void
DolphinApplicationHeader::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    NS_LOG_FUNCTION("(Content-Type: " << m_contentType << " Content-Length: " << m_contentLength
                                      << ")");
}

std::string
DolphinApplicationHeader::ToString() const
{
    NS_LOG_FUNCTION(this);
    std::ostringstream oss;
    Print(oss);
    return oss.str();
}

void
DolphinApplicationHeader::SetCode(uint8_t code)
{
    NS_LOG_FUNCTION(this << static_cast<uint8_t>(code));
    m_code = code;
}

uint8_t
DolphinApplicationHeader::GetCode() const
{
    return m_code;
}

void
DolphinApplicationHeader::SetContentType(uint8_t contentType)
{
    NS_LOG_FUNCTION(this << static_cast<uint8_t>(contentType));
    m_contentType = contentType;
}

uint8_t
DolphinApplicationHeader::GetContentType() const
{
    return m_contentType;
}

void
DolphinApplicationHeader::SetContentLength(uint32_t contentLength)
{
    NS_LOG_FUNCTION(this << contentLength);
    m_contentLength = contentLength;
}

uint32_t
DolphinApplicationHeader::GetContentLength() const
{
    return m_contentLength;
}

} // namespace ns3
