# 离线采样各个工作负载的特征
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

WARMTIME = 300
RUNTIME = 600           # 注意和backend/src/main.cpp保持一致


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

def perturb_cpu_cores(curr_cpu_allocation, curr_utilization):
    '''
    curr_cpu_allocation : list as [2, 1, 1, 2, 1, 1, 1, 1, 1, 1]
    curr_utilization : list as [0.05, 0.299, 0.1, 0.1, 0.898, 0.799, 0.3, 0.1, 0.39899999999999997, 0.2]
    每次把当前cpu利用率最低的核心都分配给利用率最高的
    '''
    total_cpu_cores = sum(curr_cpu_allocation)
    cpu_util_arr = np.array(curr_utilization)
    indices = np.argsort(cpu_util_arr)[:len(curr_cpu_allocation) // 2]    # 获取利用率最低的前1 / 2进程的索引
    cpu_free_num = 0
    perturbed_cpu_allocation = copy.deepcopy(curr_cpu_allocation)
    for i in indices:
        if curr_cpu_allocation[i] > 1:      # 如果当前该进程的CPU数量大于1，则将其减1
            perturbed_cpu_allocation[i] -= 1
            cpu_free_num += 1
    indices = np.argsort(cpu_util_arr)[-len(curr_cpu_allocation) // 2:]   # 获取利用率最高的前1 / 2进程的索引
    for i in indices:
        if cpu_free_num <= 0:
            break
        perturbed_cpu_allocation[i] += 1
        cpu_free_num -= 1
    assert sum(perturbed_cpu_allocation) == total_cpu_cores, '扰动前后总核心数不一致！'
    return perturbed_cpu_allocation

def perturb_cache_allocation(curr_cache_allocation, curr_hitrate):
    '''对缓存分配进行扰动
    0.05 : 认为命中率小于0.05的是sequential
    0.8 : 分配已经趋于饱和
    '''
    # 1. 对于当前命中率< 0.1的缓存，减少2个单位，但最低为2 
    # 对于命中率>0.8的缓存，减少1个单位, > 0.9的减少2个单位
    perturbed_cache_allocation = copy.deepcopy(curr_cache_allocation)
    total_cache = sum(curr_cache_allocation)
    cache_free_num = 0
    changing_flag = False       # 是否发生扰动
    for i in range(len(curr_cache_allocation)):
        if curr_hitrate[i] < 0.05 and curr_cache_allocation[i] > 2:
            changing_flag = True
            perturbed_cache_allocation[i] -= 2
            cache_free_num += 2
        if curr_hitrate[i] > 0.8:
            changing_flag = True
            perturbed_cache_allocation[i] -= 1
            cache_free_num += 1
            if curr_hitrate[i] > 0.9:   # 相当于>0.9的减少两个
                perturbed_cache_allocation[i] -= 1
                cache_free_num += 1
    # 2. 多余的缓存按照命中率递增的顺序依次轮回地分配给其他缓存
    indices = np.argsort(np.array(curr_hitrate))
    while cache_free_num > 0:
        for index in indices:
            if cache_free_num == 0:
                break
            if curr_hitrate[index] >= 0.05 and curr_hitrate[index] <= 0.8:
                perturbed_cache_allocation[index] += 1
                cache_free_num -= 1
    if changing_flag == True:
        assert sum(perturbed_cache_allocation) == total_cache, "扰动前后cache总量不一致"
        assert all(x > 0 for x in perturbed_cache_allocation), "Some elements in perturbed_cache_allocation are <= 0!"
        return perturbed_cache_allocation
    # 3.如果整个过程中没有任何缓存被扰动，随机选取一半的缓存 - 2，分配给其他的缓存
    random_sub_index = random.sample(range(len(curr_hitrate)), int(len(curr_hitrate)/2))
    random_sub_num = 0
    for index in random_sub_index:
        if curr_cache_allocation[index] >= 4:
            perturbed_cache_allocation[index] = curr_cache_allocation[index] - 2
            random_sub_num += 2
    remaining_cache_indexes = [i for i in range(len(curr_cache_allocation)) if i not in random_sub_index]  # 剩余缓存的索引
    for index in remaining_cache_indexes:
        if random_sub_num == 0:
            break
        perturbed_cache_allocation[index] = curr_cache_allocation[index] + 2
        random_sub_num -= 2
    assert sum(perturbed_cache_allocation) == total_cache, "扰动前后cache总量不一致"
    assert all(x > 0 for x in perturbed_cache_allocation), "Some elements in perturbed_cache_allocation are <= 0!"
    return perturbed_cache_allocation
    
def perturb_bw_allocation(curr_bw_allocation, curr_latency):
    '''对带宽分配进行扰动'''
    total_bw = sum(curr_bw_allocation)
    perturbed_bw_allocation = copy.deepcopy(curr_bw_allocation)
    # 1. 对于尾延迟最短的前一半的任务，减少其当前分配量的1/3，同时保证其不小于1
    indices = np.argsort(np.array(curr_latency))[:len(curr_bw_allocation) // 2]  # 最小延迟的前半部分
    sub_bw_num = 0  # 用于记录减少的带宽数
    # 修改前半部分的带宽
    for index in indices:
        perturbed_bw_allocation[index] = max(int(perturbed_bw_allocation[index] / 3 * 2), 1)  # 保证不小于1
        sub_bw_num += curr_bw_allocation[index] - perturbed_bw_allocation[index]  # 记录减少的带宽
    # 2. 剩下的带宽按照尾延迟从大到小均分
    indices = np.argsort(np.array(curr_latency))[-len(curr_bw_allocation) // 2:]  # 延迟最大的一半
    while sub_bw_num > 0:
        for index in indices:
            perturbed_bw_allocation[index] += 1  # 增加带宽
            sub_bw_num -= 1  # 减少剩余需要分配的带宽数
            if sub_bw_num <= 0:
                break
    # 断言检查，确保扰动前后总带宽数一致
    assert sum(perturbed_bw_allocation) == total_bw, '扰动前后总带宽数不一致！'
    assert all(x > 0 for x in perturbed_bw_allocation), "Some elements in perturbed_bw_allocation are <= 0!"
    return perturbed_bw_allocation

def offline_sample(file_path):
    """离线采样25个任务在不同资源分配下的情况"""
    logger = setup_logger(file_path + '.log')  # 使用 logging 记录日志
    logger.info("========== Begin Waiting For Warming Up ==========")
    time.sleep(WARMTIME)
    cm = ProtoSystemManagement()
    curr_config = cm.receive_config()   # 获取当前任务及cache分配信息
    all_apps = curr_config.task_id       # 所有任务的名称
    num_resources = [int(np.sum(x)) for x in curr_config.resource_allocation]       # 获取当前资源分配信息
    initial_cache_allocation = curr_config.resource_allocation[0]       # 初始分配量
    initial_cpu_allocation = curr_config.resource_allocation[1]
    initial_bandwidth_allocation = curr_config.resource_allocation[2]
    for i in range(3):
        start_time = time.time()                    # 每个阶段的开始时间
        logger.info("========== Main thread in Phase %d ==========", i + 1)
        for j in range(12):                         # 采样12个点的信息，最后一个点用于发送原始配置
            time.sleep(30)                          # 每个采样点之间间隔30s使得配置生效
            # 获取当前配置信息
            curr_config = cm.receive_config()
            curr_cache_allocation = [int(x) for x in curr_config.resource_allocation[0]]
            curr_hitrate = [float(x) for x in curr_config.hitrate]
            logger.info(" Phase %d sample %d : Current cache allocation: %s Current hitrate: %s", i + 1, j + 1, curr_cache_allocation, curr_hitrate)
            curr_cpu_allocation = [int(x) for x in curr_config.resource_allocation[1]]
            curr_cpu_utilization = [float(x) for x in curr_config.cpu_utilization]
            logger.info(" Phase %d sample %d : Current cpu allocation: %s Current utilization: %s",  i + 1, j + 1, curr_cpu_allocation, curr_cpu_utilization)
            curr_bandwidth_allocation = [int(x) for x in curr_config.resource_allocation[2]]
            curr_latency = [float(x) for x in curr_config.performance]
            logger.info(" Phase %d sample %d : Current bandwidth allocation: %s Current latency: %s",  i + 1, j + 1, curr_bandwidth_allocation, curr_latency)
            # 做扰动
            perturbed_cpu_allocation = perturb_cpu_cores(curr_cpu_allocation, curr_cpu_utilization)
            perturbed_cache_allocation = perturb_cache_allocation(curr_cache_allocation, curr_hitrate)
            perturbed_bw_allocation = perturb_bw_allocation(curr_bandwidth_allocation, curr_latency)
            new_config = []
            new_config.append(curr_config.task_id)
            new_config.append(perturbed_cache_allocation)
            new_config.append(perturbed_cpu_allocation)
            new_config.append(perturbed_bw_allocation)
            cm.send_config(new_config)
        new_config = []                             # 发送原始配置
        new_config.append(curr_config.task_id)
        new_config.append(initial_cache_allocation)
        new_config.append(initial_cpu_allocation)
        new_config.append(initial_bandwidth_allocation)
        cm.send_config(new_config)
        end_time = time.time()                  # 每个阶段的结束时间
        time.sleep(RUNTIME - (end_time - start_time))       # 阶段时间对齐


def exp_growth(x, A, B):
    return np.maximum(A * (1 - np.exp(-B * x)), np.float64(1.0))
    # A, B = params
    # eq1 = A * (1 - np.exp(-B * x)) - y
    # eq2 = A * (1 - np.exp(-B * x)) - y
    # return [eq1, eq2]
# Cache分配和命中率之间的关系
def linear_model(x, k):
    return np.minimum(k * x, np.float64(1.0))
# 带宽和延迟之间的关系
def exp_decay_model(x, A, B):
    return np.maximum(A * (np.exp(-B * x)), np.float64(0.0))

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
    cache_analyfile = file_path + '_cache_analyzed.log'
    cpu_analyze_file = file_path + '_cpu_analyzed.log'
    bw_analyze_file = file_path + '_bw_analyzed.log'
    
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
    with open(file_path+'.log', 'r') as file:
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
    print(len(cache_allocations))
    print(len(hitrates))
    # for i in range(len(cache_allocations)):
    #     print(cache_allocations[i], hitrates[i])

    # # 1.cache和hitrate之间的关系
    # for task_index in range(len(cache_allocations[0])):
    #     for phase in range(3):
    #         plt.clf()
    #         figure_path = './figures/phase' + str(phase + 1) + '/' + str(task_index + 1) + 'cache_hitrate.png'
    #         task_allocation = []
    #         task_hitrate = []
    #         for sample_index in range(12):  # 12个点为一个阶段
    #             task_allocation.append(cache_allocations[phase * 12 + sample_index][task_index])    # [cache1……cache25]
    #             task_hitrate.append(hitrates[phase * 12 + sample_index][task_index])                # [hitrate1……hitrate25]
    #         print('task',task_index + 1,'allocation :', task_allocation)
    #         x_data = np.array(task_allocation)      # 转换为 NumPy 数组，方便拟合
    #         y_data = np.array(task_hitrate)
    #         # popt, _ = fsolve(exp_growth, [1, 0.1],args=(x_data, y_data), maxfev=100)  # 初始参数猜测 [1, 0.1]
    #         initial_guess = np.mean(y_data) / np.mean(x_data)
    #         popt, _ = curve_fit(linear_model, x_data, y_data, p0=[initial_guess])
    #         # A, B = popt
    #         k = popt[0]
    #         x_fit = np.linspace(0, 50, 100)  # 生成拟合曲线的x值
    #         y_fit = linear_model(x_fit, k)
    #         plt.plot(x_fit, y_fit, label=f'Fitted curve', color='red') 
    #         plt.scatter(task_allocation, task_hitrate, c='blue', marker='o')     
    #         plt.title(f'Phase {phase + 1} - Task {task_index + 1}')
    #         plt.xlabel('Cache Allocation')
    #         plt.ylabel('Hitrate')
    #         plt.savefig(figure_path)
    #         info_str = f'Task {task_index + 1} in Phase {phase + 1} Cache-hitrate Simulate Feature : k = {k}\n'
    #         with open(cache_analyfile, 'a') as file:
    #             file.write(info_str)
    
    # # 2.bandwidth和latency之间的关系
    # for task_index in range(len(bw_allocations[0])):
    #     for phase in range(3):
    #         plt.clf()
    #         figure_path = './figures/phase' + str(phase + 1) + '/' + str(task_index + 1) + 'bandwidth_latency.png'
    #         task_allocation = []
    #         task_latency = []
    #         for sample_index in range(12):  # 12个点为一个阶段
    #             task_allocation.append(bw_allocations[phase * 12 + sample_index][task_index])    # [cache1……cache25]
    #             task_latency.append(latencies[phase * 12 + sample_index][task_index])                # [hitrate1……hitrate25]
    #         x_data = np.array(task_allocation)      # 转换为 NumPy 数组，方便拟合
    #         y_data = np.array(task_latency)
    #         popt, _ = curve_fit(exp_decay_model, x_data, y_data, p0=[ np.mean(y_data) / np.mean(x_data), 0.1], maxfev=100)  # 初始参数猜测 [1, 0.1]
    #         A, B = popt
    #         x_fit = np.linspace(0, 70, 100)  # 生成拟合曲线的x值
    #         y_fit = exp_decay_model(x_fit, A, B)
    #         plt.scatter(task_allocation, task_latency, c='blue', marker='o')     
    #         plt.plot(x_fit, y_fit, label=f'Fitted curve: A={A:.2f}, B={B:.2f}', color='red') 
    #         plt.title(f'Phase {phase + 1} - Task {task_index + 1}')
    #         plt.xlabel('Bandwidth Allocation')
    #         plt.ylabel('Latency')
    #         plt.savefig(figure_path)
    #         info_str = f'Task {task_index + 1} in Phase {phase + 1} Bandwidth-latency Simulate Feature : A={A},B={B}\n'
    #         with open(bw_analyze_file, 'a') as file:
    #             file.write(info_str)
    
    # 3. cpu和utilization的关系 ->cpu核心过少，无需拟合曲线
    for task_index in range(len(cpu_allocations[0])):
        for phase in range(3):
            plt.clf()
            figure_path = './figures/phase' + str(phase + 1) + '/task' + str(task_index + 1) + 'cpu_utilizations.png'
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
            with open(cpu_analyze_file, 'a') as file:
                file.write(info_str)
    

if __name__ == '__main__':
    # str_time = time.strftime("%Y%m%d_%H%M%S", time.localtime(time.time()))
    # offline_sample_path = '/home/md/SHMCachelib/schedule/ReinforcementLearning/Offline_Sample/' + str_time
    # offline_sample(offline_sample_path)

    offline_sample_path = '/home/md/SHMCachelib/schedule/ReinforcementLearning/Offline_Sample/20241207_152445'
    visualize(offline_sample_path)