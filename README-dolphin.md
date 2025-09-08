# Running Dolphin 

-------------------------------------

一、运行仿真程序
./ns3 run "dolphin_scenario_4 --setting=/home/hull/NS3/ns-allinone-3.40/ns-3.40/src/aqua-sim-ng/examples/settings/settings_scenario_4.json"

```
where:

`dolphin_scenario_1`: name of the simulation script that refers to `dolphin_scenario_1.cc`
`dolphin_scenario_2`: name of the simulation script that refers to `dolphin_scenario_2.cc`
`dolphin_scenario_4`: name of the simulation script that refers to `dolphin_scenario_4.cc`
`dolphin_scenario_1_nodes_50`: name of the simulation script that refers to `dolphin_scenario_5.cc`

`--setting=`: Setting file path

`--runNumber`: Run Number,Option, default:1

`--lossrate`: Lossrate,Option, default:value in json

`--packetnum`: The num of packets to be sent by application(leader loop),Option, default:value in json

`--leaderid`: Leader id,Option, default:value in json

`--senders`: Sender List,Option, default:value in json

`--receivers`: Receiver List,Option, default:value in json

`--printLoggingInfo`: Print Logging Info,0:disable,1:enable,Option, default:1

tracelog以配置文件中"trace路径/trace_runNumber_packetnum_lossrate.txt"命名

Examples:
--场景1：./ns3 run "dolphin_scenario_1 --setting=/home/hull/NS3/ns-allinone-3.40/ns-3.40/src/aqua-sim-ng/examples/settings/settings_scenario_1.json --runNumber=1 --lossrate=0.20 --packetnum=1 --senders=3 --receivers=255 --printLoggingInfo=false"
--场景2：./ns3 run "dolphin_scenario_2 --setting=/home/hull/NS3/ns-allinone-3.40/ns-3.40/src/aqua-sim-ng/examples/settings/settings_scenario_2.json --runNumber=1 --lossrate=0.20 --packetnum=1 --senders=3 --receivers=255 --printLoggingInfo=false"
--场景4：./ns3 run "dolphin_scenario_4 --setting=/home/hull/NS3/ns-allinone-3.40/ns-3.40/src/aqua-sim-ng/examples/settings/settings_scenario_4.json --runNumber=1 --lossrate=0.20 --packetnum=10000 --senders=1 --receivers=2 --printLoggingInfo=false"
--场景5（38节点）：./ns3 run "dolphin_scenario_5 --setting=/home/hull/NS3/ns-allinone-3.40/ns-3.40/src/aqua-sim-ng/examples/settings/settings_scenario_5_nodes_38.json --runNumber=1 --lossrate=0.20 --senders=1 --receivers=255 --printLoggingInfo=false"
--场景4（38节点）：./ns3 run "dolphin_scenario_4 --setting=/home/hull/NS3/ns-allinone-3.40/ns-3.40/src/aqua-sim-ng/examples/settings/settings_scenario_4_nodes_38.json --runNumber=1 --lossrate=0.20 --packetnum=2000 --senders=1 --receivers=36 --printLoggingInfo=false"
```
二、运行批量运行仿真程序脚本
```
cd trace路径
python3 src/aqua-sim-ng/analyse_codes/dolphin-run.py --nums_scenario=4 --runs=5

`--nodenum`:node num, Required

`--verbose`: Print std out,Option, default:False

`--runTools`: Run tools before simulation,Option, default:False

`--run_id`: ID of simulation runs ,Option, default:1

`--runs`: Run Number,Option, default:1

`--lossrate`: Lossrate,Option, default:0.05,0.10,0.15,0.20,0.25,0.30

`--packetnum`: The num of packets to be sent by application(leader loop),Option, default:5

`--senders_receivers`: sender and receiver id list,Option

`--nums_scenario`:Scenario1/2/3/4, Option, default:1


Examples:
--场景1：python3 src/aqua-sim-ng/analyse_codes/dolphin-run.py --nums_scenari=1 --nodenum=10 --runs=50  --packetnum 1 2 3 4 5 --senders_receivers [3,255]
--场景2：python3 src/aqua-sim-ng/analyse_codes/dolphin-run.py --nums_scenari=2 --nodenum=10 --runs=50  --packetnum 1 2 3 4 5 --senders_receivers [3,255]
--场景4：python3 src/aqua-sim-ng/analyse_codes/dolphin-run.py --nums_scenari=4 --nodenum=10 --runs=50  --packetnum=10000 --senders_receivers [6,8] [10,5] [1,10] [3,10] [1,3] [10,2] [9,6] [1,7] [3,5] [1,5] [9,5] [9,2] [2,4] [6,7] [5,6] [5,4] [7,8] [7,4] 
--场景5（38节点）：python3 src/aqua-sim-ng/analyse_codes/dolphin-run.py --nums_scenari=5 --nodenum=38 --runs=50  --lossrate=0.20
--场景4（38节点）：python3 src/aqua-sim-ng/analyse_codes/dolphin-run.py --nums_scenari=4 --nodenum=38 --runs=50  --lossrate=0.20 --packetnum=2000 --senders_receivers [1,36] [12,25] [2,26] [2,35] [6,13] [6,31] [1,3] [1,4] [1,9] [2,7] [2,8] [2,9] [3,14] [3,16] [3,17] [7,21] [7,23] [7,24]
```

