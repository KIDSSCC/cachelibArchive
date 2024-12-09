'''
SHMCachelib/log
99%尾延迟 平均延迟 总用时 吞吐量 命中率
'''

import copy
import torch
import torch.nn.functional as F
from torch.utils.data import DataLoader, TensorDataset
import matplotlib.pyplot as plt
import numpy as np
import random
from torch.utils.data import random_split
from agent import *
from scipy.optimize import curve_fit,fsolve
import re

# 0:Sequential 1:Hotspot 2:Uniform 3:D_ZIPFIAN
# TOTAL_RESOURCE = 1024 64MB一划分
database_phase_line = [[[0, 256], [1, 64], [1, 32]],    # leveldb
                       [[3, 16], [0, 256], [1, 64]],
                       [[1, 32], [1, 32], [3, 16]],
                       [[0, 256], [1, 32], [3, 16]],
                       [[0, 256], [1, 32], [3, 16]],
                       [[0, 256], [2, 64], [0, 256]],    # mongodb
                       [[2, 16], [0, 256], [2, 64]],
                       [[3, 32], [2, 16], [0, 256]],
                       [[0, 256], [3, 32], [2, 16]],
                       [[1, 64], [0, 256], [3, 32]],
                       [[0, 256], [1, 64], [0, 256]],    # mysql
                       [[1, 16], [0, 256], [1, 64]],
                       [[2, 32], [1, 16], [0, 256]],
                       [[0, 256], [2, 32], [1, 16]],
                       [[3, 64], [0, 256], [2, 32]],
                       [[0, 256], [3, 64], [0, 256]],    # sqlite
                       [[3, 16], [0, 256], [3, 64]],
                       [[1, 32], [3, 16], [0, 256]],
                       [[0, 256], [1, 32], [3, 16]],
                       [[2, 64], [0, 256], [1, 32]],
                       [[0, 256], [2, 64], [0, 256]],    # tmdb
                       [[2, 16], [0, 256], [2, 64]],
                       [[3, 32], [2, 16], [0, 256]],
                       [[0, 256], [3, 32], [2, 16]],
                       [[1, 64], [0, 256], [3, 32]]]

OffLine_Sample_Analyzed_Path_Cache = '/home/md/SHMCachelib/schedule/ReinforcementLearning/Offline_Sample/20241207_152445_cache_analyzed.log'
OffLine_Sample_Analyzed_Path_Bw = '/home/md/SHMCachelib/schedule/ReinforcementLearning/Offline_Sample/20241207_152445_bw_analyzed.log'

