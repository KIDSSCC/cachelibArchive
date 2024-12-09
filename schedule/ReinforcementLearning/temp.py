from util import *
from datetime import datetime
from predication_model import *
from agent import *
import threading
import os
import logging
import re
import matplotlib.pyplot as plt
from scipy.optimize import curve_fit,fsolve
FILE_PATH = '/home/md/SHMCachelib/schedule/ReinforcementLearning/Offline_Sample/20241207_141709'
# Cache分配和命中率之间的关系
def exp_growth(x, A, B):
    return np.maximum(A * (1 - np.exp(-B * x)), np.float64(1.0))

# 带宽和延迟之间的关系
def exp_decay(x, A, B):
    return np.maximum(A * (np.exp(-B * x)), np.float64(0.0))
def setup_logger(file_path):
    # 创建日志记录器
    logger = logging.getLogger(file_path)
    logger.setLevel(logging.INFO)
    
    # 创建文件处理器并设置日志文件名
    file_handler = logging.FileHandler(file_path)
    file_handler.setLevel(logging.INFO)

    # 创建日志格式化器
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    file_handler.setFormatter(formatter)

    # 创建控制台处理器
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)
    console_handler.setFormatter(formatter)

    # 将文件处理器和控制台处理器添加到日志记录器
    logger.addHandler(file_handler)
    logger.addHandler(console_handler)
    
    return logger
