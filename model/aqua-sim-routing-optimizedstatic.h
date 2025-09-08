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

#ifndef AQUA_SIM_ROUTING_STATIC_H
#define AQUA_SIM_ROUTING_STATIC_H

#include "aqua-sim-address.h"
#include "aqua-sim-routing.h"
// 下面几个是后加的
#include "aqua-sim-channel.h"
#include "aqua-sim-datastructure.h"
#include "aqua-sim-mac-tdma.h"
#include "aqua-sim-routing-vbf.h"

#include "ns3/application.h"
#include "ns3/applications-module.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"
#include "ns3/vector.h"

#include <algorithm>
#include <iostream>
#include <vector>
// extern std::vector<std::vector<int>> InitialNeighbor;

//
#include <map>

namespace ns3
{

/*header length of Static routing*/
#define SR_HDR_LEN (3 * sizeof(AquaSimAddress) + sizeof(int))

/**
 * \ingroup aqua-sim-ng
 *
 * \brief Optimized Static routing implementation
 */

struct gpsr_neighborhood
{
    int neighbor_num;
    int initial_num;
    int find_initial_num;
    std::vector<Vector> neighbor_position;
    std::vector<Time> t_now;
    std::vector<int> alive;
};

class AquaSimGPSRHashTable
{
  public:
    std::map<int, gpsr_neighborhood*> neighbor_table;

    AquaSimGPSRHashTable();
    ~AquaSimGPSRHashTable();

    void SetUp(int my_node, int total_nodes, std::string initial_neighbor_string);
    void CheckNeighborTimeout(int my_node, double Tout, double Tslot, int total_nodes);
    double PutInHash(Ptr<Packet> p, int my_node, int total_nodes);
    gpsr_neighborhood* GetHash(int my_node);
};

struct gpsr_packet
{
    int pkt_num;
    std::vector<uint16_t> pkt_uid;
    std::vector<uint16_t> pkt_bitstring;
    std::vector<Time> pkt_t;
    std::vector<Time> pkt_lastsend_t;
};

class AquaSimGPSRPktTable
{
  public:
    std::map<int, gpsr_packet*> pkt_table;
    AquaSimGPSRPktTable();
    ~AquaSimGPSRPktTable();

    int PutInPkt(Ptr<Packet> p, AquaSimHeader ash, int my_node, Time t1_time_out, Time t2_time_out);
    void CheckBroadcastpktTimeout(int my_node, double Tout, double Tslot);
    void SetUp(int my_node, int total_nodes);
    gpsr_packet* GetPkt(int my_node);
};

class AquaSimOptimizedStaticRouting : public AquaSimRouting
{
  public:
    AquaSimOptimizedStaticRouting();
    AquaSimOptimizedStaticRouting(char* routeFile);
    virtual ~AquaSimOptimizedStaticRouting();
    static TypeId GetTypeId(void);
    int64_t AssignStreams(int64_t stream);

    virtual bool Recv(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);

    void SetRouteTable(char* routeFile);

    uint8_t MaxForwards;

    int m_maxForward;

    TracedCallback<Ptr<const Packet>, double, AquaSimAddress, int> m_txTrace;
    TracedCallback<Ptr<const Packet>, double, AquaSimAddress, int> m_rxTrace;
    TracedCallback<double> m_InitialNetTrace;
    TracedCallback<int,double> m_InitialNodeTrace;

  protected:
    AquaSimGPSRHashTable hashTable;
    AquaSimGPSRPktTable packet_table;

    bool has_set_neighbor;

    bool m_hasSetRouteFile;
    bool m_hasSetNode;
    bool m_hasReadRouteFile;
    char m_routeFile[1000];
    std::string m_routeFile_string;
    std::string m_initial_neighbor_string;

    void ReadRouteTable(char* filename);
    AquaSimAddress FindNextHop(const Ptr<Packet> p);
    uint64_t UpdateBitStringInList(Ptr<Packet> p, uint64_t bitString);
    bool CheckNeighborSent(uint64_t bitstring);

    double m_neighbor_t_out;
    double m_neighbor_t_slot;
    double m_pkt_t1_out;
    double m_pkt_t2_out;
    double m_pkt_t_slot;
    int m_NodeNum;
    uint64_t m_bitstring;

    // 将map改为multimap，以支持多路径路由
  private:
    std::multimap<AquaSimAddress, std::pair<AquaSimAddress, AquaSimAddress>> m_rTable;

}; // class AquaSimOptimizedStaticRouting

} // namespace ns3

#endif /* AQUA_SIM_ROUTING_STATIC_H */