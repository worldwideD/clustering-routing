#ifndef DOLPHINCOMMON_H
#define DOLPHINCOMMON_H

#include "json.hpp"

#include "ns3/aqua-sim-header.h"
#include "ns3/aqua-sim-helper.h"
#include "ns3/core-module.h"
#include "ns3/dolphin-turning-helper.h"
#include "ns3/dolphin_trace.h"
#include "ns3/log.h"
#include "ns3/net-device-container.h"

#include <cstdint>
#include <string>
#include <vector>
using json = nlohmann::json;

namespace ns3
{

#define DEFAULT_SENDER 0
#define DEFAULT_RECEIVER 1

struct LossRateConfig
{
    uint64_t networking_completion_s;
    uint64_t packet_num;
};

struct DolphinConfig
{
    std::string setting_path;
    bool printLoggingInfo;
    json settings;
    uint64_t networking_completion_s;
    uint64_t simulation_time_s;
    uint64_t deploy_nodes_Interval_s;
    uint16_t node_num;
    std::vector<uint16_t> node_ids;
    int leader_id;
    double long_nodes_rate;

    std::vector<uint16_t> senders;
    std::vector<uint16_t> receivers;
    std::vector<std::pair<int, int>> src_dst_tx_list; // sender-receiver pair

    uint8_t sink;

    struct CommonConfig
    {
        std::map<std::string, LossRateConfig> loss_rate;
    } common_config;

    struct MobilityConfig
    {
        std::vector<double> mobilityEndTimeList;
    } mobility_config;

    struct PhyConfig
    {
        double relRange;
        double maxRange;
        double RxThresh;
        double CPThresh;
        double freq;
        double lossRate;
        double txRange;
        double rxRange;
        double shortRange;
        double longRange;
    } phy_config;

    struct MacConfig
    {
        double tdma_slot_duration;
        double tdma_guard_time;
        uint32_t data_max_length;
        uint16_t TdmaSlotNum;
        uint32_t data_packet_size;
        uint32_t loc_packet_size;
        std::vector<int> slot_num_table;
        std::map<int, std::vector<int>> nodeTimeSlots;
    } mac_config;

    struct NetConfig
    {
        // std::string RouteTablePath;
        // int MaxFw;
        // double nb_out;
        // double nb_slot;
        // double pkt_t1_out;
        // double pkt_t2_out;
        // double pkt_slot;
        // std::string InitialNeighborString;
        
        std::string m_routingProtocol;
        double helloInterval;
        double omega1, omega2, omega3;
        double gamma1, gamma2;
        double delta1, delta2;
        double alpha, mu, lambda, prob;
        double maxSpeed;
        double formingTime;
        double routingTime;
        std::vector<Vector> sinkPos;

        double minX, maxX, minY, maxY, minZ, maxZ;
        int regularCH;
    } net_config;

    struct AppConfig
    {
        uint64_t packet_num;
        uint32_t packet_size;
        double data_rate_bps;
        double send_start_s;
        double send_interval_s;
        Callback<void, Ptr<const Packet>, uint64_t, uint64_t, uint8_t> rx_cb;
        Callback<void, Ptr<const Packet>, uint64_t, uint64_t, uint8_t> tx_cb;
    } app_config;
};

int ParseConfig(DolphinConfig* config, int cmd_argc, char* cmd_argv[]);
int InitModulesLog(DolphinConfig* config);
int InitModules(DolphinConfig* config, AquaSimHelper& asHelper);
int CloseModules();
int CloseModules(DolphinConfig* config);
int ConfigModulesTrace();
NetDeviceContainer DeployNetDevice(Ptr<Node> node,
                                   NetDeviceContainer devices,
                                   AquaSimHelper asHelper,
                                   DolphinConfig* config);
void createApplication(DolphinConfig* config,
                       Ptr<Node> node,
                       uint16_t nodeId,
                       uint16_t rxTxType,
                       uint16_t contentType,
                       Time startTime,
                       Time stopTime,
                       std::string dstList = "");
void ChannelLossTrace(Ptr<Packet> packet,
                      double time_now,
                      int reason,
                      Ptr<AquaSimNetDevice> recver,
                      uint32_t nodeId);
void PhyLossTrace(Ptr<Packet> packet,
                  double time_now,
                  int lossReason,
                  uint32_t nodeId,
                  AquaSimHeader header);
void PhyRxTrace(Ptr<const Packet> packet, double time_now, uint32_t nodeId, AquaSimHeader header);
void PhyTxTrace(Ptr<const Packet> packet, double time_now, uint32_t nodeId, AquaSimHeader header);
void PhyCounterTrace(int typ, int act);
void MacTxTrace(Ptr<const Packet> packet, double time_now, AquaSimAddress address);
void MacRxTrace(Ptr<const Packet> packet, double time_now, AquaSimAddress address);
void MacLOSSTrace(Ptr<const Packet> packet, double time_now, AquaSimAddress address);
void NetTraceBase(TraceAction action,
                  Ptr<const Packet> packet,
                  double time_now,
                  AquaSimAddress address,
                  int loss);
void NetTxTrace(Ptr<const Packet> packet, double time_now, AquaSimAddress address, int up0down1);
void NetRxTrace(Ptr<const Packet> packet, double time_now, AquaSimAddress address, int loss);
void NetCounterTrace(uint8_t t, uint32_t sz);
void NetDelayTrace(uint32_t pid, uint8_t action, double t);
void NetRegularPdrCounter();
void CHRegularCounter(NetDeviceContainer devices);
void NodeInitialNetTrace(int node, double t);
void AppTraceBase(TraceAction action,
                  Ptr<const Packet> packet,
                  uint64_t src_id,
                  uint64_t dst_id,
                  uint64_t node_id,
                  uint8_t packet_type);
void AppTxTrace(Ptr<const Packet> packet, uint64_t src_id, uint64_t dst_id, uint8_t packet_type);
void AppRxTrace(Ptr<const Packet> packet, uint64_t src_id, uint64_t dst_id, uint8_t packet_type);
int findIndex(const std::vector<int>& arr, int target);
std::vector<int> getTimeSlotsForNode(int nodeId,
                                     const std::map<int, std::vector<int>>& nodeTimeSlots);
double CalculateNextSlotTime(double start_time, double slot_size, int slot_count, int node_id);
void TurningLeftSingle(DolphinTurningHelper* helper, Ptr<Node> node, Ptr<Node> center);
void TurningLeftMulti(DolphinTurningHelper* helper, NodeContainer nodesCon, Ptr<Node> center);
int setMobility(DolphinConfig* config, NodeContainer nodesCon, double startTime);

void appReadConfig(DolphinConfig* config, json settings);
std::size_t ParseCommonConfig(DolphinConfig* config, const json& settings);
LossRateConfig* FindLossRateConfig(DolphinConfig* config, double lossRate);
void PrintConfig(DolphinConfig* config);

std::string GetTimestamp();
} // namespace ns3
#endif // DOLPHINCOMMON_H
