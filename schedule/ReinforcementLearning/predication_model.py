import pickle
import random
import math
import sys
from itertools import zip_longest
from ScheduleFrame import *
from typing import List

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
        with open(filename, 'wb') as f:
            pickle.dump(self, f)

    @staticmethod
    def load_from_pickle(filename):
        with open(filename, 'rb') as f:
            return pickle.load(f)


if __name__ == '__main__':
    mylist = [1, 2, 3, 4, 5, 6]
    mylist2 = [1, 2, 3, 4, 5]
    print(mylist2[mylist.index(max(mylist))])
    pass
