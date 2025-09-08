/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 University of Connecticut
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Robert Martin <robert.martin@engr.uconn.edu>
 */

#include "ns3/log.h"
#include "ns3/ptr.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/vector.h"
#include <ctime>
#include "ns3/core-module.h"
#include "aqua-sim-channel.h"
#include "aqua-sim-header.h"
#include "aqua-sim-header-routing.h"

#include <cstdio>
#include <fstream>
#include <cmath>

#define FLOODING_TEST 0

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("AquaSimChannel");
NS_OBJECT_ENSURE_REGISTERED (AquaSimChannel);

AquaSimChannel::AquaSimChannel ()
{
  NS_LOG_FUNCTION(this);
  m_deviceList.clear();
  allPktCounter=0;
  sentPktCounter=0;
  allRecvPktCounter=0;
  pktLossRate = 0;
  //在此处初始化丢包率为0.1
  timeSlot = -1;
  //初始化时隙
}

AquaSimChannel::~AquaSimChannel ()
{
}

TypeId
AquaSimChannel::GetTypeId ()
{
  static TypeId tid = TypeId("ns3::AquaSimChannel")
    .SetParent<Channel> ()
    .AddConstructor<AquaSimChannel> ()
    .AddAttribute ("SetProp", "A pointer to set the propagation model.",
       PointerValue (0),
       MakePointerAccessor (&AquaSimChannel::m_prop),
       MakePointerChecker<AquaSimPropagation> ())
    .AddAttribute ("SetNoise", "A pointer to set the noise generator.",
       PointerValue (0),
       MakePointerAccessor (&AquaSimChannel::m_noiseGen),
       MakePointerChecker<AquaSimNoiseGen> ())
    .AddAttribute("SetPktLossRate","The packet lossrate of channel.",
       DoubleValue(0.1),
       MakeDoubleAccessor(&AquaSimChannel::pktLossRate),
       MakeDoubleChecker<double>())
    .AddAttribute("ShortRange","Short Dist Nodes Range",
       DoubleValue(1000),
       MakeDoubleAccessor(&AquaSimChannel::shortRange),
       MakeDoubleChecker<double>())
    .AddAttribute("LongRange","Long Dist Nodes Range",
       DoubleValue(2000),
       MakeDoubleAccessor(&AquaSimChannel::longRange),
       MakeDoubleChecker<double>())
    .AddAttribute("LongCnt","Long Dist Nodes Cnt",
       IntegerValue(0),
       MakeIntegerAccessor(&AquaSimChannel::long_cnt),
       MakeIntegerChecker<int>())
    .AddTraceSource("ChannelLoss",
       "Trace source indicating a packet has been lost ",
       MakeTraceSourceAccessor(&AquaSimChannel::channel_LOSSTrace),
       "ns3::AquaSimChannel::TxCallback")
    ;
  return tid;
}

void
AquaSimChannel::SetNoiseGenerator (Ptr<AquaSimNoiseGen> noiseGen)
{
  NS_LOG_FUNCTION(this);
  NS_ASSERT (noiseGen);
  m_noiseGen = noiseGen;
}

void
AquaSimChannel::SetPropagation (Ptr<AquaSimPropagation> prop)
{
  NS_LOG_FUNCTION(this);
  NS_ASSERT (prop);
  m_prop = prop;
}

Ptr<NetDevice>
AquaSimChannel::GetDevice (size_t i) const
{
  return m_deviceList[i];
}

uint32_t
AquaSimChannel::GetId (void) const
{
  NS_LOG_INFO("AquaSimChannel::GetId not implemented");
  return 0;
}

size_t
AquaSimChannel::GetNDevices (void) const
{
  return m_deviceList.size();
}

void
AquaSimChannel::AddDevice (Ptr<AquaSimNetDevice> device)
{
  NS_LOG_FUNCTION(this);
  m_deviceList.push_back(device);
}

void
AquaSimChannel::RemoveDevice(Ptr<AquaSimNetDevice> device)
{
  NS_LOG_FUNCTION(this);
  if (m_deviceList.empty())
    NS_LOG_DEBUG("AquaSimChannel::RemoveDevice: deviceList is empty");
  else
  {
    std::vector<Ptr<AquaSimNetDevice> >::iterator it = m_deviceList.begin();
    for(; it != m_deviceList.end(); ++it)
      {
        if(*it == device)
          {
            m_deviceList.erase(it);
          }
      }
  }
}

void setLossRate(double lossRate);

