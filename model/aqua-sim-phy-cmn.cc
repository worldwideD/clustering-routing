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

#include <string>
#include <vector>

#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "ns3/trace-source-accessor.h"

#include "aqua-sim-header.h"
#include "aqua-sim-header-mac.h"
#include "aqua-sim-energy-model.h"
#include "aqua-sim-phy-cmn.h"

//Aqua Sim Phy Cmn

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AquaSimPhyCmn");
NS_OBJECT_ENSURE_REGISTERED(AquaSimPhyCmn);

AquaSimPhyCmn::AquaSimPhyCmn(void) :
    m_powerLevels(1, 0.660),	/*0.660 indicates 1.6 W drained power for transmission*/
    m_sinrChecker(NULL)
{
  NS_LOG_FUNCTION(this);

  m_updateEnergyTime = Simulator::Now().GetSeconds();
  m_preamble = 1.5;
  m_trigger = 0.45;
  //GetNetDevice()->SetTransmissionStatus(NIDLE);
  //SetPhyStatus(PHY_IDLE);
  //m_ant = NULL;
  m_channel.clear();
  //m_mac = NULL;

  m_ptLevel = 0;
  m_PoweredOn = true;

  m_RXThresh = 0;
  m_CSThresh = 0;
  m_CPThresh = 10;
  m_pT = 0.2818;
  m_EnergyTurnOn = 0;
  m_EnergyTurnOff = 0;
  m_lambda = 0.0;
  m_L = 0;
  m_K = 2.0;
  m_freq = 25; //设置信号的频率为25kHz
  m_transRange=-1;

  m_relRange = 3000.0;
  m_maxrange = 9000.0;
  m_txRange = 180.0;
  m_rxRange = 180.0;

  m_modulationName = "default";
  AddModulation(CreateObject<AquaSimModulation>(), "default");
  if (!m_sC)
    m_sC = CreateObject<AquaSimSignalCache>();
  AttachPhyToSignalCache(m_sC, this);

  incPktCounter = 0;	//debugging purposes only
  outPktCounter = 0;
  pktRecvCounter = 0;
  currentSignalCounter = 0;

  Simulator::Schedule(Seconds(1), &AquaSimPhyCmn::UpdateIdleEnergy, this); //start energy drain
}

AquaSimPhyCmn::~AquaSimPhyCmn(void)
{
  Dispose();
}

