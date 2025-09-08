import subprocess
import os
import sys
import time
import unittest
import re
import shutil

py_path = "src/aqua-sim-ng/analyse_codes/distributed_simulation.py"
distributed_simulation_py = "src/aqua-sim-ng/analyse_codes/distributed_simulation.py"

g_runs = 1

server_args_list = [
        {
            "nums_scenario": 1,
            "nodenum": 10,
            "runs": g_runs,
            "senders_receivers": ["[3,255]","[4,255]"],
            "packetnum": ["1", "2"],
            "lossrate": ["0.05", "0.10"],
            "analyse_result_exp_num": 9
        },
        {
            "nums_scenario": 2,
            "nodenum": 10,
            "runs": g_runs,
            "senders_receivers": ["[3,255]"],
            "packetnum": ["1", "2"],
            "lossrate": ["0.05", "0.10"],
            "analyse_result_exp_num": 9
        },
        {
            "nums_scenario": 4,
            "nodenum": 10,
            "runs": g_runs,
            "senders_receivers": ["[3,5]"],
            "packetnum": ["1"],
            "lossrate": ["0.05", "0.10"],
            "analyse_result_exp_num": 5
        },
        {
            "nums_scenario": 5,
            "nodenum": 38,
            "runs": g_runs,
            "senders_receivers": ["[3,255]"],
            "packetnum": ["1"],
            "lossrate": ["0.05", "0.10"],
            "analyse_result_exp_num": 3
        },
        {
            "nums_scenario": 4,
            "nodenum": 38,
            "runs": g_runs,
            "senders_receivers": ["[3,5]"],
            "packetnum": ["1", "2"],
            "lossrate": ["0.05", "0.10"],
            "analyse_result_exp_num": 5
        }
        ]

not_cmd_key = "analyse_result_exp_num" 

output_directory_pattern = re.compile(r'(?:.*/)?output_analyse_everytimes_(\d+)_(\d+)_(\d+)$')
analyse_result_directory = "analyse_result"

# Helper function to convert dictionary to command-line arguments
def dict_to_cmd_args(args_dict,exclude_key):
    """Convert a dictionary of arguments to a list of command-line arguments."""
    cmd_args = []
    for key, value in args_dict.items():
        if key == exclude_key or (isinstance(exclude_key, list) and key in exclude_key):
            continue  # Skip this key if it matches the exclude_key
        if isinstance(value, (list, tuple)):
            value = ','.join(map(str, value))
        if key.startswith("senders_receivers") or key.startswith("packetnum") or key.startswith("lossrate") :
            cmd_args.append(f"--{key}={value}")
        else:
            cmd_args.append(f"--{key}={value}")
    return cmd_args

# Start server and client
def run_distributed_simulation(server_args, client_args, buffer_size=4096):
    global py_path
    """Run the server and client for distributed simulation."""
    try:
        # Convert server and client args to command-line arguments
        server_cmd_args = dict_to_cmd_args(server_args,not_cmd_key)
        client_cmd_args = dict_to_cmd_args(client_args,None)
        senders_receivers = server_args['senders_receivers']
        print(f"senders_receivers:{senders_receivers}")
        senders_receivers_no_spaces = str(senders_receivers).replace(" ", "")   # 去除所有空格
        #senders_receivers_list = senders_receivers.split()
        #print(f"senders_receivers_list:{senders_receivers_list}")

        command = ['/usr/bin/python3', distributed_simulation_py, 'server']
        args = [
            '--nums_scenario=1',
            '--nodenum=10',
            '--runs=1',
            '--senders_receivers', '[3,255]', '[4,255]',
            '--packetnum', '1', '2', '3',
            '--lossrate', '0.05', '0.10'
        ]
        
        server_cmd = [
            f"--nums_scenario={server_args['nums_scenario']}",
            f"--nodenum={server_args['nodenum']}",
            f"--runs={server_args['runs']}"
        ]

        server_cmd.append('--packetnum')
        for item in server_args['packetnum']:
            server_cmd.append(item)

        server_cmd.append('--lossrate')
        for item in server_args['lossrate']:
            server_cmd.append(item)

        server_cmd.append('--senders_receivers')
        for item in senders_receivers:
            server_cmd.append(item)

                # 合并命令和参数
        full_command = command + server_cmd


        #[sys.executable, distributed_simulation_py, "server", *server_cmd_args]

        print("server_cmd:"," ".join(server_cmd))

        # Start the server
        server_process = subprocess.Popen(
            full_command,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        print("Server started.")

        # Allow the server to start properly before starting the client
        time.sleep(1)

        # Start the client
        client_cmd = [sys.executable, py_path, "client", *client_cmd_args]

        print("client_cmd:"," ".join(client_cmd))

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

# Unit test for distributed simulation
class TestDistributedSimulation(unittest.TestCase):
    def setUp(self):
        """Set up server and client configurations for the test."""
        self.server_args = server_args_list[0]
        self.client_args = {
            "host": "192.168.186.14",
        }
        self.output_dir = "tracelog-scenario1-10"  # Expected output directory

    def test_server_client_simulation(self):
        """Test the server and client interaction."""
        # Run distributed simulation
        server_out, client_out = run_distributed_simulation(self.server_args, self.client_args)

        # Verify that the output directory exists
        self.assertTrue(os.path.exists(self.output_dir), "Output directory does not exist.")
        
        analyse_result = os.path.join(self.output_dir, analyse_result_directory)
        self.assertTrue(os.path.exists(analyse_result), "Analyse result directory does not exist.")

        # Verify the number of output files matches the expected runs
        files = os.listdir(analyse_result)
        analyse_result_exp_num = self.server_args["analyse_result_exp_num"]*len(self.server_args["senders_receivers"])
        self.assertEqual(len(files), analyse_result_exp_num, "Number of output files does not match expected runs.")

        # Additional checks for server and client output
        self.assertIn("Simulation complete", server_out, "Server output did not indicate successful completion.")
        self.assertIn("Client finished", client_out, "Client output did not indicate successful completion.")

if __name__ == "__main__":
    # Run unit tests
    unittest.main()
