#include "aqua-sim-routing-optimizedstatic2.h"
#include "aqua-sim-header.h"

#include "ns3/string.h"
#include "ns3/log.h"

#include <cstdio>
#include <cstdlib>  

#include <iostream>
#include <vector>
#include <algorithm>

int Finish_Initial_Network = 0;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("AquaSimOptimizedStaticRouting");
NS_OBJECT_ENSURE_REGISTERED(AquaSimOptimizedStaticRouting);

AquaSimOptimizedStaticRouting::AquaSimOptimizedStaticRouting() :
		m_hasSetRouteFile(false), m_hasSetNode(false),has_set_neighbor(false)
{   
    m_maxForward=5;
    m_pkt_t_out=100;
    m_pkt_t_slot=50;
    m_neighbor_t_out=180;
    m_neighbor_t_slot=50;
    m_NodeNum=10;

}

AquaSimOptimizedStaticRouting::AquaSimOptimizedStaticRouting(char *routeFile) :
    m_hasSetNode(false),has_set_neighbor(false)
{  
  SetRouteTable(routeFile);
}

AquaSimOptimizedStaticRouting::~AquaSimOptimizedStaticRouting()
{
}

TypeId
AquaSimOptimizedStaticRouting::GetTypeId()
{
  static TypeId tid = TypeId ("ns3::AquaSimOptimizedStaticRouting")
    .SetParent<AquaSimRouting> ()
    .AddConstructor<AquaSimOptimizedStaticRouting> ()
    .AddAttribute("SetRouteTableFile",
                    "set route table file",
                    StringValue(""),
                    MakeStringAccessor(&AquaSimOptimizedStaticRouting::m_routeFile_string),
                    MakeStringChecker())
    .AddAttribute ("max_forwards", "the max num_forwards of broadcast packets.",
      IntegerValue(5),
      MakeIntegerAccessor(&AquaSimOptimizedStaticRouting::m_maxForward),
      MakeIntegerChecker<int>()) 
    .AddAttribute ("neighbor_time_out", "Time out of neighbors",
      DoubleValue(180),
      MakeDoubleAccessor(&AquaSimOptimizedStaticRouting::m_neighbor_t_out),
      MakeDoubleChecker<double>())
    .AddAttribute ("neighbor_time_interval", "Time slot of neighbors",
      DoubleValue(50),
      MakeDoubleAccessor(&AquaSimOptimizedStaticRouting::m_neighbor_t_slot),
      MakeDoubleChecker<double>())
    .AddAttribute ("packet_time_out", "Time out of packets",
      DoubleValue(100),
      MakeDoubleAccessor(&AquaSimOptimizedStaticRouting::m_pkt_t_out),
      MakeDoubleChecker<double>())
    .AddAttribute ("packet_time_interval", "Time slot of packets",
      DoubleValue(50),
      MakeDoubleAccessor(&AquaSimOptimizedStaticRouting::m_pkt_t_slot),
      MakeDoubleChecker<double>())
    .AddAttribute ("Node_num", "the total number of nodes.",
      IntegerValue(10),
      MakeIntegerAccessor(&AquaSimOptimizedStaticRouting::m_NodeNum),
      MakeIntegerChecker<int>()) 
    .AddAttribute("Initial_Neighbor",
                    "initial neighbor string",
                    StringValue(""),
                    MakeStringAccessor(&AquaSimOptimizedStaticRouting::m_initial_neighbor_string),
                    MakeStringChecker())
    .AddTraceSource("optimizedTx","Net:A new packet is sent",
      MakeTraceSourceAccessor(&AquaSimOptimizedStaticRouting::m_txTrace),
                            "AquaSimOptimizedStaticRouting::TxCallback")
    .AddTraceSource("optimizedRx","Net:A new packet is received",
      MakeTraceSourceAccessor(&AquaSimOptimizedStaticRouting::m_rxTrace),
                            "AquaSimOptimizedStaticRouting::RxCallback")
    .AddTraceSource("optimizedInitialNet","Net:finish initial network",
      MakeTraceSourceAccessor(&AquaSimOptimizedStaticRouting::m_InitialNetTrace),
                            "AquaSimOptimizedStaticRouting::InitialNetCallback")
  ;
  return tid;
}

int64_t
AquaSimOptimizedStaticRouting::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  return 0;
}


