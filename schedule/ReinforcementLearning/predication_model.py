import pickle
import random
import math
import sys
from itertools import zip_longest
from ScheduleFrame import *
from typing import List
import time
import matplotlib.pyplot as plt
from scipy.optimize import fsolve
import torch
import re
from agent import *

Cache_Analyzed_File = '/home/md/SHMCachelib/schedule/ReinforcementLearning/Offline_Sample/20241207_152445_cache_analyzed.log'
Bandwidth_Analyzed_File = '/home/md/SHMCachelib/schedule/ReinforcementLearning/Offline_Sample/20241207_152445_bw_analyzed.log'
CPU_Analyzed_File = '/home/md/SHMCachelib/schedule/ReinforcementLearning/Offline_Sample/20241207_152445_cpu_analyzed.log'

def Calculate_features(cache1, cache2, hitrate1, hitrate2):
    # 1. hitrate1 == hitrate2 == 1
    flag = 1    # 0 线性， 1，exponential
    a = 1
    b = 1
    # 1. 两次采样命中率都是1
    if hitrate1 == 1 and hitrate2 == 1:     
        flag = 0
        a = 1 / min(cache1, cache2)         # a为斜线斜率
        return [a, b, flag]
    # 2. 第二次采样的命中率低于0.05 且分配的资源量是比较多的，认为当前是sequence    -> 4可以改
    if hitrate2 <= 0.05 and cache2 >= 4:                   
        flag = 0
        a = 0                               # a为斜线斜率
        return [a, b, flag]
    # 3. 其余都用exponential
    # 采样点不精确，cache增大，结果反而缩小
    if (cache2 > cache1 and hitrate2 < hitrate1) or (cache2 < cache1 and hitrate2 > hitrate1):
        temp = cache1
        cache1 = cache2
        cache2 = temp
    params_solution = fsolve(exp_growth, [1, 0.1], args=(cache1, hitrate1, cache2, hitrate2), maxfev=100)
    a = params_solution[0]
    b = params_solution[1]
    return [a, b, flag]

def exp_growth(params, x1, y1, x2, y2):
    '''
    params : 初始猜想解
    '''
    A, B = params
    eq1 = A * (1 - np.exp(-B * x1)) - y1
    eq2 = A * (1 - np.exp(-B * x2)) - y2
    return [eq1, eq2]

def get_neighbor(NUM_TASK, TOTAL_CACHE, solution,lb,ub,change_precision=0.5):
    ''' 随机选择一个任务修改其缓存分配，生成一个新的候选解
    NUM_TASK        当前任务数量
    TOTAL_CACHE:    总缓存大小
    :param solution:    当前解
    :param lb: 下限
    :param ub: 上限
    :param change_precision: 调整的精确度
    :return:
    '''
    neighbor = solution.clone()
    # 随机选择一个任务
    # i = np.random.randint(len(solution))
    i = torch.randint(len(solution), (1,))
    # 要改变的大小
    # change = np.random.uniform(-change_precision, change_precision)  # 在-change_precision到+change_precision之间调整
    change = (torch.rand(1) * 2 * change_precision) - change_precision
    # 修改，且满足上下限
    neighbor[i] = max(lb, min(ub, neighbor[i] + change))

    # 调整其他任务的缓存大小，以保持总和为TOTAL_CACHE
    remaining_cache = TOTAL_CACHE - torch.sum(neighbor)
    remaining_tasks = NUM_TASK - 1
    # 将变化的部分分摊到其为
    # allo_remaining_cache = remaining_cache / remaining_tasks

    for j in range(len(solution)):
        if j != i:
            # neighbor[j] += allo_remaining_cache
            # current_value = neighbor[j]
            # 计算新的值，并确保在lb和ub之间
            max_possible_increase = ub - neighbor[j]
            max_possible_decrease = neighbor[j] - lb
            # 分配剩余缓存
            if remaining_cache > 0:
                if (remaining_cache )>max_possible_increase:
                    increase = max_possible_increase
                else:
                    increase = remaining_cache
                # increase = min(max_possible_increase, remaining_cache / remaining_tasks)
                neighbor[j] += increase
                # remaining_cache -= increase
                remaining_cache = TOTAL_CACHE - torch.sum(neighbor)
            else:
                if (abs(remaining_cache) )>max_possible_decrease:
                    decrease = max_possible_decrease
                else:
                    decrease = abs(remaining_cache)
                # decrease = min(max_possible_decrease, abs(remaining_cache) / remaining_tasks)
                neighbor[j] -= decrease
                # remaining_cache += decrease
                remaining_cache = TOTAL_CACHE - torch.sum(neighbor)
    return neighbor

