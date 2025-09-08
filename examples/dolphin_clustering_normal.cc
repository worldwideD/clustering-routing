
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

#define COMPONET_NAME "Dolphin_Clustering_Normal"

NS_LOG_COMPONENT_DEFINE(COMPONET_NAME);
static std::set<uint64_t> recv_data_nodes;

void
MainRxTrace(Ptr<const Packet> packet, uint64_t src_id, uint64_t dst_id, uint8_t packet_type)
{
    NS_LOG_FUNCTION(dst_id << "has Received Data");
    recv_data_nodes.insert(dst_id);
}

void
CheckReception(DolphinConfig* config, std::vector<Ptr<Node>> nodes)
{
    if (static_cast<int>(recv_data_nodes.size()) >= config->node_num - 1)
    {
        NS_LOG_FUNCTION("All nodes except the sender have received the packet.");
    }
    else
    {
        NS_LOG_FUNCTION(
            "Still waiting for some nodes to receive the packet:" << recv_data_nodes.size());
        if (config->app_config.packet_num == 0)
        {
            NS_LOG_FUNCTION("Still waiting for some nodes to receive the packet:"
                            << recv_data_nodes.size() << ",packet_num==0 stop send");
            return;
        }
        // 如果未收齐，继续发送广播包并调度下一次检测
        for (int sendNodeId : config->senders)
        {
            createApplication(config,
                              nodes.at(sendNodeId),
                              sendNodeId,
                              DolphinApplication::SEND_BROADCAST,
                              DolphinApplication::PacketType::DATA,
                              Seconds(0),
                              Seconds(config->app_config.send_interval_s),
                              std::to_string(DolphinApplication::broadcastId));
            Config::ConnectWithoutContext("/NodeList/" + std::to_string(sendNodeId) +
                                              "/ApplicationList/*/$ns3::Application/Tx",
                                          MakeCallback(&AppTxTrace));
            config->app_config.packet_num--;
        }
        Simulator::Schedule(Seconds(config->app_config.send_interval_s + 100),
                            &CheckReception,
                            config,
                            nodes);
    }
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
            // double mobilityStartTime = config->networking_completion_s;
            double mobilityStartTime = 0;
            // config->networking_completion_s;
            //  航行在初始组网后开始,但第一个静止的模型也在列表中，因此同时开始
            setMobility(config, nodesCon, mobilityStartTime);
            /* 处理运动模型参数 End */
            if (config->net_config.regularCH == 1)
            {
                Simulator::Schedule(Seconds(config->networking_completion_s), &CHRegularCounter, addedDevices);
            }

            config->app_config.send_start_s += mobilityStartTime;

            // 所有节点部署完成，开始创建应用层
            // 创建所有节点的接收者
            int nodeId = 0;
            for (Ptr<Node> node : nodes)
            {
                createApplication(config,
                                  node,
                                  nodeId,
                                  DolphinApplication::RECV,
                                  DolphinApplication::PacketType::DATA,
                                  Seconds(0),
                                  Seconds(config->simulation_time_s + 1),
                                  std::to_string(DolphinApplication::broadcastId));

                nodeId++;
            }

            // 创建发送者
            // double startTime = config->app_config.send_start_s;
            // Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
            // double startTime = config->app_config.send_start_s + rand->GetValue() * config->app_config.packet_size * 8 / config->app_config.data_rate_bps;
            double startTime = config->app_config.send_start_s;
            // 计算传输所需时间 (秒)
            double transmissionTime = static_cast<double>(config->app_config.packet_size * 8) /
                                      config->app_config.data_rate_bps;

            double endTime = startTime + transmissionTime;
            while (startTime < config->simulation_time_s)
            {
                std::vector<uint16_t> tx;
                tx.clear();
                for (uint16_t sender : config->senders)
                {
                    tx.push_back(sender);
                }
                std::random_shuffle(tx.begin(), tx.end());

                for (auto i = tx.begin(); i != tx.end() && startTime < config->simulation_time_s; )
                {
                    startTime = endTime;
                    endTime = startTime + transmissionTime;
                    for (int j = 0; j < 1 && i != tx.end(); ++j, ++i)
                    {
                        uint16_t sender = *i;
                        createApplication(config,
                                        nodes.at(sender),
                                        sender,
                                        DolphinApplication::SEND_BROADCAST,
                                        DolphinApplication::PacketType::DATA,
                                        Seconds(startTime),
                                        Seconds(endTime),
                                        std::to_string(DolphinApplication::broadcastId));
                        std::cout << "sender = " << sender << "  time = " << startTime 
                                << std::endl;
                    }
                }
            }
            #if 0
            while (startTime < config->simulation_time_s)
            {
                std::vector<std::pair<uint16_t, uint16_t>> txrx;
                txrx.clear();
                for (uint16_t sender : config->senders)
                {
                    for (uint16_t receiver : config->receivers)
                    {
                        if (sender != receiver)
                        {
                            txrx.push_back(std::make_pair(sender, receiver));
                        }
                    }
                }
                std::random_shuffle(txrx.begin(), txrx.end());

                for (auto i = txrx.begin(); i != txrx.end() && startTime < config->simulation_time_s; )
                {
                    startTime = endTime;
                    endTime = startTime + transmissionTime;
                    for (int j = 0; j < 1 && i != txrx.end(); ++j, ++i)
                    {
                        uint16_t sender = i->first;
                        uint16_t receiver = i->second;
                        std::string receiverStr = std::to_string(receiver);
                        // createApplication(config,
                        //                 nodes.at(sender),
                        //                 sender,
                        //                 DolphinApplication::SEND_UNICAST,
                        //                 DolphinApplication::PacketType::DATA,
                        //                 Seconds(startTime),
                        //                 Seconds(endTime),
                        //                 receiverStr);
                        std::cout << "sender = " << sender << "  receiver = " << receiver << "  time = " << startTime 
                                << std::endl;
                    }
                }
            }
            #endif
            // for (const auto& point : config->src_dst_tx_list)
            // {
            //     std::cout << "Send packets from " << point.first << " to " << point.second << "\n";
            //     uint16_t sender = point.first;
            //     uint16_t receiver = point.second;
            //     std::string receiverStr = std::to_string(receiver);
            //     createApplication(config,
            //                       nodes.at(sender),
            //                       sender,
            //                       DolphinApplication::SEND_UNICAST,
            //                       DolphinApplication::PacketType::DATA,
            //                       Seconds(startTime),
            //                       Seconds(config->simulation_time_s),
            //                       receiverStr);
            // }

            // 应用层初始化完毕后，设置其它
            ConfigModulesTrace();
            // 应用层启动完毕后，设置节点能力
            Ptr<NetDevice> device;
            std::vector<int> ids;
            ids.clear();
            for (int i = 0; i < config->node_num; ++i)
                ids.emplace_back(i);
            std::random_shuffle(ids.begin(), ids.end());
            int cur = 0;
            for (NetDeviceContainer::Iterator i = addedDevices.Begin(); i != addedDevices.End();
                 ++i, ++cur)
            {
                device = (*i);
                Ptr<AquaSimNetDevice> asDevice = DynamicCast<AquaSimNetDevice>(device);
                if (asDevice)
                {
                    Ptr<Node> n = asDevice->GetNode();
                    Ptr<AquaSimRouting> rt = asDevice->GetRouting();
                    if (n->GetId() == 0 && (config->net_config.m_routingProtocol == "ns3::RoutingSOAM" ||
                                           config->net_config.m_routingProtocol == "ns3::RoutingERNC"))
                        rt->SetAttribute("Identity", StringValue("SINK"));
                    if (config->net_config.m_routingProtocol == "ns3::DolphinRoutingClustering" ||
                        config->net_config.m_routingProtocol == "ns3::RoutingAEWCA")
                        rt->SetAttribute("HelloDelay", DoubleValue(config->net_config.helloInterval * ids[cur]));
                    if (n->GetId() < config->node_num * config->long_nodes_rate)
                    {
                        rt->SetAttribute("Ability", StringValue("LONG"));
                        if (config->net_config.m_routingProtocol == "ns3::DolphinRoutingLEACH" ||
                        config->net_config.m_routingProtocol == "ns3::DolphinRoutingClustering" ||
                        config->net_config.m_routingProtocol == "ns3::RoutingSOAM" ||
                        config->net_config.m_routingProtocol == "ns3::RoutingERNC" ||
                        config->net_config.m_routingProtocol == "ns3::RoutingRCRP" ||
                        config->net_config.m_routingProtocol == "ns3::RoutingAEWCA")
                        {
                            rt->SetAttribute("Range", DoubleValue(config->phy_config.shortRange));
                        }
                    }else
                    {
                        rt->SetAttribute("Ability", StringValue("SHORT"));
                        if (config->net_config.m_routingProtocol == "ns3::DolphinRoutingLEACH" ||
                        config->net_config.m_routingProtocol == "ns3::DolphinRoutingClustering" ||
                        config->net_config.m_routingProtocol == "ns3::RoutingSOAM" ||
                        config->net_config.m_routingProtocol == "ns3::RoutingERNC" ||
                        config->net_config.m_routingProtocol == "ns3::RoutingRCRP" ||
                        config->net_config.m_routingProtocol == "ns3::RoutingAEWCA")
                            rt->SetAttribute("Range", DoubleValue(config->phy_config.shortRange));
                    }
                    rt->Initialize();
                }
            } // + config->app_config.send_interval_s
            // Simulator::Schedule(Seconds(startTime + 100), &CheckReception, config, nodes);
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

    config.app_config.rx_cb = MakeCallback(&MainRxTrace);

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
    CloseModules(&config);
    Simulator::Destroy();

    std::cout << "fin.\n";
    return 0;
}