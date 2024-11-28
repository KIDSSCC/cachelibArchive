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
from agent import *

# def sigmoid(x, a, b, c):        # 被弃用了
#         """定义 Sigmoid 函数"""
#         return a / (1 + np.exp(-b * (x - c)))

def exponential(x, a , b):
    """命中率为指数函数时"""
    if type(a) != torch.Tensor:
        x = torch.tensor(x, dtype=torch.float32)  # 如果 x 不是 Tensor 类型，则转换为 Tensor
        a = torch.tensor(a, dtype=torch.float32)  # 如果 a 不是 Tensor 类型，则转换为 Tensor
        b = torch.tensor(b, dtype=torch.float32)  # 如果 b 不是 Tensor 类型，则转换为 Tensor
    res = a * (1 - torch.exp(-b * (x - 0)))
    res = torch.minimum(res, torch.tensor(1))  # 将结果限制在最大值 1
    return res
def predict_hitrate(cache_size, action_hitrate_index):
    """
    传入一组分配的 cache 大小和对应预测曲线的参数 abc，返回在所分配 cache 大小下的命中率。
    :param cache_size: 分配的 cache 大小，可以是标量或一组值 (NumPy 数组或 PyTorch 张量)
    :param action_hitrate_index: 一条曲线
    :return: 计算得到的命中率，与 cache_size 的形状一致
    """
    res = exponential(cache_size, action_hitrate_index[0],action_hitrate_index[1])
    return res
def exp_growth(params, x1, y1, x2, y2):
    '''
    params : 初始猜想解
    '''
    A, B = params
    eq1 = A * (1 - np.exp(-B * x1)) - y1
    eq2 = A * (1 - np.exp(-B * x2)) - y2
    return [eq1, eq2]

def do_simulation(first, second):
    '''根据两个点做sigmoid模拟'''
    assert len(first[0])==len(second[0]), "任务数量不一致"  
    TOTAL_CACHE_SIZE = sum(first[0])        # 总资源量
    print("=============do simulation===============")
    num_tasks = len(first[0])  # 任务数量
    predictions = []
    # 函数模拟
    for i in range(num_tasks):
        # 分别获取两个点的 cache 和 hitrate 值
        cache1, hitrate1 = first[0][i], first[1][i]
        cache2, hitrate2 = second[0][i], second[1][i]
        # 确保cache值不同，以避免除零 -> predication_model.py
        assert cache1 != cache2, f"任务 {i} 的两个点的 cache 值不能相等"
        # 1.D_SEQUENTIAL分布，采样到的点total_list_hit_rate都为0
        if hitrate2 == 0 :
            predictions.append([0,0])
            continue
        # 2.采样点不精确，增大cache后total_list_hit_rate反而降低，做修改
        if (cache2 > cache1 and hitrate2 < hitrate1) or (cache2 < cache1 and hitrate2 > hitrate1):
            temp = cache1
            cache1 = cache2
            cache2 = temp
        # 3.其余使用指数函数进行模拟 -> 改为用指数函数模拟
        params_solution = fsolve(exp_growth, [1, 0.1], args=(cache1, hitrate1, cache2, hitrate2), maxfev=100)
        a = params_solution[0]
        b = params_solution[1]
        predictions.append([a,b])
    random_samples = torch.linspace(0, 1, 128) * TOTAL_CACHE_SIZE
    x, _ = torch.sort(random_samples)
    y_list = []
    for i in range(len(predictions)):
        # print(f'predictions[{i}] : , {predictions[i]}')
        y = predict_hitrate(x, predictions[i])
        # print('y : ', y)
        y_list.append(y)
    y = torch.stack(y_list, dim=0)
    # print("y : ",y)
    # 找到曲线采样矩阵中的最小值和最大值
    min_val = torch.min(y)
    max_val = torch.max(y)
    # 缩放矩阵到[0,1]范围内
    y = (y - min_val) / (max_val - min_val)
    return y,predictions

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

def get_curr_avg_hitrate( cache_allocation, workload_features):
    '''根据当前缓存分配和工作负载对应的特征曲线计算当前的命中率'''
    assert len(cache_allocation) == len(workload_features)      # 确保两个向量长度一致
    total_hitrate = 0.0
    for i in range(len(cache_allocation)):
        total_hitrate += exponential(cache_allocation[i], workload_features[i][0], workload_features[i][1])
    return  total_hitrate / len(cache_allocation)