def get_curr_avg_latency(bandwidth_allocation, workload_features):
    assert len(bandwidth_allocation) == len(workload_features)  # 确保两个向量长度一致
    latencies = []
    for i in range(len(bandwidth_allocation)):
        latency = exp_decay_model(x=bandwidth_allocation[i], A=workload_features[i][0],B=workload_features[i][1])
        latencies.append(latency)
    return sum(latencies) / len(latencies)
def get_curr_avg_hitrate(cache_allocation, workload_features):
    '''根据当前缓存分配和工作负载对应的特征曲线计算当前的命中率，优先满足命中率接近1的任务，剩余任务尽量提高命中率'''
    assert len(cache_allocation) == len(workload_features)  # 确保两个向量长度一致
    # 计算每个任务的命中率
    hitrates = []
    for i in range(len(cache_allocation)):
        hitrate = linear_model(x=cache_allocation[i], k=workload_features[i])
        hitrates.append(hitrate)
    # 任务的命中率和索引按命中率排序（从高到低）
    sorted_hitrates_with_index = sorted(enumerate(hitrates), key=lambda x: x[1], reverse=True)
    # 优先考虑命中率较高的任务
    total_hitrate = 0.0
    total_weight = 0.0
    max_hitrate = 1.0
    for idx, hitrate in sorted_hitrates_with_index:
        # 优先处理命中率接近1的任务
        if hitrate >= max_hitrate - 0.05:  # 设定一个容忍误差
            # 给命中率高的任务更大的权重
            weight = 2.0  # 权重可以调整，使得高命中率任务的贡献更大
        else:
            # 其他任务按常规权重处理
            weight = 1.0
        total_hitrate += hitrate * weight
        total_weight += weight

    # 返回加权后的平均命中率
    return total_hitrate / total_weight

# 任务名称与曲线的映射
def MapTaskNameToLines(apps_name, current_phase, total_cache_num, total_cpu_num, total_bw_num):
    '''
    apps_name : ['leveldb_1', 'leveldb_2', 'mongodb_1', 'mongodb_2', 'mysql_1', 'mysql_2', 'sqlite_1', 'sqlite_2', 'tmdb_1', 'tmdb_2']
    '''
    apps_id = []
    for app in apps_name:
        if app.startswith('leveldb'):  # 判断是否以 leveldb 开头
            task_num = int(app.split('_')[1]) 
            apps_id.append(task_num)
        if app.startswith('mongodb'):  
            task_num = int(app.split('_')[1]) + 5
            apps_id.append(task_num)
        if app.startswith('mysql'):  
            task_num = int(app.split('_')[1]) + 10
            apps_id.append(task_num)
        if app.startswith('sqlite'):  
            task_num = int(app.split('_')[1]) + 15
            apps_id.append(task_num)
        if app.startswith('tmdb'):  
            task_num = int(app.split('_')[1]) + 20
            apps_id.append(task_num)
    # print('apps_id',apps_id)
    # 完成任务 -> Cache曲线映射
    apps_cacheline_features = []
    for i in apps_id:
        cache_pattern = fr"Task {i} in Phase {current_phase} Cache-hitrate Simulate Feature : k = ([\d\.]+)"
        with open(Cache_Analyzed_File, 'r') as file:
            for line in file:
                match = re.search(cache_pattern, line)  # 匹配行
                if match:
                    apps_cacheline_features.append(float(match.group(1)))
    random_samples = torch.linspace(0, 1, 128) * total_cache_num  # 随机生成128个浮点数,范围在0到TOTAL_RESOURCE之间
    x, _ = torch.sort(random_samples)
    y_list = []
    for i in range(len(apps_name)):
        y = linear_model(x, apps_cacheline_features[i])
        y_list.append(y)
    cache_states = torch.stack(y_list, dim=0)       # torch.Size([10, 128])
    # 完成任务 ->带宽映射
    apps_bwline_features = []
    for i in apps_id:
        bw_pattern = fr"Task {i} in Phase {current_phase} Bandwidth-latency Simulate Feature : A=([\d\.]+),B=([-+]?\d*\.\d+|\d+)"
        with open(Bandwidth_Analyzed_File, 'r') as file:
            for line in file:
                match = re.search(bw_pattern, line)  # 匹配行
                if match:
                    # print(line)
                    A = float(match.group(1))
                    B = float(match.group(2))
                    apps_bwline_features.append([A,B])
    # print(apps_bwline_features)
    random_samples = torch.linspace(0, 1, 128) * total_bw_num  # 随机生成128个浮点数,范围在0到TOTAL_RESOURCE之间
    x, _ = torch.sort(random_samples)
    y_list = []
    for i in range(len(apps_name)):
        y = exp_decay_model(x, apps_bwline_features[i][0],apps_bwline_features[i][1])
        y_list.append(y)
    bw_states = torch.stack(y_list, dim=0)       # torch.Size([10, 128])
    # 对当前任务对CPU的需求程度进行排序
    CPU_demands = []
    for i in apps_id:
        cpu_pattern = fr"Task {i} in Phase {current_phase} CPU allocation:\[(.*?)\],\s*utilization:\[(.*?)\]"
        with open(CPU_Analyzed_File, 'r') as file:
            for line in file:
                match = re.search(cpu_pattern, line)  # 匹配行
                if match:
                    CPU = match.group(1)
                    CPU = list(map(int, CPU.split(', ')))
                    utilization = match.group(2)
                    utilization = list(map(float, utilization.split(', ')))
                    CPU_demands.append( sum(u / c for u, c in zip(utilization, CPU)) / len(CPU))
    cpu_allocation = Calculate_CPU_Allocation(CPU_demands, total_cpu_num)
    return cache_states, apps_cacheline_features, bw_states, apps_bwline_features, cpu_allocation