TypeId
AquaSimPhyCmn::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::AquaSimPhyCmn")
    .SetParent<AquaSimPhy>()
    .AddConstructor<AquaSimPhyCmn>()
    .AddAttribute("CPThresh", "Capture Threshold (db), default is 10.0 set as 10.",
      DoubleValue (10),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_CPThresh),
      MakeDoubleChecker<double> ())
    .AddAttribute("CSThresh", "Carrier sense threshold (W), default is 1.559e-11 set as 0.",
      DoubleValue(0),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_CSThresh),
      MakeDoubleChecker<double>())
    .AddAttribute("RXThresh", "Receive power threshold (W), default is 3.652e-10 set as 0.",
      DoubleValue(0),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_RXThresh),
      MakeDoubleChecker<double>())
    .AddAttribute("PT", "Transmitted signal power (W).",
      DoubleValue(0.2818),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_pT),
      MakeDoubleChecker<double>())
    .AddAttribute("Frequency", "The frequency, default is 25(khz).",
      DoubleValue(25),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_freq),
      MakeDoubleChecker<double>())
    .AddAttribute("L", "System loss default factor.",
      DoubleValue(0),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_L),
      MakeDoubleChecker<double>())
    .AddAttribute("K", "Energy spread factor, spherical spreading. Default is 2.0.",
      DoubleValue(2.0),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_K),
      MakeDoubleChecker<double>())
    .AddAttribute("TurnOnEnergy", "Energy consumption for turning on the modem (J).",
      DoubleValue(0),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_EnergyTurnOn),
      MakeDoubleChecker<double>())
    .AddAttribute("TurnOffEnergy", "Energy consumption for turning off the modem (J).",
      DoubleValue(0),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_EnergyTurnOff),
      MakeDoubleChecker<double>())
    .AddAttribute("Preamble", "Duration of preamble.",
      DoubleValue(0),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_preamble),
      MakeDoubleChecker<double>())
    .AddAttribute("Trigger", "Duration of trigger.",
      DoubleValue(0),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_trigger),
      MakeDoubleChecker<double>())
    .AddAttribute("PTLevel", "Level of transmitted signal power.",
      UintegerValue(0),
      MakeUintegerAccessor(&AquaSimPhyCmn::m_ptLevel),
      MakeUintegerChecker<uint32_t> ())
    .AddAttribute("SignalCache", "Signal cache attached to this node.",
      PointerValue(),
      MakePointerAccessor (&AquaSimPhyCmn::m_sC),
      MakePointerChecker<AquaSimSignalCache>())
    .AddAttribute("ReliableRange","Reliable Transmission Range.",
      DoubleValue(3000.0),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_relRange),
      MakeDoubleChecker<double>())
    .AddAttribute("MaxRange","Max Transmission Range.",
      DoubleValue(9000.0),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_maxrange),
      MakeDoubleChecker<double>())
    .AddAttribute("TransRange", "Transmit Angle Range.",
      DoubleValue(180.),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_txRange),
      MakeDoubleChecker<double>())
    .AddAttribute("RecvRange", "Receive Angle Range.",
      DoubleValue(180.),
      MakeDoubleAccessor(&AquaSimPhyCmn::m_rxRange),
      MakeDoubleChecker<double>())
    .AddTraceSource("Rx", "A packet was receieved.",
      MakeTraceSourceAccessor (&AquaSimPhyCmn::m_rxLogger),
      "ns3::AquaSimPhy::TracedCallback")
    .AddTraceSource("Tx", "A packet was transmitted.",
      MakeTraceSourceAccessor (&AquaSimPhyCmn::m_txLogger),
      "ns3::AquaSimPhy::TracedCallback")
    .AddTraceSource("RxColl", "Count collision on Rx",
      MakeTraceSourceAccessor (&AquaSimPhyCmn::m_rxCollTrace),
      "ns3::AquaSimPhy::TracedCallback")
    .AddTraceSource("PhyPacketLoss", "Trace Packet Loss in Phy",
      MakeTraceSourceAccessor (&AquaSimPhyCmn::PhyLossTrace),
      "ns3::AquaSimPhy::TracedCallback")
    .AddTraceSource("PhyCounterLoss", "Count Packet Loss in Phy",
      MakeTraceSourceAccessor (&AquaSimPhyCmn::PhyCounterTrace),
      "ns3::AquaSimPhy::TracedCallback")
    ;
  return tid;
}


void
AquaSimPhyCmn::SetSinrChecker(Ptr<AquaSimSinrChecker> sinrChecker)
{
  m_sinrChecker = sinrChecker;
}

void
AquaSimPhyCmn::SetSignalCache(Ptr<AquaSimSignalCache> sC)
{
  m_sC = sC;
  AttachPhyToSignalCache(m_sC,this);
}

Ptr<AquaSimSignalCache>
AquaSimPhyCmn::GetSignalCache()
{
  return m_sC;
}

void
AquaSimPhyCmn::AddModulation(Ptr<AquaSimModulation> modulation, std::string modulationName)
{
  /**
  * format:  phyobj add-modulation modulation_name modulation_obj
  * the first added modulation scheme will be the default one
  */
  if (modulation == NULL || modulationName.empty())
    NS_LOG_ERROR("AddModulation NULL value for modulation " << modulation << " or name " << modulationName);
  else if (m_modulations.count(modulationName) > 0)
    NS_LOG_WARN("Duplicate modulations");
  else {
    if (m_modulations.size() == 0) {
      m_modulationName = modulationName;
    }
    m_modulations[modulationName] = modulation;
  }
}

/**
 * update energy for transmitting for duration of P_t
 */
