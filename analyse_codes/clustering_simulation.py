import argparse
import os
import json
import subprocess
from time import time
from tqdm import tqdm
import random
import math
import queue
import openpyxl

maxwh = 5000
maxd = 2500
stdout_path = 'stdout.txt'

def getdis(px, py):
    return math.sqrt((px[0] - py[0]) ** 2 + (px[1] - py[1]) ** 2 + (px[2] - py[2]) ** 2)

def pos(n, m, maxw, maxd, shortdis, longdis ,fi = 0):
    # return [round(random.uniform(0, maxwh), 3), round(random.uniform(0, maxwh), 3), round(random.uniform(0, maxd), 3)]
    random.seed(time())
    # return [[random.uniform(0, maxw), random.uniform(0, maxw), random.uniform(0, maxd)] for _ in range(n)]
    pos = []
    p = [random.uniform(0, maxw), random.uniform(0, maxw), random.uniform(0, maxd)]
    if fi == 1:
        p[2] = 0
    pos.append(p)
    for i in range(m-1):
        flag = False
        while flag == False:
            p = [random.uniform(0, maxw), random.uniform(0, maxw), random.uniform(0, maxd)]
            for pt in pos:
                if getdis(pt, p) <= longdis:
                    flag = True
                    break
        pos.append(p)
    
    for i in range(n-m):
        flag = False
        while flag == False:
            p = [random.uniform(0, maxw), random.uniform(0, maxw), random.uniform(0, maxd)]
            for pt in pos:
                if getdis(pt, p) <= shortdis:
                    flag = True
                    break
        pos.append(p)
    return pos

def GetResult(path):
    with open(path, "r") as f:
        str = f.read()
    str = str.split(';')

    result = {}
    for s in str:
        key = s.split(' = ')[0].strip()
        val = s.split(' = ')[1].strip()
        if key in {"Delivery Ratio", "Pkt Collision Rate", "hello Pkt Collision Rate", "LS Pkt Collision Rate", "Disconnect Drop Ratio"}:
            val = float(val.strip('%'))
        else:
            val = float(val)
        result[key] = val
    return result

