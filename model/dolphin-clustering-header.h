#ifndef DOLPHIN_ROUTING_LEACH
#define DOLPHIN_ROUTING_LEACH_H

#include "aqua-sim-routing.h"
#include "aqua-sim-header.h"

namespace ns3 {
    
class DolphinClusteringHelloHeader: public Header {
public:
    DolphinClusteringHelloHeader();
    virtual ~DolphinClusteringHelloHeader();
    static TypeId GetTypeId();
    
    int Size();
    
    //Setters
    void SetPosition(Vector pos);
    void SetDirection(Vector dir);
    void SetSpeed(double s);
    void SetTimeStamp(Time s);
    void SetWeight(double w);
    void SetCHFlag(uint8_t flag);
    void SetNodeTyp(uint8_t typ);
    void SetRange(uint16_t range);
    
    //Getters
    Vector GetPosition();
    Vector GetDirection();
    double GetSpeed();
    Time GetTimeStamp();
    double GetWeight();
    uint8_t GetCHFlag();
    uint8_t GetNodeTyp();
    uint16_t GetRange();
    
    //inherited methods
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize (Buffer::Iterator start) const;
    virtual uint32_t Deserialize (Buffer::Iterator start);
    virtual void Print (std::ostream &os) const;
    virtual TypeId GetInstanceTypeId(void) const;
private:
    Vector position;    // 节点位置
    Vector direction;   // 节点运动方向
    double speed;       // 节点速度
    Time timestamp;     // 运动状态的i时间戳
    double weight;      // 节点权值
    uint8_t chFlag;     // 节点是否为簇头
    uint8_t node_typ;  // 节点类型 (长或短)
    uint16_t node_range;// 节点传输距离
};  // class DolphinClusteringHelloHeader

class DolphinClusteringAnnouncementHeader: public Header {
public:
    DolphinClusteringAnnouncementHeader();
    virtual ~DolphinClusteringAnnouncementHeader();
    static TypeId GetTypeId();
        
    int Size();
        
    //Setters
    void SetCHID(AquaSimAddress id);
    void SetNodeTyp(uint8_t typ);
        
    //Getters
    AquaSimAddress GetCHID();
    uint8_t GetNodeTyp();
        
    //inherited methods
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize (Buffer::Iterator start) const;
    virtual uint32_t Deserialize (Buffer::Iterator start);
    virtual void Print (std::ostream &os) const;
    virtual TypeId GetInstanceTypeId(void) const;
private:
    AquaSimAddress chId;    // 簇头id
    uint8_t node_typ;  // 节点类型 (长或短)
};  // class DolphinClusteringAnnouncementHeader

class DolphinClusteringLinkStateHeader: public Header {
public:
    DolphinClusteringLinkStateHeader();
    virtual ~DolphinClusteringLinkStateHeader();
    static TypeId GetTypeId();
            
    int Size();
            
    //Setters
    void SetSrc(AquaSimAddress s);
    void SetQuestFlag(uint8_t flag);
    void SetSink(AquaSimAddress sink);
    void SetTimeStamp(Time t);
    void SetLinks(uint16_t size, std::vector<AquaSimAddress> links);
    void SetMembers(uint16_t size, std::vector<AquaSimAddress> members);
            
    //Getters
    AquaSimAddress GetSrc();
    uint8_t GetQuestFlag();
    AquaSimAddress GetSink();
    Time GetTimeStamp();
    uint16_t GetLinkSize();
    uint16_t GetMemberSize();
    std::vector<AquaSimAddress> GetLinks();
    std::vector<AquaSimAddress> GetMembers();
            
    //inherited methods
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize (Buffer::Iterator start) const;
    virtual uint32_t Deserialize (Buffer::Iterator start);
    virtual void Print (std::ostream &os) const;
    virtual TypeId GetInstanceTypeId(void) const;
private:
    AquaSimAddress src;    // 簇头id
    uint8_t questFlag;
    AquaSimAddress sink;
    Time timestamp;        // 造包时间
    uint16_t linkSize;     // 簇间连接个数
    uint16_t memberSize;   // 成员个数
    std::vector<AquaSimAddress> linkAddresses;  // 簇间连接表
    std::vector<AquaSimAddress> memberAddresses;// 成员表
};  // class DolphinClusteringLinkStateHeader

class ClusteringDownLinkHeader: public Header {
public:
    ClusteringDownLinkHeader();
    virtual ~ClusteringDownLinkHeader();
    static TypeId GetTypeId();
    
    int Size();
    
    //Setters
    void SetRound(uint16_t r);
    void SetLt(double t);
    void SetFa(AquaSimAddress addr);
    
    //Getters
    uint16_t GetRound();
    double GetLt();
    AquaSimAddress GetFa();
    
    //inherited methods
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize (Buffer::Iterator start) const;
    virtual uint32_t Deserialize (Buffer::Iterator start);
    virtual void Print (std::ostream &os) const;
    virtual TypeId GetInstanceTypeId(void) const;
private:
    uint16_t round;
    double lt;
    AquaSimAddress fa;
};  // class ClusteringDownLinkHeader

class ClusteringUpLinkHeader: public Header {
public:
    ClusteringUpLinkHeader();
    virtual ~ClusteringUpLinkHeader();
    static TypeId GetTypeId();
    
    int Size();
    
    //Setters
    void SetLinks(uint16_t size, std::vector<AquaSimAddress> addrs);
    void SetLt(double t);
    void SetDst(AquaSimAddress d);
    
    //Getters
    uint16_t GetSize();
    AquaSimAddress GetDst();
    double GetLt();
    std::vector<AquaSimAddress> GetAddrs();
    
    //inherited methods
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize (Buffer::Iterator start) const;
    virtual uint32_t Deserialize (Buffer::Iterator start);
    virtual void Print (std::ostream &os) const;
    virtual TypeId GetInstanceTypeId(void) const;
private:
    uint16_t sz;
    std::vector<AquaSimAddress> Addresses;
    double lt;
    AquaSimAddress dst;
};  // class ClusteringUpLinkHeader

class PAUVHeader: public Header {
public:
    PAUVHeader();
    virtual ~PAUVHeader();
    static TypeId GetTypeId();
    
    int Size();
            
    //Setters
    void SetSrc(AquaSimAddress src);
    void SetLocSrc(Vector loc);
    void SetLocSnd(Vector loc);
            
    //Getters
    AquaSimAddress GetSrc();
    Vector GetLocSrc();
    Vector GetLocSnd();
            
    //inherited methods
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize (Buffer::Iterator start) const;
    virtual uint32_t Deserialize (Buffer::Iterator start);
    virtual void Print (std::ostream &os) const;
    virtual TypeId GetInstanceTypeId(void) const;
private:
    AquaSimAddress src;
    Vector locSrc;
    Vector locSnd;
};  // class PAUVHeader

} // namespace ns3

#endif /* DOLPHIN_CLUSTERING_HEADER_H */