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

TOTAL_CACHE_SIZE = 160
def sigmoid(x, a, b, c):
        """定义 Sigmoid 函数"""
        if a == 0:
            return np.zeros_like(x) 
        return a / (1 + np.exp(-b * (x - c)))

def exponential(x, a , b):
    """命中率为指数函数时"""
    res = a * (1 - torch.exp(-b * (x - 0)))
    res = torch.minimum(res, torch.tensor(1))  # 将结果限制在最大值 1
    return res

def exp_growth(params, x1, y1, x2, y2):
    '''
    params : 初始猜想解
    '''
    A, B = params
    eq1 = A * (1 - np.exp(-B * x1)) - y1
    eq2 = A * (1 - np.exp(-B * x2)) - y2
    return [eq1, eq2]

class Env:
    def __init__(self, first, second, simulate_num = 4, num_tasks = 10, train_batch_size = 10, validate_batch_size = 1):
        '''
        simulate_num : 模拟曲线量
        根据采样的两个点预测分配与曲线之间的关系
        first/second传入参数形如：[[A_cache,B_cahce,C_cache],[A_hitrate,B_hitrate,C_hitrate]]
        '''
        assert len(first[0])==len(second[0]), "任务数量不一致"    
        self.train_batch_size = train_batch_size    # 训练批次
        self.num_tasks =num_tasks                   # 当前任务个数
        self.max_num_tasks = 25                     # [5,25]
        self.total_list_hit_rate = []       # 存储了来自真实数据采样和变形得到的曲线特征，即[a,b,c]
        # 用于生成类似曲线用
        self.b_min = float('inf')
        self.b_max = float('-inf')
        self.a_min = float('inf')
        self.a_max = float('-inf')
        # print(len(first))
        # 前num_tasks为真实采样数据
        for i in range(len(first[0])):
            # 分别获取两个点的 cache 和 hitrate 值
            cache1, hitrate1 = first[0][i], first[1][i]
            cache2, hitrate2 = second[0][i], second[1][i]
            # 确保cache值不同，以避免除零 -> predication_model.py
            assert cache1 != cache2, f"任务 {i} 的两个点的 cache 值不能相等"
            # 1.D_SEQUENTIAL分布，采样到的点total_list_hit_rate都为0
            if hitrate2 == 0 :
                self.total_list_hit_rate.append([0,0])
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
            self.a_max = max(self.a_max, a)
            self.a_min = min(self.a_min, a)
            self.b_max = max(self.b_max, b)
            self.b_min = min(self.b_min, b)
            self.total_list_hit_rate.append([a,b])
        
        # 生成模拟数据
        for i in range(len(first[0]) * simulate_num):
            a = self.total_list_hit_rate[i % len(first[0])][0]
            b = self.total_list_hit_rate[i % len(first[0])][1]
            a_new = a * (1 + np.random.uniform(-0.1, 0.1))
            b_new = b * (1 + np.random.uniform(-0.1, 0.1))
            self.total_list_hit_rate.append([a_new,b_new])
        # print(self.total_list_hit_rate)
        self.all_lines, _ = self.gen_total_lines()         # 存储所有的total_list_hit_rate对应的line的128个值
        # 划分训练集和测试集，同时记录了其采样下标
        self.train_dataset = self.gen_dataset(size_dataset=train_batch_size, num_tasks = num_tasks, is_train=True)
        self.validate_dataset = self.gen_dataset(size_dataset=validate_batch_size, num_tasks = num_tasks, is_train=False)
        print("train_dataset batch: ", len(self.train_dataset[0]))
        # print(self.train_dataset[1],'\n=============================')
        print("validate_dataset batch: ", len(self.validate_dataset[0]))
        # print(self.validate_dataset[1],'\n=============================')
    
    def gen_total_lines(self):
        '''生成所有的曲线'''
        random_samples = torch.linspace(0, 1, 128) * TOTAL_CACHE_SIZE       # 随机生成128个浮点数,范围在0到TOTAL_CACHE_SIZE之间
        x, _ = torch.sort(random_samples)

        y_list = []
        index_list = []
        for line_index in range(len(self.total_list_hit_rate)):
            y = self.predict_hitrate(x, self.total_list_hit_rate[line_index])
            y_list.append(y)
            index_list.append(torch.tensor([line_index]))
        
        y = torch.stack(y_list, dim=0)
        index = torch.stack(index_list, dim=0)
        # 找到曲线采样矩阵中的最小值和最大值
        min_val = torch.min(y)
        max_val = torch.max(y)
        # 缩放矩阵到[0,1]范围内
        y = (y - min_val) / (max_val - min_val)

        return y, index
        
    def gen_dataset(self, size_dataset, num_tasks, is_train=True):
        '''生成 size_dataset 个 num_tasks 个任务的数据集
        return : tensor,tensor
        '''
        if is_train:
            dataset_state = []
            task_state_index = []       # 所有组的num_tasks任务状态下标
            for batch in range(size_dataset):
                # 随机选择num_tasks个任务为一组曲线
                choose_index = random.choices(range(len(self.total_list_hit_rate)), k=num_tasks)    # 选择的曲线下标

                # y = self.prediction_line(sorted_samples_x, self.prediction_line[choose_index].unsqueeze(1).repeat(1, 128))
                choose_state = torch.stack([self.all_lines[i] for i in choose_index])  
                
                task_state_index.append(choose_index)
                dataset_state.append(choose_state)

            dataset_state = torch.stack(dataset_state, dim=0)
            task_state_index = torch.tensor(task_state_index, dtype=torch.int64)
            return dataset_state,task_state_index
        else:
            dataset_state = []
            task_state_index = []       # 所有组的num_tasks任务状态下标
            for batch in range(size_dataset):
                choose_index = [i for i in range(num_tasks)]
                choose_state = torch.stack([self.all_lines[i] for i in choose_index])  
                task_state_index.append(choose_index)
                dataset_state.append(choose_state)
            dataset_state = torch.stack(dataset_state, dim=0)
            task_state_index = torch.tensor(task_state_index, dtype=torch.int64)
            return dataset_state,task_state_index
    
    def update_train_dataset(self):
        '''更新一批训练集'''
        self.train_dataset = self.gen_instances(size_dataset=self.train_batch_size, num_tasks=self.num_tasks)
        print("train_dataset : ")
        print(self.train_dataset[0])
        print(self.train_dataset[1],'\n=============================')
    
    def predict_hitrate(self, cache_size, action_hitrate_index):
        """
        传入一组分配的 cache 大小和对应预测曲线的参数 abc，返回在所分配 cache 大小下的命中率。
        :param cache_size: 分配的 cache 大小，可以是标量或一组值 (NumPy 数组或 PyTorch 张量)
        :param action_hitrate_index: 一条曲线
        :return: 计算得到的命中率，与 cache_size 的形状一致
        """
        # res = sigmoid(cache_size, action_hitrate_index[0],action_hitrate_index[1],action_hitrate_index[2])
        res = exponential(cache_size, action_hitrate_index[0],action_hitrate_index[1])
        return res

    def compete_hitrate(self, action, action_hitrate_index, validate_compute = False):
        """
        计算当前动作下的命中率，并返回一个列表，列表中包含所有任务的命中率
        :param action: 当前动作，是一个列表，长度为任务数，每个元素表示当前任务的缓存大小
        :param action_hitrate_index: 对应预测曲线的下标，长度为任务数，每个元素为该任务所对应的特征曲线
        :return: 计算得到的命中率，与 action 的形状一致
        """
        if len(action) != len(action_hitrate_index):
            print("action : ", len(action))
            print("action : ", action)
            print("action_hitrate_index : ", len(action_hitrate_index))
            print("action_hitrate_index : ", action_hitrate_index)
            raise ValueError("action 和 action_hitrate_index 的长度必须相等！")
        list_total_hitrate = []
        for i in range(len(action)):
            res = self.predict_hitrate(action[i], self.total_list_hit_rate[action_hitrate_index[i]])
            list_total_hitrate.append(res)
        # if validate_compute:
            # print("action : ", action)
            # print("action_hitrate_index : ", action_hitrate_index)
            # print(' hitrate : ', list_total_hitrate)
        return torch.tensor(list_total_hitrate, dtype=torch.float64)
    
    def step(self, action, index):
        '''根据action计算返回reward，单步决策，无state
        index : 对应训练曲线的索引，存在self.traindataset[1]中
        '''
        if action.dim() > 1:
            batch_size, num_task = action.size()
        else:
            num_task = action.size(0)
            batch_size = 1
        allocate_size = action * TOTAL_CACHE_SIZE    # 总共可以分的CACHE量
        # print('func step batch size : ', batch_size, 'num_task : ', num_task, ', action[0] is ', action[0])
        
        # 减少分配空间，曲线上探
        sub_allocate_size = allocate_size - 128 * 0.05
        sub_allocate_size = torch.where(sub_allocate_size<0., torch.tensor(0.), sub_allocate_size)

        # 增大分配空间，曲线下探
        add_allocate_size = allocate_size + 128 * 0.05
        add_allocate_size = torch.where(add_allocate_size > 128.,torch.tensor(128.), add_allocate_size)
        
        list_hitrate = []
        list_sub_allocate_hitrate = []
        list_add_allocate_hitrate = []

        for batch_idx in range(batch_size):         # 对这一批数据计算
            hitrate_line = self.compete_hitrate(allocate_size[batch_idx], index[batch_idx])
            sub_allocate_hitrate = self.compete_hitrate(sub_allocate_size[batch_idx], index[batch_idx])
            add_allocate_hitrate = self.compete_hitrate(add_allocate_size[batch_idx], index[batch_idx])

            list_hitrate.append(hitrate_line)
            list_sub_allocate_hitrate.append(sub_allocate_hitrate)            
            list_add_allocate_hitrate.append(add_allocate_hitrate)
        
        # print("curr here")
        list_hitrate = torch.stack(list_hitrate, dim=0)
        list_sub_allocate_hitrate = torch.stack(list_sub_allocate_hitrate, dim=0)
        list_add_allocate_hitrate = torch.stack(list_add_allocate_hitrate, dim=0)

        # 减少分配时，降低的命中率
        sub_hitrate = list_hitrate - list_sub_allocate_hitrate
        # 增加分配时候提高的命中率
        add_hitrate = list_add_allocate_hitrate - list_hitrate

        # 如果存在小于0，改为0
        sub_hitrate = torch.where(sub_hitrate < 0., torch.tensor(0.), sub_hitrate)
        add_hitrate = torch.where(add_hitrate < 0., torch.tensor(0.), add_hitrate)

        # n个task的平均命中率
        mean_list_hitrate = torch.mean(list_hitrate, dim=-1,keepdim=True)

        # 奖励由命中率直接驱动，命中率越高，奖励越高
        # reward_hitrate = mean_list_hitrate / (list_hitrate + 1e-9)
        reward_hitrate = list_hitrate / (mean_list_hitrate + 1e-9)

        # 当增大相同分配大小时，相较于平均增大的命中率，任务增大的命中率越大，说明应该增大该任务的分配大小，该奖励越大
        mean_add_hitrate = torch.mean(add_hitrate, dim=-1, keepdim=True)
        reward_add_hitrate = add_hitrate / (mean_add_hitrate + 1e-9)

        # 当减少相同分配大小时，如果命中率下降得多，说明分配的重要性高，奖励越大
        mean_sub_hitrate = torch.mean(sub_hitrate, dim=-1,keepdim=True)
        reward_sub_hitrate =  sub_hitrate / (mean_sub_hitrate + 1e-9)

        # 奖励由三部分组成,当前分配比例下延迟小，如果增大分配大小命中率增大的多，如果减少分配大小命中率降低的多
        reward = (2.0 * reward_hitrate) + (0.5 * reward_add_hitrate) + (0.5 * reward_sub_hitrate)

        reward = (reward - reward.mean()) / (reward.std() + 1e-9)

        # 无下一个状态
        state = None
        # 一次即结束
        done = None
        return state, reward, done
    