bool
AquaSimChannel::Recv(Ptr<Packet> p, Ptr<AquaSimPhy> phy)
{
  /*std::cout << "\nChannel: @Recv check:\n";
  p->Print(std::cout);
  std::cout << "\n";*/

  NS_LOG_FUNCTION(this << p << phy);
  NS_ASSERT(p != NULL || phy != NULL);
  return SendUp(p,phy);
}

//修改下面的函数，添加随机丢包的功能，模拟模拟水声信道的丢包
bool
AquaSimChannel::SendUp (Ptr<Packet> p, Ptr<AquaSimPhy> tifp)
{
  NS_LOG_FUNCTION(this);
  NS_LOG_DEBUG("Packet:" << p << " Phy:" << tifp << " Channel:" << this);
  Ptr<UniformRandomVariable> randomVar = CreateObject<UniformRandomVariable>();
  randomVar->SetAttribute ("Min", DoubleValue (0.0));
  randomVar->SetAttribute ("Max", DoubleValue (1.0));
  Ptr<AquaSimNetDevice> sender = Ptr<AquaSimNetDevice>(tifp->GetNetDevice());
  Ptr<AquaSimNetDevice> recver;
  Ptr<AquaSimPhy> rifp;
  Time pDelay;
  std::vector<PktRecvUnit> * recvUnits = m_prop->ReceivedCopies(sender, p, m_deviceList);
  Ptr<DolphinMobilityModel> senderMobility = GetMobilityModel(sender);
  Vector senderPos = senderMobility->GetRealPosition();
  Vector senderSpeed = senderMobility->GetDirection();
  double pi = std::acos(-1);
  double txRange = sender->GetPhy()->GetTxRange() * pi / 180;
  double senderSpeedLength = senderSpeed.GetLength();
  allPktCounter++; 
  for (std::vector<PktRecvUnit>::size_type i = 0; i < recvUnits->size(); i++) {
    //for循环里是针对每个数据包进行传输的具体操作，可以在这里执行随机丢包的操作
 //Debug .. remove这里是信道所收到的所有的包的数量
    if (sender == (*recvUnits)[i].recver)
    {
      continue;
    }
    allRecvPktCounter++; 
    recver = (*recvUnits)[i].recver;
    // 通信指向性代码，检查信号发射方向是否在收发角度范围内
    Ptr<DolphinMobilityModel> recverMobility = GetMobilityModel(recver);
    Vector recverPos = recverMobility->GetRealPosition();
    Vector recverSpeed = recverMobility->GetDirection();
    double rxRange = recver->GetPhy()->GetRxRange() * pi / 180;
    Vector diff = recverPos - senderPos;
    double diffLength = diff.GetLength();
    double recverSpeedLength = recverSpeed.GetLength();
    double txAngle = std::acos((diff.x * senderSpeed.x + diff.y * senderSpeed.y + diff.z * senderSpeed.z) / diffLength / senderSpeedLength);
    double rxAngle = std::acos(-(diff.x * recverSpeed.x + diff.y * recverSpeed.y + diff.z * recverSpeed.z) / diffLength / recverSpeedLength);
    
    if (txAngle > txRange || rxAngle > rxRange) {
      channel_LOSSTrace(p->Copy(),Simulator::Now().GetSeconds(), 5, recver,(recver->GetNode()->GetId())+1);
      continue;
    }
    // 长短距离丢包
    // if (((sender->GetNode()->GetId() >= long_cnt || recver->GetNode()->GetId() >= long_cnt) && diffLength > shortRange) ||
    //     (diffLength > longRange))
    // {
    //   channel_LOSSTrace(p->Copy(),Simulator::Now().GetSeconds(), 4, recver,(recver->GetNode()->GetId())+1);
    //   continue;
    // }
    //信道随机丢包功能的函数:设置随机数种子，以一定概率随机丢包
    double randomValue = randomVar -> GetValue();
    if(randomValue < pktLossRate){
      //执行回调函数跟踪数据包丢失情况
      NS_LOG_DEBUG("Packet Loss because of channel and pktlossrate randomvalue: "<<pktLossRate<<"   "<<randomValue <<"\n");
      channel_LOSSTrace(p->Copy(),Simulator::Now().GetSeconds(), 2, recver,(recver->GetNode()->GetId())+1);
      continue;
    }

    sentPktCounter++; //物理层接收的数据包总数
    pDelay = GetPropDelay(sender, (*recvUnits)[i].recver);
    rifp = recver->GetPhy();
    AquaSimPacketStamp pstamp;
    AquaSimHeader asHeader;
    p->RemoveHeader(pstamp);
    p->RemoveHeader(asHeader);
    // 长短距离
    EnumValue sident, rident;
    sender->GetRouting()->GetAttribute("Identity", sident);
    recver->GetRouting()->GetAttribute("Identity", rident);
    if (sender->GetRouting()->GetInstanceTypeId().GetName() == "ns3::RoutingSOAM")
    {
      if (((sender->GetNode()->GetId() >= long_cnt || recver->GetNode()->GetId() >= long_cnt) && diffLength > shortRange) ||
        (diffLength > longRange))
      {
        p->AddHeader(asHeader);
        p->AddHeader(pstamp);
        channel_LOSSTrace(p->Copy(),Simulator::Now().GetSeconds(), 4, recver,(recver->GetNode()->GetId())+1);
        continue;
      }
    }
    else
    if((asHeader.GetNetDataType() != AquaSimHeader::DATA || sident.Get() != 1 || rident.Get() != 1) && diffLength > shortRange)
    {
      p->AddHeader(asHeader);
      p->AddHeader(pstamp);
      channel_LOSSTrace(p->Copy(),Simulator::Now().GetSeconds(), 4, recver,(recver->GetNode()->GetId())+1);
      continue;
    }
    pstamp.SetPr((*recvUnits)[i].pR);
    pstamp.SetNoise(m_noiseGen->Noise((Simulator::Now() + pDelay), (recverMobility->GetRealPosition())));
    pstamp.SetTxDistance((*recvUnits)[i].distance);
    asHeader.SetDirection(AquaSimHeader::UP);
    //在当前节点的TxTime还是传输时延
    asHeader.SetTxTime(pDelay);

#if 0
    AquaSimPacketStamp pstamp;
    AquaSimHeader asHeader;
    p->RemoveHeader(pstamp);
    p->RemoveHeader(asHeader);
    //信道随机丢包功能的函数:设置随机数种子，以一定概率随机丢包
    uint64_t p_timeSlot = asHeader.GetTimeSlot();
    if (p_timeSlot != timeSlot) // 新的时隙重新随机
    {
      double randomValue = randomVar -> GetValue();
      timeSlot = p_timeSlot;
      dropFlag = (randomValue < pktLossRate);
    }

    if(dropFlag){
      //执行回调函数跟踪数据包丢失情况
      p->AddHeader(asHeader);
      p->AddHeader(pstamp);
      // NS_LOG_DEBUG("Packet Loss because of channel and pktlossrate randomvalue: "<<pktLossRate<<"   "<<randomValue <<"\n");
      channel_LOSSTrace(p->Copy(),Simulator::Now().GetSeconds(), 2, recver,(recver->GetNode()->GetId())+1);
      continue;
    }

    sentPktCounter++; //物理层接收的数据包总数
    pDelay = GetPropDelay(sender, (*recvUnits)[i].recver);
    rifp = recver->GetPhy();

    pstamp.SetPr((*recvUnits)[i].pR);
    pstamp.SetNoise(m_noiseGen->Noise((Simulator::Now() + pDelay), (recverMobility->GetRealPosition())));
    pstamp.SetTxDistance((*recvUnits)[i].distance);
    asHeader.SetDirection(AquaSimHeader::UP);
    //在当前节点的TxTime还是传输时延
    asHeader.SetTxTime(pDelay);
#endif
    p->AddHeader(asHeader);
    p->AddHeader(pstamp);

    /**
     * Send to each interface a copy, and we will filter the packet
     * in physical layer according to freq and modulation
     */
    NS_LOG_DEBUG ("Channel. NodeS:" << sender->GetAddress() << " NodeR:" << recver->GetAddress() << " S.Phy:" << sender->GetPhy() << " R.Phy:" << recver->GetPhy() << " packet:" << p
		  << " TxTime:" << asHeader.GetTxTime() << pDelay);
    

    //liucm20240905 channel use ScheduleWithContext to fixbug: Node::ReceiveFromDevice a
    uint32_t dstNodeId = recver->GetNode()->GetId();
    Simulator::ScheduleWithContext(dstNodeId,pDelay, &AquaSimPhy::Recv, rifp, p->Copy());
  }

  p = 0; //smart pointer will unref automatically once out of scope
  delete recvUnits;
  return true;
}