void
AquaSimPhyCmn::UpdateTxEnergy(Time txTime) {
	NS_LOG_FUNCTION(this << "Currently not implemented");
	double startTime = Simulator::Now().GetSeconds(), endTime = Simulator::Now().GetSeconds() + txTime.GetSeconds();

	if (NULL != EM()) {
		if (startTime >= m_updateEnergyTime) {
			EM()->DecrIdleEnergy(startTime - m_updateEnergyTime);
			m_updateEnergyTime = startTime;
		}
		EM()->DecrTxEnergy(txTime.GetSeconds());
		m_updateEnergyTime = endTime;
	}
	else
		NS_LOG_FUNCTION(this << " No EnergyModel set.");
}


void
AquaSimPhyCmn::UpdateRxEnergy(Time txTime, bool errorFlag) {
  NS_LOG_FUNCTION(txTime);

  double startTime = Simulator::Now().GetSeconds();
  double endTime = startTime + txTime.GetSeconds();

  if (EM() == NULL) {
    NS_LOG_FUNCTION(this << " No EnergyModel set.");
    return;
  }

  if (startTime > m_updateEnergyTime) {
    EM()->DecrIdleEnergy(startTime - m_updateEnergyTime);
    EM()->DecrRcvEnergy(txTime.GetSeconds());
    m_updateEnergyTime = endTime;
  }
  else{
    /* In this case, this device is receiving some other packet*/
    if (endTime > m_updateEnergyTime && errorFlag)
    {
      EM()->DecrRcvEnergy(endTime - m_updateEnergyTime);
      m_updateEnergyTime = endTime;
    }
  }


  if (EM()->GetEnergy() <= 0) {
    EM()->SetEnergy(-1);
    //GetNetDevice()->LogEnergy(0);
    //NS_LOG_INFO("AquaSimPhyCmn::UpdateRxEnergy: -t " << Simulator::Now().GetSeconds() <<
      //" -n " << GetNetDevice()->GetAddress() << " -e 0");
  }
}

void
AquaSimPhyCmn::UpdateIdleEnergy()
{
  if (!m_PoweredOn || EM() == NULL )
    return;

  if (Simulator::Now().GetSeconds() > m_updateEnergyTime && m_PoweredOn) {
    EM()->DecrIdleEnergy(Simulator::Now().GetSeconds() - m_updateEnergyTime);
    m_updateEnergyTime = Simulator::Now().GetSeconds();
  }

  // log device energy
  if (EM()->GetEnergy() > 0) {
    //GetNetDevice()->LogEnergy(1);
    //NS_LOG_INFO("AquaSimPhyCmn::UpdateRxEnergy: -t " << Simulator::Now().GetSeconds() <<
      //" -n " << GetNetDevice()->GetAddress() << " -e " << EM()->GetEnergy());
  }
  else {
    //GetNetDevice()->LogEnergy(0);
    //NS_LOG_INFO("AquaSimPhyCmn::UpdateRxEnergy: -t " << Simulator::Now().GetSeconds() <<
      //" -n " << GetNetDevice()->GetAddress() << " -e 0");
  }

  Simulator::Schedule(Seconds(1), &AquaSimPhyCmn::UpdateIdleEnergy, this);
}

bool
AquaSimPhyCmn::Decodable(double noise, double ps) {
  double epsilon = 1e-6;	//accuracy for float comparison

  if (ps < m_RXThresh) //signal is too weak
    return false;

  if (fabs(noise) <  epsilon) {
    //avoid being divided by 0 when calculating SINR
    return true;
  }

  return m_sinrChecker->Decodable(ps / noise);
}

