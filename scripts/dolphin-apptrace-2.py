import argparse
import subprocess
import re

leader_id = 3

def grep_and_count(pattern, filename):
    """使用 grep 命令统计符合模式的行数"""
    command = f"grep -P '{pattern}' {filename}"
    result = subprocess.run(command, shell=True, text=True, capture_output=True)
    
    if result.returncode == 0:
        lines = result.stdout.strip().split('\n')
        count = len(lines)
        print(f"'{pattern}' 的行数 FirstBTN:{count}")
        return lines
    else:
        print(f"执行 grep 命令时出错: {result.stderr}")
        return []

def count_lines_with_grep(pattern, filename):
    """使用 grep 命令统计匹配模式的行数"""
    command = ["grep", "-P", pattern, filename]
    result = subprocess.run(command, stdout=subprocess.PIPE, text=True)
    return len(result.stdout.strip().split('\n')) if result.stdout else 0

def get_lines_with_grep(pattern, filename):
    """使用 grep 命令统计匹配模式的行"""
    command = ["grep", "-P", pattern, filename]
    result = subprocess.run(command, stdout=subprocess.PIPE, text=True)
    
    # 返回匹配的行，确保是列表类型
    return result.stdout.strip().split('\n') if result.stdout else []

def get_packet_id(filename):
    """获取第一行中 packet_id 的值"""
    command = ["grep", "-P", '(?=.*APPLICATION)(?=.*nxt_id= 255 dst_id= 255)', filename]
    result = subprocess.run(command, stdout=subprocess.PIPE, text=True)
    
    # 提取 packet_id 的数值
    match = re.search(r'packet_id=\s*(\d+)', result.stdout)
    return match.group(1) if match else None

def calculate_time_difference(lines,leader_id):
    required_node_ids = set(range(1, 11)) - {leader_id}
    #print(f"required_node_ids:{required_node_ids}");
    collected_node_ids = set()  # 存储已收集的 node_id
    
    start_time = None  # 用于记录第一次匹配到的时间戳
    first_time_match = re.search(r'time_stamp=\s*([\d.]+)', lines[0])
    if not first_time_match:
        print("第一行未找到 time_stamp，无法计算时间差。")
        return
    first_start_time = float(first_time_match.group(1))
    collected_8 = 0
    collected_9 = 0
    
    # 遍历每一行，查找 node_id 和 time_stamp
    for line in lines:
        # 提取 node_id
        node_match = re.search(r'node_id=\s*(\d+)', line)
        
        # 如果找到了 node_id
        if node_match:
            node_id = int(node_match.group(1))
            
            # 如果这个 node_id 是我们需要的，并且还没收集到
            if node_id in required_node_ids and node_id not in collected_node_ids:
                
                # 提取当前行的 time_stamp
                time_match = re.search(r'time_stamp=\s*([\d.]+)', line)
                # 提取当前的 time_stamp 作为结束时间
                end_time = float(time_match.group(1))

                if end_time-first_start_time>300:
                    break
                collected_node_ids.add(node_id)  # 添加到已收集的 node_id 集合
                #print(f"collected_node_ids:{collected_node_ids}");
                collected_ratio = len(collected_node_ids) / len(required_node_ids)
                collected_ratio80 = len(required_node_ids)*0.8/10
                collected_ratio90 = len(required_node_ids)*0.9/10
                #送达率80%
                if collected_ratio >= collected_ratio80 and collected_8==0:
                    collected_8 = 1
                    time_difference = end_time - first_start_time
                    print(f"全网广播完成时间(s) BC80:{time_difference:.2f}")
                elif collected_ratio >= collected_ratio90 and collected_9==0:
                    collected_9 = 1
                    time_difference = end_time - first_start_time
                    print(f"全网广播完成时间(s) BC90:{time_difference:.2f}") 
                elif collected_ratio == 1.0:
                    time_difference = end_time - first_start_time
                    print(f"全网广播完成时间(s) BC:{time_difference:.2f}")
                    print("一次广播送达率 BDR:100.00%")
                    return  # 退出函数，不需要继续处理
    # 如果遍历完还没收齐所有节点
    bdr = len(collected_node_ids) / len(required_node_ids)
    if collected_8==0:
        print(f"全网广播完成时间(s) BC80:0")
    if collected_9==0:
        print(f"全网广播完成时间(s) BC90:0")
    print("全网广播完成时间(s) BC:0")
    print(f"一次广播送达率 BDR:{bdr:.2%}")

