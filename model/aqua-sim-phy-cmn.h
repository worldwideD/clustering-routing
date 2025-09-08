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
#ifndef AQUA_SIM_PHY_CMN_H
#define AQUA_SIM_PHY_CMN_H

#include <string>
#include <list>
#include <map>
#include <vector>

#include "ns3/nstime.h"
//#include "ns3/timer.h"
//#include "ns3/event-id.h"
//#include "ns3/packet.h"
#include "ns3/object.h"
#include "ns3/traced-callback.h"

#include "aqua-sim-phy.h"
#include "aqua-sim-sinr-checker.h"
#include "aqua-sim-signal-cache.h"
#include "aqua-sim-energy-model.h"
#include "aqua-sim-modulation.h"

//Aqua Sim Phy Cmn

namespace ns3 {

class Packet;

/**
 * \ingroup aqua-sim-ng
 *
 * \brief Common phy layer class. Currently only supports a single phy component per node.
 */
class AquaSimPhyCmn : public AquaSimPhy
{
public:
  AquaSimPhyCmn(void);
  virtual ~AquaSimPhyCmn(void);
  static TypeId GetTypeId(void);

  virtual void SetSinrChecker(Ptr<AquaSimSinrChecker> sinrChecker);
  virtual void SetSignalCache(Ptr<AquaSimSignalCache> sC);
  virtual void AddModulation(Ptr<AquaSimModulation> modulation, std::string modulationName);

  virtual void Dump(void) const;
  virtual bool Decodable(double noise, double ps);
  virtual void SendPktUp(Ptr<Packet> p);
  virtual bool PktTransmit(Ptr<Packet> p, int channelId = 0);
    /*
     * PktTransmit defaults to single channel if not specified
    */

  virtual void UpdateIdleEnergy(void);

  virtual void PowerOn();
  virtual void PowerOff();
  virtual void StatusShift(double);
  virtual bool IsPoweredOn();

  inline Time CalcTxTime(uint32_t pktsize, std::string * modName = NULL);
  inline double CalcPktSize(double txtime, std::string * modName = NULL);

  virtual void SignalCacheCallback(Ptr<Packet> p);
  virtual bool Recv(Ptr<Packet> p);

  ns3::TracedCallback<Ptr<Packet> , double , int , uint32_t , AquaSimHeader> PhyLossTrace;
  ns3::TracedCallback<int, int> PhyCounterTrace;
  
  virtual inline double GetPt() { return m_pT; }
  virtual inline double GetRXThresh() { return m_RXThresh; }
  virtual inline double GetCSThresh() { return m_CSThresh; }

  virtual Ptr<AquaSimModulation> Modulation(std::string * modName);


  virtual inline double Trigger(void) { return m_trigger; }
  virtual inline double Preamble(void) { return m_preamble; }

  virtual inline double GetEnergySpread(void){ return m_K; }
  virtual inline double GetFrequency(){ return m_freq; }
  virtual inline bool MatchFreq(double freq);
  virtual inline double GetL() const { return m_L; }
  virtual inline double GetLambda() { return m_lambda; }
  // 获取收发角度
  virtual inline double GetTxRange() { return m_txRange; }
  virtual inline double GetRxRange() { return m_rxRange; }

  virtual Ptr<AquaSimSignalCache> GetSignalCache();
  virtual int PktRecvCount();
  virtual int PktInRecvCount();
  int64_t AssignStreams (int64_t stream);

  /*
  * Used for some mac/routing protocols and for restricting packet range within range-propagation for channel module.
  */
  virtual void SetTransRange(double range);
  virtual double GetTransRange();

  // Check collision status of the first received packet when net_device was in IDLE state
  virtual void CollisionCheck(Ptr<Packet> packet);
  void InterferenceSignalEnd();

protected:
  virtual Ptr<Packet> PrevalidateIncomingPkt(Ptr<Packet> p);
  virtual void UpdateTxEnergy(Time txTime);
  virtual void UpdateRxEnergy(Time txTime, bool errorFlag);
  virtual Ptr<Packet> StampTxInfo(Ptr<Packet> p);
  virtual void EnergyDeplete(void);

  //TODO energy model could substitute this and better define it all.
  double m_pT;		// transmitted signal power (W)
  double m_updateEnergyTime;	// the last time we update energy.

  double m_RXThresh;	// receive power threshold (W)
  double m_CSThresh;	// carrier sense threshold (W)
  double m_CPThresh;	// capture threshold (db)

  double m_K;	// energy spread factor
  double m_freq;  // frequency
  double m_L;	// system loss default factor
  double m_lambda;  // wavelength (m), we don't use it anymore

  // preamble and trigger are fixed for a given modem (can be updated in future)
  double m_preamble;
  double m_trigger;

  //可靠传播距离
  double m_relRange;
  //最大传播距离
  double m_maxrange;

  //收发角度
  double m_txRange, m_rxRange;

  /**
  * MUST make sure Pt and tx_range are consistent at the physical layer!!!
  * so set transmission range according to PowerLevels_
  */
  //level of transmission power in an increasing order
  std::vector<double> m_powerLevels;
  int m_ptLevel;

  /**
  * Modulation Schemes. a modem can support multiple modulation schemes
  * map modulation's name to the object
  */
  std::map<const std::string, Ptr<AquaSimModulation> > m_modulations;
  std::string m_modulationName;	//the name of current modulation

  /**
  * cache the incoming signal from channel. it calculates SINR
  * and check collisions.
  */
  Ptr<AquaSimSignalCache> m_sC;
  Ptr<AquaSimSinrChecker> m_sinrChecker;

  double m_EnergyTurnOn;	//energy consumption for turning on the modem (J)
  double m_EnergyTurnOff; //energy consumption for turning off the modem (J)

  //Ptr<AquaSimAntenna> m_ant; // we don't use it anymore, however we need it as an arguments
  /**
  * points to the same object as node_ in class MobileNode
  * but n_ is of type UnderwaterSensorNode * instead of MobileNode *
  */

  double m_transRange;  //max tramission range for modem. **NOTE: Only functional with range-propagation model**.
  bool m_PoweredOn;  //true: power on false:power off

  friend class AquaSimEnergyModel;

  virtual void DoDispose();

private:
  //Ptr<AquaSimMac> m_mac;
  //Ptr<AquaSimEnergyModel> m_eM;

  uint32_t incPktCounter;
  uint32_t outPktCounter;
  int pktRecvCounter;

  ns3::TracedCallback<Ptr<const Packet> , double , uint32_t , AquaSimHeader> m_rxLogger;
  ns3::TracedCallback<Ptr<const Packet> , double , uint32_t , AquaSimHeader> m_txLogger;
  ns3::TracedCallback<> m_rxCollTrace;

  // Collision flag in order to monitor whether there have been incoming packets wihtin the TxTime delay of the original packet.
  // If yes, then mark the original packet as collided as well.
  //这是一个整体的参数，每个设备维护一个
  bool m_collision_flag = false;
  int currentSignalCounter;
  int currentSignalid;

}; //AquaSimPhyCmn

} //namespace ns3

#endif /* AQUA_SIM_PHY_CMN_H */