void
AquaSimOptimizedStaticRouting::SetRouteTable(char *routeFile)
{
  NS_LOG_UNCOND("Set Route Table");
  m_hasSetRouteFile = false;
  strcpy(m_routeFile,routeFile);
  //if( m_hasSetNode ) {
    ReadRouteTable(m_routeFile);
  //}
}

/*
 * Load the static routing table in filename
 *
 * @param filename   the file containing routing table
 * */
void
AquaSimOptimizedStaticRouting::ReadRouteTable(char *filename)
{
  //NS_LOG_UNCOND("try to open"<<filename);
	FILE* stream = fopen(filename, "r");
	int current_node, dst_node, nxt_hop1, nxt_hop2;

	if( stream == NULL ) {
    NS_LOG_ERROR("Cannot open routing table file: " << filename);
		printf("ERROR: Cannot find routing table file!aaaaaaaaaaaaaaaaa\nEXIT...\n");
		exit(0);
	}
//输入从一行三个改为一行四个，有两个nxt_hop
	while( !feof(stream) ) {
		fscanf(stream, "%d:%d:%d:%d", &current_node, &dst_node, &nxt_hop1, &nxt_hop2);
     //NS_LOG_UNCOND("&current_node "<<&current_node<<"   current_node "<<current_node);
		if( AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() == current_node ) {
			 m_rTable.insert({AquaSimAddress(dst_node), {AquaSimAddress(nxt_hop1), AquaSimAddress(nxt_hop2)}});//在multimap中存储两个下一跳
       //NS_LOG_UNCOND("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() << "): Received route table : "<<m_rTable[0]AquaSimAddress(dst_node).GetAsInt()<<"    "<<AquaSimAddress(nxt_hop1).GetAsInt()<<"    "<<AquaSimAddress(nxt_hop2).GetAsInt());
		}
	}

	fclose(stream);
}


