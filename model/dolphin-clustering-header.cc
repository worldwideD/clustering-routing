#include "dolphin-clustering-header.h"
#include "aqua-sim-address.h"
#include "aqua-sim-header-routing.h"
#include "aqua-sim-header.h"
#include "ns3/log.h"
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DolphinClusteringHeader");
NS_OBJECT_ENSURE_REGISTERED(DolphinClusteringHelloHeader);

//  Hello类消息的Header
DolphinClusteringHelloHeader::DolphinClusteringHelloHeader()
{
}

DolphinClusteringHelloHeader::~DolphinClusteringHelloHeader()
{
}

TypeId
DolphinClusteringHelloHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DolphinClusteringHelloHeader")
        .SetParent<Header>()
        .AddConstructor<DolphinClusteringHelloHeader>()
    ;
    return tid;
}

TypeId
DolphinClusteringHelloHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}


int
DolphinClusteringHelloHeader::Size()
{
    return GetSerializedSize();
}

uint32_t
DolphinClusteringHelloHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    position.x = ((double)i.ReadU16());
    position.y = ((double)i.ReadU16());
    position.z = ((double)i.ReadU16());
    direction.x = ((double)i.ReadU16()) / 1000.0-1;
    direction.y = ((double)i.ReadU16()) / 1000.0-1;
    direction.z = ((double)i.ReadU16()) / 1000.0-1;
    speed = ((double)i.ReadU16()) / 1000.0;
    timestamp = Seconds(((double)i.ReadU32()) / 1000.0);
    weight = ((double)i.ReadU32()) / 1000.0;
    chFlag = i.ReadU8();
    node_typ = i.ReadU8();
    node_range = i.ReadU16();

    return GetSerializedSize();
}

uint32_t
DolphinClusteringHelloHeader::GetSerializedSize(void) const
{
  //reserved bytes for header
  return (6+6+2+4+4+1+1+2);
}

void
DolphinClusteringHelloHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU16 ((uint16_t)(position.x +0.5));
    i.WriteU16 ((uint16_t)(position.y +0.5));
    i.WriteU16 ((uint16_t)(position.z +0.5));
    i.WriteU16 ((uint16_t)((direction.x+1)*1000.0 +0.5));
    i.WriteU16 ((uint16_t)((direction.y+1)*1000.0 +0.5));
    i.WriteU16 ((uint16_t)((direction.z+1)*1000.0 +0.5));
    i.WriteU16 ((uint16_t)(speed*1000.0 + 0.5));
    i.WriteU32((uint32_t)(timestamp.GetSeconds() * 1000.0 + 0.5));
    i.WriteU32 ((uint32_t)(weight*1000.0 + 0.5));
    i.WriteU8(chFlag);
    i.WriteU8(node_typ);
    i.WriteU16(node_range);
}

void
DolphinClusteringHelloHeader::Print(std::ostream &os) const
{
    os << "Clustering Routing Hello/ACK Header is: position=(" << position.x << "," <<
    position.y << "," << position.z << ")  direction=(" << direction.x << "," <<
    direction.y << "," << direction.z << ")   speed=" << speed << "  weight=" <<
    weight << "  chFlag=" << chFlag << "\n";
}

void
DolphinClusteringHelloHeader::SetPosition(Vector pos)
{
    position = pos;
}

void
DolphinClusteringHelloHeader::SetDirection(Vector dir)
{
    direction = dir;
}

void
DolphinClusteringHelloHeader::SetSpeed(double s)
{
    speed = s;
}

void
DolphinClusteringHelloHeader::SetTimeStamp(Time s)
{
    timestamp = s;
}

void
DolphinClusteringHelloHeader::SetWeight(double w)
{
    weight = w;
}

void
DolphinClusteringHelloHeader::SetCHFlag(uint8_t flag)
{
    chFlag = flag;
}

void
DolphinClusteringHelloHeader::SetNodeTyp(uint8_t typ)
{
    node_typ = typ;
}

void
DolphinClusteringHelloHeader::SetRange(uint16_t range)
{
    node_range = range;
}

Vector
DolphinClusteringHelloHeader::GetPosition()
{
    return position;
}

Vector
DolphinClusteringHelloHeader::GetDirection()
{
    return direction;
}

double
DolphinClusteringHelloHeader::GetSpeed()
{
    return speed;
}

Time
DolphinClusteringHelloHeader::GetTimeStamp()
{
    return timestamp;
}

double
DolphinClusteringHelloHeader::GetWeight()
{
    return weight;
}

