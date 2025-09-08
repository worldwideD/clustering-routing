
/*
 * scenario 2
 *
 *  Created on: 9/2024
 *
 *
 */

#include "ns3/applications-module.h"
#include "ns3/aqua-sim-ng-module.h"
#include "ns3/aqua-sim-propagation.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/dolphin-common.h"
#include "ns3/dolphin_trace.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"

#include <fstream>
#include <iomanip> // setprecision
#include <math.h>
#include <random>
#include <sstream> // stringstream
using json = nlohmann::json;

using namespace ns3;

#define COMPONET_NAME "Dolphin_Scenario_2"

NS_LOG_COMPONENT_DEFINE(COMPONET_NAME);

uint16_t
GetRandomNodeId(uint16_t node_num, uint16_t exclude_id)
{
    uint32_t seed = static_cast<uint32_t>(time(0)); // 获取当前时间戳作为种子
    RngSeedManager::SetSeed(seed);                  // 设置随机数种子

    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uint16_t randomIntValue = 0;
    do
    {
        randomIntValue = uv->GetInteger(0, node_num);
    } while (randomIntValue == exclude_id);
    return randomIntValue;
}

/*布防节点和应用层任务（指定时间运行）*/
void
DeployAndStartApplications(NodeContainer nodesCon,
                           NetDeviceContainer devices,
                           AquaSimHelper asHelper,
                           uint32_t index,
                           DolphinConfig* config)
{
    std::vector<Ptr<Node>> nodes = std::vector<Ptr<Node>>(nodesCon.Begin(), nodesCon.End());
    if (index < nodes.size())
    {
        NetDeviceContainer addedDevices = DeployNetDevice(nodes[index], devices, asHelper, config);

        NS_LOG_FUNCTION("NetDevice(" << index << ") deployed on node at "
                                     << Simulator::Now().GetSeconds() / 60 << " minutes");
        index++;
        // 如果还有剩余节点，调度下一个部署
        if (index < nodes.size())
        {
            Simulator::Schedule(Seconds(config->deploy_nodes_Interval_s),
                                &DeployAndStartApplications,
                                nodesCon,
                                addedDevices,
                                asHelper,
                                index,
                                config);
        }
        else
        {
            // 所有节点部署完成，开始其它模块创建和运行
            /* 处理运动模型参数 begin */
            double mobilityStartTime = 0;
            // config->networking_completion_s;
            //  航行在初始组网后开始,但第一个静止的模型也在列表中，因此同时开始
            setMobility(config, nodesCon, mobilityStartTime);
            /* 处理运动模型参数 End */

            // 所有节点部署完成，开始创建应用层
            int nodeId = 0;
            // 随机选择一个非领航节点在第一次向东运动时失效
            // 失效后开始发送广播报文
            uint16_t randomStopNodeId = 1;
            // GetRandomNodeId(config->node_num - 1, config->leader_id);
            double appRandomStopTime = config->mobility_config.mobilityEndTimeList.at(0) + 100;

            // 创建10个节点收包应用层
            for (Ptr<Node> node : nodes)
            {
                if (nodeId == randomStopNodeId &&
                    config->mobility_config.mobilityEndTimeList.size() > 0)
                {
                    createApplication(config,
                                      node,
                                      nodeId,
                                      DolphinApplication::RECV,
                                      DolphinApplication::PacketType::DATA,
                                      Seconds(0 + 10),
                                      Seconds(appRandomStopTime));
                }
                else
                {
                    createApplication(config,
                                      node,
                                      nodeId,
                                      DolphinApplication::RECV,
                                      DolphinApplication::PacketType::DATA,
                                      Seconds(0 + 10),
                                      Seconds(config->simulation_time_s + 1));
                }
                nodeId++;
            }
            // 失效后开始发送广播报文
            double appSendBroadcastTime = appRandomStopTime;
            config->app_config.send_start_s = appSendBroadcastTime;
            createApplication(config,
                              nodes.at(config->leader_id - 1),
                              config->leader_id - 1,
                              DolphinApplication::SEND_BROADCAST,
                              DolphinApplication::PacketType::TASK,
                              Seconds(config->app_config.send_start_s + 10),
                              Seconds(config->simulation_time_s + 1),
                              std::to_string(DolphinApplication::broadcastId));

            // 应用层初始化完毕后，设置其它
            ConfigModulesTrace();

            // 应用层启动完毕后，开始启动定位包发送
            Ptr<NetDevice> device;
            for (NetDeviceContainer::Iterator i = addedDevices.Begin(); i != addedDevices.End();
                 ++i)
            {
                device = (*i);
                Ptr<AquaSimNetDevice> asDevice = DynamicCast<AquaSimNetDevice>(device);
                if (asDevice)
                {
                    asDevice->GetMac()->LocPacketSend(); // 调用loc发包
                }
            }
        }
    }
}

int
main(int argc, char* argv[])
{
    DolphinConfig config;
    int ret = ParseConfig(&config, argc, argv);
    if (ret < 0)
    {
        std::cerr << "ParseConfig error(" << config.setting_path << ")" << std::endl;
        return -1;
    }
    std::cout << "-----------Reading settings.json(" << config.setting_path << ")-----------\n";
    LogComponentEnable(COMPONET_NAME, LOG_FUNCTION);
    LogComponentEnable(COMPONET_NAME, LOG_PREFIX_ALL);
    InitModulesLog(&config);

    std::cout << "-----------Initializing simulation-----------\n";
    double simStop = config.simulation_time_s; // seconds
    int nodes = config.node_num;

    NodeContainer nodesCon;
    nodesCon.Create(nodes);

    PacketSocketHelper socketHelper;
    socketHelper.Install(nodesCon);

    AquaSimHelper asHelper = AquaSimHelper::Default();
    // 初始化各层参数
    InitModules(&config, asHelper);

    NetDeviceContainer devices;

    std::cout << "Creating Nodes\n";

    /* 创建nodes begin */
    Simulator::ScheduleNow(&DeployAndStartApplications, nodesCon, devices, asHelper, 0, &config);
    /* 创建nodes end */

    Packet::EnablePrinting(); // for debugging purposes
    std::cout << "-----------Running Simulation-----------\n";
    Simulator::Stop(Seconds(simStop + 30));

    Simulator::Run();

    asHelper.GetChannel()->PrintCounters();
    CloseModules();
    Simulator::Destroy();

    std::cout << "fin.\n";
    return 0;
}