//查看/修改下面的统计收发包逻辑，查看是否正确
void
AquaSimChannel::PrintCounters()
{
  std::cout << "Channel Counters= SendDownToChannel(" << allPktCounter << ") ChannelAllSent(should be =n*sendup)("
            << allRecvPktCounter << ") PhyAllRecvers(" << sentPktCounter << ")\n";


  // //****gather total amount of messages sent
  // int totalSentPkts =0;
  // std::cout << "Sent Pkts(Source_NetDevice->Stack):\n";
  // for (std::vector<Ptr<AquaSimNetDevice> >::iterator it = m_deviceList.begin(); it != m_deviceList.end(); ++it)
  // {
  //   totalSentPkts += (*it)->TotalSentPkts();
  // }
  // std::cout << " (NetworkTotal) " << totalSentPkts << "\n";

  // //*****gather specific net device receival amount
  // int totalSendUpPkts =0;
  // std::cout << "SendUp Pkts(Sink_RoutingLayer):\n";
  // for (std::vector<Ptr<AquaSimNetDevice> >::iterator it = m_deviceList.begin(); it != m_deviceList.end(); ++it)
  // {
  //   totalSendUpPkts += (*it)->GetRouting()->SendUpPktCount();
  //   //std::cout << " (" << (*it)->GetAddress() << ") " << (*it)->GetRouting()->SendUpPktCount() << "\n";
  // }
  // std::cout << " (NetworkTotal) " << totalSendUpPkts << "\n";

  int totalSentPkts = 0;
  std::cout << "Phylayer Sent Counts:\n";
  for (std::vector<Ptr<AquaSimNetDevice> >::iterator it = m_deviceList.begin(); it != m_deviceList.end(); ++it)
  {
    totalSentPkts += (*it)->GetPhy()->PktRecvCount();
    //std::cout << " (" << (*it)->GetAddress() << ") " << (*it)->GetPhy()->PktRecvCount() << "\n";
  }
  std::cout << " (NetworkTotal) " << totalSentPkts << "\n";

  int totalRecvPkts = 0;
  std::cout << "Phylayer Recv from Channel Counts:\n";
  for (std::vector<Ptr<AquaSimNetDevice> >::iterator it = m_deviceList.begin(); it != m_deviceList.end(); ++it)
  {
    totalRecvPkts += (*it)->GetPhy()->PktInRecvCount();
    //std::cout << " (" << (*it)->GetAddress() << ") " << (*it)->GetPhy()->PktRecvCount() << "\n";
  }
  std::cout << " (NetworkTotal) " << totalRecvPkts << "\n";


  //****gather number of forwards for each pkt if possible.
  //possible look at phy-cmn layer namely AquaSimPhyCmn::PktTransmit()
}