bool
AquaSimOptimizedStaticRouting::Recv (Ptr<Packet> p, const Address &dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION(this << p);
  if(!m_hasReadRouteFile){
    AquaSimOptimizedStaticRouting::ReadRouteTable((char*)m_routeFile_string.c_str());
    m_hasReadRouteFile = true;

    MaxForwards=m_maxForward;

  }
  if(!has_set_neighbor){
    hashTable.AquaSimGPSRHashTable::SetUp(AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt(),m_NodeNum,m_initial_neighbor_string);
    packet_table.AquaSimGPSRPktTable::SetUp(AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt(),m_NodeNum);
    has_set_neighbor = true;
    hashTable.CheckNeighborTimeout(AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt(),m_neighbor_t_out,m_neighbor_t_slot,m_NodeNum);
    packet_table.CheckBroadcastpktTimeout(AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt(),m_pkt_t_out,m_pkt_t_slot);
  }
  

  AquaSimAddress next_hop = FindNextHop (p);

  AquaSimHeader ash;
  p->PeekHeader(ash);
  

  NS_LOG_UNCOND("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() << "): Received packet : " << ash.GetSize() << " bytes ; " << ash.GetTxTime().GetSeconds() << " sec. ; Src: " << ash.GetSAddr().GetAsInt() << "  ; Dest: " << ash.GetDAddr().GetAsInt()  << " ; Next Hop: " << next_hop.GetAsInt());
  NS_LOG_UNCOND("packet Uid = "<<p->GetUid());

  if(next_hop.GetAsInt()==65535)
    {
      NS_LOG_UNCOND("Node("<<AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()<<") Dropping packet " << p << " due to cannot find next hop");
      m_rxTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),5);
      return false;
    }
  //处理位置广播包
  if (static_cast<int>(ash.GetNetDataType()))
    {
      NS_LOG_UNCOND("a location broadcast packet,put in hash");
      double time_finish=hashTable.PutInHash(p,AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt(),m_NodeNum);
      if(time_finish!=0) {
        NS_LOG_UNCOND("Finish initial network at "<< time_finish);
        m_InitialNetTrace(time_finish);
      }
      m_rxTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),0);
      return true;
    }
  //NS_LOG_UNCOND("not broadcast packet");
  if (IsDeadLoop (p))
    {
      NS_LOG_UNCOND("Dropping packet " << p << " due to route loop");
      m_rxTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),1);
      //drop(p, DROP_RTR_ROUTE_LOOP);
      p = 0;
      return false;
    }
  else if (AmISrc (p))
    {
      p->RemoveHeader(ash);
      NS_LOG_UNCOND("i am source,give new header");
      ash.SetDirection(AquaSimHeader::DOWN);
      ash.SetNumForwards(0);
      ash.SetNextHop(next_hop);
      ash.SetDAddr(AquaSimAddress::ConvertFrom(dest));
      ash.SetErrorFlag(false);
      ash.SetUId(p->GetUid());
      ash.SetSize(ash.GetSize() + SR_HDR_LEN); //add the overhead of static routing's header
			p->AddHeader(ash);
		}
  else if (!((ash.GetNextHop().GetAsInt()==AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt())||(next_hop == AquaSimAddress::GetBroadcast())))
    {
      m_rxTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),2);
      //NS_LOG_UNCOND("Next H.: " << ash.GetNextHop().GetAsInt());
      NS_LOG_UNCOND("Node("<<AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()<<") Dropping packet " << p << " due to duplicate");
      //drop(p, DROP_MAC_DUPLICATE);
      p = 0;
      return false;
    }
  m_rxTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),0);
  //increase the number of forwards
  
  uint8_t numForwards = ash.GetNumForwards() + 1;
  ash.SetNextHop(next_hop);
  ash.SetNumForwards (numForwards);
  p->AddHeader(ash);

  if (AmIDst (p))
    {
      AquaSimHeader a;
      p->PeekHeader(a);
      int HopNum=a.GetNumForwards()-1;
      m_txTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),0);
      NS_LOG_UNCOND("I am destination. Sending up.");
      SendUp (p);
      return true;
    }

  //find the next hop and forward
  
  if (next_hop != AquaSimAddress::GetBroadcast ())
    {
      //send to mac
      NS_LOG_UNCOND("Node(" << AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt() << ") find the next hop: Node("<<next_hop.GetAsInt()<<"), send to mac");
      SendDown (p, next_hop, Seconds (0.0));
      m_txTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),1);
      return true;
    }
  else
    {
      //NS_LOG_UNCOND("drop or broadcast?");
      if(packet_table.PutInPkt(p,AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()))
      {
        if(ash.GetNumForwards()>MaxForwards)
        {
          m_rxTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),3);
          NS_LOG_INFO("Dropping packet " << p << " due to too many numforwards");
          return false;
        }
        if(ash.GetSAddr()==AquaSimAddress::ConvertFrom(m_device->GetAddress()))//本节点产生的广播包不再sendup
        {
          NS_LOG_UNCOND("a broadcast packet from myself. Send to mac.");
          m_txTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),1);
          SendDown (p, next_hop, Seconds (0.0));
          return true;
        }
        else{
        NS_LOG_UNCOND("a broadcast packet. Sending up and down.");
        m_txTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),0);
        m_txTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),1);
          Ptr<Packet> cpkt = p->Copy();
          SendUp(cpkt);
          SendDown (p, next_hop, Seconds (0.0));
          return true;
       }
      }
      else{
        m_rxTrace(p,Simulator::Now().GetSeconds(),AquaSimAddress::ConvertFrom(m_device->GetAddress()),4);
        NS_LOG_INFO("Dropping packet " << p << " due to Broadcast Storm");
        p=0;
        return false;
      }

    }
  return false;
}

/*
 * @param p   a packet
 * @return    the next hop to route packet p
 * */