def visualize(file_path):
    '''读取日志文件，解析配置和性能情况'''
    cache_pattern = r"Phase (\d+) sample (\d+) : Current cache allocation: (\[.*?\]) Current hitrate: (\[.*?\])"
    cpu_pattern = r"Phase (\d+) sample (\d+) : Current cpu allocation: (\[.*?\]) Current utilization: (\[.*?\])"
    bw_pattern = r"Phase (\d+) sample (\d+) : Current bandwidth allocation: (\[.*?\]) Current latency: (\[.*?\])"
    cache_allocations =[]
    hitrates = []
    cpu_allocations = []
    utilizations = []
    bw_allocations = []
    latencies = []
    analyze_file_path = file_path + '_analyzed.log'         # 记录根据采样结果
    file_path = file_path + '.log'
    '''
    cache_allocations : [[cache1……cache25],
                         [cache1……cache25],
                         ……
                         [cache1……cache25]] 共18 * 3个
    hitrates : [hitrate1……hitrate25,
                 hitrate1……hitrate25,
                 ……
                 hitrate1……hitrate25] 共18 * 3个
    '''
    with open(file_path, 'r') as file:
        for line in file:
            # 使用正则表达式查找匹配的内容
            match = re.search(cache_pattern, line)
            if match:
                # 提取 allocation 和 hitrate 信息
                phase = match.group(1)
                sample = match.group(2)
                allocation = match.group(3)
                hitrate = match.group(4)
                # 转换成数值列表并存储
                cache_allocations.append(eval(allocation))  # eval 将字符串转为列表
                hitrates.append(eval(hitrate))
            match = re.search(cpu_pattern, line)
            if match:
                # 提取 allocation 和 hitrate 信息
                phase = match.group(1)
                sample = match.group(2)
                allocation = match.group(3)
                utilization = match.group(4)
                # 转换成数值列表并存储
                cpu_allocations.append(eval(allocation))  # eval 将字符串转为列表
                utilizations.append(eval(utilization))
            match = re.search(bw_pattern, line)
            if match:
                # 提取 allocation 和 hitrate 信息
                phase = match.group(1)
                sample = match.group(2)
                allocation = match.group(3)
                latency = match.group(4)
                # 转换成数值列表并存储
                bw_allocations.append(eval(allocation))  # eval 将字符串转为列表
                latencies.append(eval(latency))

    for i in range(len(cache_allocations)):
        print(cache_allocations[i])

    # # 1.cache和hitrate之间的关系
    # for phase in range(3):
    #     for task_index in range(len(cache_allocations[0])):
    #         plt.clf()
    #         figure_path = './figures/phase' + str(phase + 1) + '/' + str(task_index + 1) + 'cache_hitrate.png'
    #         task_allocation = []
    #         task_hitrate = []
    #         for sample_index in range(12):  # 12个点为一个阶段
    #             task_allocation.append(cache_allocations[phase * 12 + sample_index][task_index])    # [cache1……cache25]
    #             task_hitrate.append(hitrates[phase * 12 + sample_index][task_index])                # [hitrate1……hitrate25]
    #         x_data = np.array(task_allocation)      # 转换为 NumPy 数组，方便拟合
    #         y_data = np.array(task_hitrate)
    #         # popt, _ = curve_fit(exp_growth, x_data, y_data, p0=[1, 0.1], maxfev=100)  # 初始参数猜测 [1, 0.1]
    #         # A, B = popt
    #         # x_fit = np.linspace(0, 32, 100)  # 生成拟合曲线的x值
    #         # y_fit = exp_growth(x_fit, A, B)
    #         # plt.plot(x_fit, y_fit, label=f'Fitted curve: A={A:.2f}, B={B:.2f}', color='red') 
    #         plt.scatter(task_allocation, task_hitrate, c='blue', marker='o')     
    #         plt.title(f'Phase {phase + 1} - Task {task_index + 1}')
    #         plt.xlabel('Cache Allocation')
    #         plt.ylabel('Hitrate')
    #         plt.savefig(figure_path)
    #         info_str = f'Task {task_index + 1} in Phase {phase + 1} Cache-hitrate Simulate Feature : A={A},B={B}\n'
    #         with open(analyze_file_path, 'a') as file:
    #             file.write(info_str)
    
    # # 2.bandwidth和latency之间的关系
    # for phase in range(3):
    #     for task_index in range(len(bw_allocations[0])):
    #         plt.clf()
    #         figure_path = './figures/phase' + str(phase + 1) + '/' + str(task_index + 1) + 'bandwidth_latency.png'
    #         task_allocation = []
    #         task_latency = []
    #         for sample_index in range(12):  # 12个点为一个阶段
    #             task_allocation.append(bw_allocations[phase * 12 + sample_index][task_index])    # [cache1……cache25]
    #             task_latency.append(latencies[phase * 12 + sample_index][task_index])                # [hitrate1……hitrate25]
    #         x_data = np.array(task_allocation)      # 转换为 NumPy 数组，方便拟合
    #         y_data = np.array(task_latency)
    #         popt, _ = curve_fit(exp_decay, x_data, y_data, p0=[1, 0.1], maxfev=100)  # 初始参数猜测 [1, 0.1]
    #         A, B = popt
    #         x_fit = np.linspace(0, 32, 100)  # 生成拟合曲线的x值
    #         y_fit = exp_decay(x_fit, A, B)
    #         plt.scatter(task_allocation, task_latency, c='blue', marker='o')     
    #         plt.plot(x_fit, y_fit, label=f'Fitted curve: A={A:.2f}, B={B:.2f}', color='red') 
    #         plt.title(f'Phase {phase + 1} - Task {task_index + 1}')
    #         plt.xlabel('Bandwidth Allocation')
    #         plt.ylabel('Latency')
    #         plt.savefig(figure_path)
    #         info_str = f'Task {task_index + 1} in Phase {phase + 1} Bandwidth-latency Simulate Feature : A={A},B={B}\n'
    #         with open(analyze_file_path, 'a') as file:
    #             file.write(info_str)
    
    # 3. cpu和utilization的关系 ->cpu核心过少，无需拟合曲线
    for phase in range(3):
        for task_index in range(len(cpu_allocations[0])):
            plt.clf()
            figure_path = './figures/phase' + str(phase + 1) + '/' + str(task_index + 1) + 'cpu_utilizations.png'
            task_allocation = []
            task_utilization = []
            for sample_index in range(12):
                task_allocation.append(cpu_allocations[phase * 12 + sample_index][task_index])
                task_utilization.append(utilizations[phase * 12 + sample_index][task_index])
            plt.scatter(task_allocation, task_utilization, c='blue', marker='o')    
            plt.title(f'Phase {phase + 1} - Task {task_index + 1}')
            plt.xlabel('CPU Allocation')
            plt.ylabel('utilization')
            plt.savefig(figure_path)
            info_str = f'Task {task_index + 1} in Phase {phase + 1} CPU allocation:{task_allocation}, utilization:{task_utilization}\n'
            with open(analyze_file_path, 'a') as file:
                file.write(info_str)
    
if __name__ == '__main__':
    visualize(FILE_PATH)