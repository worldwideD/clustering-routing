#include "dolphin-common.h"

#include "json.hpp"

#include "ns3/applications-module.h"
#include "ns3/aqua-sim-ng-module.h"
#include "ns3/aqua-sim-propagation.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/dolphin_trace.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"

#include <filesystem>
#include <fstream>
#include <iomanip> // setprecision
#include <math.h>
#include <random>
#include <regex>
#include <sstream> // stringstream

namespace fs = std::filesystem;

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DolphinCommon");

static std::ofstream traceFile; // 输出配置，写在main前面
static std::ofstream phyChlTraceFile;
static DolphinConfig* g_config;
int controlPktCnt = 0;
int switchCHCnt = 0;
int beCHCnt = 0;
int beCMCnt = 0;
int helloCnt = 0;
int lsCnt = 0;
int controlDataCnt = 0;
int dataPktSent = 0;
int dataPktReceived = 0;
int dataDropForLoop = 0;
int dataPktDataCnt = 0;
std::map<uint32_t, double> pktTime;
double pdrInterval = 0;
int regularPDR = 0;
std::vector<double> pdrs;
double chInterval = 0;
Ptr<UniformRandomVariable> randInt = CreateObject<UniformRandomVariable>();
int regularCH = 0;
std::vector<int> chNums;
std::vector<double> chCovers;
std::vector<double> chRanges;
std::vector<double> chVariances;
double delayCnt = 0;
int helloRx = 0;
int helloCol = 0;
int lsRx = 0;
int lsCol = 0;
int controlRx = 0;
int controlCol = 0;

std::string
ExtractScenario(const std::string& path)
{
    std::string keyword = "dolphin_scenario";

    std::size_t keywordPos = path.find(keyword);

    if (keywordPos != std::string::npos)
    {
        // 从关键字的位置开始提取，直到遇到下一个非字母数字字符
        std::size_t endPos = path.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_",
            keywordPos);

        // 如果没有找到结束符，默认提取到字符串结尾
        if (endPos == std::string::npos)
        {
            return path.substr(keywordPos);
        }

        // 提取从关键字开始到结束符之间的部分
        return path.substr(keywordPos, endPos - keywordPos);
    }

    // 如果没有找到关键字，返回空字符串
    return keyword;
}

void
ensureDirectoryExists(const std::string& filePath)
{
    // 获取文件路径的目录部分
    fs::path path(filePath);
    fs::path dir = path.parent_path(); // 获取目录部分
    std::cout << dir << std::endl;

    // 检查目录是否存在
    if (!fs::exists(dir))
    {
        // 如果目录不存在，则创建目录
        try
        {
            if (!fs::create_directory(dir))
            {
                std::cerr << "Failed to create directory: " << dir << std::endl;
                // std::exit(EXIT_FAILURE);
            }
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "Error creating directory: " << e.what() << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }
}

static void
InitConfig(DolphinConfig* config)
{
    config->printLoggingInfo = true;
    config->networking_completion_s = 0;
    config->simulation_time_s = 0;
    config->deploy_nodes_Interval_s = 0;
    config->node_num = 0;
    config->leader_id = -1;
    config->sink = 0;

    config->phy_config.relRange = 0;
    config->phy_config.maxRange = 0;
    config->phy_config.txRange = 0;
    config->phy_config.rxRange = 0;
    config->phy_config.RxThresh = 0;
    config->phy_config.CPThresh = 0;
    config->phy_config.freq = 0;
    config->phy_config.lossRate = 0;

    config->mac_config.tdma_slot_duration = 0;
    config->mac_config.tdma_guard_time = 0;
    config->mac_config.data_max_length = 0;
    config->mac_config.TdmaSlotNum = 0;

    config->app_config.send_start_s = 0;
    config->app_config.send_interval_s = 0;
    config->app_config.packet_num = 0;
    config->app_config.packet_size = 0;
    config->app_config.data_rate_bps = 0;

    config->net_config.maxSpeed = 0;
    config->net_config.helloInterval = 0;
    config->net_config.regularCH = 0;

    pktTime.clear();
    pdrs.clear();
    chNums.clear();
    chCovers.clear();
    chRanges.clear();
    chVariances.clear();
}

std::vector<uint16_t>
ParseIntegerArray(const std::string& str)
{
    std::vector<uint16_t> result;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        result.push_back(std::stoi(token));
    }
    return result;
}

std::vector<std::pair<int, int>>
ParsePointList(std::string pointsInput)
{
    std::vector<std::pair<int, int>> points;

    // Trim leading and trailing spaces if necessary
    pointsInput.erase(0, pointsInput.find_first_not_of(" \t\n\r"));
    pointsInput.erase(pointsInput.find_last_not_of(" \t\n\r") + 1);

    // 解析输入字符串
    std::stringstream ss(pointsInput);
    std::string pair_str;
    while (std::getline(ss, pair_str, ';'))
    {
        // Trim spaces around the pair string
        pair_str.erase(0, pair_str.find_first_not_of(" \t\n\r"));
        pair_str.erase(pair_str.find_last_not_of(" \t\n\r") + 1);

        std::stringstream pair_ss(pair_str);
        std::string first, second;

        // Parse the pair
        if (std::getline(pair_ss, first, ',') && std::getline(pair_ss, second, ','))
        {
            try
            {
                int x = std::stoi(first);
                int y = std::stoi(second);
                points.emplace_back(x, y);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Error parsing point: " << pair_str << " -> " << e.what() << std::endl;
            }
        }
        else
        {
            std::cerr << "Invalid format: " << pair_str << std::endl;
        }
    }
    return points;
}