class Env:
    def __init__(self, num_tasks, TOTAL_RESOURCE, train_batch_size = 5, validate_batch_size = 1, Offline_Sample_Analyzed_Path = '', Train_Opt = 0):
        '''
        num_tasks : 任务数量，特征曲线已有
        TOTAL_RESOURCE : 总资源量
        Offline_Sample_Result_Path : 真实采样数据
        Train_Opt : 当前训练目标资源， 0为Cache-hitrate;1为bandwidth-latency
        '''
        self.train_batch_size = train_batch_size    # 训练批次
        self.validate_batch_size = validate_batch_size
        self.num_tasks =num_tasks                   # 当前任务个数
        self.max_num_tasks = 25                     # [5,25] 好像用不到
        self.Train_Opt = Train_Opt
        self.total_list_hit_rate = []       # 存储了来自真实数据采样和变形得到的曲线特征，即[类型，对应参数]
        self.TOTAL_RESOURCE = TOTAL_RESOURCE
        self.Offline_Sample_Analyzed_Path = Offline_Sample_Analyzed_Path    
        self.TotalFeatures = []    
        
        # 读取离线采样文件，获取各个任务不同状态下的各个曲线
        self.ReadfileAndGenTotalLines()     # self.TotalFeatures应当为 25 * 3 * (3 or 4)，即每个元素为[task, phase, A, B]
        self.Total_Lines, self.Total_task_phase = self.GenTotalLines()      # self.Total_Lines应该是25 * 3 * 128
        self.train_dataset = self.GenDataset(size_dataset=train_batch_size, num_tasks = num_tasks)
        self.validate_dataset = self.GenDataset(size_dataset=validate_batch_size, num_tasks = num_tasks)

    def ReadfileAndGenTotalLines(self):
        '''读取离线采样分析好的文件，记录各个值'''
        if self.Train_Opt == 0:         # Cache - hitrate
            pattern = r'Task (\d+) in Phase (\d+) Cache-hitrate Simulate Feature : k = ([0-9.]+)'
            task_phase_features = []
            with open(self.Offline_Sample_Analyzed_Path, 'r') as file:
                for line in file:
                    match = re.search(pattern, line)  # 匹配行
                    if match:
                        task = int(match.group(1))  # 获取 Task
                        phase = int(match.group(2))  # 获取 Phase
                        k = float(match.group(3))  # 获取 A
                        task_phase_features.append([task, phase, k])
                    if len(task_phase_features) == 3:
                        self.TotalFeatures.append(task_phase_features)  # self.TotalFeatures为 25 * 3的矩阵，每个元素为[task, phase, k]
                        task_phase_features = []
            # print(len(self.TotalFeatures))  
            # print(len(self.TotalFeatures[0]))
            # print(len(self.TotalFeatures[0][0]))
        elif self.Train_Opt == 1:       # bandwidth - latency
            pattern = r'Task (\d+) in Phase (\d+) Bandwidth-latency Simulate Feature : A=([\d\.]+),B=([-+]?\d*\.\d+|\d+)'
            task_phase_features = []
            with open(self.Offline_Sample_Analyzed_Path, 'r') as file:
                for line in file:
                    match = re.search(pattern, line)  # 匹配行
                    if match:
                        task = int(match.group(1))  # 获取 Task
                        phase = int(match.group(2))  # 获取 Phase
                        A = float(match.group(3))  # 获取 A
                        B = float(match.group(4))  # 获取 B
                        task_phase_features.append([task, phase, A, B])
                    if len(task_phase_features) == 3:
                        self.TotalFeatures.append(task_phase_features)  # self.TotalFeatures为 25 * 3的矩阵，每个元素为[task, phase, A, B]
                        task_phase_features = []
            # print(len(self.TotalFeatures))  
            # print(len(self.TotalFeatures[0]))
            # print(len(self.TotalFeatures[0][0]))
    
    def GenTotalLines(self):
        '''生成所有的曲线，一共应当是25 * 3个'''
        random_samples = torch.linspace(0, 1, 128) * self.TOTAL_RESOURCE       # 随机生成128个浮点数,范围在0到TOTAL_RESOURCE之间
        x, _ = torch.sort(random_samples)
        y_list = []                 # 存储了所有任务所有状态的曲线
        index_list = []             # 每个曲线对应在self.TotalFeatures中的index
        for task_index in range(len(self.TotalFeatures)):
            task_phase_lines=[]
            task_phase_index = []
            for phase_index in range(len(self.TotalFeatures[0])):
                y = self.PredictReward(x, self.TotalFeatures[task_index][phase_index])
                # print(y.shape)
                task_phase_lines.append(y)
                task_phase_index.append(torch.tensor([task_index, phase_index]))
                # print(task_phase_lines.shape)
            # print('task_phase_lines', task_phase_lines)         # 3 * 128
            # print('task_phase_index', task_phase_index)         # [tensor([24,  0]), tensor([24,  1]), tensor([24,  2])]
            # print('task_phase_lines.shape',task_phase_lines.shape)
            y_list.append(torch.stack(task_phase_lines, dim=0))
            index_list.append(torch.stack(task_phase_index, dim=0))
        y = torch.stack(y_list, dim=0)          # 25 * 3 * 128 ->cache ; 25 * 3 * 128 ->bandwidth
        # print('y.shape', y.shape)
        index = torch.stack(index_list, dim=0)      # 25 * 3 * 2 ->cache ; 25 * 3 * 2 ->bandwidth
        # print('index.shape', index.shape)
        return y, index
    
    def PredictReward(self, x, features):
        '''计算reward'''
        if self.Train_Opt == 0:         # cache - hitrate 斜线
            res = features[2] * x
            res = torch.minimum(res, torch.tensor(1))  # hitrate <= 1
            return res
        elif self.Train_Opt == 1:       # bandwidth - latency
            res = features[2] * (torch.exp(-features[3] * x))
            res = torch.maximum(res, torch.tensor(0))
            return res                                  # latency >= 0
    
    def GenDataset(self, size_dataset, num_tasks):
        '''生成 size_dataset 个 num_tasks 个任务的数据集'''
        dataset_state = []
        task_state_index = []
        for _ in range(size_dataset):
            # 随机选择num_tasks个任务为一组曲线
            choose_task_index = random.sample(range(len(self.TotalFeatures)), k=num_tasks)       # 从25个任务中随机挑选num_tasks个任务
            choose_phase_index = random.randint(0, len(self.TotalFeatures[0]) - 1)          # 随机选择一个状态
            choosed_line = torch.stack([self.Total_Lines[i][choose_phase_index] for i in choose_task_index])
            task_state_index.append([[i,choose_phase_index] for i in choose_task_index])
            dataset_state.append(choosed_line)
        dataset_state = torch.stack(dataset_state, dim=0)       # batch_size * numtasks * 128
        # print(dataset_state)
        task_state_index = torch.tensor(task_state_index, dtype=torch.int64)        # batch_size * numtasks * 2
        # print(dataset_state.shape, task_state_index.shape)
        '''
        dataset_state = size_dataset * num_tasks * 128
        task_state_index = size_dataset * num_tasks * 2         2:[task_index, phase_index]
        '''
        return dataset_state,task_state_index
    
    def update_train_dataset(self):
        '''更新一批训练集'''
        self.train_dataset = self.GenDataset(size_dataset=self.train_batch_size, num_tasks = self.num_tasks)
        print("================================ train_dataset changed ================================ ")
        # print(self.train_dataset[0])
        # print(self.train_dataset[1],'\n=============================')

    def CompeteReward(self, action, action_reward_index):
        '''
        action为[ , ,……,  ]
        action_reward_index为当前任务曲线在self.Total_task_phase中的坐标 eg:[0, 4]'''
        if len(action) != len(action_reward_index):
            print("action : ", len(action))
            print("action : ", action)
            print("action_reward_index : ", len(action_reward_index))
            print("action_reward_index : ", action_reward_index)
            raise ValueError("action 和 action_reward_index 的长度必须相等！")
        list_total_reward = []
        # print("action_reward_index : ", action_reward_index)
        # print("action_reward_index[0][0].item()", action_reward_index[0][0].item())
        # print("action_reward_index[0][1].item()", action_reward_index[0][1].item())
        for i in range(len(action)):
            res = self.PredictReward(action[i], self.TotalFeatures[action_reward_index[i][0].item()][action_reward_index[i][1].item()] )
            list_total_reward.append(res)
        return torch.tensor(list_total_reward, dtype=torch.float64)
    
    def step(self, action, index):
        '''根据action计算返回reward，单步决策，无state'''
        if action.dim() > 1:
            batch_size, num_task = action.size()
            '''
            action = [  [allocations1, allocations2,……, allocations n],
                        [allocations1, allocations2,……, allocations n],
                        [allocations1, allocations2,……, allocations n],
                        [allocations1, allocations2,……, allocations n],
                        ……,
                        [allocations1, allocations2,……, allocations n],]
            index = [   [[0,0],[1,0],……, [24,0]],这一批选择了0~24号任务中
                        [[0,0],[1,0],……, [24,0]],
                        [[0,0],[1,0],……, [24,0]],
                        ……
                        [[0,0],[1,0],……, [24,0]],]任务特征值
            '''
        else:
            num_task = action.size(0)
            batch_size = 1
        # Cache划分
        if self.Train_Opt == 0:       
            allocate_size = action * self.TOTAL_RESOURCE
            # 减少分配空间
            sub_allocate_size = allocate_size - self.TOTAL_RESOURCE * 0.05
            sub_allocate_size = torch.where(sub_allocate_size < 0., torch.tensor(0.), sub_allocate_size)
            # 增大分配空间
            add_allocate_size = allocate_size + self.TOTAL_RESOURCE * 0.05
            add_allocate_size = torch.where(add_allocate_size > self.TOTAL_RESOURCE, torch.tensor(self.TOTAL_RESOURCE), add_allocate_size)
            list_hitrate = []
            list_sub_allocate_hitrate = []
            list_add_allocate_hitrate = []
            for batch_idx in range(batch_size):
                hitrate_line = self.CompeteReward(allocate_size[batch_idx], index[batch_idx])
                sub_allocate_hitrate = self.CompeteReward(sub_allocate_size[batch_idx], index[batch_idx])
                add_allocate_hitrate = self.CompeteReward(add_allocate_size[batch_idx], index[batch_idx])
                list_hitrate.append(hitrate_line)
                list_sub_allocate_hitrate.append(sub_allocate_hitrate)
                list_add_allocate_hitrate.append(add_allocate_hitrate)
            list_hitrate = torch.stack(list_hitrate, dim=0)
            list_sub_allocate_hitrate = torch.stack(list_sub_allocate_hitrate, dim=0)
            list_add_allocate_hitrate = torch.stack(list_add_allocate_hitrate, dim=0)
            sub_hitrate = list_hitrate - list_sub_allocate_hitrate
            add_hitrate = list_add_allocate_hitrate - list_hitrate
            sub_hitrate = torch.where(sub_hitrate < 0., torch.tensor(0.), sub_hitrate)
            add_hitrate = torch.where(add_hitrate < 0., torch.tensor(0.), add_hitrate)
            # 优先满足命中率接近1的任务
            is_near_one = list_hitrate >= 0.99
            reward_near_one = is_near_one.float() * 10.0
            # 剩余任务的平均命中率
            remaining_hitrate = list_hitrate * (~is_near_one).float()
            mean_remaining_hitrate = torch.mean(remaining_hitrate, dim=-1, keepdim=True)
            reward_mean_hitrate = remaining_hitrate / (mean_remaining_hitrate + 1e-9)
            # 综合奖励
            reward = reward_near_one + reward_mean_hitrate
            reward = (reward - reward.mean()) / (reward.std() + 1e-9)
            state = None
            done = None
            return state, reward, done
        # 带宽划分：
        if self.Train_Opt == 1:       
            allocate_size = action * self.TOTAL_RESOURCE
            # 减少分配空间
            sub_allocate_size = allocate_size - self.TOTAL_RESOURCE * 0.05
            sub_allocate_size = torch.where(sub_allocate_size < 0., torch.tensor(0.), sub_allocate_size)
            # 增大分配空间
            add_allocate_size = allocate_size + self.TOTAL_RESOURCE * 0.05
            add_allocate_size = torch.where(add_allocate_size > self.TOTAL_RESOURCE, torch.tensor(self.TOTAL_RESOURCE), add_allocate_size)
            list_delay = []
            list_sub_allocate_delay = []
            list_add_allocate_delay = []
            for batch_idx in range(batch_size):
                delay = self.CompeteReward(allocate_size[batch_idx], index[batch_idx])
                sub_allocate_delay = self.CompeteReward(sub_allocate_size[batch_idx], index[batch_idx])
                add_allocate_delay = self.CompeteReward(add_allocate_size[batch_idx], index[batch_idx])
                list_delay.append(delay)
                list_sub_allocate_delay.append(sub_allocate_delay)
                list_add_allocate_delay.append(add_allocate_delay)
            # [batch,num_task]
            list_delay = torch.stack(list_delay, dim=0)
            list_sub_allocate_delay = torch.stack(list_sub_allocate_delay, dim=0)
            list_add_allocate_delay = torch.stack(list_add_allocate_delay, dim=0)
            # 减少分配时，增加的延迟
            add_delay = list_sub_allocate_delay - list_delay
            # 增大分配时，减小的延迟
            sub_delay = list_delay - list_add_allocate_delay
            # 如果存在小于0，改为0
            add_delay = torch.where(add_delay < 0., torch.tensor(0.), add_delay)
            sub_delay = torch.where(sub_delay < 0., torch.tensor(0.), sub_delay)
            # n个task的平均延迟
            mean_list_delay = torch.mean(list_delay, dim=-1,keepdim=True)
            # 平均延迟 / 每个任务的延迟 : 相较于平均延迟，任务延迟越小该奖励越大
            reward_delay = mean_list_delay/ (list_delay + 1e-9)
            # 当减小相同分配大小时，相较于平均增大的延迟，任务增加的延迟越大，说明不应该减少该任务的分配大小，该奖励越大
            mean_add_delay = torch.mean(add_delay, dim=-1,keepdim=True)
            reward_add_delay = add_delay / (mean_add_delay + 1e-9)
            # 当增大相同分配大小时，相较于平均减少的延迟，任务减少的延迟越大，说明应该增大该任务的分配大小，该奖励越大
            mean_sub_delay = torch.mean(sub_delay, dim=-1,keepdim=True)
            reward_sub_delay =  sub_delay / (mean_sub_delay + 1e-9)
            # 奖励由三部分组成,当前分配比例下延迟小，如果增大分配大小延迟减少的多，如果减少分配大小延迟增大的多
            reward = (1.0 * reward_delay) + (0.5 * reward_add_delay) + (0.5 * reward_sub_delay)
            reward = (reward - reward.mean()) / (reward.std() + 1e-9)
            # 无下一个状态
            state = None
            # 一次即结束
            done = None
            return state, reward, done
            

    

if __name__ == '__main__':
    env = Env(num_tasks=10, TOTAL_RESOURCE=160, Offline_Sample_Analyzed_Path=OffLine_Sample_Analyzed_Path_Bw, Train_Opt=1)

    # x = torch.linspace(0, 70, 100)
    # line = log_func(x, 1, (math.e - 1) / 32)
    # plt.plot(x, line, label='line')
    # plt.legend()
    # plt.axvline(x=32, color='red', linestyle='--', linewidth=2, label='x = 32')
    # plt.title("Function Mapping")
    # plt.xlabel("Original Values")
    # plt.ylabel("Mapped Values (0-1)")
    # plt.savefig('figures/Simulated_temp.png')

    