/**
* stamp the packet with information required by channel
* different channel model may require different information
* overload this method if needed
*/
Ptr<Packet>
AquaSimPhyCmn::StampTxInfo(Ptr<Packet> p)
{
  AquaSimPacketStamp pstamp;
//  std::cout << "PT VALUE:" << m_pT << "\n";
  pstamp.SetPt(m_pT);

  pstamp.SetPr(m_lambda);
  pstamp.SetFreq(m_freq);
  // Disable this for mac_routing dev
//  pstamp.SetPt(m_powerLevels[m_ptLevel]);
  //
  pstamp.SetTxRange(m_transRange);
  //pstamp.SetModName(m_modulationName);

  MacHeader mach;
  AquaSimHeader ash;

  p->RemoveHeader(ash);
  p->RemoveHeader(mach);

  // Set Tx power to pstamp for mac_routing dev
  // Set the current transmission range as well, to separate the collision domains
  if (mach.GetDemuxPType() == MacHeader::UWPTYPE_MAC_LIBRA)
  {
	  MacLibraHeader mac_libra_h;

	  p->RemoveHeader(mac_libra_h);

	  pstamp.SetPt(mac_libra_h.GetTxPower());
//	  std::cout << "TX POWER VALUE:" << mac_libra_h.GetTxPower() << "\n";

	  // Skip INIT messages
	  if (mac_libra_h.GetPType() != 4)
	  {
//		  std::cout << "TX RANGE: " << mac_libra_h.GetNextHopDistance() << "\n";
		  pstamp.SetTxRange(mac_libra_h.GetNextHopDistance());
	  }

	  EM()->SetTxPower(mac_libra_h.GetTxPower());
	  ///
	  p->AddHeader(mac_libra_h);
  }
  else
  {
	  // EXPERIMENTAL !!! Consider the tx power in EM()
	  EM()->SetTxPower(m_pT);
  }

  p->AddHeader(mach);
  p->AddHeader(ash);

  p->AddHeader(pstamp);
  return p;
}

/**
* we will cache the incoming packet in phy layer
* and send it to MAC layer after receiving the entire one
*/
bool
AquaSimPhyCmn::Recv(Ptr<Packet> p)
{
  AquaSimPacketStamp pstamp;
  AquaSimHeader asHeader;
  p->RemoveHeader(pstamp);
  p->PeekHeader(asHeader);
  p->AddHeader(pstamp);
  m_rxLogger(p->Copy(),Simulator::Now().GetSeconds(),GetNetDevice()->GetNode()->GetId(),asHeader);
  if (asHeader.GetDirection() == AquaSimHeader::DOWN) {
    NS_LOG_DEBUG("Phy_Recv DOWN. Pkt counter(" << outPktCounter++ << ") on node(" <<
		 GetNetDevice()->GetAddress() << ")");
    PktTransmit(p);
  }
  else {
    if (asHeader.GetDirection() != AquaSimHeader::UP) {
      NS_LOG_WARN("Direction for pkt-flow not specified, "
	      "sending pkt up the stack on default.");
    }
    incPktCounter++;
    p = PrevalidateIncomingPkt(p);
    if (p != NULL) {
      //只有在p不是NULL的情况下，才会传递数据包
      //在是NULL的情况下，会经过另外的函数来传递数据包的副本
      m_sC->AddNewPacket(p);
    }
  }
  return true;
}

bool AquaSimPhyCmn::MatchFreq(double freq)
{
  double epsilon = 1e-6;	//accuracy for float comparison

  return std::fabs(freq - m_freq) < epsilon;
}

/**
* This function pre-validate a incoming packet,
* it checks if this reception fails because of a hardware issue
* meanwhile, we update the energy consumption on this node.
*
* @param p the received packet
* @return  NULL if this packet cannot be received even if it cannot be decoded
* 			p would be freed and set to NULL in this case
* 			otherwise, return p
*/