uint8_t
DolphinClusteringHelloHeader::GetCHFlag()
{
    return chFlag;
}

uint8_t
DolphinClusteringHelloHeader::GetNodeTyp()
{
    return node_typ;
}

uint16_t
DolphinClusteringHelloHeader::GetRange()
{
    return node_range;
}

NS_OBJECT_ENSURE_REGISTERED(DolphinClusteringAnnouncementHeader);

//  分簇通告类消息的Header
DolphinClusteringAnnouncementHeader::DolphinClusteringAnnouncementHeader()
{
}

DolphinClusteringAnnouncementHeader::~DolphinClusteringAnnouncementHeader()
{
}

TypeId
DolphinClusteringAnnouncementHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DolphinClusteringAnnouncementHeader")
        .SetParent<Header>()
        .AddConstructor<DolphinClusteringAnnouncementHeader>()
    ;
    return tid;
}

TypeId
DolphinClusteringAnnouncementHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}


int
DolphinClusteringAnnouncementHeader::Size()
{
    return GetSerializedSize();
}

uint32_t
DolphinClusteringAnnouncementHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    chId = (AquaSimAddress)i.ReadU16();
    node_typ = i.ReadU8();

    return GetSerializedSize();
}

uint32_t
DolphinClusteringAnnouncementHeader::GetSerializedSize(void) const
{
  //reserved bytes for header
  return (2+1);
}

void
DolphinClusteringAnnouncementHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU16(chId.GetAsInt());
    i.WriteU8(node_typ);
}

void
DolphinClusteringAnnouncementHeader::Print(std::ostream &os) const
{
    os << "Clustering Routing Announcement Header is: Addr=" << chId << "\n";
}

void
DolphinClusteringAnnouncementHeader::SetCHID(AquaSimAddress id)
{
    chId = id;
}

void
DolphinClusteringAnnouncementHeader::SetNodeTyp(uint8_t typ)
{
    node_typ = typ;
}

AquaSimAddress
DolphinClusteringAnnouncementHeader::GetCHID()
{
    return chId;
}

uint8_t
DolphinClusteringAnnouncementHeader::GetNodeTyp()
{
    return node_typ;
}

NS_OBJECT_ENSURE_REGISTERED(DolphinClusteringLinkStateHeader);

//  链路状态消息的Header
DolphinClusteringLinkStateHeader::DolphinClusteringLinkStateHeader()
{
}

DolphinClusteringLinkStateHeader::~DolphinClusteringLinkStateHeader()
{
}

TypeId
DolphinClusteringLinkStateHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DolphinClusteringLinkStateHeader")
        .SetParent<Header>()
        .AddConstructor<DolphinClusteringLinkStateHeader>()
    ;
    return tid;
}

TypeId
DolphinClusteringLinkStateHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}


int
DolphinClusteringLinkStateHeader::Size()
{
    return GetSerializedSize();
}

uint32_t
DolphinClusteringLinkStateHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    src = (AquaSimAddress)i.ReadU16();
    questFlag = i.ReadU8();
    sink = (AquaSimAddress)i.ReadU16();
    timestamp = Seconds(((double)i.ReadU32()) / 1000.0);
    linkSize = i.ReadU16();
    memberSize = i.ReadU16();
    linkAddresses.clear();
    memberAddresses.clear();
    for (int n = 0; n < linkSize; ++n)
    {
        linkAddresses.emplace_back((AquaSimAddress)i.ReadU16());
    }
    for (int n = 0; n < memberSize; ++n)
    {
        memberAddresses.emplace_back((AquaSimAddress)i.ReadU16());
    }
    return GetSerializedSize();
}

uint32_t
DolphinClusteringLinkStateHeader::GetSerializedSize(void) const
{
  //reserved bytes for header
  return (2+1+2+4+2+2) + (linkSize + memberSize) * 2;
}

void
DolphinClusteringLinkStateHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU16(src.GetAsInt());
    i.WriteU8(questFlag);
    i.WriteU16(sink.GetAsInt());
    i.WriteU32((uint32_t)(timestamp.GetSeconds() * 1000.0 + 0.5));
    i.WriteU16(linkSize);
    i.WriteU16(memberSize);
    for (auto iter = linkAddresses.begin(); iter != linkAddresses.end(); iter++)
    {
        i.WriteU16(iter->GetAsInt());
    }
    for (auto iter = memberAddresses.begin(); iter != memberAddresses.end(); iter++)
    {
        i.WriteU16(iter->GetAsInt());
    }
}