def Calculate_CPU_Allocation(cpu_demands, total_cpu_num):
    # print('len(cpu_demands)', len(cpu_demands))
    # print('cpu_demands', cpu_demands)
    # print('total_cpu_num', total_cpu_num)
    cpu_allocation = [0] * len(cpu_demands)
    sorted_demands = sorted(enumerate(cpu_demands), key=lambda x: x[1], reverse=True)
    remaining_cpu = total_cpu_num
    while remaining_cpu > 0:
        for idx in sorted_demands:
            if remaining_cpu == 0:
                break
            cpu_allocation[idx[0]] += 1
            remaining_cpu -= 1
    # print(cpu_allocation)
    return cpu_allocation
def exp_decay_model(x, A, B):
    return torch.maximum(A * (torch.exp(-B * x)), torch.tensor(0.0, dtype=torch.float64))
def linear_model(x, k):
    return torch.minimum(k * x, torch.tensor(1.0, dtype=torch.float64))

def cache_simulated_annealing(NUM_TASK, TOTAL_CACHE, func, x0, features, T_max, T_min, L, max_stay_counter, cooling_rate, precision, lb, ub, change_precision):
    '''模拟退火算法
    从最高温降到最低温（保持不变），到达最低温时如果连续max_stay_counter次迭代中最优解没有改变则退出算法。
    :param func: 目标函数
    :param x0: 初始解向量
    :param features: 特征向量向量，每个元素是一个向量，对应一个任务的命中率曲线
    :param T_max: 初始温度
    :param T_min: 最低温度
    :param L: 每个温度下的迭代次数
    :param max_stay_counter: 在达到最低温度后，如果连续max_stay_counter次迭代中最优解没有改变，则停止算法
    :param cooling_rate: 温度变化率
    :param precision: 在达到最低温度后，统计stay_counter的精度
    :param lb: 解向量的下界
    :param ub: 解向量的上界
    :param change_precision: 邻域生成算子的调整精度
    :return:
    '''
    current_solution = x0  # 当前解
    current_hitrate = func(current_solution, features)  # 当前解的命中率
    best_solution = current_solution  # 最优解
    best_hitrate = current_hitrate  # 最优解的综合命中率

    temperature = T_max  # 初始温度
    flag_reach_min_temp =False
    stay_counter = 0  # 记录连续迭代中最优解未改变的次数

    while(True):
        for i in range(L):
            new_solution = get_neighbor(NUM_TASK, TOTAL_CACHE, current_solution,lb,ub,change_precision)  # 生成邻居解
            new_hitrate = func(new_solution, features)  # 邻居解的延迟
            delta_hitrate = new_hitrate - current_hitrate  # 延迟变化量
            # 判断是否接受邻居解
            if delta_hitrate > 0 :
                current_solution = new_solution
                current_hitrate = new_hitrate
                best_solution = current_solution
                best_hitrate = current_hitrate
                # 到达最低温，新解有一定的下降，若下降精度小于precision
                if flag_reach_min_temp :
                    if abs(delta_hitrate) < precision:
                        stay_counter += 1
                    else:
                        stay_counter = 0
                continue
            elif torch.exp(delta_hitrate / temperature) > torch.rand(1) :
                current_solution = new_solution
                current_hitrate = new_hitrate
                if flag_reach_min_temp:
                    stay_counter += 1
                continue
            if flag_reach_min_temp:
                stay_counter += 1
        if temperature > T_min:
            temperature = temperature * cooling_rate  # 降低温度
        else:
            flag_reach_min_temp = True
        # 检查是否达到最低温度和连续迭代中最优解未改变的次数
        if flag_reach_min_temp and stay_counter >= max_stay_counter:
            break
    best_solution = best_solution.tolist()
    truncated = [int(k) for k in best_solution]
    differences = [x - t for x, t in zip(best_solution, truncated)]
    total_difference = round(sum(best_solution) - sum(truncated))
    indices = sorted(range(len(differences)), key=lambda i: -differences[i])
    for i in range(total_difference):
        truncated[indices[i]] += 1
    return truncated, best_hitrate
    # best_solution = x0.tolist()
    # best_hitrate = func(x0, features)
    # truncated = [int(k) for k in best_solution]
    # differences = [x - t for x, t in zip(best_solution, truncated)]
    # total_difference = round(sum(best_solution) - sum(truncated))
    # indices = sorted(range(len(differences)), key=lambda i: -differences[i])
    # for i in range(total_difference):
    #     truncated[indices[i]] += 1
    # return truncated, best_hitrate