if __name__ == '__main__':
    point1=[[16,        16,     16,         16,     16,         16,     16,   16,            16,      16],
            [0.0001 , 0.8601,  0.2398,  0.4711,  0.29215,   0.26915,  0.7311,  0,        0.5329,  0.2957]]
    point2=[[20,        20,     10,         21,     12,         14,     17,     9,           17,     20],
            [0,       0.9324,  0.0447,  0.7632,  0.297075,  0.1392,   0.7185,  0,        0.7132,  0.3795]]
    env=Env(point1,point2, simulate_num=4)

    # 训练
    # point3=[[20, 20, 10, 21, 12, 14, 17, 9,  17, 20],[0.385,   0,       0.8063,  0.0508,  0.804575,  0.2685,   0,       0.63,     0,       0.724]]
    # point4=[[18, 18, 12, 24, 16, 9,  18, 14, 16, 15],[0.3415,  0,       0.8841,  0.0003, 0.82305,   0.17145,  0,       0.6912,   0,       0.5854]]
    # env=Env(point3,point4)
    total_list_hit_rate = env.total_list_hit_rate
    # print(total_list_hit_rate)
    x = torch.linspace(0, 30, 100)
    print("validate_dataset is ",env.validate_dataset[1][0])
    for i in range(len(env.validate_dataset[1][0])):
        # print(f'line {i + 1} a = {env.total_list_hit_rate[env.validate_dataset[1][0][i]][0]} b = {env.total_list_hit_rate[env.validate_dataset[1][0][i]][1]}')
        # line = exponential(x, env.total_list_hit_rate[i][0], env.total_list_hit_rate[i][1])
        line = exponential(x, env.total_list_hit_rate[env.validate_dataset[1][0][i]][0], env.total_list_hit_rate[env.validate_dataset[1][0][i]][1])
        plt.plot(x, line, label='line' + str(i + 1))
    plt.legend()
    plt.axvline(x=16, color='red', linestyle='--', linewidth=2, label='x = 16')

    # plt.scatter(point1[0], point1[1], color='blue', label='point1', zorder=1)
    # plt.scatter(point2[0], point2[1], color='red', label='point2', zorder=1)
    # plt.axhline(y = 0.86, color='green', linestyle='--', linewidth=2, label='y = 16')
    plt.title("Function Mapping")
    plt.xlabel("Original Values")
    plt.ylabel("Mapped Values (0-1)")
    plt.savefig('figures/exponential.png')

    
