import ast
import subprocess
import os
import sys
import time
import re
import pytest
import shutil

distributed_simulation_py = "src/aqua-sim-ng/analyse_codes/distributed_simulation.py"

g_runs = 1

server_args_list = [
        {
            "nums_scenario": 4,
            "nodenum": 10,
            "runs": g_runs,
            "senders_receivers": ["[3,7]"],
            "packetnum": ["80"],
            "lossrate": ["0.20"]
        }
]

output_directory_pattern = re.compile(r'(?:.*/)?output_analyse_everytimes_(\d+)_(\d+)_(\d+)$')
analyse_result_directory = "analyse_result"

def run_analyse(trace_file, nums_scenario, nums_nodes):
    try:
        analyse_py = 'src/aqua-sim-ng/analyse_codes/analyse.py'
        if nums_nodes==38 or nums_scenario==4:
            analyse_py = 'src/aqua-sim-ng/analyse_codes/analyse_2.0.py'
        if nums_scenario==1:
            analyse_py = 'src/aqua-sim-ng/analyse_codes/analyse_3.0.py'            
        # 调用analyse.py并捕获输出，同时传递nums_nodes参数
        result = subprocess.run([sys.executable, analyse_py, '--nums_nodes', str(nums_nodes), trace_file],
                                capture_output=True, text=True, check=True)
        return result.stdout
    except subprocess.CalledProcessError as e:
        print(f"Error processing file {trace_file}: {e}")
        return None

def dict_to_cmd_args(args_dict, exclude_key):
    """Convert a dictionary of arguments to a list of command-line arguments."""
    cmd_args = []
    for key, value in args_dict.items():
        if key == exclude_key or (isinstance(exclude_key, list) and key in exclude_key):
            continue  # Skip this key if it matches the exclude_key
        if isinstance(value, (list, tuple)):
            value = ','.join(map(str, value))
        cmd_args.append(f"--{key}={value}")
    return cmd_args