def bandwidth_simulated_annealing(NUM_TASK, TOTAL_BANDWIDTH, func, x0, features, T_max, T_min, L, max_stay_counter, cooling_rate, precision, lb, ub, change_precision):
    '''模拟退火算法
    从最高温降到最低温（保持不变），到达最低温时如果连续max_stay_counter次迭代中最优解没有改变则退出算法。
    :param func: 目标函数
    :param x0: 初始解向量
    :param features: 特征向量向量，每个元素是一个向量，对应一个任务的命中率曲线
    :param T_max: 初始温度
    :param T_min: 最低温度
    :param L: 每个温度下的迭代次数
    :param max_stay_counter: 在达到最低温度后，如果连续max_stay_counter次迭代中最优解没有改变，则停止算法
    :param cooling_rate: 温度变化率
    :param precision: 在达到最低温度后，统计stay_counter的精度
    :param lb: 解向量的下界
    :param ub: 解向量的上界
    :param change_precision: 邻域生成算子的调整精度
    :return:
    '''
    current_solution = x0  # 当前解
    current_latency = func(current_solution, features)  # 当前解的命中率
    best_solution = current_solution  # 最优解
    best_latency = current_latency  # 最优解的综合命中率

    temperature = T_max  # 初始温度
    flag_reach_min_temp =False
    stay_counter = 0  # 记录连续迭代中最优解未改变的次数

    while(True):
        for i in range(L):
            new_solution = get_neighbor(NUM_TASK, TOTAL_BANDWIDTH, current_solution,lb,ub,change_precision)  # 生成邻居解
            new_latency = func(new_solution, features)  # 邻居解的延迟
            delta_latency = new_latency - current_latency  # 延迟变化量
            # 判断是否接受邻居解
            if delta_latency < 0 :
                current_solution = new_solution
                current_latency = new_latency
                best_solution = current_solution
                best_latency = current_latency
                # 到达最低温，新解有一定的下降，若下降精度小于precision
                if flag_reach_min_temp :
                    if abs(delta_latency) < precision:
                        stay_counter += 1
                    else:
                        stay_counter = 0
                continue
            elif torch.exp(delta_latency / temperature) > torch.rand(1) :
                current_solution = new_solution
                current_latency = new_latency
                if flag_reach_min_temp:
                    stay_counter += 1
                continue
            if flag_reach_min_temp:
                stay_counter += 1
        if temperature > T_min:
            temperature = temperature * cooling_rate  # 降低温度
        else:
            flag_reach_min_temp = True

        # 检查是否达到最低温度和连续迭代中最优解未改变的次数
        if flag_reach_min_temp and stay_counter >= max_stay_counter:
            break
    # print(iteration, temperature)
    best_solution = best_solution.tolist()
    truncated = [int(k) for k in best_solution]
    differences = [x - t for x, t in zip(best_solution, truncated)]
    total_difference = round(sum(best_solution) - sum(truncated))
    indices = sorted(range(len(differences)), key=lambda i: -differences[i])
    for i in range(total_difference):
        truncated[indices[i]] += 1
    return truncated, best_latency