def extract_src_ids(lines):
    """从匹配的行中提取 src_id 的值"""
    src_ids = []
    for line in lines:
        match = re.search(r'src_id=\s*(\d+)', line)
        if match:
            src_ids.append(int(match.group(1)))
    return src_ids

def extract_node_ids(lines):
    """从匹配的行中提取 node_id 的值"""
    node_ids = []
    for line in lines:
        match = re.search(r'node_id=\s*(\d+)', line)
        if match:
            node_ids.append(int(match.group(1)))
    return node_ids

def check_coverage(src_ids,leader_id):
    print(f"{leader_id}")
    # 生成从 1 到 10 的数字，并排除 leader_id
    required_ids = {num for num in range(1, 11) if num != leader_id}
    
    src_ids_set = set(src_ids)
    # 找出未覆盖的数值
    uncovered_ids = required_ids - src_ids_set
    print(f"uncovered_ids:{uncovered_ids}")
    # 如果 uncovered_ids 为空，说明全部覆盖，返回 True 和空集合；否则返回 False 和未覆盖的值
    return len(uncovered_ids) == 0, uncovered_ids

def main():

    parser = argparse.ArgumentParser(description="Run App trace.")

    # Add an argument to specify the number of runs
    parser.add_argument("--filepath", type=str, default="trace.txt", help="trace file name")
    parser.add_argument("--leaderid", type=int, default=3, help="leader id")

    # Parse the command-line arguments
    args = parser.parse_args()

    # Get file path
    filename = args.filepath
    print(f"trace file: {filename}")

    # Get leader id
    leader_id = args.leaderid
    print(f"leader_id: {leader_id}")

    # 1. 统计第一种模式的行数
    pattern1 = '(?=.*APPLICATION)(?=.*packet_type= 3)(?=.*action= 4)'
    count1 = count_lines_with_grep(pattern1, filename)
    print(f"APP发送广播的次数 BN:{count1}")

    # 2. 统计第二种模式的行数
    pattern2 = '(?=.*APPLICATION)(?=.*packet_type= 2 action= 2)'
    count2 = count_lines_with_grep(pattern2, filename)
    print(f"领航节点收到确认的次数 ACKN:{count2}")
    lines2 = get_lines_with_grep(pattern2, filename)
    # 提取 src_id 的数值
    src_ids = extract_src_ids(lines2)
    # 输出所有 src_id
    print(f"提取到的 src_id:{src_ids}")
    is_covered, missing_ids = check_coverage(src_ids, leader_id)
    # 检查是否涵盖 1 到 10
    if is_covered:
        print("领航节点收到 1 到 10 的确认。ALLACK:1")
    else:
        print(f"领航节点未收到 1 到 10 的确认。ALLACK:0({missing_ids})")
        
    # 3. 获取 packet_id
    packet_id = get_packet_id(filename)
    if packet_id:
        print(f"找到的 packet_id: {packet_id}")

        pattern3 = f'(?=.*APPLICATION)(?=.*packet_id= {packet_id})'

        lines3 = grep_and_count(pattern3, filename)
        node_ids = extract_node_ids(lines3)
        #print(f"{lines3}")
        # 4. 计算时间戳的差值
        calculate_time_difference(lines3,leader_id)

    else:
        print("未找到符合条件的 packet_id")

if __name__ == "__main__":
    main()