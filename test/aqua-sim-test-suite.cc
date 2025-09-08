/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

// Include a header file from your module to test.
// #include "ns3/aqua-sim.h"

// An essential include is test.h
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
#include "ns3/test.h"

#include <chrono>
#include <iostream>

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;

// This is an example TestCase.
class AquaSimTestCase1 : public TestCase
{
  public:
    AquaSimTestCase1();
    virtual ~AquaSimTestCase1();

  private:
    virtual void DoRun(void);
};

// Add some help text to this case to describe what it is intended to test
AquaSimTestCase1::AquaSimTestCase1()
    : TestCase("AquaSim test case (does nothing)")
{
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
AquaSimTestCase1::~AquaSimTestCase1()
{
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
AquaSimTestCase1::DoRun(void)
{
    // A wide variety of test macros are available in src/core/test.h
    NS_TEST_ASSERT_MSG_EQ(true, true, "true doesn't equal true for some reason");
    // Use this one for floating point comparisons
    NS_TEST_ASSERT_MSG_EQ_TOL(0.01, 0.01, 0.001, "Numbers are not equal within tolerance");
}

// This is an example TestCase.
class DolphinApplicationTestCase : public TestCase
{
  public:
    DolphinApplicationTestCase();
    virtual ~DolphinApplicationTestCase();

  private:
    virtual void DoRun(void);
};

// Add some help text to this case to describe what it is intended to test
DolphinApplicationTestCase::DolphinApplicationTestCase()
    : TestCase("Dolphin application test case")
{
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
DolphinApplicationTestCase::~DolphinApplicationTestCase()
{
}

struct TestPacket
{
    int id;           // 报文ID
    double timestamp; // 时间戳
};

static uint64_t m_sent = 0;     //!< number of bytes sent
static uint64_t m_received = 0; //!< number of bytes received
static std::vector<TestPacket> tx_packets;

// 打印报文列表
void
PrintPackets(const std::vector<TestPacket>& packets)
{
    std::cout << "Current Packets:\n";
    for (const auto& packet : packets)
    {
        std::cout << "Packet ID: " << packet.id << ", Timestamp: " << packet.timestamp << "\n";
    }
}

// 增加报文
void
AddPacket(std::vector<TestPacket>& packets, const TestPacket& newPacket)
{
    packets.push_back(newPacket);

    // 确保按时间戳排序
    std::sort(packets.begin(), packets.end(), [](const TestPacket& a, const TestPacket& b) {
        return a.timestamp < b.timestamp;
    });
    std::cout << "Added Packet ID: " << newPacket.id << "\n";
}

// 删除报文（通过ID）
void
RemovePacket(std::vector<TestPacket>& packets, int packetId)
{
    auto it = std::remove_if(packets.begin(), packets.end(), [packetId](const TestPacket& p) {
        return p.id == packetId;
    });

    if (it != packets.end())
    {
        packets.erase(it, packets.end());
        std::cout << "Removed Packet ID: " << packetId << "\n";
    }
    else
    {
        std::cout << "Packet ID " << packetId << " not found.\n";
    }
}

// 检查时间间隔是否符合条件的函数
bool
CheckIntervals(const std::vector<TestPacket>& packets, double minInterval, double maxInterval)
{
    for (size_t i = 1; i < packets.size(); ++i)
    {
        double interval = packets[i].timestamp - packets[i - 1].timestamp;
        std::cout << "Interval between Packet " << packets[i - 1].id << " and Packet "
                  << packets[i].id << ": " << interval << " seconds\n";

        if (interval < minInterval || interval > maxInterval)
        {
            std::cout << "Interval not within bounds (" << minInterval << ", " << maxInterval
                      << ").\n";
            return false; // 不符合条件
        }
    }
    return true; // 所有间隔均符合条件
}

void
RxTrace(Ptr<const Packet> packet, uint64_t src_id, uint64_t dst_id, uint8_t packet_type)
{
    m_received++;
    std::cout << "RxTrace:" << m_received << "\n";
}

void
TxTrace(Ptr<const Packet> packet, uint64_t src_id, uint64_t dst_id, uint8_t packet_type)
{
    if (packet_type != DolphinApplication::PacketType::NODEDOWEROFF)
    {
        m_sent++;
        std::cout << "TxTrace:" << m_sent << "\n";
        // 获取当前时间点
        auto now = std::chrono::system_clock::now();

        // 获取自纪元以来的时间间隔（单位：纳秒）
        auto duration = now.time_since_epoch();

        // 转换为秒，并存储为 double
        double timestamp =
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1e9;

        AddPacket(tx_packets, {m_sent, Simulator::Now().GetSeconds()});
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
            // config->networking_completion_s;
            //  航行在初始组网后开始,但第一个静止的模型也在列表中，因此同时开始
            setMobility(config, nodesCon, mobilityStartTime);
            /* 处理运动模型参数 End */

            config->app_config.send_start_s += mobilityStartTime;

            /*已作废，没有静止状态
            // 场景1的应用层开始广播时间设置:第一次航行结束
            if (config->mobility_config.mobilityEndTimeList.size() > 1)
            {
                config->app_config.send_start_s +=
                    config->mobility_config.mobilityEndTimeList.at(1);
            }*/

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

            std::cout << "createApplication over\n";
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
            } // + config->app_config.send_interval_s
        }
    }
    std::cout << "DeployAndStartApplications over\n";
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void
DolphinApplicationTestCase::DoRun(void)
{
    DolphinConfig config;
    int packetNum = 100;
    std::string packetNumStr = "--packetnum=" + std::to_string(packetNum);
    char* argv[] = {(char*)"DolphinApplicationTest",
                    (char*)"src/aqua-sim-ng/examples/settings/settings_scenario_4.json",
                    (char*)packetNumStr.c_str(),
                    (char*)"--senders=3",
                    (char*)"--receivers=7"};
    int argc = sizeof(argv) / sizeof(argv[0]);

    config.setting_path = "src/aqua-sim-ng/examples/settings/settings_scenario_4.json";
    std::cout << "-----------Reading settings.json(" << config.setting_path << ")-----------\n";
    int ret = ParseConfig(&config, argc, argv);
    if (ret < 0)
    {
        std::cerr << "ParseConfig error(" << config.setting_path << ")" << std::endl;
        return;
    }

    InitModulesLog(&config);

    config.app_config.rx_cb = MakeCallback(&RxTrace);
    config.app_config.tx_cb = MakeCallback(&TxTrace);

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

    std::cout << "Creating Nodes:" << config.node_num << "\n";

    Simulator::ScheduleNow(&DeployAndStartApplications, nodesCon, devices, asHelper, 0, &config);

    Packet::EnablePrinting(); // for debugging purposes
    std::cout << "-----------Running Simulation-----------\n";
    Simulator::Stop(Seconds(simStop + 30));

    Simulator::Run();

    asHelper.GetChannel()->PrintCounters();
    CloseModules();
    Simulator::Destroy();
    std::cout << "-----------Running Simulation over-----------\n";
    std::cout << "m_sent:" << m_sent << "\n";
    PrintPackets(tx_packets);

    std::string packetNumErrorStr = "Sent packets num doesn't equal " + std::to_string(packetNum);

    // A wide variety of test macros are available in src/core/test.h
    NS_TEST_ASSERT_MSG_EQ(m_sent, packetNum, packetNumErrorStr);

    NS_TEST_ASSERT_MSG_EQ(true,
                          CheckIntervals(tx_packets, 290, 310),
                          "send interval doesn't equal 300");
}

// The TestSuite class names the TestSuite, identifies what type of TestSuite,
// and enables the TestCases to be run.  Typically, only the constructor for
// this class must be defined
//
class AquaSimTestSuite : public TestSuite
{
  public:
    AquaSimTestSuite();
};

AquaSimTestSuite::AquaSimTestSuite()
    : TestSuite("aqua-sim", UNIT)
{
    // TestDuration for TestCase can be QUICK, EXTENSIVE or TAKES_FOREVER
    AddTestCase(new AquaSimTestCase1, TestCase::QUICK);
    AddTestCase(new DolphinApplicationTestCase, TestCase::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static AquaSimTestSuite aquaSimTestSuite;