int
ParseConfig(DolphinConfig* config, int cmd_argc, char* cmd_argv[])
{
    NS_LOG_UNCOND(cmd_argv[0]);
    g_config = config;
    InitConfig(config);

    uint32_t runNumber = 1;
    double lossRate_cmd = -1;
    int32_t packet_num_cmd = -1;
    int32_t leader_id = -1;
    std::string senders_str = "";
    std::string receivers_str = "";
    std::string pointsInput_str = "";

    CommandLine cmd;
    cmd.AddValue("setting", "Setting file path", config->setting_path);
    cmd.AddValue("runNumber", "Run Number", runNumber);
    cmd.AddValue("lossrate", "Lossrate", lossRate_cmd);
    cmd.AddValue("packetnum", "Send packet num", packet_num_cmd);
    cmd.AddValue("leaderid", "Leader id", leader_id);
    cmd.AddValue("senders", "List of senders(default:1,begin with 1)", senders_str);
    cmd.AddValue("receivers", "List of receviers(default:1)", receivers_str);
    cmd.AddValue("printLoggingInfo", "Enable logging information", config->printLoggingInfo);
    cmd.AddValue("srcDstTxList", "List of point pairs in 'x,y x,y' format", pointsInput_str);
    cmd.AddValue("regularPDR", "If regularly record PDR", regularPDR);
    cmd.AddValue("pdrInterval", "PDR record interval", pdrInterval);
    cmd.AddValue("regularCH", "If regularly record PDR", regularCH);
    cmd.AddValue("CHInterval", "PDR record interval", chInterval);

    cmd.Parse(cmd_argc, cmd_argv);

    RngSeedManager::SetSeed(time(nullptr)); // 设置全局种子
    RngSeedManager::SetRun(runNumber);      // 设置不同的运行号可以生成不同序列

    std::ifstream settings_file(config->setting_path);
    if (!settings_file.is_open())
    {
        std::cerr << "无法打开 settings.json(" << config->setting_path << ")" << std::endl;
        return -1;
    }
    json settings;
    settings_file >> settings;
    config->settings = settings;

    // 解析 JSON 配置项

    // 解析common配置项
    ParseCommonConfig(config, settings);

    /* main程序配置项 begin */
    // 读取初始组网完成时间T_N(秒)
    config->networking_completion_s = settings["initialNetworkingCompletion"];
    if (regularPDR == 1)
    {
        Simulator::Schedule(Seconds(config->networking_completion_s + pdrInterval), &NetRegularPdrCounter);
    }
    if (regularCH == 1)
    {
        config->net_config.regularCH = 1;
    }
    config->deploy_nodes_Interval_s = settings["deployNodesInterval"];
    config->node_num = settings["node_num"];
    int32_t leader_id_setting = -1;
    if (settings.contains("leader_id"))
        leader_id_setting = settings["leader_id"];
    config->leader_id = (leader_id == -1) ? leader_id_setting : leader_id;
    config->simulation_time_s = settings["simulation_time"];
    if (settings.contains("sink"))
    {
        config->sink = settings["sink"];
        config->net_config.sinkPos.clear();
        for (int i = 0; i < config->sink; ++i)
            config->net_config.sinkPos.emplace_back(Vector(settings["node_positions"][i][0],
                                                           settings["node_positions"][i][1],
                                                           settings["node_positions"][i][2]));
    }
    config->simulation_time_s += config->deploy_nodes_Interval_s * config->node_num;

    if (pointsInput_str.empty())
    {
        switch (config->node_num)
        {
        case 3:
            config->src_dst_tx_list = {{0, 1}, {1, 2}, {2, 1}};
            break;
        case 4:
            config->src_dst_tx_list = {{1, 0}, {2, 3}};
            // config->src_dst_tx_list = {{0, 3}, {1, 0}, {2, 3}};
            break;
        case 6:
            // qua
            config->src_dst_tx_list = {{3, 2}, {4, 1}};
            // config->src_dst_tx_list = {{1, 2}, {2, 5}, {5, 1}};
            // triangle
            config->src_dst_tx_list = {{2, 1}, {3, 5}};
            // config->src_dst_tx_list = {{1, 4}, {4, 5}, {5, 1}};
            break;
        case 10:
            // TODO
            config->src_dst_tx_list = {{1, 0}, {2, 3}};
            // config->src_dst_tx_list = {{0, 3}, {1, 0}, {2, 3}};
            break;
        default:
            config->src_dst_tx_list = {{1, 0}};
            break;
        }
    }

    else
    {
        config->src_dst_tx_list = ParsePointList(pointsInput_str);
    }
    config->src_dst_tx_list.erase(
        std::remove_if(config->src_dst_tx_list.begin(),
                       config->src_dst_tx_list.end(),
                       [&config](const std::pair<int, int>& p) {
                           return (p.first > config->node_num ||
                                   (p.second != 255 && p.second > config->node_num));
                       }),
        config->src_dst_tx_list.end());
    NS_LOG_UNCOND("Filtered Point Pairs:");
    for (const auto& pair : config->src_dst_tx_list)
    {
        NS_LOG_UNCOND("(" << pair.first << ", " << pair.second << ")");
    }
    if (config->src_dst_tx_list.empty())
    {
        std::cerr << "no points settings" << std::endl;
        return -1;
    }
    if (senders_str.empty())
    {
        if (settings.contains("senders"))
        {
            config->senders = settings["senders"].get<std::vector<uint16_t>>();
        }
        else
            config->senders.push_back(DEFAULT_SENDER);
    }
    else
    {
        config->senders = ParseIntegerArray(senders_str);
    }
    for (auto it = config->senders.begin(); it != config->senders.end();)
    {
        if (*it > config->node_num && *it != DolphinApplication::broadcastId)
        {
            it = config->senders.erase(it);
        }
        else
        {
            // 如果不是广播，nodeid-1
            if (*it != DolphinApplication::broadcastId)
                *it -= 1;
            ++it; // 移动到下一个元素
        }
    }
    if (config->senders.empty())
    {
        std::cerr << "no senders settings" << std::endl;
        return -1;
    }
    if (receivers_str.empty())
    {
        if (settings.contains("receivers"))
        {
            config->receivers = settings["receivers"].get<std::vector<uint16_t>>();
        }
        else
            config->receivers.push_back(DEFAULT_RECEIVER);
    }
    else
    {
        config->receivers = ParseIntegerArray(receivers_str);
    }
    for (auto it = config->receivers.begin(); it != config->receivers.end();)
    {
        if (*it > config->node_num && *it != DolphinApplication::broadcastId)
        {
            it = config->receivers.erase(it);
        }
        else
        {
            // 如果不是广播，nodeid-1
            if (*it != DolphinApplication::broadcastId)
                *it -= 1;
            ++it; // 移动到下一个元素
        }
    }
    if (config->receivers.empty())
    {
        std::cerr << "no receivers settings" << std::endl;
        return -1;
    }

    if (settings.contains("node_ids"))
    {
        config->node_ids = settings["node_ids"].get<std::vector<uint16_t>>();
    }
    else
    {
        config->node_ids.reserve(config->node_num); // 预分配内存，优化性能

        for (uint16_t i = 1; i <= config->node_num; ++i)
        {
            config->node_ids.push_back(i);
        }
    }

    /* main程序配置项 end */

    /* 物理层配置 begin */
    config->phy_config.relRange = settings["relRange"];
    config->phy_config.maxRange = settings["maxRange"];
    config->phy_config.txRange = settings["txRange"];
    config->phy_config.rxRange = settings["rxRange"];
    config->phy_config.RxThresh = settings["RxThresh"];
    config->phy_config.CPThresh = settings["CPThresh"];
    config->phy_config.freq = settings["frequency"];
    config->phy_config.shortRange = settings["short_range"];
    config->phy_config.longRange = settings["long_range"];
    double lossRateSetting = settings["lossRate"];
    config->phy_config.lossRate = (lossRate_cmd == -1) ? lossRateSetting : lossRate_cmd;
    /* 物理层配置 end */

    /* mac配置 begin */
    config->mac_config.tdma_slot_duration = settings["tdma_slot_duration"];
    config->mac_config.tdma_guard_time = settings["tdma_guard_time"];
    config->mac_config.data_max_length = settings["data_max_length"];
    config->mac_config.TdmaSlotNum = settings["TdmaSlotNum"];
    config->mac_config.data_packet_size = settings["data_packet_size"];
    config->mac_config.loc_packet_size = settings["loc_packet_size"];
    config->mac_config.slot_num_table = settings["SlotNumTable"].get<std::vector<int>>();
    json timeSlotsConfig = settings["time_slots"];
    for (size_t timeSlot = 0; timeSlot < timeSlotsConfig.size(); ++timeSlot)
    {
        for (int node : timeSlotsConfig[timeSlot])
        {
            if (node != -1)
            { // 跳过 -1 表示的空时隙
                config->mac_config.nodeTimeSlots[node].push_back(
                    timeSlot); // 使用 timeSlot + 1 表示时隙编号（从1开始）
            }
        }
    }

    /* mac配置 end */

    /* 网络层配置 begin */
    config->net_config.m_routingProtocol = settings["routing_protocol"];
    if (config->net_config.m_routingProtocol == "ns3::DolphinRoutingClustering" || config->net_config.m_routingProtocol == "ns3::DolphinRoutingLEACH")
    {
        config->net_config.helloInterval = settings["hello_interval"];
        config->net_config.omega1 = settings["omega"][0];
        config->net_config.omega2 = settings["omega"][1];
        config->net_config.omega3 = settings["omega"][2];
        config->net_config.gamma1 = settings["gamma"][0];
        config->net_config.gamma2 = settings["gamma"][1];
        config->net_config.delta1 = settings["delta"][0];
        config->net_config.delta2 = settings["delta"][1];
        config->net_config.lambda = settings["lambda"];
        config->net_config.alpha = settings["alpha"];
        config->net_config.mu = settings["mu"];
        config->net_config.prob = settings["prob"];
        config->net_config.maxSpeed = settings["max_speed"];
        config->net_config.formingTime = settings["forming_time"];
        config->net_config.routingTime = settings["routing_time"];
        config->long_nodes_rate = settings["longNodes_rate"];
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingPAUV")
    {
        config->net_config.minX = settings["MinX"];
        config->net_config.maxX = settings["MaxX"];
        config->net_config.minY = settings["MinY"];
        config->net_config.maxY = settings["MaxY"];
        config->net_config.minZ = settings["MinZ"];
        config->net_config.maxZ = settings["MaxZ"];
        config->long_nodes_rate = settings["longNodes_rate"];
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingSOAM")
    {
        config->net_config.helloInterval = settings["hello_interval"];
        config->net_config.formingTime = settings["forming_time"];
        config->net_config.routingTime = settings["routing_time"];
        config->long_nodes_rate = settings["longNodes_rate"];
        config->net_config.maxSpeed = settings["max_speed"];
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingRCRP")
    {
        config->net_config.helloInterval = settings["hello_interval"];
        config->net_config.alpha = settings["alpha"];
        config->net_config.prob = settings["prob"];
        config->long_nodes_rate = settings["longNodes_rate"];
        config->net_config.maxSpeed = settings["max_speed"];
    }else if (config->net_config.m_routingProtocol == "ns3::RoutingALRP")
    {
        config->net_config.helloInterval = settings["hello_interval"];
        config->net_config.prob = settings["prob"];
        config->long_nodes_rate = settings["longNodes_rate"];
        config->net_config.maxSpeed = settings["max_speed"];
        config->net_config.routingTime = settings["routing_time"];
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingERNC")
    {
        config->net_config.helloInterval = settings["hello_interval"];
        config->long_nodes_rate = settings["longNodes_rate"];
        config->net_config.maxSpeed = settings["max_speed"];
        config->net_config.minX = settings["MinX"];
        config->net_config.maxX = settings["MaxX"];
        config->net_config.minY = settings["MinY"];
        config->net_config.maxY = settings["MaxY"];
        config->net_config.minZ = settings["MinZ"];
        config->net_config.maxZ = settings["MaxZ"];
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingAEWCA")
    {
        config->net_config.helloInterval = settings["hello_interval"];
        config->net_config.omega1 = settings["omega"][0];
        config->net_config.omega2 = settings["omega"][1];
        config->net_config.omega3 = settings["omega"][2];
        config->net_config.maxSpeed = settings["max_speed"];
        config->net_config.formingTime = settings["forming_time"];
        config->net_config.routingTime = settings["routing_time"];
        config->long_nodes_rate = settings["longNodes_rate"];
    }
    // config->net_config.RouteTablePath = settings["MultiRouteTableFile"];
    // config->net_config.MaxFw = settings["max_forwards"];
    // config->net_config.nb_out = settings["neighbor_time_out"];
    // config->net_config.nb_slot = settings["neighbor_time_interval"];
    // config->net_config.pkt_t2_out = settings["packet_time_out"];
    // config->net_config.pkt_t1_out = settings["broad_timeout_T1_s"];
    // config->net_config.pkt_slot = settings["packet_time_interval"];
    // // if (config->node_num <= 10)
    // //{
    // std::vector<std::vector<int>> InitialNeighbor =
    //     settings["initial_neighbor"].get<std::vector<std::vector<int>>>();

    // std::stringstream InitialNeighborChar;
    // for (int i = 0; i < config->node_num; i++)
    // {
    //     int result = 0;
    //     for (int j = config->node_num - 1; j >= 0; j--)
    //     {
    //         if (InitialNeighbor[i][j] != 0)
    //         {
    //             result = result + std::pow(2, j);
    //         }
    //     }
    //     InitialNeighborChar << result;
    //     if (i < config->node_num - 1)
    //     {
    //         InitialNeighborChar << ",";
    //     }
    // }
    // config->net_config.InitialNeighborString = InitialNeighborChar.str();
    //}

    /* 网络层配置 end */

    /* 应用层配置项 begin */
    // appReadConfig(config, settings);
    // 应用层发包周期长度=时隙数量*（发送时隙长度+保护时隙长度）,25*（6+6）
    config->app_config.send_interval_s =
        config->mac_config.TdmaSlotNum *
        (config->mac_config.tdma_slot_duration + config->mac_config.tdma_guard_time);
    // 领航节点发包轮次存储在packet_num中，优先使用命令行参数中设置，其次为common配置中丢包率所对应的packet_num,最后使用配置中packet_num
    if (settings.contains("packet_num"))
        config->app_config.packet_num = settings["packet_num"];
    config->app_config.packet_num =
        (packet_num_cmd != -1) ? packet_num_cmd : config->app_config.packet_num;
    if (settings.contains("packet_size"))
        config->app_config.packet_size = settings["packet_size"];
    if (settings.contains("data_rate_bps"))
        config->app_config.data_rate_bps = settings["data_rate_bps"];
    else
    {
        config->app_config.data_rate_bps = 0;
    }
    /* 应用层配置项 end */

    uint32_t lossrate_int = static_cast<uint32_t>(config->phy_config.lossRate * 100);

    // 从common中读取与lossrate匹配的参数
    LossRateConfig* foundConfig = FindLossRateConfig(config, config->phy_config.lossRate);
    if (foundConfig)
    {
        config->networking_completion_s = foundConfig->networking_completion_s;
        if (config->app_config.packet_num == 0)
            config->app_config.packet_num = foundConfig->packet_num;
    }
    config->app_config.send_start_s = config->networking_completion_s;
    PrintConfig(config);

    // trace file
    std::string setting_trace_file_path = settings["traceFile"];
    // 正则匹配 "-数字/" 这种模式
    std::regex pattern(R"(-\d+/)");
    std::string replacement = std::to_string(config->node_num)
                              + "_" + std::to_string((int)(config->net_config.helloInterval * config->node_num))
                              + "_" + std::to_string((int)config->net_config.maxSpeed)
                              + "_" + std::to_string((int)config->app_config.data_rate_bps)
                              + "_" + std::to_string((int)(config->long_nodes_rate * 100)) + "/";

    // 替换匹配部分
    std::string trace_file_path = std::regex_replace(setting_trace_file_path, pattern, replacement);

    ensureDirectoryExists(trace_file_path);
    std::size_t pos = trace_file_path.find(".txt");
    if (pos != std::string::npos)
    {
        std::string main_str = ExtractScenario(cmd_argv[0]);
        int receiver_id =
            config->receivers.at(0) >= 255 ? 255 : g_config->node_ids.at(config->receivers.at(0));
        int sender_id =
            config->senders.at(0) >= 255 ? 255 : g_config->node_ids.at(config->senders.at(0));
        trace_file_path.insert(
            pos,
            "_" + std::to_string(runNumber) + "_" + std::to_string(config->app_config.packet_num) +
                "_" + std::to_string(lossrate_int) + "_" + std::to_string(config->node_num) + "_" +
                std::to_string(sender_id) + "_" + std::to_string(receiver_id) + "_" + main_str);
    }
    NS_LOG_UNCOND("-----------trace file path(" << trace_file_path << ")-----------\n");
    if (fs::exists(trace_file_path))
    {
        // 如果文件存在，先删除文件
        fs::remove(trace_file_path);
    }
    traceFile.open(trace_file_path);

    // phy trace file
    std::string setting_phy_trace_file_path = settings["phyTraceFile"];
    std::string phy_trace_file_path =
        std::regex_replace(setting_phy_trace_file_path, pattern, replacement);

    ensureDirectoryExists(phy_trace_file_path);
    pos = phy_trace_file_path.find(".txt");
    if (pos != std::string::npos)
    {
        std::string main_str = ExtractScenario(cmd_argv[0]);
        int receiver_id =
            config->receivers.at(0) >= 255 ? 255 : g_config->node_ids.at(config->receivers.at(0));
        int sender_id =
            config->senders.at(0) >= 255 ? 255 : g_config->node_ids.at(config->senders.at(0));
        phy_trace_file_path.insert(
            pos,
            "_" + std::to_string(runNumber) + "_" + std::to_string(config->app_config.packet_num) +
                "_" + std::to_string(lossrate_int) + "_" + std::to_string(config->node_num) + "_" +
                std::to_string(sender_id) + "_" + std::to_string(receiver_id) + "_" + main_str);
    }
    NS_LOG_UNCOND("-----------trace file path(" << phy_trace_file_path << ")-----------\n");
    if (fs::exists(phy_trace_file_path))
    {
        // 如果文件存在，先删除文件
        fs::remove(phy_trace_file_path);
    }
    phyChlTraceFile.open(phy_trace_file_path);
    return 0;
}

int
InitModulesLog(DolphinConfig* config)
{
    if (config->printLoggingInfo)
    {
        LogComponentEnable("DolphinCommon", LOG_FUNCTION);
        LogComponentEnable("DolphinCommon", LOG_PREFIX_ALL);
        LogComponentEnable("DolphinApplication", LOG_FUNCTION);
        LogComponentEnable("DolphinApplication", LOG_PREFIX_ALL);
        LogComponentEnable("AquaSimOptimizedStaticRouting", LOG_FUNCTION);
        LogComponentEnable("AquaSimOptimizedStaticRouting", LOG_PREFIX_ALL);
        LogComponentEnable("DolphinMobilityConstantVelocity", LOG_FUNCTION);
        LogComponentEnable("DolphinMobilityConstantVelocity", LOG_PREFIX_ALL);
        LogComponentEnable("AquaSimGpsrRouting", LOG_FUNCTION);
        LogComponentEnable("AquaSimGpsrRouting", LOG_PREFIX_ALL);
        LogComponentEnable("AquaSimTdmaMac", LOG_FUNCTION);
        LogComponentEnable("AquaSimTdmaMac", LOG_PREFIX_ALL);
        LogComponentEnable("DolphinMobilityRandomWayPoint", LOG_FUNCTION);
        LogComponentEnable("DolphinMobilityRandomWayPoint", LOG_PREFIX_ALL);
    }
    else
    {
        // 禁用所有日志
        LogComponentDisableAll(LOG_LEVEL_ALL);
    }
    return 0;
}

int
InitModules(DolphinConfig* config, AquaSimHelper& asHelper)
{
    //  信道模型和物理层参数配置
    AquaSimChannelHelper channel = AquaSimChannelHelper::Default();
    channel.SetChannel("ns3::AquaSimChannel",
                       "SetPktLossRate",
                       DoubleValue(config->phy_config.lossRate),
                        "LongCnt",
                        IntegerValue(int(config->long_nodes_rate * (config->node_num - config->sink) + config->sink)),
                        "ShortRange",
                        DoubleValue(config->phy_config.shortRange),
                        "LongRange",
                        DoubleValue(config->phy_config.longRange));
    channel.SetPropagation("ns3::AquaSimRangePropagation");
    asHelper.SetChannel(channel.Create());
    asHelper.SetPhy("ns3::AquaSimPhyCmn",
                    "ReliableRange",
                    DoubleValue(config->phy_config.relRange),
                    "MaxRange",
                    DoubleValue(config->phy_config.maxRange),
                    "TransRange",
                    DoubleValue(config->phy_config.txRange),
                    "RecvRange",
                    DoubleValue(config->phy_config.rxRange),
                    "CPThresh",
                    DoubleValue(config->phy_config.CPThresh),
                    "RXThresh",
                    DoubleValue(config->phy_config.RxThresh));

    // MAC配置时隙大小，数量配置写入
    // asHelper.SetMac("ns3::AquaSimTdmaMac",
    //                 "TdmaSlotPeriod",
    //                 UintegerValue(config->mac_config.TdmaSlotNum),
    //                 "TdmaSlotDuration",
    //                 TimeValue(Seconds(config->mac_config.tdma_slot_duration)),
    //                 "DataPacketLength",
    //                 UintegerValue(config->mac_config.data_packet_size),
    //                 "LocPacketLength",
    //                 UintegerValue(config->mac_config.loc_packet_size),
    //                 "TdmaGuardTime",
    //                 TimeValue(Seconds(config->mac_config.tdma_guard_time)),
    //                 "tdmaDataLength",
    //                 UintegerValue(config->mac_config.data_max_length));

    // asHelper.SetMac("ns3::AquaSimAloha",
    //                 "Persistent",
    //                 DoubleValue(0.9),
    //                 "MinBackoff", 
    //                 DoubleValue(0), 
    //                 "MaxBackoff", 
    //                 DoubleValue(3.0));
    // asHelper.SetMac("ns3::AquaSimAloha",
    //                 "AckOn",
    //                 IntegerValue(0));
    asHelper.SetMac("ns3::AquaSimBroadcastMac");
    // MAC配置部分参数使用默认值//
    //  asHelper.SetMac("ns3::AquaSimTdmaMac", "TdmaSlotPeriod", UintegerValue(nodes));
    //  asHelper.SetMac("ns3::AquaSimTdmaMac");
    //------------------------------------------------//

    /* 网络层初始化 Begin */
    // clustering routing
    asHelper.SetRouting(config->net_config.m_routingProtocol);
    if (config->net_config.m_routingProtocol == "ns3::DolphinRoutingClustering")
    {
        Config::SetDefault("ns3::DolphinRoutingClustering::HelloInterval",
                       DoubleValue(config->net_config.helloInterval * config->node_num));
        Config::SetDefault("ns3::DolphinRoutingClustering::Omega1",
                        DoubleValue(config->net_config.omega1));
        Config::SetDefault("ns3::DolphinRoutingClustering::Omega2",
                        DoubleValue(config->net_config.omega2));
        Config::SetDefault("ns3::DolphinRoutingClustering::Omega3",
                        DoubleValue(config->net_config.omega3));
        Config::SetDefault("ns3::DolphinRoutingClustering::Gamma1",
                        DoubleValue(config->net_config.gamma1));
        Config::SetDefault("ns3::DolphinRoutingClustering::Gamma2",
                        DoubleValue(config->net_config.gamma2));
        Config::SetDefault("ns3::DolphinRoutingClustering::Delta1",
                        DoubleValue(config->net_config.delta1));
        Config::SetDefault("ns3::DolphinRoutingClustering::Delta2",
                        DoubleValue(config->net_config.delta2));
        Config::SetDefault("ns3::DolphinRoutingClustering::Alpha",
                        DoubleValue(config->net_config.alpha));
        Config::SetDefault("ns3::DolphinRoutingClustering::Mu",
                        DoubleValue(config->net_config.mu));
        Config::SetDefault("ns3::DolphinRoutingClustering::Lambda",
                        DoubleValue(config->net_config.lambda));
        Config::SetDefault("ns3::DolphinRoutingClustering::Prob",
                        DoubleValue(config->net_config.prob));
        Config::SetDefault("ns3::DolphinRoutingClustering::MaxSpeed",
                        DoubleValue(config->net_config.maxSpeed));
        Config::SetDefault("ns3::DolphinRoutingClustering::N_nodes",
                        IntegerValue(config->node_num));
        Config::SetDefault("ns3::DolphinRoutingClustering::FormingTime",
                        DoubleValue(config->net_config.formingTime));
        Config::SetDefault("ns3::DolphinRoutingClustering::RoutingTime",
                        DoubleValue(config->net_config.routingTime));
        Config::SetDefault("ns3::DolphinRoutingClustering::MaintenanceTime",
                        DoubleValue(config->networking_completion_s));
    }
    else if (config->net_config.m_routingProtocol == "ns3::DolphinRoutingLEACH")
    {
        Config::SetDefault("ns3::DolphinRoutingLEACH::Interval",
                       TimeValue(Seconds(config->net_config.helloInterval)));
        Config::SetDefault("ns3::DolphinRoutingLEACH::Prob",
                        DoubleValue(config->net_config.prob));
        Config::SetDefault("ns3::DolphinRoutingLEACH::FormingTime",
                        DoubleValue(config->net_config.formingTime));
        Config::SetDefault("ns3::DolphinRoutingLEACH::RoutingTime",
                        DoubleValue(config->net_config.routingTime));
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingPAUV")
    {
        Config::SetDefault("ns3::RoutingPAUV::MinX",
                       DoubleValue(config->net_config.minX));
        Config::SetDefault("ns3::RoutingPAUV::MaxX",
                       DoubleValue(config->net_config.maxX));
        Config::SetDefault("ns3::RoutingPAUV::MinY",
                       DoubleValue(config->net_config.minY));
        Config::SetDefault("ns3::RoutingPAUV::MaxY",
                       DoubleValue(config->net_config.maxY));
        Config::SetDefault("ns3::RoutingPAUV::MinZ",
                       DoubleValue(config->net_config.minZ));
        Config::SetDefault("ns3::RoutingPAUV::MaxZ",
                       DoubleValue(config->net_config.maxZ));
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingSOAM")
    {
        Config::SetDefault("ns3::RoutingSOAM::Interval",
                       TimeValue(Seconds(config->net_config.helloInterval)));
        Config::SetDefault("ns3::RoutingSOAM::WaitingInterval",
                       TimeValue(Seconds(config->net_config.routingTime)));
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingRCRP")
    {
        Config::SetDefault("ns3::RoutingRCRP::Interval",
                       TimeValue(Seconds(config->net_config.helloInterval)));
        Config::SetDefault("ns3::RoutingRCRP::Prob",
                        DoubleValue(config->net_config.prob));
        Config::SetDefault("ns3::RoutingRCRP::Thres",
                        DoubleValue(config->net_config.alpha));
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingALRP")
    {
        Config::SetDefault("ns3::RoutingALRP::K",
                       DoubleValue(config->net_config.prob));
        Config::SetDefault("ns3::RoutingALRP::T",
                        DoubleValue(config->net_config.routingTime));
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingERNC")
    {
        Config::SetDefault("ns3::RoutingERNC::Interval",
                       TimeValue(Seconds(config->net_config.helloInterval)));
        Config::SetDefault("ns3::RoutingERNC::MinX",
                       DoubleValue(config->net_config.minX));
        Config::SetDefault("ns3::RoutingERNC::MaxX",
                       DoubleValue(config->net_config.maxX));
        Config::SetDefault("ns3::RoutingERNC::MinY",
                       DoubleValue(config->net_config.minY));
        Config::SetDefault("ns3::RoutingERNC::MaxY",
                       DoubleValue(config->net_config.maxY));
        Config::SetDefault("ns3::RoutingERNC::MinZ",
                       DoubleValue(config->net_config.minZ));
        Config::SetDefault("ns3::RoutingERNC::MaxZ",
                       DoubleValue(config->net_config.maxZ));
    }
    else if (config->net_config.m_routingProtocol == "ns3::RoutingAEWCA")
    {
        Config::SetDefault("ns3::RoutingAEWCA::HelloInterval",
                       DoubleValue(config->net_config.helloInterval * config->node_num));
        Config::SetDefault("ns3::RoutingAEWCA::Omega1",
                        DoubleValue(config->net_config.omega1));
        Config::SetDefault("ns3::RoutingAEWCA::Omega2",
                        DoubleValue(config->net_config.omega2));
        Config::SetDefault("ns3::RoutingAEWCA::Omega3",
                        DoubleValue(config->net_config.omega3));
        Config::SetDefault("ns3::RoutingAEWCA::MaxSpeed",
                        DoubleValue(config->net_config.maxSpeed));
        Config::SetDefault("ns3::RoutingAEWCA::N_nodes",
                        IntegerValue(config->node_num));
        Config::SetDefault("ns3::RoutingAEWCA::FormingTime",
                        DoubleValue(config->net_config.formingTime));
        Config::SetDefault("ns3::RoutingAEWCA::RoutingTime",
                        DoubleValue(config->net_config.routingTime));
        Config::SetDefault("ns3::RoutingAEWCA::MaintenanceTime",
                        DoubleValue(config->networking_completion_s));
    }
    // OptimizedStaticRouting

    // asHelper.SetRouting("ns3::AquaSimOptimizedStaticRouting");
    // Config::SetDefault("ns3::AquaSimOptimizedStaticRouting::SetRouteTableFile",
    //                    StringValue(config->net_config.RouteTablePath.c_str()));
    // Config::SetDefault("ns3::AquaSimOptimizedStaticRouting::max_forwards",
    //                    IntegerValue(config->net_config.MaxFw));
    // Config::SetDefault("ns3::AquaSimOptimizedStaticRouting::neighbor_time_out",
    //                    DoubleValue(config->net_config.nb_out));
    // Config::SetDefault("ns3::AquaSimOptimizedStaticRouting::neighbor_time_interval",
    //                    DoubleValue(config->net_config.nb_slot));
    // Config::SetDefault("ns3::AquaSimOptimizedStaticRouting::packet_time_out",
    //                    DoubleValue(config->net_config.pkt_t2_out));
    // Config::SetDefault("ns3::AquaSimOptimizedStaticRouting::broad_timeout_T1_s",
    //                    DoubleValue(config->net_config.pkt_t1_out));
    // Config::SetDefault("ns3::AquaSimOptimizedStaticRouting::packet_time_interval",
    //                    DoubleValue(config->net_config.pkt_slot));
    // Config::SetDefault("ns3::AquaSimOptimizedStaticRouting::Node_num",
    //                    IntegerValue(config->node_num));
    // Config::SetDefault("ns3::AquaSimOptimizedStaticRouting::Initial_Neighbor",
    //                    StringValue(config->net_config.InitialNeighborString));

    // GPSRRouting
    /*
    asHelper.SetRouting("ns3::AquaSimGpsrRouting");
    Config::SetDefault("ns3::AquaSimGpsrRouting::max_forwards",
                       IntegerValue(config->net_config.MaxFw));
    Config::SetDefault("ns3::AquaSimGpsrRouting::neighbor_time_out",
                       DoubleValue(config->net_config.nb_out));
    Config::SetDefault("ns3::AquaSimGpsrRouting::neighbor_time_interval",
                       DoubleValue(config->net_config.nb_slot));
    Config::SetDefault("ns3::AquaSimGpsrRouting::packet_time_out",
                       DoubleValue(config->net_config.pkt_t2_out));
    Config::SetDefault("ns3::AquaSimGpsrRouting::broad_timeout_T1_s",
                       DoubleValue(config->net_config.pkt_t1_out));
    Config::SetDefault("ns3::AquaSimGpsrRouting::packet_time_interval",
                       DoubleValue(config->net_config.pkt_slot));
    Config::SetDefault("ns3::AquaSimGpsrRouting::Node_num", IntegerValue(config->node_num));
    */
    /* 网络层end */
    return 0;
}

int
CloseModules()
{
    traceFile.close();
    phyChlTraceFile.close();
    return 0;
}

int
CloseModules(DolphinConfig* config)
{
    traceFile << "Control Packets Count = " << controlPktCnt << ";  Hello Packets Count = " << helloCnt << ";   LS Packets Count = " << lsCnt 
              << ";   Switch CH Count = " << switchCHCnt << ";   Become CH Count = " << beCHCnt << ";   Become CM Count = " << beCMCnt 
              << ";    Control Overhead(bytes/T) = " << controlDataCnt / ((config->simulation_time_s - config->networking_completion_s) / 100.)
              << ";    Data Packets Sent = " << dataPktSent << ";   Data Packets Received = " << dataPktReceived 
              << ";    Delivery Ratio = " << ((dataPktSent > 0 && config->node_num > 1)? 100. *  dataPktReceived / dataPktSent / (config->node_num - 1): 0)
              << "%;    Disconnect Drop Ratio = " << ((dataPktSent > 0)? 100. *  (dataPktSent - dataPktReceived - dataDropForLoop) / dataPktSent: 0)
              << "%;    Data Packets Traffic(bytes) = " << dataPktDataCnt 
              << ";    Avg End2End Delay = " << ((dataPktReceived > 0)? delayCnt / dataPktReceived: 0)
              << ";    Pkt Collision Count = " <<  controlCol
              << ";    Pkt Collision Rate = " <<  ((controlRx > 0)? 100. * controlCol / controlRx: 0)
              << "%;    hello Pkt Collision Rate = " << ((helloRx > 0)? 100. * helloCol / helloRx: 0)
              << "%;    LS Pkt Collision Rate = " << ((lsRx > 0)? 100. * lsCol / lsRx: 0)
              << "%"
              << std::endl;
    if (regularPDR == 1)
    {
        for (auto it = pdrs.begin(); it != pdrs.end(); it++)
            traceFile << (*it) / (config->node_num - 1) << " ";
        traceFile << std::endl;
    }
    if (regularCH == 1)
    {
        for (auto it = chNums.begin(); it != chNums.end(); it++)
            traceFile << *it << " ";
        traceFile << std::endl;
        for (auto it = chCovers.begin(); it != chCovers.end(); it++)
            traceFile << *it << " ";
        traceFile << std::endl;
        for (auto it = chRanges.begin(); it != chRanges.end(); it++)
            traceFile << *it << " ";
        traceFile << std::endl;
        for (auto it = chVariances.begin(); it != chVariances.end(); it++)
            traceFile << *it << " ";
        traceFile << std::endl;
    }
    traceFile.close();
    phyChlTraceFile.close();
    return 0;
}

int
ConfigModulesTrace()
{
    // Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Mac/MactdmaTx",
    //                               MakeCallback(&MacTxTrace));
    // Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Mac/MactdmaRx",
    //                               MakeCallback(&MacRxTrace));
    // Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Mac/MactdmaLOSS",
    //                               MakeCallback(&MacLOSSTrace));
    // Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::Application/Tx",
    //                               MakeCallback(&AppTxTrace));
    // Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::Application/Rx",
    //                               MakeCallback(&AppRxTrace));
    // Config::ConnectWithoutContext("ChannelList/*/ChannelLoss", MakeCallback(&ChannelLossTrace));
    // Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Phy/PhyPacketLoss",
    //                               MakeCallback(&PhyLossTrace));
    // Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Phy/Rx",
    //                               MakeCallback(&PhyRxTrace));
    // Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Phy/Tx",
    //                               MakeCallback(&PhyTxTrace));
    // Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Phy/PhyCounterLoss",
    //                               MakeCallback(&PhyCounterTrace));

    // Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Routing/Tx",
    //                               MakeCallback(&NetTxTrace));
    // Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Routing/Rx",
    //                               MakeCallback(&NetRxTrace));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Routing/Counter",
                                  MakeCallback(&NetCounterTrace));
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::NetDevice/Routing/Delay",
                                  MakeCallback(&NetDelayTrace));
    // Config::ConnectWithoutContext(
    //     "/NodeList/*/DeviceList/*/$ns3::NetDevice/Routing/optimizedInitialNet",
    //     MakeCallback(&NetInitialNetTrace));
    // Config::ConnectWithoutContext(
    //     "/NodeList/*/DeviceList/*/$ns3::NetDevice/Routing/optimizedInitialNode",
    //     MakeCallback(&NodeInitialNetTrace));

    return 0;
}

/*部署NetDevice*/
NetDeviceContainer
DeployNetDevice(Ptr<Node> node,
                NetDeviceContainer devices,
                AquaSimHelper asHelper,
                DolphinConfig* config)
{
    Ptr<AquaSimNetDevice> newDevice = CreateObject<AquaSimNetDevice>();
    devices.Add(asHelper.Create(node, newDevice));
    newDevice->GetPhy()->SetTransRange(1500);

    /* mac分配时隙 begin*/
    // 当前节点的时隙号
    // IntergeVector iv;
    // std::vector<int> values =
    //     getTimeSlotsForNode((newDevice->GetNode()->GetId()) + 1, config->mac_config.nodeTimeSlots);
    // iv.setValues(values);
    // if (iv.getSize() == 0)
    // {
    //     std::cout << newDevice->GetNode()->GetId() + 1 << " no slot to tx ";
    // }
    // else
    // {
    //     iv.setValues(values);
    //     std::cout << newDevice->GetNode()->GetId() + 1 << " :";
    //     newDevice->GetMac()->SetAttribute("TdmaSlotNumberList", IntergeVectorValue(iv));
    //     // newDevice->GetMac()->LocPacketSend ();//在分配时隙后调用loc发包
    //     iv.print(); // 打印:id对应的slot number list
    // }

    // int SlotNum = findIndex(config->mac_config.slot_num_table, newDevice->GetNode()->GetId());
    // newDevice->GetMac()->SetAttribute("TdmaSlotNumber", UintegerValue(SlotNum));
    // newDevice->GetPhy()->SetTransRange(1500);
    // newDevice->GetMac()->LocPacketSend(); // 在分配时隙后调用loc发包
    /* mac分配时隙 end*/
    return devices;
}

/*创建应用层*/
void
createApplication(DolphinConfig* config,
                  Ptr<Node> node,
                  uint16_t nodeId,
                  uint16_t rxTxType,
                  uint16_t contentType,
                  Time startTime,
                  Time stopTime,
                  std::string dstList)
{
    /*为该节点设置应用层 begin*/
    DolphinApplicationHelper app("ns3::PacketSocketFactory", config->node_num);
    app.SetAttribute("RxTxType", UintegerValue(rxTxType));
    app.SetAttribute("ContentType", UintegerValue(contentType));
    app.SetAttribute("SendIntervalS", DoubleValue(config->app_config.send_interval_s));
    uint64_t max_bytes = config->app_config.packet_num * config->app_config.packet_size;
    app.SetAttribute("MaxBytes", UintegerValue(max_bytes));
    app.SetAttribute("DataRate", DataRateValue(config->app_config.data_rate_bps));
    // Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    // double data_rate = rand->GetValue() * 24 + 8;
    // app.SetAttribute("DataRate", DataRateValue(data_rate));
    app.SetAttribute("PacketSize", UintegerValue(config->app_config.packet_size));
    app.SetAttribute("RemoteDestinationList", StringValue(dstList));
    ApplicationContainer apps = app.Install(node);
    apps.Start(startTime);
    apps.Stop(stopTime);
}

void
ChannelLossTrace(Ptr<Packet> packet,
                 double time_now,
                 int reason,
                 Ptr<AquaSimNetDevice> recver,
                 uint32_t nodeId)
{
    AquaSimPacketStamp pstamp;
    AquaSimHeader header;
    packet->RemoveHeader(pstamp);
    packet->PeekHeader(header);
    AquaSimAddress srcAddr = header.GetSAddr();
    uint32_t nxt_id = (recver->GetNode()->GetId()) + 1;
    AquaSimAddress dstAddr = header.GetDAddr();
    // 信道丢包不存在所在节点，所在节点ID设置为发送到信道的节点id
    uint32_t node_id = nodeId;
    uint32_t packetId = packet->GetUid();
    uint32_t packet_size = packet->GetSize();
    int packet_type = static_cast<int>(header.GetNetDataType());
    int action;
    uint8_t direct = header.GetDirection(); // 0down,2up
    if (direct == 0)                        // 向下,Channel的向下没有意义
    {
        action = 4;
    }
    else // 向上发给物理层
    {
        action = 3;
    }
    int lossReason = reason;
    packet->AddHeader(pstamp);
    // 将数据包 ID 写入文件
    phyChlTraceFile << "src_id=" << " " << srcAddr.GetAsInt() << " "
                    << "nxt_id=" << " " << nxt_id << " "
                    << "dst_id=" << " " << dstAddr.GetAsInt() << " "
                    << "node_id=" << " " << node_id << " "
                    << "layer=" << " " << "Channel" << " "
                    << "packet_id=" << " " << packetId << " "
                    << "packet_size=" << " " << packet_size << " "
                    << "packet_type=" << " " << packet_type << " "
                    << "action=" << " " << action << " "
                    << "time_stamp=" << " " << time_now << " "
                    << "payload_size=" << " " << 40 << " "
                    << "LossReason=" << " " << lossReason << std::endl;
}

// 物理层丢包的几种情况LossReason：（1）干扰丢包（2）信道随机丢包（3）超时丢包
// （4）可靠通信距离外丢包 （5）指向性丢包 不同的Action:
// （1）从Mac收到的包（2）从信道收到的包（3）向Mac发的包（4）向信道发的包
void
PhyLossTrace(Ptr<Packet> packet,
             double time_now,
             int lossReason,
             uint32_t nodeId,
             AquaSimHeader header)
{
    AquaSimAddress srcAddr = header.GetSAddr();
    // 物理层处的丢包，是无法获取下一跳节点的id的吧
    uint32_t nxt_id = (uint32_t)255;
    AquaSimAddress dstAddr = header.GetDAddr();
    // 丢包原因和节点Id由函数调用者传入
    uint32_t node_id = nodeId + 1;
    uint32_t packetId = packet->GetUid();
    uint32_t packet_size = packet->GetSize();
    int packet_type = static_cast<int>(header.GetNetDataType());
    int action;
    uint8_t direct = header.GetDirection(); // 0down,2up
    if (direct == 0)                        // 向下发往
    {
        action = 4;
    }
    else // 向上发给物理层
    {
        action = 3;
    }
    packet->AddHeader(header);
    uint32_t src_id =
        (srcAddr.GetAsInt() != 255) ? g_config->node_ids.at(srcAddr.GetAsInt() - 1) : 255;
    nxt_id = (nxt_id != 255) ? g_config->node_ids.at(nxt_id - 1) : 255;
    uint32_t dst_id =
        (dstAddr.GetAsInt() != 255) ? g_config->node_ids.at(dstAddr.GetAsInt() - 1) : 255;
    node_id = (node_id != 255) ? g_config->node_ids.at(node_id - 1) : 255;
    // 将数据包 ID 写入文件
    phyChlTraceFile << "src_id=" << " " << src_id << " "
                    << "nxt_id=" << " " << nxt_id << " "
                    << "dst_id=" << " " << dst_id << " "
                    << "node_id=" << " " << node_id << " "
                    << "layer=" << " " << "PHY" << " "
                    << "packet_id=" << " " << packetId << " "
                    << "packet_size=" << " " << packet_size << " "
                    << "packet_type=" << " " << packet_type << " "
                    << "action=" << " " << action << " "
                    << "time_stamp=" << " " << time_now << " "
                    << "payload_size=" << " " << 40 << " "
                    << "LossReason=" << " " << lossReason << std::endl;
}

void
PhyRxTrace(Ptr<const Packet> packet, double time_now, uint32_t nodeId, AquaSimHeader header)
{
    AquaSimAddress srcAddr = header.GetSAddr();
    // 下一跳节点id为255
    uint32_t nxt_id = (uint32_t)255;
    AquaSimAddress dstAddr = header.GetDAddr();
    // 丢包原因和节点Id由函数调用者传入
    uint32_t node_id = nodeId + 1;
    uint32_t packetId = packet->GetUid();
    uint32_t packet_size = packet->GetSize();
    int packet_type = static_cast<int>(header.GetNetDataType());
    int action;
    uint8_t direct = header.GetDirection(); // 0down,2up
    if (direct == 0)                        // 从MAC层来的包
    {
        action = 1;
    }
    else // 从下面Channel来的包
    {
        action = 2;
    }
    uint32_t src_id =
        (srcAddr.GetAsInt() != 255) ? g_config->node_ids.at(srcAddr.GetAsInt() - 1) : 255;
    nxt_id = (nxt_id != 255) ? g_config->node_ids.at(nxt_id - 1) : 255;
    uint32_t dst_id =
        (dstAddr.GetAsInt() != 255) ? g_config->node_ids.at(dstAddr.GetAsInt() - 1) : 255;
    node_id = (node_id != 255) ? g_config->node_ids.at(node_id - 1) : 255;
    // 将数据包 ID 写入文件
    phyChlTraceFile << "src_id=" << " " << src_id << " "
                    << "nxt_id=" << " " << nxt_id << " "
                    << "dst_id=" << " " << dst_id << " "
                    << "node_id=" << " " << node_id << " "
                    << "layer=" << " " << "PHY" << " "
                    << "packet_id=" << " " << packetId << " "
                    << "packet_size=" << " " << packet_size << " "
                    << "packet_type=" << " " << packet_type << " "
                    << "action=" << " " << action << " "
                    << "time_stamp=" << " " << time_now << " "
                    << "payload_size=" << " " << 40 << " "
                    << "LossReason=" << " " << 0 << std::endl;
}

void
PhyTxTrace(Ptr<const Packet> packet, double time_now, uint32_t nodeId, AquaSimHeader header)
{
    AquaSimAddress srcAddr = header.GetSAddr();
    // 下一跳节点为255
    uint32_t nxt_id = (uint32_t)255;
    AquaSimAddress dstAddr = header.GetDAddr();
    // 丢包原因和节点Id由函数调用者传入
    uint32_t node_id = nodeId + 1;
    uint32_t packetId = packet->GetUid();
    uint32_t packet_size = packet->GetSize();
    int packet_type = static_cast<int>(header.GetNetDataType());
    int action;
    uint8_t direct = header.GetDirection(); // 0down,2up
    if (direct == 0)                        // 发往Channel的包
    {
        action = 4;
    }
    else // 向上传递给MAC层的包
    {
        action = 3;
    }
    uint32_t src_id =
        (srcAddr.GetAsInt() != 255) ? g_config->node_ids.at(srcAddr.GetAsInt() - 1) : 255;
    nxt_id = (nxt_id != 255) ? g_config->node_ids.at(nxt_id - 1) : 255;
    uint32_t dst_id =
        (dstAddr.GetAsInt() != 255) ? g_config->node_ids.at(dstAddr.GetAsInt() - 1) : 255;
    node_id = (node_id != 255) ? g_config->node_ids.at(node_id - 1) : 255;
    // 将数据包 ID 写入文件
    phyChlTraceFile << "src_id=" << " " << src_id << " "
                    << "nxt_id=" << " " << nxt_id << " "
                    << "dst_id=" << " " << dst_id << " "
                    << "node_id=" << " " << node_id << " "
                    << "layer=" << " " << "PHY" << " "
                    << "packet_id=" << " " << packetId << " "
                    << "packet_size=" << " " << packet_size << " "
                    << "packet_type=" << " " << packet_type << " "
                    << "action=" << " " << action << " "
                    << "time_stamp=" << " " << time_now << " "
                    << "payload_size=" << " " << 40 << " "
                    << "LossReason=" << " " << 0 << std::endl;
}

void
PhyCounterTrace(int typ, int act)
{
    controlRx++;
    if (act == 1)
        controlCol++;
    if (typ == 1)
    {
        helloRx++;
        if (act == 1)
            helloCol++;
    }
    else if (typ == 5)
    {
        lsRx++;
        if (act == 1)
            lsCol++;
    }
}

/*  mac trace begin */

// 处理 MAC 层 Trace Source 的函数,数据包，当前时间s，当前节点地址,MAC层的发包动作
void
MacTxTrace(Ptr<const Packet> packet, double time_now, AquaSimAddress address)
{
    // uint32_t packetId = packet->GetUid();
    int packet_type; // 0是数据包，1是位置包
    int action;      // 1从网络层收的包，2从物理层收的包，3向网络层发包，4向物理层发包
    uint32_t packet_size = packet->GetSize(); // 数据包大小
    AquaSimHeader ash;
    // MacHeader mach;
    // LocalizationHeader loch;
    packet->PeekHeader(ash);
    packet_type = static_cast<int>(ash.GetNetDataType());
    // NS_LOG_UNCOND( "m_net_packet_type: " << static_cast<int>(ash.GetNetDataType()) <<
    // std::endl;
    uint8_t direct = ash.GetDirection(); // 0down,2up
    AquaSimAddress srcAddr = ash.GetSAddr();
    AquaSimAddress dstAddr = ash.GetDAddr();
    AquaSimAddress nxtAddr = ash.GetNextHop();
    if (direct == 0) // 向std::ofstream 下发送给物理层
    {
        action = 4;
    }
    else // 向上发给网络层
    {
        action = 3;
    }
    uint32_t src_id =
        (srcAddr.GetAsInt() != 255) ? g_config->node_ids.at(srcAddr.GetAsInt() - 1) : 255;
    uint32_t nxt_id =
        (nxtAddr.GetAsInt() != 255) ? g_config->node_ids.at(nxtAddr.GetAsInt() - 1) : 255;
    uint32_t dst_id =
        (dstAddr.GetAsInt() != 255) ? g_config->node_ids.at(dstAddr.GetAsInt() - 1) : 255;
    uint32_t node_id =
        (address.GetAsInt() != 255) ? g_config->node_ids.at(address.GetAsInt() - 1) : 255;
    if (packet_type == 0)
    {
        traceFile << "src_id=" << " " << src_id << " "
                  << "nxt_id=" << " " << nxt_id << " "
                  << "dst_id=" << " " << dst_id << " "
                  << "node_id=" << " " << node_id << " "
                  << "layer=" << " " << "MAC" << " "
                  << "packet_id=" << " " << packet->GetUid() << " "
                  << "packet_size=" << " " << packet_size << " "
                  << "packet_type=" << " " << packet_type << " "
                  << "action=" << " " << action << " "
                  << "time_stamp=" << " " << time_now << " "
                  << "payload_size=" << " " << g_config->mac_config.data_packet_size << " "
                  << "LossReason=" << " " << 0 << std::endl;
    }
    else
    {
        traceFile << "src_id=" << " " << src_id << " "
                  << "nxt_id=" << " " << nxt_id << " "
                  << "dst_id=" << " " << dst_id << " "
                  << "node_id=" << " " << node_id << " "
                  << "layer=" << " " << "MAC" << " "
                  << "packet_id=" << " " << packet->GetUid() << " "
                  << "packet_size=" << " " << packet_size << " "
                  << "packet_type=" << " " << packet_type << " "
                  << "action=" << " " << action << " "
                  << "time_stamp=" << " " << time_now << " "
                  << "payload_size=" << " " << g_config->mac_config.loc_packet_size << " "
                  << "LossReason=" << " " << 0 << std::endl;
    }
}

void
MacRxTrace(Ptr<const Packet> packet, double time_now, AquaSimAddress address)
{
    int packet_type; // 0是数据包，1是位置包
    int action;      // 1从网络层收的包，2从物理层收的包，3向网络层发包，4向物理层发包
    uint32_t packet_size = packet->GetSize(); // 数据包大小
    AquaSimHeader ash;

    packet->PeekHeader(ash);
    packet_type = static_cast<int>(ash.GetNetDataType());
    // NS_LOG_UNCOND( "m_net_packet_type: " << static_cast<int>(ash.GetNetDataType()) <<
    // std::endl;
    uint8_t direct = ash.GetDirection(); // 0down,2up
    AquaSimAddress srcAddr = ash.GetSAddr();
    AquaSimAddress dstAddr = ash.GetDAddr();
    AquaSimAddress nxtAddr = ash.GetNextHop();
    if (direct == 0) // 从网络层收的包
    {
        action = 1;
    }
    else // 从物理层收的包
    {
        action = 2;
    }

    uint32_t src_id =
        (srcAddr.GetAsInt() != 255) ? g_config->node_ids.at(srcAddr.GetAsInt() - 1) : 255;
    uint32_t nxt_id =
        (nxtAddr.GetAsInt() != 255) ? g_config->node_ids.at(nxtAddr.GetAsInt() - 1) : 255;
    uint32_t dst_id =
        (dstAddr.GetAsInt() != 255) ? g_config->node_ids.at(dstAddr.GetAsInt() - 1) : 255;
    uint32_t node_id =
        (address.GetAsInt() != 255) ? g_config->node_ids.at(address.GetAsInt() - 1) : 255;
    if (packet_type == 0)
    {
        traceFile << "src_id=" << " " << src_id << " "
                  << "nxt_id=" << " " << nxt_id << " "
                  << "dst_id=" << " " << dst_id << " "
                  << "node_id=" << " " << node_id << " "
                  << "layer=" << " " << "MAC" << " "
                  << "packet_id=" << " " << packet->GetUid() << " "
                  << "packet_size=" << " " << packet_size << " "
                  << "packet_type=" << " " << packet_type << " "
                  << "action=" << " " << action << " "
                  << "time_stamp=" << " " << time_now << " "
                  << "payload_size=" << " " << g_config->mac_config.data_packet_size << " "
                  << "LossReason=" << " " << 0 << std::endl;
    }
    else
    {
        traceFile << "src_id=" << " " << src_id << " "
                  << "nxt_id=" << " " << nxt_id << " "
                  << "dst_id=" << " " << dst_id << " "
                  << "node_id=" << " " << node_id << " "
                  << "layer=" << " " << "MAC" << " "
                  << "packet_id=" << " " << packet->GetUid() << " "
                  << "packet_size=" << " " << packet_size << " "
                  << "packet_type=" << " " << packet_type << " "
                  << "action=" << " " << action << " "
                  << "time_stamp=" << " " << time_now << " "
                  << "payload_size=" << " " << g_config->mac_config.loc_packet_size << " "
                  << "LossReason=" << " " << 0 << std::endl;
    }
}

void
MacLOSSTrace(Ptr<const Packet> packet, double time_now, AquaSimAddress address)
{
    // uint32_t packetId = packet->GetUid();
    int packet_type; // 0是数据包，1是位置包
    int action;      // 1从网络层收的包，2从物理层收的包，3向网络层发包，4向物理层发包
    uint32_t packet_size = packet->GetSize(); // 数据包大小
    AquaSimHeader ash;
    // MacHeader mach;
    // LocalizationHeader loch;
    packet->PeekHeader(ash);
    packet_type = static_cast<int>(ash.GetNetDataType());
    // NS_LOG_UNCOND( "m_net_packet_type: " << static_cast<int>(ash.GetNetDataType()) <<
    // std::endl;
    uint8_t direct = ash.GetDirection(); // 0down,2up
    AquaSimAddress srcAddr = ash.GetSAddr();
    AquaSimAddress dstAddr = ash.GetDAddr();
    AquaSimAddress nxtAddr = ash.GetNextHop();
    if (direct == 0) // 从网络层收的包
    {
        action = 1;
    }
    else // 从物理层收的包
    {
        action = 2;
    }

    uint32_t src_id =
        (srcAddr.GetAsInt() != 255) ? g_config->node_ids.at(srcAddr.GetAsInt() - 1) : 255;
    uint32_t nxt_id =
        (nxtAddr.GetAsInt() != 255) ? g_config->node_ids.at(nxtAddr.GetAsInt() - 1) : 255;
    uint32_t dst_id =
        (dstAddr.GetAsInt() != 255) ? g_config->node_ids.at(dstAddr.GetAsInt() - 1) : 255;
    uint32_t node_id =
        (address.GetAsInt() != 255) ? g_config->node_ids.at(address.GetAsInt() - 1) : 255;
    if (packet_type == 0)
    {
        traceFile << "src_id=" << " " << src_id << " "
                  << "nxt_id=" << " " << nxt_id << " "
                  << "dst_id=" << " " << dst_id << " "
                  << "node_id=" << " " << node_id << " "
                  << "layer=" << " " << "MAC" << " "
                  << "packet_id=" << " " << packet->GetUid() << " "
                  << "packet_size=" << " " << packet_size << " "
                  << "packet_type=" << " " << packet_type << " "
                  << "action=" << " " << 6 << " "
                  << "time_stamp=" << " " << time_now << " "
                  << "payload_size=" << " " << g_config->mac_config.data_packet_size << " "
                  << "LossReason=" << " " << 0 << std::endl;
    }
    else
    {
        traceFile << "src_id=" << " " << src_id << " "
                  << "nxt_id=" << " " << nxt_id << " "
                  << "dst_id=" << " " << dst_id << " "
                  << "node_id=" << " " << node_id << " "
                  << "layer=" << " " << "MAC" << " "
                  << "packet_id=" << " " << packet->GetUid() << " "
                  << "packet_size=" << " " << packet_size << " "
                  << "packet_type=" << " " << packet_type << " "
                  << "action=" << " " << 6 << " "
                  << "time_stamp=" << " " << time_now << " "
                  << "payload_size=" << " " << g_config->mac_config.loc_packet_size << " "
                  << "LossReason=" << " " << 0 << std::endl;
    }
}

/*  mac trace end */

// net trace begin
// loss : 0：未丢包；1：环路丢包；
//        2:不是下一跳地址丢包；3：seqnum不是最新丢包；4：广播风暴抑制丢包；5：找不到路由丢包
void
NetTraceBase(TraceAction action,
             Ptr<const Packet> packet,
             double time_now,
             AquaSimAddress address,
             int loss)
{
    int packet_type;
    AquaSimHeader ash;
    packet->PeekHeader(ash);
    AquaSimAddress srcAddr = ash.GetSAddr();
    AquaSimAddress dstAddr = ash.GetDAddr();
    AquaSimAddress nxtAddr = ash.GetNextHop();
    packet_type = static_cast<int>(ash.GetNetDataType());

    uint32_t src_id =
        (srcAddr.GetAsInt() != 255) ? g_config->node_ids.at(srcAddr.GetAsInt() - 1) : 255;
    uint32_t nxt_id =
        (nxtAddr.GetAsInt() != 255) ? g_config->node_ids.at(nxtAddr.GetAsInt() - 1) : 255;
    uint32_t dst_id =
        (dstAddr.GetAsInt() != 255) ? g_config->node_ids.at(dstAddr.GetAsInt() - 1) : 255;
    uint32_t node_id =
        (address.GetAsInt() != 255) ? g_config->node_ids.at(address.GetAsInt() - 1) : 255;
    traceFile << "src_id=" << " " << src_id << " "
              << "nxt_id=" << " " << nxt_id << " "
              << "dst_id=" << " " << dst_id << " "
              << "node_id=" << " " << node_id << " "
              << "layer=" << " " << "NET" << " "
              << "packet_id=" << " " << packet->GetUid() << " "
              << "packet_size=" << " " << ash.GetSize() << " "
              << "packet_type=" << " " << packet_type << " "
              << "action=" << " " << static_cast<int>(action) << " "
              << "time_stamp=" << " " << time_now << " "
              << "payload_size=" << " " << 40 << " "
              << "LossReason=" << " " << loss << " "
              << "bitstring=" << " " << ash.GetSendBitString() << std::endl;
}

void
NetTxTrace(Ptr<const Packet> packet, double time_now, AquaSimAddress address, int up0down1)
{
    if (up0down1 == 1)
        NetTraceBase(TraceAction::TXToDown, packet, time_now, address, 0);
    else if (up0down1 == 0)
        NetTraceBase(TraceAction::TXToUP, packet, time_now, address, 0);
}

void
NetRxTrace(Ptr<const Packet> packet, double time_now, AquaSimAddress address, int loss)
{
    AquaSimHeader ash;
    packet->PeekHeader(ash);

    if (ash.GetDirection() == AquaSimHeader::DOWN)
        NetTraceBase(TraceAction::RXFromUP, packet, time_now, address, loss);
    else if (ash.GetDirection() == AquaSimHeader::UP)
        NetTraceBase(TraceAction::RXFromDown, packet, time_now, address, loss);
}


// 1: hello    2: LS     3: switchCH    4: become CH      5: become CM    6: data forwarded    7:data size forwarded
void
NetCounterTrace(uint8_t t, uint32_t sz)
{
    if (t <= 2)
        controlPktCnt++;
    if (t == 1)
        helloCnt++;
    if (t == 2)
        lsCnt++;
    if (t == 3)
        switchCHCnt++;
    if (t == 4)
        beCHCnt++;
    if (t == 5)
        beCMCnt++;
    if (t == 6)
    {
        if (sz == 0)
            dataPktSent++;
        else if (sz == 1)
            dataPktReceived++;
        else if (sz == 2)
            dataDropForLoop++;
    }
    if (t == 7)
        dataPktDataCnt += sz;
    if (t <= 3 && Simulator::Now().GetSeconds() >= 100)
        controlDataCnt += sz;
}

void
NetRegularPdrCounter()
{
    double now = Simulator::Now().ToDouble(Time::S);
    pdrs.emplace_back(((dataPktSent > 0)? 100. *  dataPktReceived / dataPktSent: 0));
    if (now < pdrInterval * 10)
    {
        Simulator::Schedule(Seconds(pdrInterval), &NetRegularPdrCounter);
    }
}

void
CHRegularCounter(NetDeviceContainer devices)
{
    int ch = 0;
    std::vector <int> sz;
    sz.clear();
    Ptr<NetDevice> device;
    Ptr<AquaSimNetDevice> asDevice;
    EnumValue ident;
    IntegerValue cm;
    for (NetDeviceContainer::Iterator i = devices.Begin(); i != devices.End(); i++)
    {
        device = (*i);
        asDevice = DynamicCast<AquaSimNetDevice>(device);
        asDevice->GetRouting()->GetAttribute("Identity", ident);
        if (ident.Get() == 1 || ident.Get() == 3)
        {
            ch++;
            asDevice->GetRouting()->GetAttribute("NMembers", cm);
            sz.emplace_back(cm.Get() + 1);
        }
    }
    
    chNums.emplace_back(ch);
    if (ch == 0)
    {
        chCovers.emplace_back(0);
        // chRanges.emplace_back(100000);
        // chVariances.emplace_back(10000000);
    }else
    {
        int covers = std::min(std::accumulate(sz.begin(), sz.end(), 0), (int)devices.GetN());
        chCovers.emplace_back(1. * covers / devices.GetN());
        chRanges.emplace_back((*std::max_element(sz.begin(), sz.end())) - (*std::min_element(sz.begin(), sz.end())));
        double avg = 1. * covers / ch;
        double var = 0.;
        for (auto i = sz.begin(); i != sz.end(); i++)
            var += ((*i) - avg) * ((*i) - avg);
        chVariances.emplace_back(var / ch);
    }
    double inter = randInt->GetValue(50, 200);
    Simulator::Schedule(Seconds(inter), &CHRegularCounter, devices);
}

void
NetDelayTrace(uint32_t pid, uint8_t action, double t)
{
    if (action == 0)
        pktTime[pid] = t;
    else
        delayCnt += t - pktTime[pid];
}

void
NodeInitialNetTrace(int node, double t)
{
    // traceFile << "Node "<<node<<" finish initial network at " << t << std::endl;
    traceFile << "src_id=" << " " << node << " "
              << "nxt_id=" << " " << 255 << " "
              << "dst_id=" << " " << 255 << " "
              << "node_id=" << " " << node << " "
              << "layer=" << " " << "NET" << " "
              << "packet_id=" << " " << -1 << " "
              << "packet_size=" << " " << 0 << " "
              << "packet_type=" << " " << 6 << " " // 此处新增packet type
              << "action=" << " " << -1 << " "
              << "time_stamp=" << " " << t << " "
              << "payload_size=" << " " << 0 << " "
              << "LossReason=" << " " << 0 << " " << std::endl;
}

// net trace end

/*  app trace begin */

void
AppTraceBase(TraceAction action,
             Ptr<const Packet> packet,
             uint64_t in_src_id,
             uint64_t in_dst_id,
             uint64_t in_node_id,
             uint8_t packet_type)
{
    std::string layer = "APPLICATION";
    uint32_t packet_size = packet->GetSize(); // 数据包大小
    int actionConvert = (packet_type == DolphinApplication::PacketType::NODEDOWEROFF)
                            ? static_cast<int>(TraceAction::OTHERS)
                            : static_cast<int>(action);
    std::string actionDescription =
        (action == TraceAction::TXToDown) ? "sent to net" : "received from net";

    uint32_t src_id = (in_src_id != 255) ? g_config->node_ids.at(in_src_id - 1) : 255;
    uint32_t nxt_id = (in_dst_id != 255) ? g_config->node_ids.at(in_dst_id - 1) : 255;
    uint32_t dst_id = (in_dst_id != 255) ? g_config->node_ids.at(in_dst_id - 1) : 255;
    uint32_t node_id = (in_node_id != 255) ? g_config->node_ids.at(in_node_id - 1) : 255;
    traceFile << "src_id=" << " " << src_id << " "
              << "nxt_id=" << " " << nxt_id << " "
              << "dst_id=" << " " << dst_id << " "
              << "node_id=" << " " << node_id << " "
              << "layer=" << " " << layer << " "
              << "packet_id=" << " " << packet->GetUid() << " "
              << "packet_size=" << " " << packet->GetSize() << " "
              << "packet_type=" << " " << static_cast<int>(packet_type) << " "
              << "action=" << " " << actionConvert << " "
              << "time_stamp=" << " " << Simulator::Now().GetSeconds() << " "
              << "payload_size=" << " " << packet_size << " "
              << "LossReason=" << " " << 0 << std::endl;
}

void
AppTxTrace(Ptr<const Packet> packet, uint64_t src_id, uint64_t dst_id, uint8_t packet_type)
{
    AppTraceBase(TraceAction::TXToDown, packet, src_id, dst_id, src_id, packet_type);
    if (!g_config->app_config.tx_cb.IsNull())
    {
        g_config->app_config.tx_cb(packet, src_id, dst_id, packet_type);
    }
}

void
AppRxTrace(Ptr<const Packet> packet, uint64_t src_id, uint64_t dst_id, uint8_t packet_type)
{
    AppTraceBase(TraceAction::RXFromDown, packet, src_id, dst_id, dst_id, packet_type);
    if (!g_config->app_config.rx_cb.IsNull())
    {
        g_config->app_config.rx_cb(packet, src_id, dst_id, packet_type);
    }
}

/*  app trace end */
int
findIndex(const std::vector<int>& arr, int target)
{
    for (size_t i = 0; i < arr.size(); ++i)
    {
        if (arr[i] == target)
        {
            return i; // 返回目标数的索引
        }
    }
    return -1; // 如果未找到目标数，则返回 -1
}

std::vector<int>
getTimeSlotsForNode(int nodeId, const std::map<int, std::vector<int>>& nodeTimeSlots)
{
    // 创建一个空的 vector<int> 用于存储结果
    std::vector<int> timeSlots;

    // 在 map 中查找指定节点
    auto it = nodeTimeSlots.find(nodeId);
    if (it != nodeTimeSlots.end())
    {
        // 找到节点，拷贝其时隙信息到 timeSlots
        timeSlots = it->second;
    }
    else
    {
        std::cout << "节点 " << nodeId << " 不存在或没有配置时隙。";
    }

    return timeSlots;
}

// 计算节点可发送的最早时间
double
CalculateNextSlotTime(double start_time, double slot_size, int slot_count, int node_id)
{
    if (slot_count <= 0 || slot_size <= 0 || node_id < 0)
    {
        std::cerr << "Invalid input parameters!" << std::endl;
        return -1.0;
    }

    // 该节点的时隙id列表
    std::vector<int> node_slots =
        getTimeSlotsForNode(node_id + 1, g_config->mac_config.nodeTimeSlots);

    double earliest_time = std::numeric_limits<double>::max();
    int start_index = static_cast<int>(std::ceil(start_time / slot_size));

    for (int slot : node_slots)
    {
        if (slot < 0 || slot >= slot_count)
        {
            std::cerr << "Invalid slot value: " << slot << std::endl;
            continue;
        }

        // 计算最早满足 (index % slot_count == slot) 的 index
        int i = start_index;
        while (i % slot_count != slot)
        {
            ++i;
        }

        double time = i * slot_size;
        if (time < earliest_time)
        {
            earliest_time = time;
        }
    }

    return earliest_time;
}

// 处理转弯的函数
void
TurningLeftSingle(DolphinTurningHelper* helper, Ptr<Node> node, Ptr<Node> center)
{
    helper->Turning(NodeContainer(node), center);
}

void
TurningLeftMulti(DolphinTurningHelper* helper, NodeContainer c, Ptr<Node> center)
{
    helper->Turning(c, center);
}

int
setMobility(DolphinConfig* config, NodeContainer nodesCon, double startTime)
{
    json settings = config->settings;
    //  process positions
    std::vector<Vector> positions;
    std::vector<Vector2D> perrors, lerrors;
    std::vector<std::vector<double>> raw_positions =
        settings["node_positions"].get<std::vector<std::vector<double>>>();
    int positions_n = raw_positions.size();
    double scale = 1.0;
    if (settings.contains("scale"))
        scale = settings["scale"];
    for (const auto& pos : raw_positions)
        positions.push_back(Vector(pos[0] * scale, pos[1] * scale, pos[2] * scale));
    // position errors
    if (settings["position_error_config"] == 2)
    {
        Vector2D err =
            Vector2D(settings["position_error_batch"][0], settings["position_error_batch"][1]);
        for (int i = 0; i < positions_n; ++i)
            perrors.push_back(err);
    }
    else
    {
        int errors_n = settings["position_error_individual"].size();
        perrors = std::vector<Vector2D>(config->node_num);
        for (int i = 0; i < errors_n; ++i)
        {
            int id = settings["position_error_individual"][i]["node_id"];
            auto err = settings["position_error_individual"][i]["error"];
            perrors[id] = Vector2D(err[0], err[1]);
        }
    }
    // location errors
    if (settings["location_error_config"] == 2)
    {
        Vector2D err =
            Vector2D(settings["location_error_batch"][0], settings["location_error_batch"][1]);
        for (int i = 0; i < positions_n; ++i)
            lerrors.push_back(err);
    }
    else
    {
        int errors_n = settings["location_error_individual"].size();
        lerrors = std::vector<Vector2D>(config->node_num);
        for (int i = 0; i < errors_n; ++i)
        {
            int id = settings["location_error_individual"][i]["node_id"];
            auto err = settings["location_error_individual"][i]["error"];
            lerrors[id] = Vector2D(err[0], err[1]);
        }
    }
    //  process speed errors
    double speederr = 0.05, direrr = 0.05;
    std::vector<double> speederrs, direrrs;
    double pc = 0.0, sc = settings["speed_error_update"],
           dc = settings["speed_direction_error_update"];
    bool mob_individual_flag =
        (settings["speed_error_config"] == 1 || settings["speed_direction_error_config"] == 1 ||
         settings["mobility_model_config"] == 1);
    if (settings["speed_error_config"] == 2)
    {
        speederr = settings["speed_error_batch"];
        if (mob_individual_flag)
            speederrs = std::vector<double>(config->node_num, speederr);
    }
    else
    {
        int errors_n = settings["speed_error_individual"].size();
        speederrs = std::vector<double>(config->node_num);
        for (int i = 0; i < errors_n; ++i)
        {
            int id = settings["speed_error_individual"][i]["node_id"];
            speederrs[id] = settings["speed_error_individual"][i]["error"];
        }
    }
    //  process direction errors
    if (settings["speed_direction_error_config"] == 2)
    {
        direrr = settings["speed_direction_error_batch"];
        if (mob_individual_flag)
            direrrs = std::vector<double>(config->node_num, direrr);
    }
    else
    {
        int errors_n = settings["speed_direction_error_individual"].size();
        direrrs = std::vector<double>(config->node_num);
        for (int i = 0; i < errors_n; ++i)
        {
            int id = settings["speed_direction_error_individual"][i]["node_id"];
            direrrs[id] = settings["speed_direction_error_individual"][i]["error"];
        }
    }
    //  process mobility models
    std::vector<DolphinMobilityHelper*> m_mobi;
    std::vector<DolphinTurningHelper*> m_turning;
    // double modelStartTime = startTime + config->networking_completion_s;
    double modelStartTime = startTime;
    if (settings["mobility_model_config"] == 3) // part movable
    {
        double rate=settings["movable_rate"];
        int n=config->node_num - config->sink;
        std::vector<double> modelStartTimes(config->node_num, modelStartTime);
        int x = std::ceil(n * rate);

        
        // 创建一个包含x个1和n-x个0的向量
        std::vector<int> nodes(n, 0);
        for (int i = 0; i < x; ++i) {
            nodes[i] = 1;
        }
        // 打乱顺序以实现随机分布
        std::srand(static_cast<unsigned int>(std::time(0)));
        std::random_shuffle(nodes.begin(), nodes.end());

        // SINK也是长节点
        // 将向量转换为字符数组
        char ans[config->node_num + 1];
        for (int i = 0; i < config->sink; ++i)
            ans[i] = '0';
        for (int i = 0; i < n; ++i) {
            ans[i + config->sink] = nodes[i] + '0';
        }
        
        ans[config->node_num] = '\0'; // 添加字符串结束符

        auto model = settings["mobility_models_batch"][0]["model"];
        auto param = settings["mobility_models_batch"][0]["params"];
        if (model != "ns3::DolphinMobilityRandomWayPoint")
        {
            NS_LOG_UNCOND("mobility_model_config 3 only fit DolphinMobilityRandomWayPoint!");
        }
        std::vector<std::vector<double>> Npositions =
        settings["node_positions"].get<std::vector<std::vector<double>>>();
        for (int i = 0; i < config->node_num; i++)
        {
            double startTimeSetting = 0;
            if (param.contains("start_time"))
                startTimeSetting = param["start_time"];
            startTimeSetting = std::max(startTimeSetting, 0.01);
            double lengthSetting = param["length"];
            auto speed = param["speed"];
            double pause = param["pause"];
            double xmin = param["X"][0];
            double xmax = param["X"][1];
            double ymin = param["Y"][0];
            double ymax = param["Y"][1];
            double zmin = param["Z"][0];
            double zmax = param["Z"][1];
            //auto xRange = param["X"];
            //auto yRange = param["Y"];
            //auto zRange = param["Z"];
            DolphinMobilityHelper* mobility = new DolphinMobilityHelper();
            if (ans[i] == '1' && !(config->net_config.m_routingProtocol == "ns3::RoutingERNC" && i == 0))
            {
                mobility->SetMobilityModel(model,
                                        "MinSpeed", DoubleValue(speed[0]),
                                        "MaxSpeed", DoubleValue(speed[1]),
                                        "MaxPauseTime", DoubleValue(pause),
                                        "MinX", DoubleValue(xmin),
                                        "MaxX", DoubleValue(xmax),
                                        "MinY", DoubleValue(ymin),
                                        "MaxY", DoubleValue(ymax),
                                        "MinZ", DoubleValue(zmin),
                                        "MaxZ", DoubleValue(zmax));
            }
            else
            {
                mobility->SetMobilityModel("ns3::DolphinMobilityConstantVelocity");
                mobility->SetSpeed(0,
                                   Vector(1, 0, 0),
                                   speederr,
                                   direrr);
            }
            
            modelStartTimes[i] += startTimeSetting;
            mobility->SetTime(modelStartTimes[i], lengthSetting);
            mobility->SetCycle(pc, sc, dc);
            

            mobility->Install(nodesCon.Get(i), 1);
            mobility->SetPosition(positions[i],perrors[i],lerrors[i],nodesCon.Get(i));

            m_mobi.push_back(mobility);
            modelStartTimes[i] += lengthSetting;
            config->mobility_config.mobilityEndTimeList.push_back(modelStartTimes[i]);
        }

        std::cout<<ans<<std::endl;
    }
    else if (settings["mobility_model_config"] == 2 && !mob_individual_flag) // all batch
    {
        int model_num = settings["mobility_models_batch"].size();
        for (int i = 0; i < model_num; ++i)
        {
            auto model = settings["mobility_models_batch"][i]["model"];
            auto param = settings["mobility_models_batch"][i]["params"];
            if (model == "ns3::DolphinMobilityConstantVelocity")
            {
                DolphinMobilityHelper* mobility = new DolphinMobilityHelper();
                mobility->SetMobilityModel(model);
                double startTimeSetting = 0;
                if (param.contains("start_time"))
                    startTimeSetting = param["start_time"];
                startTimeSetting = std::max(startTimeSetting, 0.01);
                double lengthSetting = param["length"];
                double speed = param["speed"];
                auto direction = param["direction"];
                modelStartTime += startTimeSetting;
                mobility->SetTime(modelStartTime, lengthSetting);
                mobility->SetCycle(pc, sc, dc);
                mobility->SetSpeed(speed,
                                   Vector(direction[0], direction[1], direction[2]),
                                   speederr,
                                   direrr);
                if (i == 0) // first model should allocate initial positions
                {
                    mobility->Install(nodesCon, 1);
                    mobility->SetPosition(positions, perrors, lerrors, nodesCon);
                }
                else
                    mobility->Install(nodesCon, 0);
                m_mobi.push_back(mobility);
                modelStartTime += lengthSetting;
                config->mobility_config.mobilityEndTimeList.push_back(modelStartTime);
            }
            else if (model == "Turning")
            {
                DolphinTurningHelper* turning = new DolphinTurningHelper();
                turning->Setspeed(param["speed"]);
                double startTimeSetting = 0;
                if (param.contains("start_time"))
                    startTimeSetting = param["start_time"];
                startTimeSetting = std::max(startTimeSetting, 0.01);
                modelStartTime += startTimeSetting;
                Simulator::Schedule(Seconds(modelStartTime),
                                    &TurningLeftMulti,
                                    turning,
                                    nodesCon,
                                    nodesCon.Get(param["center"]));
                m_turning.push_back(turning);
                double lengthSetting = param["length"];
                modelStartTime += lengthSetting;
            }
            else if (model == "ns3::DolphinMobilityRandomWayPoint")
            {
                double startTimeSetting = 0;
                if (param.contains("start_time"))
                    startTimeSetting = param["start_time"];
                startTimeSetting = std::max(startTimeSetting, 0.01);
                double lengthSetting = param["length"];
                auto speed = param["speed"];
                double pause = param["pause"];
                auto xRange = param["X"];
                auto yRange = param["Y"];
                auto zRange = param["Z"];
                DolphinMobilityHelper* mobility = new DolphinMobilityHelper();
                mobility->SetMobilityModel(model,
                                           "MinSpeed", DoubleValue(speed[0]),
                                           "MaxSpeed", DoubleValue(speed[1]),
                                           "MaxPauseTime", DoubleValue(pause),
                                           "MinX", DoubleValue(xRange[0]),
                                           "MaxX", DoubleValue(xRange[1]),
                                           "MinY", DoubleValue(yRange[0]),
                                           "MaxY", DoubleValue(yRange[1]),
                                           "MinZ", DoubleValue(zRange[0]),
                                           "MaxZ", DoubleValue(zRange[1]));
                
                modelStartTime += startTimeSetting;
                mobility->SetTime(modelStartTime, lengthSetting);
                mobility->SetCycle(pc, sc, dc);
                
                if (i == 0) // first model should allocate initial positions
                {
                    mobility->Install(nodesCon, 1);
                    mobility->SetPosition(positions, perrors, lerrors, nodesCon);
                }
                else
                    mobility->Install(nodesCon, 0);
                m_mobi.push_back(mobility);
                modelStartTime += lengthSetting;
                config->mobility_config.mobilityEndTimeList.push_back(modelStartTime);
            }
            else
            {
                std::cerr << "mobility model " << model << " error or have not been supported "
                          << std::endl;
                return -1;
            }
        }
    }
    else if (settings["mobility_model_config"] == 2) // batch model individual error
    {
        int model_num = settings["mobility_models_batch"].size();
        std::vector<double> modelStartTimes(config->node_num, modelStartTime);
        for (int i = 0; i < model_num; ++i)
            for (int j = 0; j < config->node_num; ++j)
            {
                auto model = settings["mobility_models_batch"][i]["model"];
                auto param = settings["mobility_models_batch"][i]["params"];
                if (model == "ns3::DolphinMobilityConstantVelocity")
                {
                    DolphinMobilityHelper* mobility = new DolphinMobilityHelper();
                    mobility->SetMobilityModel(model);
                    double startTimeSetting = 0;
                    if (param.contains("start_time"))
                        startTimeSetting = param["start_time"];
                    startTimeSetting = std::max(startTimeSetting, 0.01);
                    double lengthSetting = param["length"];
                    double speed = param["speed"];
                    auto direction = param["direction"];
                    modelStartTimes[j] += startTimeSetting;
                    config->mobility_config.mobilityEndTimeList.push_back(modelStartTimes[j]);
                    mobility->SetTime(modelStartTimes[j], lengthSetting);
                    mobility->SetCycle(pc, sc, dc);
                    mobility->SetSpeed(speed,
                                       Vector(direction[0], direction[1], direction[2]),
                                       speederrs[j],
                                       direrrs[j]);
                    if (i == 0) // first model should allocate initial positions
                    {
                        mobility->Install(nodesCon.Get(j), 1);
                        mobility->SetPosition(positions[j],
                                              perrors[j],
                                              lerrors[j],
                                              nodesCon.Get(j));
                    }
                    else
                        mobility->Install(nodesCon.Get(j), 0);
                    m_mobi.push_back(mobility);
                    modelStartTimes[j] += lengthSetting;
                }
                else if (model == "Turning")
                {
                    DolphinTurningHelper* turning = new DolphinTurningHelper();
                    turning->Setspeed(param["speed"]);
                    double startTimeSetting = 0;
                    if (param.contains("start_time"))
                        startTimeSetting = param["start_time"];
                    startTimeSetting = std::max(startTimeSetting, 0.01);
                    modelStartTimes[j] += startTimeSetting;
                    Simulator::Schedule(Seconds(startTimeSetting + startTime),
                                        &TurningLeftSingle,
                                        turning,
                                        nodesCon.Get(j),
                                        nodesCon.Get(param["center"]));
                    m_turning.push_back(turning);
                    double lengthSetting = param["length"];
                    modelStartTimes[j] += lengthSetting;
                }
                else
                {
                    std::cerr << "mobility model " << model << " error or have not been supported "
                              << std::endl;
                    return -1;
                }
            }
    }
    else //  all individual
    {
        int n = settings["mobility_models_individual"].size();
        std::vector<double> modelStartTimes(config->node_num, modelStartTime);
        for (int j = 0; j < n; ++j)
        {
            int model_num = settings["mobility_models_individual"][j]["models"].size(),
                id = settings["mobility_models_individual"][j]["node_id"];
            id--;
            for (int i = 0; i < model_num; ++i)
            {
                auto model = settings["mobility_models_individual"][j]["models"][i]["model"];
                auto param = settings["mobility_models_individual"][j]["models"][i]["params"];
                if (model == "ns3::DolphinMobilityConstantVelocity")
                {
                    DolphinMobilityHelper* mobility = new DolphinMobilityHelper();
                    mobility->SetMobilityModel(model);
                    double startTimeSetting = 0;
                    if (param.contains("start_time"))
                        startTimeSetting = param["start_time"];
                    startTimeSetting = std::max(startTimeSetting, 0.01);
                    double lengthSetting = param["length"];
                    double speed = param["speed"];
                    auto direction = param["direction"];
                    modelStartTimes[id] += startTimeSetting;
                    config->mobility_config.mobilityEndTimeList.push_back(modelStartTimes[id]);
                    mobility->SetTime(modelStartTimes[id], lengthSetting);
                    mobility->SetCycle(pc, sc, dc);
                    mobility->SetSpeed(speed,
                                       Vector(direction[0], direction[1], direction[2]),
                                       speederrs[id],
                                       direrrs[id]);
                    if (i == 0) // first model should allocate initial positions
                    {
                        mobility->Install(nodesCon.Get(id), 1);
                        mobility->SetPosition(positions[id],
                                              perrors[id],
                                              lerrors[id],
                                              nodesCon.Get(id));
                    }
                    else
                        mobility->Install(nodesCon.Get(id), 0);
                    m_mobi.push_back(mobility);
                    modelStartTimes[id] += lengthSetting;
                }
                else if (model == "Turning")
                {
                    DolphinTurningHelper* turning = new DolphinTurningHelper();
                    turning->Setspeed(param["speed"]);
                    double startTimeSetting = 0;
                    if (param.contains("start_time"))
                        startTimeSetting = param["start_time"];
                    startTimeSetting = std::max(startTimeSetting, 0.01);
                    modelStartTimes[id] += startTimeSetting;
                    Simulator::Schedule(Seconds(startTimeSetting + startTime),
                                        &TurningLeftSingle,
                                        turning,
                                        nodesCon.Get(id),
                                        nodesCon.Get(param["center"]));
                    m_turning.push_back(turning);
                    double lengthSetting = param["length"];
                    modelStartTimes[id] += lengthSetting;
                }
                else
                {
                    std::cerr << "mobility model " << model << " error or have not been supported "
                              << std::endl;
                    return -1;
                }
            }
        }
    }
    return 0;
}

/*读取common配置*/
std::size_t
ParseCommonConfig(DolphinConfig* config, const json& settings)
{
    if (settings.contains("common") && settings["common"].contains("lossRate"))
    {
        const auto& lossRateData = settings["common"]["lossRate"];

        for (auto& [key, value] : lossRateData.items())
        {
            // double lossRateValue = std::stod(key);
            LossRateConfig lossConfig;
            lossConfig.networking_completion_s = value["initialNetworkingCompletion"];
            lossConfig.packet_num = value["packet_num"];
            config->common_config.loss_rate[key] = lossConfig;
        }
    }

    return config->common_config.loss_rate.size();
}

// 根据指定的损失率查找对应的配置
LossRateConfig*
FindLossRateConfig(DolphinConfig* config, double lossRate)
{
    char buffer[50];
    std::sprintf(buffer, "%.2f", lossRate);
    std::string lossRateStr(buffer);
    auto it = config->common_config.loss_rate.find(lossRateStr);
    if (it != config->common_config.loss_rate.end())
    {
        return &(it->second);
    }
    return nullptr;
}

void
PrintConfig(DolphinConfig* config)
{
    NS_LOG_UNCOND("[config]Setting file: " << config->setting_path);
    NS_LOG_UNCOND("[config]Networking Completion Time: " << config->networking_completion_s);
    NS_LOG_UNCOND("[config]Simulation Time: " << config->simulation_time_s);
    NS_LOG_UNCOND("[config]Deploy Nodes Interval(s): " << config->deploy_nodes_Interval_s);
    NS_LOG_UNCOND("[config]Node Num: " << config->node_num);
    NS_LOG_UNCOND("[config]Leader ID: " << config->leader_id);
    NS_LOG_UNCOND("[config]Print Logging Info: " << config->printLoggingInfo);
    NS_LOG_UNCOND("[config]LossRate: " << config->phy_config.lossRate);
    std::ostringstream oss;
    oss << "[config]Senders: ";
    for (const auto& value : config->senders)
    {
        oss << value << " ";
    }
    NS_LOG_UNCOND(oss.str());
    oss.str("");
    oss.clear();
    oss << "[config]Receivers: ";
    for (const auto& value : config->receivers)
    {
        oss << value << " ";
    }
    NS_LOG_UNCOND(oss.str());

    NS_LOG_UNCOND("[config]APP-Packet Num: " << config->app_config.packet_num);
    NS_LOG_UNCOND("[config]APP-Packet Size: " << config->app_config.packet_size);
    NS_LOG_UNCOND("[config]APP-Data Rate(bps): " << config->app_config.data_rate_bps);
    NS_LOG_UNCOND("[config]APP-Send Start(s):  " << config->app_config.send_start_s);
    NS_LOG_UNCOND("[config]APP-Send Interval(s): " << config->app_config.send_interval_s);
}

std::string
GetTimestamp()
{
    // 获取当前时间
    std::time_t now = std::time(nullptr);

    // 将时间转换为 tm 结构
    std::tm* localTime = std::localtime(&now);

    // 创建一个字符串流来格式化时间戳
    std::stringstream ss;
    ss << (localTime->tm_year + 1900) // 年份
       << (localTime->tm_mon + 1)     // 月份 (0-11, 加 1)
       << localTime->tm_mday << "_"   // 日期
       << localTime->tm_hour          // 小时
       << localTime->tm_min;          // 分钟

    return ss.str();
}
} // namespace ns3