//这个函数之前没有涉及修改Net_Device状态的代码，在此处开始修改
Ptr<Packet>
AquaSimPhyCmn::PrevalidateIncomingPkt(Ptr<Packet> p)
{
  // std::cout<<"Transmit Status:"<<GetNetDevice()->GetTransmissionStatus()<<"\n";
  NS_LOG_FUNCTION(this << p);

  AquaSimPacketStamp pstamp;
  AquaSimHeader asHeader;
  p->RemoveHeader(pstamp);
  p->RemoveHeader(asHeader);
  NS_LOG_DEBUG ("TxTime=" << asHeader.GetTxTime());
  Time txTime = asHeader.GetTxTime();
  double dist = pstamp.GetTxDistance();
  uint32_t nodeId = GetNetDevice()->GetNode()->GetId();

  if(dist > m_maxrange){
    //TODO:插入trace函数追踪数据包丢失
    // std::cout<<"Phy Packet Dropped because of distance the packetId is "<< p->GetUid() << " \n";
    PhyLossTrace(p,Simulator::Now().GetSeconds(),4,nodeId,asHeader);
    p = 0;
    return NULL;
  }
  // Time end = CalcTxTime(asHeader.GetSize());
  // std::cout << "in node " << nodeId+1 <<  " packet " << p->GetUid() << " will be end at  time " << end+Simulator::Now() << std::endl;
  currentSignalCounter += 1;  // 不论是否可接收，都会对其他信号产生干扰，将干扰信号数量加1
  if (currentSignalCounter > 1)  /* possible collision 当前存在其他干扰信号，则产生冲突*/
  {
    NS_LOG_DEBUG("PrevalidateIncomingPkt: packet error");
    //将不满足传输条件的数据包标记为Error数据包
    asHeader.SetErrorFlag(true);
    // Set the collision flag to true as well
    // NS_LOG_UNCOND(this<<"At time "<<  Simulator::Now() <<" Phy Packet Dropped because of influence the packetId is "<< p->GetUid() << " node id is " << nodeId+1 << " influenced by packet " << currentSignalid);
    PhyLossTrace(p,Simulator::Now().GetSeconds(),(dist <= m_relRange)? 1: 4,nodeId,asHeader);
    if (dist <= m_relRange)
      PhyCounterTrace(asHeader.GetNetDataType(), 1);
    m_collision_flag = true;
    m_rxCollTrace();
    // 已被干扰的信号，直接在结束时将信号计数减一
    Simulator::Schedule(CalcTxTime(asHeader.GetSize()), &AquaSimPhyCmn::InterferenceSignalEnd, this);
    p = 0;
    return NULL;
  }
  currentSignalid = p->GetUid();
  //大于可靠通信距离的包，也直接丢弃
  if(dist > m_relRange){
    NS_LOG_DEBUG("PrevalidateIncomingPkt: packet error");
    asHeader.SetErrorFlag(true);
    // std::cout<<"Phy Packet Dropped because of distance the packetId is "<< p->GetUid() << " \n";
    PhyLossTrace(p->Copy(),Simulator::Now().GetSeconds(),4,nodeId,asHeader);
  }

  if(GetNetDevice()->GetTransmissionStatus() == NIDLE){
    //现将当前的设备状态修改为RECV
    GetNetDevice()->SetTransmissionStatus(RECV);
    // Simulator::Schedule(CalcTxTime(asHeader.GetSize()), &AquaSimPhyCmn::InterferenceSignalEnd, this);
  }  

  MacHeader mach;
  p->PeekHeader(mach);
  if(mach.GetDemuxPType() == MacHeader::UWPTYPE_LOC) {
    GetNetDevice()->GetMacLoc()->SetPr(pstamp.GetPr());
  }

	// Get recv power for mac_routing dev
	if (mach.GetDemuxPType() == MacHeader::UWPTYPE_MAC_LIBRA)
	{
		MacLibraHeader mac_libra_h;

		p->RemoveHeader(mach);
		p->RemoveHeader(mac_libra_h);

		mac_libra_h.SetRxPower(pstamp.GetPr());

		p->AddHeader(mac_libra_h);
		p->AddHeader(mach);
	}

  p->AddHeader(asHeader);
  // If the packet is not marked as collided (error flag is false), then delay the packet on the TxTime, to make sure that no other packets cause
  // collisions to this packet. If some packets appear within the TxTime delay interval of the given packet, then mark it as collided as well.
  if (asHeader.GetErrorFlag() == false)
  {
    // 可被接收且当前未被干扰，在结束时再判断是否会有干扰
    Simulator::Schedule(CalcTxTime(asHeader.GetSize()),&AquaSimPhyCmn::CollisionCheck, this, p->Copy());
    return NULL;
  }else
  {
    // 已被干扰的信号，直接在结束时将信号计数减一
    Simulator::Schedule(CalcTxTime(asHeader.GetSize()), &AquaSimPhyCmn::InterferenceSignalEnd, this);
  }
  return p;
}