def simulated_annealing2(NUM_TASK, TOTAL_CACHE, func, x0, features, T_max, T_min, L, max_stay_counter, cooling_rate, precision, lb, ub, change_precision):
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
            # elif np.exp(delta_hitrate / temperature) > np.random.rand() :
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
    # print(iteration, temperature)
    return best_solution, best_hitrate

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


class OnlineProfile(ScheduleFrame):
    def __init__(self, all_apps: List[str], 
                 n_resources: List[int]):
        super().__init__()
        self.all_apps = all_apps
        self.num_apps = len(all_apps)

        # cache可以分0，但cpu和bandwidth不可，此处统一arm的数量，生成分配方案时再进行限制，
        assert len(n_resources) == 3, "The dimension of resources should be 3"
        self.cpu_n_arms = n_resources[1] + 1
        self.bandwidth_n_arms = n_resources[2] + 1
        
        # 采样相关
        self.times = 0

        # # 负载变化相关，未更新
        self.history_reward_window = 4
        self.history_reward = []
        
        # 针对cache资源的单独判定
        self.cache_mode = True
        self.cache_perturbed = None
        self.n_cache = n_resources[0]
        self.cache_history_arm = []
        self.cache_hitrate = []
        self.estimate = []

        self.new_arm = []
        return

    def select_arm(self):
        return self.new_arm

    def update(self, reward, chosen_arm):
        """
        目前是只扰动了Cache
        TODO:
            chosen_arm 调整为长度为3的list, 每个元素为一个dict, 对应该资源在每个任务上的分配
            reward 调整为一个长度为3的list, [0]为延迟对应的reward值, [1]为dict, 每个任务对应的cpu利用率，[2]为dict，每个任务对应的cache命中率
        """
        self.new_arm = []
        # 动态负载变化监测
        if not self.cache_mode:
            if len(self.history_reward) < self.history_reward_window:
                self.history_reward.append(sum(map(float, reward[2].values())))
            else:
                half_window = self.history_reward_window // 2
                first_half = self.history_reward[:half_window]
                second_half = self.history_reward[half_window:]
                first_aver = sum(first_half) / len(first_half)
                second_aver = sum(second_half) / len(second_half)
                print('{} --> {},  {}'.format(first_aver, second_aver, abs(second_aver - first_aver) / first_aver))
                if (first_aver - second_aver) / first_aver > 0.10:
                    # workload change
                    print('----- test workload change -----')
                    self.reset()
                else:
                    self.history_reward.pop(0)

        # 调整缓存资源
        if self.cache_mode:
            # 针对cache资源的单独处理
            cache_arm = list(chosen_arm[0].values())
            if self.times == 0:
                #TODO:记录第一次的cache信息，并进行扰动
                self.cache_perturbed = perturb_list_integers_no_same(cache_arm, self.n_cache)
                self.cache_history_arm.append(cache_arm)
                self.cache_hitrate.append(reward[2])
                self.new_arm = [self.cache_perturbed, list(chosen_arm[1].values()), list(chosen_arm[2].values())]
            elif self.times == 1:
                #TODO:记录第二次的信息，并进行动态规划求解
                self.cache_history_arm.append(cache_arm)
                self.cache_hitrate.append(reward[2])
                # 通过动态规划直接求解应划分的缓存大小, 保存在self.estimate
                print('history_arm: ', str(self.cache_history_arm))
                print('hitrate: ', str(self.cache_hitrate))
                self.cache_mode = False
        self.times += 1


    def get_now_reward(self, performance, context_info=None):
        # update the context
        # tmp = [list(row) for row in zip_longest(*context_info, fillvalue=None)]
        # sum_context = np.zeros(self.n_features)
        # for i, app in enumerate(self.all_apps):
        #     self.context[app] = tmp[i]
        #     sum_context += np.array(self.context[app])
        # for app in self.all_apps:
        #     self.other_context[app] = list((sum_context - np.array(self.context[app])) / (len(self.all_apps) - 1))

        # calculate the reward for hitrate etc bigger is greater
        # th_reward = sum(float(x) for x in performance) / len(performance)
        # return th_reward

        # smaller is greater
        aver_latency = sum(float(x) for x in performance) / len(performance)
        th_reward = 500 / aver_latency
        return th_reward, aver_latency

    def reset(self):

        # 采样相关
        self.times = 0

        # # 负载变化相关，未更新
        self.history_reward = []
       
        # 针对cache资源的单独判定
        self.cache_mode = True
        self.cache_perturbed = None
        self.cache_history_arm = []
        self.cache_hitrate = []
        self.estimate = []


    def save_to_pickle(self, filename):
        '''将当前模型保存'''
        with open(filename, 'wb') as f:
            pickle.dump(self, f)

    @staticmethod
    def load_from_pickle(filename):
        with open(filename, 'rb') as f:
            return pickle.load(f)


