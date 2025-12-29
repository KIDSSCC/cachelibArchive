import pickle
import random
import math
import sys
import config
from itertools import zip_longest
from ScheduleFrame import *
from typing import List

def check_list_has_equal(list1, list2):
    for i in range(len(list1)):
        if list1[i] == list2[i]:
            return True, i
    return False, -1

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
    return x_value
def cache_estimate(allocation, hitrate, total_resources):
    # TODO: allocation和hitrate是长度为2的list，对应两次缓存划分以及对应的缓存命中率
    # 据此对工作集大小进行估算，并通过动态规划进行求解
    n_cache= total_resources
    for i in range(len(allocation)):
        allocation[i] = [int(k) for k in allocation[i]]
        hitrate[i] = [float(k) for k in list(hitrate[i].values())]
    points = []
    for i in range(len(allocation)):
        points.append(list(zip(allocation[i], hitrate[i])))
    
    # print('point is ', str(points))
    target_point = [find_target(points[0][i], points[1][i], 0.7) for i in range(len(points[0]))]
    target_point = [math.ceil(x * 0.9) if x is not None else -1 for x in target_point]
    target_point = [x if x >= 0 else -1 for x in target_point ]
    # print(target_point)

    # 至此完成了工作集的预测，先行根据预测结果分配出一部分资源
    n = len(target_point)
    
    allocation = [0] * n            # 用于记录每个任务的分配情况，0 表示未分配，1 表示分配
    for i in range(len(target_point)):
        if target_point[i] == -1:
            allocation[i] = 2
            total_resources -= 2
        else:
            allocation[i] = 5
            total_resources -= 5
            target_point[i] -= 5

    # 动态规划求解
    dp = [0] * (total_resources + 1)
    choices = [[False] * (total_resources + 1) for _ in range(n)]

    for i in range(n):
        task_need = target_point[i]
        if task_need < 0:
            continue
        for j in range(total_resources, task_need - 1, -1):
            if dp[j] < dp[j - task_need] + 1:
                dp[j] = dp[j - task_need] + 1
                choices[i][j] = True

    remaining_resources = total_resources
    # 从最后一个任务逆序回溯选择过程
    for i in range(n - 1, -1, -1):
        if target_point[i] >= 0 and choices[i][remaining_resources]:
            allocation[i] += target_point[i]  # 表示该任务被分配了资源
            remaining_resources -= target_point[i]  # 减少剩余资源
    
    # 将多余资源均分至所有任务上
    index = 0
    while remaining_resources > 0:
        while target_point[index] < 0:
            index = (index+1)%len(allocation)
        allocation[index] += 1
        remaining_resources -= 1
        index = (index+1)%len(allocation)

    # 对分配方案进行核验，满足约束条件
    while np.sum(allocation) > n_cache:
        max_index = allocation.index(max(allocation))
        allocation[max_index] -= 1
    return allocation

def perturb_list_integers_no_same(lst, n_resource, epsilon=10):
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

def simple_perturb(lst, n_resource, epsilon=10):
    perturbed = []
    direction = -1

    for x in lst:
        perturbed.append(x + direction * epsilon)
        direction = direction * -1
    
    if direction != -1:
        # 扰动后的资源总和不等于原值
        epsilon = epsilon / 2
        perturbed[0] += epsilon
        perturbed[1] -= epsilon

    return perturbed