def find_target(point1, point2, y_value):
    x1, y1 = point1
    x2, y2 = point2
    if x1 == x2:
        print('两点横轴不能相同')
        sys.exit(1)
    if y1 == y2:
        return None
    m = (y2 - y1) / (x2 - x1)
    c = y1 - m * x1
    x_value = (y_value - c) / m
    # return math.ceil(x_value)
    return x_value

def cache_estimate(allocation, hitrate, total_resources):
    # TODO: allocation和hitrate是长度为2的list，对应两次缓存划分以及对应的缓存命中率
    # 据此对工作集大小进行估算，并通过动态规划进行求解，定死缓存划分的方案
    for i in range(len(allocation)):
        allocation[i] = [int(k) for k in allocation[i]]
        hitrate[i] = [float(k) for k in list(hitrate[i].values())]
    points = []
    for i in range(len(allocation)):
        points.append(list(zip(allocation[i], hitrate[i])))
    target_point = [find_target(points[0][i], points[1][i], 1.0) for i in range(len(points[0]))]
    target_point = [math.ceil(x * 1.1) if x is not None else -1 for x in target_point]
    target_point = [x if x >= 0 else -1 for x in target_point ]
    print(target_point)
    # 动态规划求解
    n = len(target_point)
    dp = [0] * (total_resources + 1)
    choices = [[False] * (total_resources + 1) for _ in range(n)]

    for i in range(n):
        task_need = target_point[i]
        if task_need == -1:
            continue
        for j in range(total_resources, task_need - 1, -1):
            if dp[j] < dp[j - task_need] + 1:
                dp[j] = dp[j - task_need] + 1
                choices[i][j] = True
    allocation = [0] * n  # 用于记录每个任务的分配情况，0 表示未分配，1 表示分配
    remaining_resources = total_resources
    # 从最后一个任务逆序回溯选择过程
    for i in range(n - 1, -1, -1):
        if target_point[i] != -1 and choices[i][remaining_resources]:
            allocation[i] = target_point[i]  # 表示该任务被分配了资源
            remaining_resources -= target_point[i]  # 减少剩余资源
    
    index = 0
    while remaining_resources > 0:
        allocation[index] += 1
        remaining_resources -= 1
        index = (index+1)%len(allocation)
    return allocation

def perturb_list_integers_no_same(lst, n_resource, epsilon=6):
    '''epsilon ： 扰动幅度'''
    # 计算元素的总和
    total = n_resource
    # 对每个元素进行整数扰动，确保扰动后值不同于原值
    perturbed = []
    for x in lst:
        while True:
            # 生成一个小的随机整数扰动，范围 [-epsilon, epsilon]
            delta = random.randint(-epsilon, epsilon)
            new_value = x + delta
            # 确保新值大于0，且新值不等于原值
            if new_value != x and new_value > 0:
                perturbed.append(new_value)
                break
    # 重新调整数组以保持总和不变
    perturbed_sum = sum(perturbed)
    difference = total - perturbed_sum
    # 根据差值调整元素，确保调整后总和不变
    i = 0
    unit = 1
    count = 0
    while difference != 0:
        if difference > 0:
            # 如果总和变小了，需要增加一些值
            if perturbed[i] + unit != lst[i] and perturbed[i] + unit > 0:  # 防止增加后等于原值
                perturbed[i] += unit
                difference -= unit
                count = 0
                unit = 1
        elif difference < 0:
            # 如果总和变大了，需要减少一些值
            if perturbed[i] - unit != lst[i] and perturbed[i] - unit > 0:  # 防止减少后等于原值
                perturbed[i] -= unit
                difference += unit
                count = 0
                unit = 1
        i = (i + 1) % len(perturbed)  # 循环调整每个元素
        count += 1
        if count == len(perturbed):
            unit += 1
    return perturbed