AquaSimAddress
AquaSimOptimizedStaticRouting::FindNextHop(const Ptr<Packet> p)
{
  //NS_LOG_UNCOND("try to find next hop");
    AquaSimHeader ash;
    p->PeekHeader(ash);
    AquaSimAddress dst=ash.GetDAddr();
    AquaSimAddress nxt_hop1, nxt_hop2,nxt_hop_fail;
    int find1 = 0;
    int find2 = 0;
    gpsr_neighborhood* hashPtr;
    hashPtr=hashTable.AquaSimGPSRHashTable::GetHash(AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt());
    
    std::multimap<AquaSimAddress, std::pair<AquaSimAddress, AquaSimAddress>>::iterator it = m_rTable.find(dst);
    //检查是否找到了元素
    if (it == m_rTable.end()) {
      //NS_LOG_UNCOND("your next hop is not on route table.you are a broadcast packet.");
        return AquaSimAddress::GetBroadcast();
    }
    else{
      nxt_hop1 = it->second.first;
      nxt_hop2 = it->second.second;
      }
    for(int i=0;i<m_NodeNum;i++)
    {
      NS_LOG_UNCOND("Node("<<AquaSimAddress::ConvertFrom(m_device->GetAddress())<<") neighbor "<<i+1<<" is alive?"<<hashPtr->alive[i]);
    }
      if((hashPtr->alive[nxt_hop1.GetAsInt()-1]==1)||(nxt_hop1.GetAsInt()==AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()))
        find1 = 1;
      if (nxt_hop2.GetAsInt()<51)//如果nexthop2=-1，那么是65535
      {
        if((hashPtr->alive[nxt_hop2.GetAsInt()-1]==1)||(nxt_hop2.GetAsInt()==AquaSimAddress::ConvertFrom(m_device->GetAddress()).GetAsInt()))
        find2 = 1;
      }
      NS_LOG_UNCOND("nxt_hop1  "<<nxt_hop1.GetAsInt()<<" find1  "<<find1<<"  nxt_hop2  "<<nxt_hop2.GetAsInt()<<" find2  "<<find2);
      nxt_hop_fail = AquaSimAddress(-1);
      if(find1==1)//第一个下一跳在邻居表里
      {
        return nxt_hop1;
      }  
      else if(find2==1)//第一个下一跳不在邻居表里且第二个下一跳在邻居表里
      {
        return nxt_hop2;
      }
      else return nxt_hop_fail;
     
}


//下面是新增加的有关地理位置包的部分
AquaSimGPSRHashTable::AquaSimGPSRHashTable(){
  NS_LOG_FUNCTION(this);
}

AquaSimGPSRHashTable::~AquaSimGPSRHashTable()
{
  NS_LOG_FUNCTION(this);
  for (std::map<int,gpsr_neighborhood*>::iterator it=neighbor_table.begin(); it!=neighbor_table.end(); ++it) {

    delete it->second;
  }
  neighbor_table.clear();
}


void
AquaSimGPSRHashTable::SetUp(int my_node,int total_nodes,std::string initial_neighbor_string)
{

std::stringstream ss(initial_neighbor_string);
std::string token;
std::vector<int> numbers;
while (std::getline(ss, token, ',')) {
        numbers.push_back(std::stoi(token)); // 将字符串转换为整数并存储
    }
int a=numbers[my_node-1];
int YourInitialNeighborNum=0;
std::vector<int> YourInitialNeighbor(total_nodes,0);
for(int i=total_nodes-1;i>=0;i--)
{
  YourInitialNeighbor[i] = ((a >> i) & 1)*-1;
  YourInitialNeighborNum=YourInitialNeighborNum+((a >> i) & 1);
}
//NS_LOG_UNCOND("node "<<my_node<<" initial neighbor "<<YourInitialNeighbor[0]<<YourInitialNeighbor[1]<<YourInitialNeighbor[2]<<YourInitialNeighbor[3]<<YourInitialNeighbor[4]<<YourInitialNeighbor[5]<<YourInitialNeighbor[6]<<YourInitialNeighbor[7]<<YourInitialNeighbor[8]<<YourInitialNeighbor[9]);
std::vector<Vector> positionss(total_nodes,{0, 0, 0});
std::vector<Time> new_t(total_nodes,Seconds(9000000));

  NS_LOG_UNCOND("setting up AquaSimGPSRHashTable of node("<<my_node<<")");
 //neighbor_table.clear();
  //根据节点数量初始化map
  gpsr_neighborhood* gp;
 
    gp=new gpsr_neighborhood();
    gp->initial_num=YourInitialNeighborNum;
    gp->neighbor_num=0;
    std::copy(std::begin(new_t), std::end(new_t), std::begin(gp->t_now));
    std::copy(std::begin(positionss), std::end(positionss), std::begin(gp->neighbor_position));
    std::copy(std::begin(YourInitialNeighbor),std::end(YourInitialNeighbor),std::begin(gp->alive));
    NS_LOG_UNCOND("Node("<<my_node<<") has "<<gp->neighbor_num<<" initial neighbors");
   
    neighbor_table.insert(std::make_pair(my_node, gp)); 
  //NS_LOG_UNCOND("setting up AquaSimGPSRHashTable successfully");
}