/**
* 给信号计数减1，如果此时信号数量为0，信道才能重新变为空闲
*/
void
AquaSimPhyCmn::InterferenceSignalEnd() {
  currentSignalCounter -= 1;
  if (currentSignalCounter == 0) {
    GetNetDevice()->SetTransmissionStatus(NIDLE);
    m_collision_flag = false;
  }
}

/**
* pass packet p to channel
*/
bool
AquaSimPhyCmn::PktTransmit(Ptr<Packet> p, int channelId) {
  NS_LOG_FUNCTION(this << p);

  AquaSimPacketStamp pstamp;
  AquaSimHeader asHeader;
  p->RemoveHeader(pstamp);  //awkward but for universal encapsulation.
  p->PeekHeader(asHeader);

  //下面的代码用于判断数据包到达时，设备的当前状态
  //当前不需要考虑设备所在的状态
  /*
  *  Stamp the packet with the interface arguments
  */
  StampTxInfo(p);

  Time txSendDelay = this->CalcTxTime(asHeader.GetSize(), &m_modulationName );
  // Simulator::Schedule(txSendDelay, &AquaSimNetDevice::SetTransmissionStatus, GetNetDevice(), NIDLE);
  //Simulator::Schedule(txSendDelay, &AquaSimPhyCmn::SetPhyStatus, this, PHY_IDLE);
  /**
  * here we simulate multi-channel (different frequencies),
  * not multiple tranceiver, so we pass the packet to channel_ directly
  * p' uw_txinfo_ carries channel frequency information
  *
  * NOTE channelId must be set by upper layer and AquaSimPhyCmn::Recv() should be edited accordingly.
  */
  NotifyTx(p);
  m_txLogger(p,Simulator::Now().GetSeconds(),GetNetDevice()->GetNode()->GetId(),asHeader);
  return m_channel.at(channelId)->Recv(p, this);
}

/**
* send packet to upper layer, supposed to be MAC layer,
* but actually go to any specificed module.
*/
void
AquaSimPhyCmn::SendPktUp(Ptr<Packet> p)
{
  NS_LOG_FUNCTION(this);
  AquaSimHeader ash;
  MacHeader mach;
	p->RemoveHeader(ash);
  p->PeekHeader(mach);
  p->AddHeader(ash);

  m_txLogger(p,Simulator::Now().GetSeconds(),GetNetDevice()->GetNode()->GetId(),ash);
  PhyCounterTrace(ash.GetNetDataType(), 0);
  // uint8_t direct = ash.GetDirection();
  //This can be shifted to within the switch to target specific packet types.
  if (GetNetDevice()->IsAttacker()){
    GetNetDevice()->GetAttackModel()->Recv(p);
    return;
  }

  switch (mach.GetDemuxPType()){
  case MacHeader::UWPTYPE_OTHER:
    if(m_device->MacEnabled())
      if (!GetMac()->RecvProcess(p))
        NS_LOG_DEBUG(this << "Mac Recv error");
    break;

  // Add case for MAC_LIBRA mac type
  case MacHeader::UWPTYPE_MAC_LIBRA:
    if(m_device->MacEnabled())
      if (!GetMac()->RecvProcess(p))
        NS_LOG_DEBUG(this << "Mac Recv error");
    break;

  case MacHeader::UWPTYPE_LOC:
    GetNetDevice()->GetMacLoc()->Recv(p);
    break;
  case MacHeader::UWPTYPE_SYNC:
    GetNetDevice()->GetMacSync()->RecvSync(p);
    break;
  case MacHeader::UWPTYPE_SYNC_BEACON:
    GetNetDevice()->GetMacSync()->RecvSyncBeacon(p);
    break;
  case MacHeader::UWPTYPE_NDN:
    GetNetDevice()->GetNamedData()->Recv(p);
    break;
  default:
    NS_LOG_DEBUG("SendPKtUp: Something went wrong.");
  }
}