def gen_feasible_config(n_resource, top_k_arm, p_t, app_id, lower_bound):
    matrix = [[0] * (n_resource + 1) for _ in range(len(app_id) + 1)]
    for i in range(1, len(app_id) + 1):
        matrix[i][0] = None
    history = [[[] for _ in range(n_resource + 1)] for _ in range(len(app_id) + 1)]
    for i in range(1, len(app_id) + 1):
        for j in range(1, n_resource + 1):
            possible_reward = []
            # 当前任务的所有备选arm，其中再加最小分配限制，去重
            k_arm = [i for i in top_k_arm[i-1] if i>=lower_bound]
            k_arm.append(lower_bound)
            k_arm = list(set(k_arm))
            p_t_c = p_t[app_id[i-1]]
            
            for k in range(len(k_arm)):
                if j < k_arm[k]:
                    possible_reward.append(None)
                    continue
                if matrix[i-1][j-k_arm[k]] is None:
                    possible_reward.append(None)
                    continue
                possible_reward.append(matrix[i-1][j-k_arm[k]] + p_t_c[k_arm[k]])
            filter_list = [ x for x in possible_reward if x is not None]
            if len(filter_list) == 0:
                matrix[i][j] = None
            else:
                max_reward = max(filter_list)
                matrix[i][j] = max_reward
                max_index = possible_reward.index(max_reward)

                
                history[i][j].extend(history[i-1][j-k_arm[max_index]])
                history[i][j].append(k_arm[max_index])
    return history[len(app_id)][n_resource]


def get_top_k(arr, k):
    """
    select the top k items with the highest expected reward
    Args:
        arr list<double>: the expected reward of each configuration
        k int: the number of top k items to select
        times int: the number of times the bandit has been played
    Returns:
        arr_top_k_id list<int>: the indices of the top k items
    """
    if random.random() < 0.05:
        arr_top_k_id = [random.randint(0, len(arr) - 1) for _ in range(k)]
    else:
        arr_top_k_id = np.argsort(arr)[-k:]
    return list(arr_top_k_id)