if __name__ == '__main__':
    point1=[[16, 16, 16, 16, 16, 16, 16, 16, 16, 16],[0.0001 , 0.8601,  0.2398,  0.4711,  0.29215,   0.26915,  0.7311,  0,        0.5329,  0.2957]]
    point2=[[20, 20, 10, 21, 12, 14, 17, 9,  17, 20],[0,       0.9324,  0.0447,  0.7632,  0.297075,  0.1392,   0.7185,  0,        0.7132,  0.3795]]
    state, features = do_simulation(point1, point2)
    curr_hitrate = get_curr_avg_hitrate(point2[0], features)
    task_num = len(point2[0])
    total_cache = sum(point1[0])
    print('当前命中率 : ', curr_hitrate, ' 任务数目 : ', task_num, ' 总cache资源量 : ', total_cache)
    model_path = './train_dir/num_task_10_20241128_120715/model_T10_I295.pt'
    state_dict = torch.load(model_path, weights_only=True)
    agent = Agent()
    agent.model.load_state_dict(state_dict)
    action_probs = agent.get_action(state)
    print('agent choose action_probs: ', action_probs * 160)
    curr_hitrate = get_curr_avg_hitrate(action_probs * 160, features)
    print('action决策预测命中率 ： ',curr_hitrate)
    s = time.process_time()
    best_cache_solution, best_hitrate = simulated_annealing2( len(point1[0]), 160,
                                                            get_curr_avg_hitrate, 
                                                            x0=action_probs * 160,
                                                            features=features,
                                                            T_max=100, T_min=1e-3, L=30,
                                                            max_stay_counter=10, 
                                                            precision=0.5,
                                                            cooling_rate=0.95,lb=2,
                                                            ub=sum(point1[0]),
                                                            change_precision=10
                                                          )
    print("模拟退火算法耗时: ",time.process_time()-s)
    print(f'best cache solution: {best_cache_solution},\n best avg hitrate: {best_hitrate}')
    print("***********************")
    # curr_hitrate = get_curr_avg_hitrate(point2[0], workload_features)
    # task_num = len(point2[0])
    # total_cache = sum(point1[0])
    # print('当前命中率 : ', curr_hitrate, ' 任务数目 : ', task_num, ' 总cache资源量 : ', total_cache)
    # s = time.process_time()
    # best_solution, best_hitrate = simulated_annealing2(task_num, total_cache, get_curr_avg_hitrate, 
    #                                                    x0=point1[0],features=workload_features,
    #                                                    T_max=100, T_min=1e-3, L=150, max_stay_counter=200, precision=0.5,
    #                                                     cooling_rate=0.95,lb=2,ub=total_cache,change_precision=10
    #                                                     )
    # print("模拟退火算法耗时: ",time.process_time()-s)
    # print("最佳缓存分配：", best_solution, "最佳命中率：", best_hitrate, "命中提升幅度：", (best_hitrate - curr_hitrate) / curr_hitrate)
    # print("***********************")
    # point3=[[20, 20, 10, 21, 12, 14, 17, 9,  17, 20],[0.385,   0,       0.8063,  0.0508,  0.804575,  0.2685,   0,       0.63,     0,       0.724]]
    # point4=[[18, 18, 12, 24, 16, 9,  18, 14, 16, 15],[0.3415,  0,       0.8841,  0.0003, 0.82305,   0.17145,  0,       0.6912,   0,       0.5854]]