def Simulation(model, long_rate, nodenum, speed, hello, bps, movable_rate, o, g, d, args):
    # print(f'running model {model}  nodenum = {nodenum}   long_rate = {long_rate}  speed = {speed}    hello = {hello}    bps = {bps}')
    # print(o, g, d)
    configTemplate = f'./src/aqua-sim-ng/examples/settings/{model}/settings_{model}_normal.json'
    # print(f'Reading {configTemplate}.')
    with open(configTemplate, "r") as f:
        data = json.load(f)

    simulation_results = []
    trace_dir = f'./tracelog-{model}/{nodenum}_{(int)(float(hello) * nodenum)}_{speed}_{bps}_{long_rate}'
    # if model == 'LEACH':
    #     trace_dir = f'./tracelog-{model}/{nodenum}_{int(hello)}_{speed}_{bps}_{long_rate}'
    if model == 'PAUV':
        trace_dir = f'./tracelog-{model}/{nodenum}_0_0_{bps}_{long_rate}'
    if nodenum <= 30:
        maxwh = 3000
    else:
        maxwh = 5000

    
                            
    for runNum in tqdm(range(args.runs), desc='runNum'):
        configName = f"./src/aqua-sim-ng/examples/settings/{model}/settings_{model}_{runNum+1}.json"
        outputs = {}
        for key, val in data.items():
            if key == "node_positions":
                positions = pos(nodenum, int(nodenum * long_rate / 100.), maxwh, maxd)
                outputs['node_positions'] = positions
            elif key == 'node_num':
                outputs['node_num'] = nodenum
            elif key == 'mobility_models_batch':
                v = val
                v[0]['params']['speed'] = [1, speed]
                v[0]['params']['X'] = v[0]['params']['Y'] = [0, maxwh]
                v[0]['params']['Z'] = [0, maxd]
                outputs['mobility_models_batch'] = v
            elif key == 'simulation_time':
                outputs['simulation_time'] = 100 + 20 * nodenum * (nodenum - 1)
            elif key == 'hello_interval' and model == 'clustering':
                outputs['hello_interval'] = (float)(hello)
            elif key == 'hello_interval' and model == 'LEACH':
                outputs['hello_interval'] = (float)(hello)
            elif key == 'omega':
                outputs['omega'] = o#[args.omega, round(1 - 2 * args.omega, 3), args.omega]
            elif key == 'gamma':
                outputs['gamma'] = g#[args.gamma, 1 - args.gamma]
            elif key == 'delta':
                outputs['delta'] = d#[args.delta, 1 - args.delta]
            elif key == 'mu':
                outputs['mu'] = args.mu
            elif key == 'lambda':
                outputs['lambda'] = args.lamb
            # elif key == 'prob' and model == 'LEACH':
            #     outputs['prob'] = prob
            elif key == 'max_speed':
                outputs['max_speed'] = speed
            elif key == 'longNodes_rate':
                outputs['longNodes_rate'] = long_rate / 100.
            elif key == 'forming_time' and model == 'clustering':
                outputs['forming_time'] = 20
            elif key == 'routing_time' and model == 'clustering':
                outputs['routing_time'] = 50
            elif key == 'initialNetworkingCompletion' and model=='clustering':
                outputs['initialNetworkingCompletion'] = 100
            elif key == 'data_rate_bps':
                outputs['data_rate_bps'] = bps
            elif key == 'movable_rate':
                outputs['movable_rate'] = movable_rate
            else:
                 outputs[key] = val
        if model == 'PAUV':
            outputs['MinX'] = outputs['MinY'] = outputs['MinZ'] = 0
            outputs['MaxX'] = outputs['MaxY'] = maxwh
            outputs['MaxZ'] = maxd
        outputs['senders'] = [i+1 for i in range(nodenum)]
        outputs['receivers'] = [i+1 for i in range(nodenum)]
        with open(configName, "w") as f:
            json.dump(outputs, f, indent=4, ensure_ascii=False)

        random.seed(time())                   
        txList = ''
        txset = set()
        txnum = int(nodenum * 0.5)
        for _ in range(txnum):
            src = random.randint(0, nodenum-1)
            dst = random.randint(0, nodenum-1)
            while src == dst or (src * 1000 + dst) in txset:
                src = random.randint(0, nodenum-1)
                dst = random.randint(0, nodenum-1)
            txset.add(src * 1000 + dst)
            txList = txList + str(src) + ',' + str(dst)
            if _ < txnum - 1:
                txList = txList + ';'
        
        senders = ''
        for i in range(nodenum):
            senders = senders + str(i)
        receivers = senders
                                
        command = [
            "./ns3", "run",
            f'dolphin_clustering_normal'
            f' --setting={configName}'
            f' --runNumber={runNum+1}'
            f' --lossrate=0.0'
            f' --packetnum=1000'
            # f' --senders=1'
            # f' --receivers=2'
            f' --printLoggingInfo=False'
            f' --srcDstTxList={txList}'
        ]
                                
        with open(stdout_path, "w") as f:
            result = subprocess.run(command, stdout=f, stderr=f)
                                
        if result.returncode != 0:
            print(f"❌ 运行失败，错误码: {result.returncode}")
                                
                                
        traceFile = f'trace_{runNum+1}_1000_0_{nodenum}_1_1_dolphin_scenario.txt'
        traceFile = os.path.join(trace_dir, traceFile)
        simulation_results.append(GetResult(traceFile))
                            
    avg_result = {
        'Control Packets Count': 0,
        'Hello Packets Count': 0,
        'LS Packets Count': 0,
        'Switch CH Count': 0,
        'Become CH Count': 0,
        'Become CM Count': 0,
        'Control Overhead(bytes/T)': 0,
        'Delivery Ratio': 0,
        'Data Packets Traffic(bytes)': 0,
        'Avg End2End Delay': 0,
        'Pkt Collision Count': 0,
        'hello Pkt Collision Count': 0,
        'LS Pkt Collision Count': 0,
    }

    for result in simulation_results:
        avg_result['Control Packets Count'] += result['Control Packets Count']
        avg_result['Hello Packets Count'] += result['Hello Packets Count']
        avg_result['LS Packets Count'] += result['LS Packets Count']
        avg_result['Switch CH Count'] += result['Switch CH Count']
        avg_result['Become CH Count'] += result['Become CH Count']
        avg_result['Become CM Count'] += result['Become CM Count']
        avg_result['Control Overhead(bytes/T)'] += result['Control Overhead(bytes/T)']
        avg_result['Delivery Ratio'] += result['Delivery Ratio']
        avg_result['Data Packets Traffic(bytes)'] += result['Data Packets Traffic(bytes)']
        avg_result['Avg End2End Delay'] += result['Avg End2End Delay']
        avg_result['Pkt Collision Count'] += result['Pkt Collision Count']
        avg_result['hello Pkt Collision Count'] += result['hello Pkt Collision Count']
        avg_result['LS Pkt Collision Count'] += result['LS Pkt Collision Count']

    for k in avg_result:
        avg_result[k] = round(avg_result[k] / args.runs, 3)
    print(avg_result)
    return avg_result

