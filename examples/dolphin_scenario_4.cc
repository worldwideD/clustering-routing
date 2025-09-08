
/*
 * scenario 4
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

#define COMPONET_NAME "Dolphin_Scenario_4"

NS_LOG_COMPONENT_DEFINE(COMPONET_NAME);

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
            double mobilityStartTime = config->networking_completion_s;
            // 航行在初始组网后开始
            // mobilityStartTime += config->deploy_nodes_Interval_s * config->node_num;
            setMobility(config, nodesCon, mobilityStartTime);
            /* 处理运动模型参数 End */

            config->app_config.send_start_s += mobilityStartTime;

            /*已作废，没有静止状态
            // 场景4的应用层开始单播时间设置:第一次航行开始，也就是静止模型结束
            if (config->mobility_config.mobilityEndTimeList.size() > 1)
            {
                config->app_config.send_start_s +=
                    config->mobility_config.mobilityEndTimeList.at(0);
            }
            */

            // 所有节点部署完成，开始创建应用层
            // 创建接收者
            for (uint16_t receiverApp = 0; receiverApp < config->node_num; receiverApp++)
            {
                std::string senderStr = "";
                createApplication(config,
                                  nodes.at(receiverApp),
                                  receiverApp,
                                  DolphinApplication::RECV,
                                  DolphinApplication::PacketType::DATA,
                                  Seconds(0),
                                  Seconds(config->simulation_time_s + 1));
            }
            // 创建发送者
            double startTime = config->app_config.send_start_s;
            // 计算传输所需时间 (秒)
            double transmissionTime = static_cast<double>(config->app_config.packet_num *
                                                          config->app_config.packet_size * 8) /
                                      config->app_config.data_rate_bps;

            double endTime = startTime + transmissionTime + 100;

            // TODO add sender config->node_num
            for (uint16_t sender : config->senders)
            {
                for (uint16_t receiver : config->receivers)
                {
                    if (sender != receiver)
                    {
                        std::string receiverStr = std::to_string(receiver);
                        createApplication(config,
                                          nodes.at(sender),
                                          sender,
                                          DolphinApplication::SEND_UNICAST,
                                          DolphinApplication::PacketType::DATA,
                                          Seconds(startTime),
                                          Seconds(endTime),
                                          receiverStr);
                        startTime = endTime + 100;
                        endTime = startTime + transmissionTime + 100;
                    }
                }
            }
            /*
                        for (uint16_t sender = 0; sender < 1; sender++)
                        {
                            for (uint16_t receiver = 0; receiver < config->node_num; receiver++)
                            {
                                if (sender != receiver)
                                {
                                    std::string receiverStr = std::to_string(receiver);
                                    createApplication(config,
                                                      nodes.at(sender),
                                                      sender,
                                                      DolphinApplication::SEND_UNICAST,
                                                      DolphinApplication::PacketType::DATA,
                                                      Seconds(startTime),
                                                      Seconds(endTime),
                                                      receiverStr);
                                    startTime = endTime + 100;
                                    endTime = startTime + transmissionTime + 100;
                                }
                            }
                        }
            */
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