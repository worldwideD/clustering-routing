#ifndef DOLPHIN_APPLICATION_HEADER_H
#define DOLPHIN_APPLICATION_HEADER_H

#include <ns3/header.h>
#include <ns3/nstime.h>

namespace ns3
{

class Packet;

/**
 * \ingroup dolphin
 * \brief Header used by web browsing applications to transmit information about
 *        content type, content length and timestamps for delay statistics.
 *
 * The header contains the following fields (and their respective size when
 * serialized):
 *   - content type (2 bytes);
 *   - content length (4 bytes);
 *
 */
class DolphinApplicationHeader : public Header
{
  public:
    static const uint8_t code = 0x11;
    /// Creates an empty instance.
    DolphinApplicationHeader();

    /**
     * Returns the object TypeId.
     * \return The object TypeId.
     */
    static TypeId GetTypeId();

    // Inherited from ObjectBase base class.
    TypeId GetInstanceTypeId() const override;

    // Inherited from Header base class.
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * \return The string representation of the header.
     */
    std::string ToString() const;

    /**
     * \param code(const code value).
     */
    void SetCode(uint8_t code);

    /**
     * \return The ccode.
     */
    uint8_t GetCode() const;

    /**
     * \param contentType The content type.
     */
    void SetContentType(uint8_t contentType);

    /**
     * \return The content type.
     */
    uint8_t GetContentType() const;

    /**
     * \param contentLength The content length (in bytes).
     */
    void SetContentLength(uint32_t contentLength);

    /**
     * \return The content length (in bytes).
     */
    uint32_t GetContentLength() const;

  private:
    uint8_t m_code;           //!<" Code field.
    uint8_t m_contentType;    //!<" Content type field.
    uint32_t m_contentLength; //!<" Content length field (in bytes unit).
}; // end of `class DolphinApplicationHeader`

} // namespace ns3

#endif /* THREE_GPP_HTTP_HEADER_H */