/**
* process packet with signal cache, looking for collisions and SINR
*/
void
AquaSimPhyCmn::SignalCacheCallback(Ptr<Packet> p) {
  NS_LOG_FUNCTION(this << p);
  if(m_PoweredOn == false){
    return;
  }
  NS_LOG_DEBUG("PhyCmn::SignalCacheCallback: device(" << GetNetDevice()->GetAddress()
            << ") p_id:" << p->GetUid() << " at:" << Simulator::Now().GetSeconds()<<"\n");
  //TODO check for packet collision at signal cache before calling this

  AquaSimHeader asHeader;
  p->RemoveHeader(asHeader);
  asHeader.SetTxTime(Seconds(0.01));	//arbitrary processing time here
  p->AddHeader(asHeader);

  pktRecvCounter++; //debugging...

  SendPktUp(p);
}

void
AquaSimPhyCmn::PowerOn() {
  NS_LOG_FUNCTION(this);

  if (GetNetDevice()->GetTransmissionStatus() == DISABLE)
    NS_LOG_FUNCTION(this << " Node " << GetNetDevice()->GetNode() << " is disabled.");
  else
  {
    m_PoweredOn = true;
    GetNetDevice()->SetTransmissionStatus(NIDLE);
    //SetPhyStatus(PHY_IDLE);
    if (EM() != NULL) {
	    //minus the energy consumed by power on
	    EM()->SetEnergy(std::max(0.0, EM()->GetEnergy() - m_EnergyTurnOn));
	    m_updateEnergyTime = std::max(Simulator::Now().GetSeconds(), m_updateEnergyTime);
    }
  }
}

void
AquaSimPhyCmn::PowerOff() {
  NS_LOG_FUNCTION(this);
  if(m_PoweredOn == false){
    return;
  }
  if (GetNetDevice()->GetTransmissionStatus() == DISABLE)
    NS_LOG_FUNCTION(this << " Node " << GetNetDevice()->GetNode() << " is disabled.");
  else
  {
    m_PoweredOn = false;
    GetNetDevice()->SetTransmissionStatus(SLEEP);
    //SetPhyStatus(PHY_SLEEP);
    if (EM() == NULL)
	    return;
    //minus the energy consumed by power off
    EM()->SetEnergy(std::max(0.0, EM()->GetEnergy() - m_EnergyTurnOff));

    if (Simulator::Now().GetSeconds() > m_updateEnergyTime) {
      EM()->DecrIdleEnergy(Simulator::Now().GetSeconds() - m_updateEnergyTime);
      m_updateEnergyTime = Simulator::Now().GetSeconds();
    }
  }
}

bool
AquaSimPhyCmn::IsPoweredOn() {
  return m_PoweredOn;
}

void
AquaSimPhyCmn::Dump(void) const
{
  NS_LOG_DEBUG("AquaSimPhyCmn Dump: Channel_default(" << m_channel.at(0) << ") " <<
	       "Pt(" << m_pT << ") " <<
	       //"Gt(" << m_ant->GetTxGain(0, 0, 0, m_lambda) << ") " <<
	       "lambda(" << m_lambda << ") " <<
	       "L(" << m_L << ")\n");
}

void
AquaSimPhyCmn::StatusShift(double txTime) {
  double endTime = Simulator::Now().GetSeconds() + txTime;
  /*  The receiver is receiving a packet when the
  transmitter begins to transmit a data.
  We assume the half-duplex mode, the transmitter
  stops the receiving process and begins the sending
  process.
  */
  if (m_updateEnergyTime < endTime)
  {
    double overlapTime = m_updateEnergyTime - Simulator::Now().GetSeconds();
    double actualTxTime = endTime - m_updateEnergyTime;
    EM()->DecrEnergy(overlapTime, EM()->GetTxPower() - EM()->GetRxPower());
    EM()->DecrTxEnergy(actualTxTime);
    m_updateEnergyTime = endTime;
  }
  else {
    double overlapTime = txTime;
    EM()->DecrEnergy(overlapTime, EM()->GetTxPower() - EM()->GetRxPower());
  }
}