def beam_search(n_resources, all_apps, p_t, min_value=0):
    """
    从各个子bandit中选出top_k的配置, 并进行组合, 形成全局的配置
    Args:
        n_resources int: the total number of current resource
        all_apps list<str>: the list of all applications
        p_t list<double>: the expected reward of each configuration
    Returns:
        action dict<str, int>: the selected configuration
    """
    action = {}.fromkeys(all_apps)
    num_app = len(all_apps)
    # 原方案是根据num_app的数量动态调整top_k的数值，这里先固定为3
    # top_k = int(10 ** (np.log10(end_condition) / num_app))
    top_k = 3

    top_k_arm = [get_top_k(p_t[all_apps[i]], top_k) for i in range(num_app)]
    feasible_configs = gen_feasible_config(n_resources, top_k_arm, p_t, all_apps, min_value)
    # gen_feasible_config下会存在部分资源未利用，将其重新分配到随机的任务上
    # 后续可以考虑将其附加到距离最优分配最近的任务上
    assert sum(feasible_configs) <= n_resources, 'The allocated cache exceeds the limit'
    if len(feasible_configs) != len(all_apps):
        print('Error, feasible_configs is: {}'.format(str(feasible_configs)))
    surplus_resources = n_resources - sum(feasible_configs)
    allocate_list = [0] * (len(all_apps) // 5)
    index = 0
    while surplus_resources > 0:
        allocate_list[index] += 1
        surplus_resources -= 1
        index = (index + 1) % len(allocate_list)
    for item in allocate_list:
        feasible_configs[random.randint(0, len(all_apps) - 1)] += item
    for i in range(num_app):
        action[all_apps[i]] = feasible_configs[i]
    return action


def latin_hypercube_sampling(n_samples, n_apps, min_value, max_value, ratio):
    """
    满足约束条件的拉丁超立方采样函数
    Args:
        n_samples int: 样本数量
        n_apps int: 应用数量
        min_value int: 资源下限
        max_value int: 资源上限
        ratio int: 资源分配比例
    Returns:
        list: 满足约束条件的样本列表
    """
    # 初始化样本矩阵
    weight = np.zeros((n_samples, n_apps))

    # 对每个维度进行采样
    for i in range(n_apps):
        # 将每个维度均分成 n_samples 份
        perm = np.random.permutation(n_samples)
        # 在每个子区间内随机选择一个点
        for j in range(n_samples):
            weight[j, i] = (perm[j] + np.random.uniform()) / n_samples
    
    result = []
    current_resources = int((max_value - min_value * n_apps) / ratio)
    for i in range(n_samples):
        allocation = [min_value] * n_apps
        for j in range(n_apps):
            allocation[j] += int(current_resources * weight[i, j] / np.sum(weight[i])) * ratio
        assert np.sum(allocation) <= max_value, "Allocation exceeds maximum value"
        while max_value > np.sum(allocation):
            random_index = np.random.randint(0, n_apps - 1)
            allocation[random_index] += 1
        result.append(allocation)

    return result


class OLUCB(ScheduleFrame):
    def __init__(self, all_apps: List[str], 
                 n_resources: List[int], 
                 alpha: float, 
                 factor_alpha: float, 
                 n_features: int, 
                 sample=True):
        super().__init__()
        self.all_apps = all_apps
        self.num_apps = len(all_apps)
        self.n_features = n_features

        # cache可以分0，但cpu和bandwidth不可，此处统一arm的数量，生成分配方案时再进行限制，
        # assert len(n_resources) == 3, "The dimension of resources should be 3"
        # self.cpu_n_arms = n_resources[1] + 1
        # self.bandwidth_n_arms = n_resources[2] + 1
        
        self.init_factor = [alpha, factor_alpha]
        self.alpha = self.init_factor[0]
        self.factor_alpha = self.init_factor[1]

        # 采样相关
        self.times = 0
        self.sampling_model = sample
        self.sample_times = config.SAMPLE_TIMES
        self.ratio = config.SAMPLE_RATIO
        self.cpu_sample_config = None
        self.bandwidth_sample_config = None
        # cache和bandwidth指标为尾延迟，cpu评价指标为cpu利用率，分开保存
        self.c_b_sample_result = {}
        self.cpu_sample_result = {}

        # 近似收敛相关
        self.curr_best_config = None
        self.curr_best_reward = None
        self.duration_period = 0
        self.patience = config.PATIENCE
        self.epsilon = config.EPSILON

        # # 负载变化相关，未更新
        self.load_change_threshold = config.LOAD_CHANGE_THRESHOLD
        self.history_reward_window = config.HISTORY_REWARD_WINDOW
        self.history_reward = []

        # 每一维度的资源单独维度一组bandit
        # self.A_cpu = {}
        # self.b_cpu = {}
        # self.p_cpu_t = {}
        # for app in self.all_apps:
        #     self.A_cpu[app] = np.zeros((self.cpu_n_arms, self.n_features * 2, self.n_features * 2))
        #     self.b_cpu[app] = np.zeros((self.cpu_n_arms, self.n_features * 2, 1))
        #     self.p_cpu_t[app] = np.zeros(self.cpu_n_arms)
        #     for arm in range(self.cpu_n_arms):
        #         self.A_cpu[app][arm] = np.eye(self.n_features * 2)

        # self.A_bandwidth = {}
        # self.b_bandwidth = {}
        # self.p_bandwidth_t = {}
        # for app in self.all_apps:
        #     self.A_bandwidth[app] = np.zeros((self.bandwidth_n_arms, self.n_features * 2, self.n_features * 2))
        #     self.b_bandwidth[app] = np.zeros((self.bandwidth_n_arms, self.n_features * 2, 1))
        #     self.p_bandwidth_t[app] = np.zeros(self.bandwidth_n_arms)
        #     for arm in range(self.bandwidth_n_arms):
        #         self.A_bandwidth[app][arm] = np.eye(self.n_features * 2)

        # 上下文维护
        self.context = {}
        self.other_context = {}
        sum = np.zeros(n_features)
        for app in self.all_apps:
            self.context[app] = [1.0 for _ in range(self.n_features)]
            sum += np.array(self.context[app])
        for app in self.all_apps:
            self.other_context[app] = list((sum - np.array(self.context[app])) / (len(self.all_apps) - 1))

        # # 超立方采样
        # if self.sampling_model:
        #     # self.cache_sample_config =      latin_hypercube_sampling(self.sample_times, self.num_apps, 0, self.cache_n_arms - 1, self.ratio)
        #     self.cpu_sample_config =        latin_hypercube_sampling(self.sample_times, self.num_apps, 1, self.cpu_n_arms - 1, 1)
        #     self.bandwidth_sample_config =  latin_hypercube_sampling(self.sample_times, self.num_apps, 1, self.bandwidth_n_arms - 1, 1)
        
        # 针对cache资源的单独判定
        self.cache_mode = True
        self.cache_perturbed = None
        self.n_cache = n_resources[0]
        self.cache_history_arm = []
        self.cache_hitrate = []
        self.estimate = []
        return

    def select_arm(self):
        cache_allocation = []
        if self.cache_mode:
            if len(self.estimate)!=0:
                cache_allocation = self.estimate
                self.cache_mode = False
            elif self.cache_perturbed is not None:
                cache_allocation = self.cache_perturbed

        self.times += 1
        return [cache_allocation]

    def update(self, reward, chosen_arm):
        """
        TODO:
            chosen_arm 调整为长度为3的list, 每个元素为一个dict, 对应该资源在每个任务上的分配
            reward 调整为一个长度为3的list, [0]为延迟对应的reward值, [1]为dict, 每个任务对应的cpu利用率，[2]为dict，每个任务对应的cache命中率
        """
        schedule_finish = False
        workload_change = False
        # print('----- update, self.times is {}'.format(self.times))
        # 动态负载变化监测
        if not self.cache_mode and self.times > self.patience:
            self.history_reward.append(sum(map(float, reward[0].values())))
            if len(self.history_reward) >= self.history_reward_window:
                half_window = self.history_reward_window // 2
                first_half = self.history_reward[:half_window]
                second_half = self.history_reward[half_window:]
                first_aver = sum(first_half) / len(first_half)
                second_aver = sum(second_half) / len(second_half)

                if (first_aver - second_aver) / first_aver > 0.10:
                    # workload change
                    print('----- test workload change -----')
                    self.reset()
                    workload_change = True
                else:
                    self.history_reward.pop(0)

        # 调整缓存资源
        if self.cache_mode:
            # 针对cache资源的单独处理
            cache_arm = list(chosen_arm[0].values())
            if self.times == 1:
                #记录第一次的cache信息，并进行扰动
                even_list = [self.n_cache // self.num_apps] * self.num_apps
                if cache_arm == even_list:
                    # 当前配置就是均分状态，进行分配扰动
                    self.cache_perturbed = simple_perturb(cache_arm, self.n_cache, 20)
                else:
                    # 当前配置不是均分状态，恢复为均分状态，并检查是否有未发生变化的分配。
                    self.cache_perturbed = even_list
                    sign = -1
                    while check_list_has_equal(self.cache_perturbed, cache_arm)[0]:
                        equal_one = check_list_has_equal(self.cache_perturbed, cache_arm)[1]
                        select_one = (equal_one + 1)%self.num_apps

                        self.cache_perturbed[equal_one] += sign * 4
                        self.cache_perturbed[select_one] -= sign * 4
                        sign *= -1
                             
                # print('origin arm is {}', str(cache_arm))
                # print('perturbed arm is {}', str(self.cache_perturbed))
                self.cache_history_arm.append(cache_arm)
                self.cache_hitrate.append(reward[0])
            elif self.times == 6:
                #记录第二次的信息，并进行动态规划求解
                self.cache_history_arm.append(cache_arm)
                self.cache_hitrate.append(reward[0])
                # 通过动态规划直接求解应划分的缓存大小, 保存在self.estimate
                self.estimate = cache_estimate(self.cache_history_arm, self.cache_hitrate, self.n_cache)

                # print('cache_history_arm is {}', str(self.cache_history_arm))
                # print('cache_hitrate is {}', str(self.cache_hitrate))
                # print('estimate arm is: {}', str(self.estimate))
                schedule_finish = True
                
        return schedule_finish, workload_change

    def get_now_reward(self, performance, context_info=None):
        # update the context
        tmp = [list(row) for row in zip_longest(*context_info, fillvalue=None)]
        sum_context = np.zeros(self.n_features)
        for i, app in enumerate(self.all_apps):
            self.context[app] = tmp[i]
            sum_context += np.array(self.context[app])
        for app in self.all_apps:
            self.other_context[app] = list((sum_context - np.array(self.context[app])) / (len(self.all_apps) - 1))

        # calculate the reward for hitrate etc bigger is greater
        # th_reward = sum(float(x) for x in performance) / len(performance)
        # return th_reward

        # smaller is greater
        aver_latency = sum(float(x) for x in performance) / len(performance)
        th_reward = 400 / (400 + aver_latency)
        return th_reward, aver_latency

    def reset(self):
        self.alpha = self.init_factor[0]
        self.factor_alpha = self.init_factor[1]

        # 采样相关
        self.times = 0
        self.sampling_model = True
        self.sample_times = config.SAMPLE_TIMES
        self.ratio = config.SAMPLE_RATIO
        self.cpu_sample_config = None
        self.bandwidth_sample_config = None
        # cache和bandwidth指标为尾延迟，cpu评价指标为cpu利用率，分开保存
        self.c_b_sample_result = {}
        self.cpu_sample_result = {}

        # 近似收敛相关
        self.curr_best_config = None
        self.curr_best_reward = None
        self.duration_period = 0
        self.patience = config.PATIENCE
        self.epsilon = config.EPSILON

        # # 负载变化相关，未更新
        self.load_change_threshold = config.LOAD_CHANGE_THRESHOLD
        self.history_reward_window = config.HISTORY_REWARD_WINDOW
        self.history_reward = []

        # 每一维度的资源单独维度一组bandit
        # self.A_cpu = {}
        # self.b_cpu = {}
        # self.p_cpu_t = {}
        # for app in self.all_apps:
        #     self.A_cpu[app] = np.zeros((self.cpu_n_arms, self.n_features * 2, self.n_features * 2))
        #     self.b_cpu[app] = np.zeros((self.cpu_n_arms, self.n_features * 2, 1))
        #     self.p_cpu_t[app] = np.zeros(self.cpu_n_arms)
        #     for arm in range(self.cpu_n_arms):
        #         self.A_cpu[app][arm] = np.eye(self.n_features * 2)

        # self.A_bandwidth = {}
        # self.b_bandwidth = {}
        # self.p_bandwidth_t = {}
        # for app in self.all_apps:
        #     self.A_bandwidth[app] = np.zeros((self.bandwidth_n_arms, self.n_features * 2, self.n_features * 2))
        #     self.b_bandwidth[app] = np.zeros((self.bandwidth_n_arms, self.n_features * 2, 1))
        #     self.p_bandwidth_t[app] = np.zeros(self.bandwidth_n_arms)
        #     for arm in range(self.bandwidth_n_arms):
        #         self.A_bandwidth[app][arm] = np.eye(self.n_features * 2)

        # 上下文维护
        self.context = {}
        self.other_context = {}
        sum = np.zeros(self.n_features)
        for app in self.all_apps:
            self.context[app] = [1.0 for _ in range(self.n_features)]
            sum += np.array(self.context[app])
        for app in self.all_apps:
            self.other_context[app] = list((sum - np.array(self.context[app])) / (len(self.all_apps) - 1))

        # 超立方采样
        if self.sampling_model:
            # self.cache_sample_config =      latin_hypercube_sampling(self.sample_times, self.num_apps, 0, self.cache_n_arms - 1, self.ratio)
            self.cpu_sample_config =        latin_hypercube_sampling(self.sample_times, self.num_apps, 1, self.cpu_n_arms - 1, 1)
            self.bandwidth_sample_config =  latin_hypercube_sampling(self.sample_times, self.num_apps, 1, self.bandwidth_n_arms - 1, 1)
        
        # 针对cache资源的单独判定
        self.cache_mode = True
        self.cache_perturbed = None
        self.cache_history_arm = []
        self.cache_hitrate = []
        self.estimate = []


    def save_to_pickle(self, filename):
        with open(filename, 'wb') as f:
            pickle.dump(self, f)

    @staticmethod
    def load_from_pickle(filename):
        with open(filename, 'rb') as f:
            return pickle.load(f)


if __name__ == '__main__':
    pass