void
DolphinClusteringLinkStateHeader::Print(std::ostream &os) const
{
    os << "Clustering Routing LinkState Header is: Addr=" << src
        << "quest flag =" << questFlag
        << "sink =" << sink
        << "link count=" << linkSize / 2
        << "member count=" << memberSize / 2 << "\n";
}

void
DolphinClusteringLinkStateHeader::SetSrc(AquaSimAddress s)
{
    src = s;
}

void
DolphinClusteringLinkStateHeader::SetQuestFlag(uint8_t flag)
{
    questFlag = flag;
}

void
DolphinClusteringLinkStateHeader::SetSink(AquaSimAddress s)
{
    sink = s;
}

void
DolphinClusteringLinkStateHeader::SetTimeStamp(Time s)
{
    timestamp = s;
}

void
DolphinClusteringLinkStateHeader::SetLinks(uint16_t size, std::vector<AquaSimAddress> links)
{
    linkSize = size;
    linkAddresses = links;
}

void
DolphinClusteringLinkStateHeader::SetMembers(uint16_t size, std::vector<AquaSimAddress> members)
{
    memberSize = size;
    memberAddresses = members;
}

AquaSimAddress
DolphinClusteringLinkStateHeader::GetSrc()
{
    return src;
}

uint8_t
DolphinClusteringLinkStateHeader::GetQuestFlag()
{
    return questFlag;
}

AquaSimAddress
DolphinClusteringLinkStateHeader::GetSink()
{
    return sink;
}

Time
DolphinClusteringLinkStateHeader::GetTimeStamp()
{
    return timestamp;
}

uint16_t
DolphinClusteringLinkStateHeader::GetLinkSize()
{
    return linkSize;
}

uint16_t
DolphinClusteringLinkStateHeader::GetMemberSize()
{
    return memberSize;
}

std::vector<AquaSimAddress>
DolphinClusteringLinkStateHeader::GetLinks()
{
    return linkAddresses;
}

std::vector<AquaSimAddress>
DolphinClusteringLinkStateHeader::GetMembers()
{
    return memberAddresses;
}

NS_OBJECT_ENSURE_REGISTERED(ClusteringDownLinkHeader);

//  Hello类消息的Header
ClusteringDownLinkHeader::ClusteringDownLinkHeader()
{
}

ClusteringDownLinkHeader::~ClusteringDownLinkHeader()
{
}

TypeId
ClusteringDownLinkHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ClusteringDownLinkHeader")
        .SetParent<Header>()
        .AddConstructor<ClusteringDownLinkHeader>()
    ;
    return tid;
}

TypeId
ClusteringDownLinkHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}


int
ClusteringDownLinkHeader::Size()
{
    return GetSerializedSize();
}

uint32_t
ClusteringDownLinkHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    round = i.ReadU16();
    lt = ((double)i.ReadU32()) / 1000.0;
    fa = (AquaSimAddress)i.ReadU16();

    return GetSerializedSize();
}

uint32_t
ClusteringDownLinkHeader::GetSerializedSize(void) const
{
  //reserved bytes for header
  return (2+4+2);
}

void
ClusteringDownLinkHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU16(round);
    i.WriteU32 ((uint32_t)(lt*1000.0 + 0.5));
    i.WriteU16(fa.GetAsInt());
}

void
ClusteringDownLinkHeader::Print(std::ostream &os) const
{
    os << "Clustering Down Link Header is: round = " << round 
    << "  lt = " << lt 
    << "  fa = " << fa.GetAsInt() << "\n";
}

void
ClusteringDownLinkHeader::SetRound(uint16_t r)
{
    round = r;
}

void
ClusteringDownLinkHeader::SetLt(double t)
{
    lt = t;
}

void
ClusteringDownLinkHeader::SetFa(AquaSimAddress addr)
{
    fa = addr;
}

uint16_t
ClusteringDownLinkHeader::GetRound()
{
    return round;
}

double
ClusteringDownLinkHeader::GetLt()
{
    return lt;
}

AquaSimAddress
ClusteringDownLinkHeader::GetFa()
{
    return fa;
}

NS_OBJECT_ENSURE_REGISTERED(ClusteringUpLinkHeader);

//  Hello类消息的Header
ClusteringUpLinkHeader::ClusteringUpLinkHeader()
{
}

ClusteringUpLinkHeader::~ClusteringUpLinkHeader()
{
}

TypeId
ClusteringUpLinkHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ClusteringUpLinkHeader")
        .SetParent<Header>()
        .AddConstructor<ClusteringUpLinkHeader>()
    ;
    return tid;
}