三、运行批量运行仿真结果分析

由于文件中涉及一些地址,而大家拉取到本地的文件地址名称各不相同,所以这里另作说明。
这里介绍最为简单明了的操作方式，拉取代码之后，如果要使用analyse_code中的文件来分析trace，可以将analyse.py和analyse_all_painting.py复制到ns3.40文件夹下。
运行python analyse_all_painting.py即可分析所有trace文件并出图，还会生成每一个trace的分析到output文件夹。
前提是trace文件也是放在ns3.40文件夹下的。

## 使用说明书

### 1). 分析脚本 `analyse_2.0.py`

#### 功能描述
`analyse_2.0.py` 是一个用于分析网络模拟输出数据的脚本。它能够处理日志文件，并计算出多种网络性能指标，如丢包率、控制开销占比、任务送达率等。

#### 依赖环境
- Python 环境
- 必要的Python库：`re`, `subprocess`, `os`, `argparse`, `numpy`, `matplotlib`

#### 参数说明
- `--nums_nodes`: 必需参数，指定网络中的节点数量。
- `trace_file`: 必需参数，指定要分析的日志文件路径。

#### 使用步骤
1. 确保你的Python环境中已经安装了所有必要的库。
2. 使用命令行工具，导航到脚本所在的目录。
3. 执行以下命令，替换`<nums_nodes>`和`<trace_file_path>`为实际的节点数量和日志文件路径：
   ```
   python analyse_2.0.py --nums_nodes=<nums_nodes> <trace_file_path>
   ```
4. 脚本将输出分析结果，包括各种网络性能指标。

### 2). 批量分析脚本 `analyse_all_painting_2.0.py`

#### 功能描述
`analyse_all_painting_2.0.py` 是一个用于批量处理和分析多个日志文件，并生成图表的脚本。它能够处理不同丢包率和包数量的模拟结果，并输出统计图表。

#### 依赖环境
- Python环境
- 必要的Python库：`sys`, `subprocess`, `os`, `re`, `multiprocessing`, `numpy`, `matplotlib`, `argparse`, `shutil`, `time`

#### 参数说明
- `--nums_nodes`: 必需参数，指定网络中的节点数量。
- `--packetnum`: 可选参数，指定要分析的包数量，默认为`[5]`。
- `--lossrate`: 可选参数，指定要分析的丢包率，默认为`[0.05, 0.10, 0.15, 0.20, 0.25, 0.30]`。
- `--nums_scenario`: 必需参数，指定模拟场景编号。
- `--only_paint`: 可选参数，指定是否仅生成图表，默认为`False`。