gpsr_neighborhood*
AquaSimGPSRHashTable::GetHash(int my_node)
{
  std::map<int,gpsr_neighborhood*>::iterator it;

  it = neighbor_table.find(my_node);

  if (it == neighbor_table.end())
    return NULL;

  return it->second;
}

double
AquaSimGPSRHashTable::PutInHash(Ptr<Packet> p,int my_node,int total_nodes)
{
  double tFinish=0;
  AquaSimHeader ash;
  LocalizationHeader loch;
  p->PeekHeader(ash);
  p->PeekHeader(loch);
  AquaSimAddress src=ash.GetSAddr();
  Time tstamp=ash.GetTimeStamp();
	gpsr_neighborhood* hashPtr;
	
  Vector nowposition;
  nowposition=loch.GetNodePosition();

  hashPtr = GetHash(my_node);

  int i=src.GetAsInt()-1;
    if (hashPtr->alive[i]==1) {
        //已经存在的邻居，更新其位置和时间
        hashPtr->neighbor_position[i]=nowposition;
        hashPtr->t_now[i]=Simulator::Now();
        neighbor_table[my_node]=hashPtr;
        }
    else if((hashPtr->alive[i]==-1))
    {
      //在初始邻居表里的邻居
        NS_LOG_UNCOND("Node("<<my_node<<") find initial neighbor node(" << src.GetAsInt()<<") at "<<Simulator::Now().GetSeconds());
        hashPtr->neighbor_position[i]=nowposition;
        hashPtr->t_now[i]=Simulator::Now();
        hashPtr->alive[i]=1;
        hashPtr->neighbor_num++;
        hashPtr->initial_num--;
        if(hashPtr->initial_num==0)
        {
          Finish_Initial_Network++;
          NS_LOG_UNCOND("!!!Node("<<my_node<<") find all initial neighbors at "<<Simulator::Now().GetSeconds());
          if(Finish_Initial_Network==total_nodes)
          {
            tFinish=Simulator::Now().GetSeconds();
            NS_LOG_UNCOND("!!!!!!all nodes find their initial neighbors at "<<tFinish);
          }
          
        }
        neighbor_table[my_node]=hashPtr;
    }
    else//有新邻居
      {
        NS_LOG_UNCOND("Node("<<my_node<<") find new neighbor node(" << src.GetAsInt()<<") at "<<Simulator::Now().GetSeconds());
        hashPtr->neighbor_position[i]=nowposition;
        hashPtr->t_now[i]=Simulator::Now();
        hashPtr->alive[i]=1;
        hashPtr->neighbor_num++;
        neighbor_table[my_node]=hashPtr;
      }
  return tFinish;
}

void AquaSimGPSRHashTable::CheckNeighborTimeout(int my_node,double Tout,double Tslot,int total_nodes)
{
    //NS_LOG_UNCOND("Begin DeleteNeighbor at "<<Simulator::Now().GetSeconds());
    Time time_out = Time::FromDouble(Tout, Time::S);
    Time time_slot = Time::FromDouble(Tslot, Time::S);

        gpsr_neighborhood* neighborhood;
        neighborhood = GetHash(my_node);
        int update = 0;
        for (int i = 0; i < total_nodes; i++)
        {
            if ((neighborhood->alive[i]==1)&&(Simulator::Now() - neighborhood->t_now[i] > time_out))
            {

              NS_LOG_UNCOND("Node("<<my_node<<")'s Neighbor "<<i+1<<" is deleted from AquaSimGPSRHashTable because of time out");
                // Swap with the last element
                neighborhood->alive[i]=0;
                neighborhood->neighbor_num--;
                update=1;
            }
        }
        if(update==1)   NS_LOG_UNCOND("Node("<<my_node<<") has "<<neighborhood->neighbor_num<<" neighbors after updating");
  
        neighbor_table[my_node]=neighborhood;
    //NS_LOG_UNCOND("Finished deleting hash at "<<Simulator::Now().GetSeconds());

     Simulator::Schedule(time_slot, &AquaSimGPSRHashTable::CheckNeighborTimeout, this,my_node,Tout,Tslot,total_nodes);
}
//

