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
            "nums_scenario": 1,
            "nodenum": 10,
            "runs": g_runs,
            "senders_receivers": ["[3,255]","[4,255]"],
            "packetnum": ["1", "2"],
            "lossrate": ["0.20"],
            "analyse_result_exp_num": 5,
            "analyse_result_exp_num_persender": 7
        },
        {
            "nums_scenario": 4,
            "nodenum": 10,
            "runs": g_runs,
            "senders_receivers": ["[3,5]","[3,10]"],
            "packetnum": ["1"],
            "lossrate": ["0.20"],
            "analyse_result_exp_num": 5,
            "analyse_result_exp_num_persender": 5
        },
        {
            "nums_scenario": 5,
            "nodenum": 38,
            "runs": g_runs,
            "senders_receivers": ["[3,255]","[4,255]"],
            "packetnum": ["1"],
            "lossrate": ["0.20"],
            "analyse_result_exp_num": 3,
            "analyse_result_exp_num_persender": 3
        },
        {
            "nums_scenario": 4,
            "nodenum": 38,
            "runs": g_runs,
            "senders_receivers": ["[3,5]","[10,5]"],
            "packetnum": ["1"],
            "lossrate": ["0.20"],
            "analyse_result_exp_num": 5,
            "analyse_result_exp_num_persender": 5
        }
]

not_cmd_key = "analyse_result_exp_num"
output_directory_pattern = re.compile(r'(?:.*/)?output_analyse_everytimes_(\d+)_(\d+)_(\d+)$')
analyse_result_directory = "analyse_result"


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


@pytest.mark.parametrize("server_args", server_args_list)
def test_distributed_simulation(server_args):
    print(f"test_distributed_simulation:{server_args}")
    """Test the server and client interaction for different server configurations."""
    client_args = {
        "host": "192.168.186.14",
    }
    output_dir = f"tracelog-scenario{server_args['nums_scenario']}-{server_args['nodenum']}"

    if os.path.exists(output_dir) and os.path.isdir(output_dir):
        shutil.rmtree(output_dir)
    
    assert not os.path.exists(output_dir), "Analyse result directory has not been deleted."

    # Run distributed simulation
    server_out, client_out = run_distributed_simulation(server_args, client_args)

    # Verify that the output directory exists
    assert os.path.exists(output_dir), f"Output directory {output_dir} does not exist."

    analyse_result = os.path.join(output_dir, analyse_result_directory)
    assert os.path.exists(analyse_result), "Analyse result directory does not exist."

    # Verify the number of output files matches the expected runs
    files = os.listdir(analyse_result)
    loop = len(server_args["senders_receivers"])
    analyse_result_exp_num = server_args["analyse_result_exp_num"] + server_args["analyse_result_exp_num_persender"]* loop
    assert len(files) == analyse_result_exp_num, "Number of output files does not match expected runs."

    # Additional checks for server and client output
    assert "Simulation complete" in server_out, "Server output did not indicate successful completion."
    assert "Client finished" in client_out, "Client output did not indicate successful completion."