/*
  While outdated due to the use of tracers within Routing layer, this is left
  due to potential legacy issues.
*/
void
AquaSimChannel::FilePrintCounters(double simTime, int attSlot)
{
  NS_LOG_FUNCTION(this << "Does not implement anything.");

}

Time
AquaSimChannel::GetPropDelay (Ptr<AquaSimNetDevice> tdevice, Ptr<AquaSimNetDevice> rdevice)
{
  NS_LOG_DEBUG("Channel Propagation Delay:" << m_prop->PDelay(GetMobilityModel(tdevice), GetMobilityModel(rdevice)).GetSeconds());

  return m_prop->PDelay(GetMobilityModel(tdevice), GetMobilityModel(rdevice));
}

double
AquaSimChannel::Distance(Ptr<AquaSimNetDevice> tdevice, Ptr<AquaSimNetDevice> rdevice)
{
  return GetMobilityModel(tdevice)->GetRealDistanceFrom(GetMobilityModel(rdevice));
}

Ptr<DolphinMobilityModel>
AquaSimChannel::GetMobilityModel(Ptr<AquaSimNetDevice> device)
{
  Ptr<DolphinMobilityModel> model = device->GetNode()->GetObject<DolphinMobilityModel>();
  if (model == 0)
    {
      std::cout<<"\nMobilityModel does not exist for device " << device;
    }
  return model;
}

Ptr<AquaSimNoiseGen>
AquaSimChannel::GetNoiseGen()
{
  return m_noiseGen;
}

void
AquaSimChannel::DoDispose()
{
  NS_LOG_FUNCTION (this);
  for (std::vector<Ptr<AquaSimNetDevice> >::iterator iter = m_deviceList.begin (); iter != m_deviceList.end (); iter++)
    {
      *iter = 0;
    }
  m_deviceList.clear();
  m_noiseGen=0;
  m_prop=0;
}
 }// namespace ns3
