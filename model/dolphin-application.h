#ifndef DOLPHIN_APPLICATION_H
#define DOLPHIN_APPLICATION_H

#include "dolphin_trace.h"

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class Address;
class RandomVariableStream;
class Socket;

class DolphinApplication : public Application
{
  public:
    static const uint32_t broadcastId = 255;

    enum RxTxType
    {
        DEFAULT = 1 << 0,
        RECV = 1 << 1,
        SEND_UNICAST = 1 << 2,
        SEND_BROADCAST = 1 << 3
    };

    enum PacketType
    {
        UNKNOWN = 0,
        TASK,        // 1
        ACK,         // 2
        DATA,        // 3
        NODEDOWEROFF // 4
    };

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    DolphinApplication();

    virtual ~DolphinApplication();

    /**
     * \brief Set the total number of bytes to send.
     *
     * Once these bytes are sent, no packet is sent again, even in on state.
     * The value zero means that there is no limit.
     *
     * \param maxBytes the total number of bytes to send
     */
    void SetMaxBytes(uint64_t maxBytes);

    /**
     * \brief Return a pointer to associated socket.
     * \return pointer to associated socket
     */
    Ptr<Socket> GetSocket(void) const;

    /**
     * \brief Assign a fixed random variable stream number to the random variables
     * used by this model.
     *
     * \param stream first stream index to use
     * \return the number of stream indices assigned by this model
     */
    int64_t AssignStreams(int64_t stream);

    void SetDataSize(uint32_t dataSize);
    uint32_t GetDataSize() const;

    void SetFill(uint8_t fill, uint32_t dataSize);
    void SetFill(uint8_t* fill, uint32_t fillSize, uint32_t dataSize);

    void RecvPacket(Ptr<Socket> socket);
    void SendPacketByPayload(uint8_t* buffer, uint32_t size);

  protected:
    virtual void DoDispose(void);

  private:
    // inherited from Application base class.
    virtual void StartApplication(void); // Called at time specified by Start
    virtual void StopApplication(void);  // Called at time specified by Stop

    // helpers
    /**
     * \brief Cancel all pending events.
     */
    void CancelEvents();

    // Event handlers
    /**
     * \brief Start an On period
     */
    void StartSending();
    /**
     * \brief Start an Off period
     */
    void StopSending();
    /**
     * \brief Send a packet
     */
    void SendPacket();

    // Flag to initialize socket-dst map
    bool m_socket_init;
    // List of sockets for each destination
    std::vector<Ptr<Socket>> m_socket_list;
    std::vector<int> recv_ack;
    bool m_recv_all_ack;
    Ptr<Socket> m_recvsocket;

    // List of destination nodes
    std::vector<uint32_t> m_peer_list;
    std::string m_peer_list_string;

    Ptr<RandomVariableStream> m_onTime;  //!< rng for On Time
    Ptr<RandomVariableStream> m_offTime; //!< rng for Off Time
    DataRate m_cbrRate;                  //!< Rate that data is generated
    DataRate m_cbrRateFailSafe;          //!< Rate that data is generated (check copy)
    uint16_t m_contentType;              //!< Packet content type(TASK\DATA\ACK)
    uint32_t m_payloadSize;              //!< Size of packets
    uint8_t* m_payloadData;              //!< Payload data
    uint32_t m_residualBits;             //!< Number of generated, but not sent, bits
    Time m_lastStartTime;                //!< Time last packet sent
    uint64_t m_maxBytes;                 //!< Limit total number of bytes sent
    uint64_t m_totBytes;                 //!< Total bytes sent so far
    EventId m_startStopEvent;            //!< Event id for next start or stop event
    EventId m_sendEvent;                 //!< Event id of pending "send packet" event
    TypeId m_tid;                        //!< Type of the socket used
    double m_sendIntervalS;              // interval to send command
    uint64_t m_rxTxType;                 // enum type

    uint32_t m_numberOfDestinations = 0;

    /// Traced Callback: transmitted packets.
    TracedCallback<Ptr<const Packet>, uint64_t, uint64_t, uint8_t> m_txTrace;
    TracedCallback<Ptr<const Packet>, uint64_t, uint64_t, uint8_t> m_rxTrace;

  private:
    /**
     * \brief Schedule the next packet transmission
     */
    void ScheduleNextTx();
    /**
     * \brief Schedule the next On period start
     */
    void ScheduleStartEvent();
    /**
     * \brief Schedule the next Off period start
     */
    void ScheduleStopEvent();
    /**
     * \brief Handle a Connection Succeed event
     * \param socket the connected socket
     */
    void ConnectionSucceeded(Ptr<Socket> socket);
    /**
     * \brief Handle a Connection Failed event
     * \param socket the not connected socket
     */
    void ConnectionFailed(Ptr<Socket> socket);

    std::vector<uint32_t> ParseList(std::string input);
    int SendTo(Ptr<Packet> packet, uint32_t dst);
};

} // namespace ns3

#endif /* AQUA_SIM_APPLICATION_H */