AquaSimGPSRPktTable::AquaSimGPSRPktTable()
{
  NS_LOG_FUNCTION(this);
}

AquaSimGPSRPktTable::~AquaSimGPSRPktTable()
{
  NS_LOG_FUNCTION(this);
  for (std::map<int,gpsr_packet*>::iterator it=pkt_table.begin(); it!=pkt_table.end(); ++it) {

    delete it->second;
  }
  pkt_table.clear();
}


gpsr_packet* 
AquaSimGPSRPktTable::GetPkt(int my_node)
{
std::map<int,gpsr_packet*>::iterator it;

  it = pkt_table.find(my_node);

  if (it == pkt_table.end())
    return NULL;

  return it->second;

}



void
AquaSimGPSRPktTable::SetUp(int my_node,int total_nodes)
{
    gpsr_packet* gpkt;

    gpkt=new gpsr_packet();
    std::vector<Time> new_t(30000, Seconds(9000000));
    gpkt->pkt_uid.assign(30000, UINT16_MAX);//不初始化就报错
    gpkt->pkt_t=new_t;
    gpkt->pkt_num=0;
    pkt_table.insert(std::make_pair(my_node, gpkt)); 

    //NS_LOG_UNCOND("finished setting AquaSimGPSRPktTable");


}

bool//返回0说明pkt表里有未超时的相同包，因此不继续广播；返回1说明是新的包，进行广播。
AquaSimGPSRPktTable::PutInPkt(Ptr<Packet> p,int my_node)
{
  uint16_t pktuid =p->GetUid();//uint16_t
  gpsr_packet* hashPkt;
  std::vector<uint16_t>my_pkt;

	//NS_LOG_UNCOND("check if there is a same packet");
  hashPkt = GetPkt(my_node);
   
  my_pkt=hashPkt->pkt_uid;
  auto it = std::find(my_pkt.begin(), my_pkt.end(), pktuid);
    // 检查是否找到了元素
    if (it != my_pkt.end()) {
      NS_LOG_UNCOND("there is a same packet, its uid is "<<pktuid<<", so this packet will not be forwarded");
        //int index = it - my_pkt.begin();
        //hashPkt->pkt_t[index]=Simulator::Now();
        return false;
    }
    else{
      NS_LOG_UNCOND("it is a new packet, its uid is "<<pktuid<<", so this packet be forwarded");
      int num=hashPkt->pkt_num;
      hashPkt->pkt_uid[num]=pktuid;
      hashPkt->pkt_t[num]=Simulator::Now();
      hashPkt->pkt_num++;
      pkt_table[my_node]=hashPkt;
      return true;
    }

}

void
AquaSimGPSRPktTable::CheckBroadcastpktTimeout(int my_node,double Tout,double Tslot)
{
    //NS_LOG_UNCOND("Begin CheckBroadcastpktTimeout at " << Simulator::Now().GetSeconds());
    Time time_out = Time::FromDouble(Tout, Time::S);
    Time time_slot = Time::FromDouble(Tslot, Time::S);

    gpsr_packet* packet;
    packet = GetPkt(my_node);

    if (packet) { // check for nullptr
      bool updated = false;
      for (size_t i = 0; i < packet->pkt_uid.size(); )
      {
          if (Simulator::Now() - packet->pkt_t[i] > time_out)
          {
              //NS_LOG_UNCOND("Packet " << packet->pkt_uid[i] << " is deleted from AquaSimGPSRPktTable because of time out");
              // 交换当前超时的元素与末尾元素
              std::swap(packet->pkt_uid[i], packet->pkt_uid.back());
              std::swap(packet->pkt_t[i], packet->pkt_t.back());
              // 删除末尾元素
              packet->pkt_uid.pop_back();
              packet->pkt_t.pop_back();
              packet->pkt_num--;
              updated = true;
          }
          else
          {
              ++i;
          }
      }

      //if (updated)  NS_LOG_UNCOND("Node(" << nodeid << ") has " << packet->pkt_num << " packets after updating ");
    }
    // 重新调度自己
    Simulator::Schedule(time_slot, &AquaSimGPSRPktTable::CheckBroadcastpktTimeout, this,my_node,Tout,Tslot);
}



}  // namespace ns3