def run_distributed_simulation(server_args, client_args, buffer_size=4096):
    """Run the server and client for distributed simulation."""
    print(f"server_args:{server_args}")
    try:
        # Convert server and client args to command-line arguments
        client_cmd_args = dict_to_cmd_args(client_args, None)

        senders_receivers = server_args['senders_receivers']

        command = ['/usr/bin/python3', distributed_simulation_py, 'server']
        server_cmd = [
            f"--nums_scenario={server_args['nums_scenario']}",
            f"--nodenum={server_args['nodenum']}",
            f"--runs={server_args['runs']}"
        ]

        server_cmd.append('--packetnum')
        server_cmd.extend(server_args['packetnum'])

        server_cmd.append('--lossrate')
        server_cmd.extend(server_args['lossrate'])

        server_cmd.append('--senders_receivers')
        server_cmd.extend(senders_receivers)

        # 合并命令和参数
        full_command = command + server_cmd

        print("server_cmd:", " ".join(server_cmd))

        # Start the server
        server_process = subprocess.Popen(
            full_command,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        print("Server started.")

        # Allow the server to start properly before starting the client
        time.sleep(1)

        # Start the client
        client_cmd = [sys.executable, distributed_simulation_py, "client", *client_cmd_args]

        print("client_cmd:", " ".join(client_cmd))

        client_process = subprocess.Popen(
            client_cmd,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        print("Client started.")

        # Wait for both processes to complete
        server_out, server_err = server_process.communicate()
        client_out, client_err = client_process.communicate()

        # Print server and client output for debugging
        print("Server Output:")
        print(server_out)
        print("Server Errors:")
        print(server_err)
        print("Client Output:")
        print(client_out)
        print("Client Errors:")
        print(client_err)

        return server_out, client_out
    except Exception as e:
        print(f"Error running distributed simulation: {e}")
        return None, None

def run_simulation(params):
    enable_stdout = True
    try:
        start_time = time.time()
        simulation_id, packet_num1, lossrate1, senders1, receivers1, nums_scenario1, nodenum1 = params
        #if nums_scenario1==4:
            #packet_num1 = 10000
        if nums_scenario1==5:
            nodenum1 = 38
        print(f"simulation_id:{simulation_id}, packet_num1:{packet_num1}, lossrate1:{lossrate1}, senders1:{senders1}, receivers1:{receivers1}, nums_scenario1:{nums_scenario1}, nodenum1:{nodenum1}")

        setting_file = (
        f"./src/aqua-sim-ng/examples/settings/settings_scenario_{nums_scenario1}_nodes_{nodenum1}.json"
        if nums_scenario1 == 5 or nodenum1 == 38
        else f"./src/aqua-sim-ng/examples/settings/settings_scenario_{nums_scenario1}.json"
        )
        print(setting_file)

        # Define the command to run the NS-3 simulation with appropriate parameters
        ns3_command = [
            "./ns3", "run", 
            f'dolphin_scenario_{nums_scenario1}'
            f' --setting={setting_file} '
            f' --runNumber={simulation_id}'
            f' --lossrate={lossrate1}'
            f' --packetnum={packet_num1}'
            f' --senders={senders1}'
            f' --receivers={receivers1}'
            f' --printLoggingInfo={enable_stdout}'
        ]

        if enable_stdout:
            stdout_folder = "stdout"
            os.makedirs(stdout_folder, exist_ok=True)
            output_path = os.path.join(stdout_folder, f"simulation_output_run_{simulation_id}_{packet_num1}_{int(lossrate1*100)}_{senders1}_{receivers1}_scenario_{nums_scenario1}_nodes_{nodenum1}.txt")
            # Open a file to redirect the output of the simulation
            with open(f"{output_path}", "w") as output_file:
                # Run the NS-3 simulation and redirect both stdout and stderr to the output filesubprocess.DEVNULL
                subprocess.run(ns3_command, stdout=output_file, stderr=output_file)

            print(f"Output saved to simulation_output_run_{simulation_id}_{packet_num1}_{int(lossrate1*100)}_{senders1}_{receivers1}_scenario_{nums_scenario1}_nodes_{nodenum1}.txt")

        else:
            subprocess.run(ns3_command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        # 记录结束时间
        end_time = time.time()

        # 计算并输出执行时间
        execution_time = end_time - start_time

        analysis_result = run_analyse(f"tracelog-scenario{nums_scenario1}-{nodenum1}/trace_{simulation_id}_{packet_num1}_{int(lossrate1*100)}_{nodenum1}_{senders1}_{receivers1}_dolphin_scenario_{nums_scenario1}.txt",nums_scenario1,nodenum1)
        if analysis_result:
            output_scenario = f"[R]scenario:{nums_scenario1}\n"
            output_nodes_num = f"[R]nodes:{nodenum1}\n"
            output_packet_num = f"[R]packets:{packet_num1}\n"
            output_filename = f'[R]OUT:output_{simulation_id}_{packet_num1}_{int(lossrate1*100)}_{senders1}_{receivers1}.txt\n'
            output_end = f'[R]OUTEND'
            print(output_filename+output_scenario+output_nodes_num+output_packet_num+analysis_result+output_end, flush=True)

        return f"Simulation complete: {simulation_id} completed({execution_time:.2f})."
    except Exception as e:
        print(f"Error in run_simulation for param {params}: {e}")
        return f"Failed for param {params}"


def get_filtered_lines_with_stats(file_path, keywords):
    """
    获取文件中同时包含所有关键字的行，并统计行数及 time_stamp 差值情况。

    :param file_path: 文件路径
    :param keywords: 关键字列表（行中必须包含所有关键字）
    :return: 包含过滤行的列表、行数、time_stamp 差值检查结果
    """
    filtered_lines = []
    time_stamps = []

    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            for line in file:
                # 检查行是否包含所有关键字
                if all(keyword in line for keyword in keywords):
                    #print(f"add line:{line}")
                    filtered_lines.append(line.strip())
                    
                    # 提取 time_stamp 值（假设为整数）
                    match = re.search(r"time_stamp= (\d+)", line)
                    if match:
                        time_stamps.append(int(match.group(1)))
                        #print(f"add time_stamp:{int(match.group(1))}")
    except FileNotFoundError:
        print(f"Error: File not found: {file_path}")
    except Exception as e:
        print(f"Error: {e}")

    # 检查 time_stamp 是否相差 300
    time_stamp_diff_correct = all(
        abs(time_stamps[i] - time_stamps[i - 1]) == 300 for i in range(1, len(time_stamps))
    )
    print(f"len(filtered_lines):{len(filtered_lines)},time_stamp_diff_correct:{time_stamp_diff_correct}")
    return filtered_lines, len(filtered_lines), time_stamp_diff_correct

def convert_args(server_args):
    # 拆分 senders_receivers 数据为 senders 和 receivers
    senders, receivers = eval(server_args["senders_receivers"][0])
    
    # 提取其他参数
    simulation_id = server_args.get("runs", None)  # 使用默认值 None 以防 key 不存在
    packet_num1 = int(server_args["packetnum"][0])
    lossrate1 = float(server_args["lossrate"][0])
    nums_scenario1 = server_args["nums_scenario"]
    nodenum1 = server_args["nodenum"]
    
    # 打包为目标格式
    params = (simulation_id, packet_num1, lossrate1, senders, receivers, nums_scenario1, nodenum1)
    return params

@pytest.mark.parametrize("server_args", server_args_list)
def test_application(server_args):
    print(f"test_application:{server_args}")
    """Test application."""
    output_dir = f"tracelog-scenario{server_args['nums_scenario']}-{server_args['nodenum']}"

    if os.path.exists(output_dir) and os.path.isdir(output_dir):
        shutil.rmtree(output_dir)
    
    assert not os.path.exists(output_dir), "Analyse result directory has not been deleted."

    # Run distributed simulation
    #server_out, client_out = run_distributed_simulation(server_args, client_args)
    # Run simulation
    sim_params = convert_args(server_args)
    simulation_out = run_simulation(sim_params)

    # Verify that the trace file exists
    # 获取 senders_receivers 列表中的所有整数对
    senders_receivers = server_args['senders_receivers']
    for pair in senders_receivers:
        try:
            # 将字符串转为 Python 列表对象
            parsed_pair = ast.literal_eval(pair)  # 安全解析
            if isinstance(parsed_pair, list) and len(parsed_pair) == 2:
                sender, receiver = parsed_pair
                # 获取第一个lossrate的值
                lossrate_str = server_args_list[0]["lossrate"][0]
                # 将字符串转换为浮动数
                lossrate_float = float(lossrate_str)
                # 将浮动数转换为整数
                lossrate_int = int(lossrate_float*100)
                # 获取第一个packetnum的值
                packetnum_str = server_args_list[0]["packetnum"][0]
                # 将字符串转换为整数
                packetnum_int = int(packetnum_str)

                trace_file = f"trace_1_{packetnum_int}_{lossrate_int}_{server_args['nodenum']}_{sender}_{receiver}_dolphin_scenario_{server_args['nums_scenario']}.txt"
                print(f"trace_file: {trace_file}")

                # check trace file
                trace_file_path = os.path.join(output_dir, trace_file)
                assert os.path.exists(trace_file_path), "Trace file does not exist."
                
                keywords = ["layer= APPLICATION", "action= 4"]
                filtered_lines, line_count, is_time_stamp_correct  = get_filtered_lines_with_stats(trace_file_path, keywords)
                # check app send interval(300s)
                assert is_time_stamp_correct==True, "Time stamp difference is not consistently 300."

                # check app send packet num
                assert line_count==packetnum_int, f"Send packet num({line_count})!=setting({packetnum_int})."
                
        except (ValueError, SyntaxError):
            print(f"Invalid format: {pair}")


    # Additional checks for server and client output
    assert "Simulation complete" in simulation_out, "Simulation output did not indicate successful completion."
 