#### 使用步骤
1. 确保你的Python环境中已经安装了所有必要的库。
2. 使用命令行工具，导航到脚本所在的目录。
3. 执行以下命令，替换`<nums_nodes>`和`<nums_scenario>`为实际的节点数量和模拟场景编号：
   ```
   python analyse_all_painting_2.0.py --nums_nodes=<nums_nodes> --nums_scenario=<nums_scenario> --packetnum=<packetnum_list> --lossrate=<lossrate_list> --only_paint=True
   ```
   其中`<packetnum_list>`和`<lossrate_list>`是可选的，用于指定包数量和丢包率列表。
4. 脚本将处理日志文件，并生成相应的统计图表，保存在指定的目录中。

#### 注意事项
- 确保日志文件的命名符合脚本中的正则表达式模式，以便脚本能够正确识别和处理文件。
- 如果`--only_paint`参数设置为`True`，则脚本将仅生成图表，不会重新分析日志文件。这适用于已经生成了分析结果的情况。
- 脚本中的图表生成依赖于`matplotlib`库，确保其已正确安装并配置。

```
Examples:
--场景1：python3 src/aqua-sim-ng/analyse_codes/analyse_all_painting.py --nums_nodes=10 --nums_scenario=1 --packetnum=1,2,3,4,5
--场景2：python3 src/aqua-sim-ng/analyse_codes/analyse_all_painting.py --nums_nodes=10 --nums_scenario=2 --packetnum=1,2,3,4,5
--场景4: python3 src/aqua-sim-ng/analyse_codes/analyse_all_painting.py --nums_nodes=10 --nums_scenario=4 --packetnum=10000
--场景4（38节点）：python3 src/aqua-sim-ng/analyse_codes/analyse_all_painting_2.0.py  --nums_scenario=4 --nums_nodes=38 --packetnum=2000 --lossrate=0.20
--场景5（38节点）：python3 src/aqua-sim-ng/analyse_codes/analyse_all_painting_2.0.py  --nums_scenario=5 --nums_nodes=38 --packetnum=10 --lossrate=0.20

4、多服务器自动化仿真并行运行

步骤1：开启server，并设置参数
--场景1：python3 src/aqua-sim-ng/analyse_codes/distributed_simulation.py server --nums_scenari=1 --nodenum=10 --runs=50  --packetnum 20 --lossrate 0.2
--场景2：python3 src/aqua-sim-ng/analyse_codes/distributed_simulation.py server --nums_scenari=2 --nodenum=10 --runs=50  --packetnum 20 --senders_receivers [3,255]
--场景4: python3 src/aqua-sim-ng/analyse_codes/distributed_simulation.py server --nums_scenario=4 --nodenum=10 --runs=50 --packetnum 100 --senders_receivers [6,8] [10,5] [1,10] [3,10] [1,3] [10,2] [9,6] [1,7] [3,5] [1,5] [9,5] [9,2] [2,4] [6,7] [5,6] [5,4] [7,8] [7,4]
--场景4（38节点）：python3 src/aqua-sim-ng/analyse_codes/distributed_simulation.py server --nums_scenario=4 --nodenum=38 --runs=50 --packetnum 100 --lossrate 0.20 --senders_receivers [1,36] [12,25] [2,26] [2,35] [6,13] [6,31] [1,3] [1,4] [1,9] [2,7] [2,8] [2,9] [3,14] [3,16] [3,17] [7,21] [7,23] [7,24]
--场景5（38节点）：python3 src/aqua-sim-ng/analyse_codes/distributed_simulation.py server --nums_scenario=5 --nodenum=38 --runs=1 --packetnum 20 --lossrate 0.2

步骤2：在多个服务器开启client，连接服务器地址，client会上报空闲CPU，server会下发client空闲cpu50%个任务给client执行
python3 src/aqua-sim-ng/analyse_codes/distributed_simulation.py client --host=192.168.186.14

步骤3：结果输出在"tracelog_scenario场景号_节点数\analyse_result"目录下，比如场景1的所有结果输出到"tracelog-scenario1-10\analyse_result"
```