if __name__ == '__main__':
    # apps = ['leveldb_1', 'leveldb_2','leveldb_3','leveldb_4','leveldb_5',
    #          'mongodb_1', 'mongodb_2','mongodb_3','mongodb_4','mongodb_5',
    #          'tmdb_1', 'tmdb_2','tmdb_3', 'tmdb_4','tmdb_5',
    #          'mysql_1', 'mysql_2','mysql_3', 'mysql_4','mysql_5', 
    #          'sqlite_1', 'sqlite_2','sqlite_3', 'sqlite_4','sqlite_5']
    # cache_state,cache_features, bandwidth_state, bandwidth_features, cpu_allocation = MapTaskNameToLines(apps, 1,total_cache_num=len(apps) * 16, 
    #                                                                                      total_cpu_num=33,
    #                                                                                      total_bw_num=len(apps) * 20)
    # cache_model_path = '/home/md/SHMCachelib/schedule/ReinforcementLearning/train_dir/Cache_num_task_10_20241207_184235/model_T10_I126.pt'
    # bandwidth_model_path = '/home/md/SHMCachelib/schedule/ReinforcementLearning/train_dir/BandWidth_num_task_10_20241208_153300/model_T15_I294.pt'
    # cache_state_dict = torch.load(cache_model_path, weights_only=True)
    # bandwidth_state_dict = torch.load(bandwidth_model_path, weights_only=True)
    # cache_agent = Agent()
    # bandwith_agent = Agent()
    # cache_agent.model.load_state_dict(cache_state_dict)
    # bandwith_agent.model.load_state_dict(bandwidth_state_dict)
    # cache_action_probs = cache_agent.get_action(cache_state)
    # bandwidth_action_probs = bandwith_agent.get_action(bandwidth_state)
    # print("强化学习算法决策cache分配为: ", str(cache_action_probs * len(apps) * 16))
    # print("强化学习算法决策带宽分配为: ", str(bandwidth_action_probs * len(apps) * 20))
    # curr_hitrate = get_curr_avg_hitrate(cache_action_probs * len(apps) * 16, cache_features)
    # curr_latency = get_curr_avg_latency(bandwidth_action_probs * len(apps) * 20, bandwidth_features)
    # print('action决策预测平均命中率 ： ',curr_hitrate)
    # print('action决策预测平均延迟 ： ',curr_latency)
    
    # s = time.process_time()
    # # 3. 模拟退火算法对cache分配和bandwidth分配进行优化
    # best_cache_solution, best_hitrate = cache_simulated_annealing( len(apps), 
    #                                                          len(apps) * 16,
    #                                                         get_curr_avg_hitrate, 
    #                                                         x0 = cache_action_probs * len(apps) * 16,
    #                                                         features=cache_features,
    #                                                         T_max=100, T_min=1e-3, 
    #                                                         L=30, 
    #                                                         max_stay_counter=10, 
    #                                                         precision=0.5,
    #                                                         cooling_rate=0.95,
    #                                                         lb=2,
    #                                                         ub= len(apps) * 16,
    #                                                         change_precision=10
    #                                                         )
    # best_bandwidth_solution, best_latency = bandwidth_simulated_annealing( len(apps), 
    #                                                          len(apps) * 20,
    #                                                         get_curr_avg_latency, 
    #                                                         x0 = bandwidth_action_probs * len(apps) * 20,
    #                                                         features=bandwidth_features,
    #                                                         T_max=100, T_min=1e-3, 
    #                                                         L=30, 
    #                                                         max_stay_counter=10, 
    #                                                         precision=0.5,
    #                                                         cooling_rate=0.95,
    #                                                         lb = 5,
    #                                                         ub= len(apps) * 20,
    #                                                         change_precision=10
    #                                                         )
    # print(f'best cache solution: {best_cache_solution},\n best avg hitrate: {best_hitrate}')
    # print(f'best bandwidth solution: {best_bandwidth_solution},\n best avg latency: {best_latency}')
    # print("模拟退火算法耗时: ",time.process_time()-s)
    # print("***********************")
    total_cache = [2, 26, 1, 2, 25, 2, 20, 22, 2, 25, 29, 32, 26, 24, 26, 3, 25, 32, 3, 22, 2, 23, 2, 1, 23]
    print(sum(total_cache))