def main():
    random.seed(time())
    parser = argparse.ArgumentParser()


    # parser.add_argument("--nodenum", default=50, type=int)
    # parser.add_argument("--long_rate", default=50, type=double)
    # parser.add_argument("--max_speed", default=3, type=double)
    # parser.add_argument("--hello_interval", default=60, type=double)
    parser.add_argument("--nodenum", default="50", type=str)
    parser.add_argument("--long_rate", default="50", type=str)
    parser.add_argument("--max_speed", default="3", type=str)
    parser.add_argument("--hello_interval", default="2", type=str)
    parser.add_argument("--bps", default="8", type=str)
    parser.add_argument("--omega", default=0.4, type=float)
    parser.add_argument("--gamma", default=0.5, type=float)
    parser.add_argument("--delta", default=0.5, type=float)
    parser.add_argument("--mu", default=0.5, type=float)
    parser.add_argument("--lamb", default=0.5, type=float)
    parser.add_argument("--runs", default=10, type=int)
    parser.add_argument("--models", default="clustering", type=str)

    args = parser.parse_args()
    models = args.models.split(';')
    nodenums = args.nodenum.split(';')
    longrates = args.long_rate.split(';')
    speeds = args.max_speed.split(';')
    hellos = args.hello_interval.split(';')
    bpss = args.bps.split(';')

    model_default = "clustering"
    # model_default = "LEACH"
    long_rate_default = 50
    nodenum_default = 100
    hello_interval_default = 2
    speed_default = 3
    bps_default = 8

    # for model in models:
    #     Simulation(model, long_rate_default, nodenum_default, speed_default, hello_interval_default, bps_default, args)

    # for long_rate in longrates:
    #     long_rate = int(long_rate)
    #     for nodenum in nodenums:
    #         nodenum = int(nodenum)
    #         Simulation(model_default, long_rate, nodenum, speed_default, hello_interval_default, bps_default, args)

    # for speed in speeds:
    #     speed = int(speed)
    #     for nodenum in nodenums:
    #         nodenum = int(nodenum)
    #         Simulation(model_default, long_rate_default, nodenum, speed, hello_interval_default, bps_default, args)

    # for hello in hellos:
    #     hello = int(hello)
    #     print(f'hello = {hello}')
    #     for nodenum in nodenums:
    #         nodenum = int(nodenum)
    #         Simulation(model_default, long_rate_default, nodenum, speed_default, hello, bps_default, [0.4, 0.2, 0.4], [0.5, 0.5], [0.5, 0.5], args)

    values = [['moverate',
               'nodenum',
        'Control Packets Count',
        'Hello Packets Count',
        'LS Packets Count',
        'Switch CH Count',
        'Become CH Count',
        'Become CM Count',
        'Control Overhead(bytes/T)',
        'Delivery Ratio',
        'Data Packets Traffic(bytes)',
        'Avg End2End Delay',
        'Pkt Collision Count',
        'hello Pkt Collision Count',
        'LS Pkt Collision Count']]
    for move in [0, 0.25, 0.5, 0.75, 1]:
        print(f'move ratio = {move}')
        for nodenum in nodenums:
            nodenum = int(nodenum)
            print(f'node num = {nodenum}')
            result = Simulation(args.models, long_rate_default, nodenum, speed_default, args.hello_interval, 8, move, [0.4, 0.2, 0.4], [0.5, 0.5], [0.5, 0.5], args)
            vals = [move, nodenum]
            for v in result.values():
                vals.append(v)
            values.append(vals)
        

    index = len(values)
    workbook = openpyxl.Workbook()
    sheet = workbook.active
    for i in range(0, index):
        for j in range(0, len(values[i])):
            sheet.cell(row=i+1, column=j+1, value=values[i][j])
    workbook.save(f'result_{args.models}.xlsx')


    # for nodenum in nodenums:
    #     nodenum = int(nodenum)
    #     Simulation(model_default, long_rate_default, nodenum, speed_default, hello_interval_default, bps_default, [0.4, 0.2, 0.4], [0.5, 0.5], [0.5, 0.5], args)
    
    # for p in [0.3, 0.4, 0.5]:
    #     Simulation(model_default, long_rate_default, nodenum_default, speed_default, hello_interval_default, bps_default, p, args)

    # for bps in bpss:
    #     bps = int(bps)
    #     for nodenum in nodenums:
    #         nodenum = int(nodenum)
    #         Simulation(model_default, long_rate_default, nodenum, speed_default, hello_interval_default, bps, args)
                            
    

if __name__ == "__main__":
    main()