/**
* @para ModName the name of selected modulation scheme
*	@return     NULL if ModName cannot be found in m_modulations
*	@return		a pointer to the corresponding AquaSimModulation obj
*/
Ptr<AquaSimModulation>
AquaSimPhyCmn::Modulation(std::string * modName) {
  if (m_modulations.size() == 0) {
    NS_LOG_WARN("No modulations\n");
    return NULL;
  }
  else if (modName == NULL) {
    modName = &m_modulationName;
    auto pos = m_modulations.find(*modName);
    if (m_modulations.end() != pos) {
      return pos->second;
    }
    else {
      NS_LOG_WARN("Failed to locate modulation " << modName->c_str() << "\n");
      return NULL;
    }
  }
  return NULL;
}

void
AquaSimPhyCmn::EnergyDeplete() {
  //NS_LOG_FUNCTION(this);
  //NS_LOG_DEBUG("Energy is depleted on node " << GetNetDevice()->GetNode());

  //TODO fix energy model and then allow this   SetPhyStatus(PHY_DISABLE);
}

/**
 * calculate transmission time of a packet of size pktsize
 * we consider the preamble
*/
Time
AquaSimPhyCmn::CalcTxTime (uint32_t pktSize, std::string * modName)
{
  //NS_ASSERT(modName == NULL);
  return Time::FromDouble(m_modulations.find(m_modulationName)->second->TxTime(pktSize*8), Time::S)
      + Time::FromInteger(Preamble(), Time::S);
}

double
AquaSimPhyCmn::CalcPktSize (double txTime, std::string * modName)
{
  return Modulation(modName)->PktSize (txTime - Preamble()) / 8.;
}

int
AquaSimPhyCmn::PktRecvCount()
{
  return pktRecvCounter;
}

int
AquaSimPhyCmn::PktInRecvCount()
{
  return incPktCounter;
}

void AquaSimPhyCmn::DoDispose()
{
  NS_LOG_FUNCTION(this);
  m_sC->Dispose();
  m_sC=0;
  m_sinrChecker=0;
  for (std::map<const std::string, Ptr<AquaSimModulation> >::iterator it=m_modulations.begin(); it!=m_modulations.end(); ++it)
    it->second=0;
  AquaSimPhy::DoDispose();
}

void AquaSimPhyCmn::SetTransRange(double range)
{
  NS_LOG_FUNCTION(this);
  m_transRange = range;
}

double AquaSimPhyCmn::GetTransRange()
{
  return m_transRange;
}

// Check collision status of the first received packet when net_device was in IDLE state
void 
AquaSimPhyCmn::CollisionCheck(Ptr<Packet> packet)
{
  AquaSimHeader asHeader;
  packet->RemoveHeader(asHeader);
  // If some packets were receved during the TxTime of the original packet, then mark it as collided as well
  //put the packet into the incoming queue  
  asHeader.SetErrorFlag(m_collision_flag);
  uint32_t nodeId = GetNetDevice() -> GetNode() -> GetId();
  if(m_collision_flag == true){
    NS_LOG_FUNCTION(this<<"Phy Packet was influenced, the packetId is "<< packet->GetUid() << " node id is " << nodeId+1);
    PhyLossTrace(packet,Simulator::Now().GetSeconds(),1,nodeId,asHeader);
    PhyCounterTrace(asHeader.GetNetDataType(), 1);
  }
  // else
  // {
  //   std::cout<<"Phy Packet wasn't influenced, the packetId is "<< packet->GetUid() << " node id is " << nodeId+1 << " \n";
  // }
  // 将干扰信号减1
  InterferenceSignalEnd();
  packet->AddHeader(asHeader);
  m_sC->AddNewPacket(packet);
}

int64_t
AquaSimPhyCmn::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  return 0;
}