TypeId
ClusteringUpLinkHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}


int
ClusteringUpLinkHeader::Size()
{
    return GetSerializedSize();
}

uint32_t
ClusteringUpLinkHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    sz = i.ReadU16();
    for (int n = 0; n < sz; ++n)
    {
        Addresses.emplace_back((AquaSimAddress)i.ReadU16());
    }
    lt = ((double)i.ReadU32()) / 1000.0;
    dst = (AquaSimAddress)i.ReadU16();

    return GetSerializedSize();
}

uint32_t
ClusteringUpLinkHeader::GetSerializedSize(void) const
{
  //reserved bytes for header
  return (2+4+2+2*sz);
}

void
ClusteringUpLinkHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU16(sz);
    for (auto iter = Addresses.begin(); iter != Addresses.end(); iter++)
    {
        i.WriteU16(iter->GetAsInt());
    }
    i.WriteU32 ((uint32_t)(lt*1000.0 + 0.5));
    i.WriteU16(dst.GetAsInt());
}

void
ClusteringUpLinkHeader::Print(std::ostream &os) const
{
    os << "Clustering Up Link Header is: sz = " << sz 
    << "  lt = " << lt 
    << "  dst = " << dst.GetAsInt() << "\n";
}

void
ClusteringUpLinkHeader::SetLinks(uint16_t size, std::vector<AquaSimAddress> addrs)
{
    sz = size;
    Addresses = addrs;
}

void
ClusteringUpLinkHeader::SetLt(double t)
{
    lt = t;
}

void
ClusteringUpLinkHeader::SetDst(AquaSimAddress d)
{
    dst = d;
}

uint16_t
ClusteringUpLinkHeader::GetSize()
{
    return sz;
}

std::vector<AquaSimAddress>
ClusteringUpLinkHeader::GetAddrs()
{
    return Addresses;
}

double
ClusteringUpLinkHeader::GetLt()
{
    return lt;
}

AquaSimAddress
ClusteringUpLinkHeader::GetDst()
{
    return dst;
}

NS_OBJECT_ENSURE_REGISTERED(PAUVHeader);

//  PAUV Header
PAUVHeader::PAUVHeader()
{
}

PAUVHeader::~PAUVHeader()
{
}

TypeId
PAUVHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::PAUVHeader")
        .SetParent<Header>()
        .AddConstructor<PAUVHeader>()
    ;
    return tid;
}

TypeId
PAUVHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}


int
PAUVHeader::Size()
{
    return GetSerializedSize();
}

uint32_t
PAUVHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    src = (AquaSimAddress)i.ReadU16();
    locSrc.x = ((double)i.ReadU16());
    locSrc.y = ((double)i.ReadU16());
    locSrc.z = ((double)i.ReadU16());
    locSnd.x = ((double)i.ReadU16());
    locSnd.y = ((double)i.ReadU16());
    locSnd.z = ((double)i.ReadU16());

    return GetSerializedSize();
}

uint32_t
PAUVHeader::GetSerializedSize(void) const
{
  //reserved bytes for header
  return (2+6+6);
}

void
PAUVHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU16(src.GetAsInt());
    i.WriteU16 ((uint16_t)(locSrc.x +0.5));
    i.WriteU16 ((uint16_t)(locSrc.y +0.5));
    i.WriteU16 ((uint16_t)(locSrc.z +0.5));
    i.WriteU16 ((uint16_t)(locSnd.x +0.5));
    i.WriteU16 ((uint16_t)(locSnd.y +0.5));
    i.WriteU16 ((uint16_t)(locSnd.z +0.5));
}

void
PAUVHeader::Print(std::ostream &os) const
{
    os << "P-AUV Header is:   src: " << src.GetAsInt() << "src position: (" << locSrc.x << "," <<
    locSrc.y << "," << locSrc.z << ")  snd position=(" << locSnd.x << "," <<
    locSnd.y << "," << locSnd.z << "\n";
}

void
PAUVHeader::SetSrc(AquaSimAddress s)
{
    src = s;
}

void
PAUVHeader::SetLocSrc(Vector loc)
{
    locSrc = loc;
}

void
PAUVHeader::SetLocSnd(Vector loc)
{
    locSnd = loc;
}

AquaSimAddress
PAUVHeader::GetSrc()
{
    return src;
}

Vector
PAUVHeader::GetLocSrc()
{
    return locSrc;
}

Vector
PAUVHeader::GetLocSnd()
{
    return locSnd;
}