import subprocess
import re

def grep_and_count(pattern, filename):
    """使用 grep 命令统计符合模式的行数"""
    command = f"grep -P '{pattern}' {filename}"
    result = subprocess.run(command, shell=True, text=True, capture_output=True)
    
    if result.returncode == 0:
        lines = result.stdout.strip().split('\n')
        count = len(lines)
        print(f"匹配模式 '{pattern}' 的行数: {count}")
        return lines
    else:
        print(f"执行 grep 命令时出错: {result.stderr}")
        return []

def calculate_time_difference(lines):
    """计算最后一行和第一行中 time_stamp 的差值"""
    if len(lines) < 2:
        print("匹配到的行数不足，无法计算时间差。")
        return
    
    # 提取第一行和最后一行的 time_stamp
    first_time_stamp = re.search(r'time_stamp=\s*(\d+)', lines[0])
    last_time_stamp = re.search(r'time_stamp=\s*(\d+)', lines[-1])
    
    if first_time_stamp and last_time_stamp:
        start_time = int(first_time_stamp.group(1))
        end_time = int(last_time_stamp.group(1))
        time_difference = end_time - start_time
        print(f"最后一行和第一行 time_stamp 的差值: {time_difference}")
    else:
        print("未能在行中找到 time_stamp。")

def main():
    filename = "trace_1.txt"  # 文件名

    # 统计第一种模式
    pattern1 = '(?=.*APPLICATION)(?=.*packet_type= 3)(?=.*action= 4)'
    lines1 = grep_and_count(pattern1, filename)

    # 统计第二种模式
    pattern2 = '(?=.*APPLICATION)(?=.*packet_type= 2 action= 2)'
    lines2 = grep_and_count(pattern2, filename)

    # 统计第三种模式并计算时间差
    pattern3 = '(?=.*APPLICATION)(?=.*packet_id= 300)'
    lines3 = grep_and_count(pattern3, filename)
    calculate_time_difference(lines3)

if __name__ == "__main__":
    main()