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

TOTAL_CACHE_SIZE = 160
def sigmoid(x, a, b, c):
        """定义 Sigmoid 函数"""
        return a / (1 + np.exp(-b * (x - c)))

class Env:
    def __init__(self, first, second, num_tasks = 10, train_batch_size = 4096, validate_batch_size = 100):
        '''
        根据采样的两个点预测分配与曲线之间的关系
        first/second传入参数形如：[[A_cache,B_cahce,C_cache],[A_hitrate,B_hitrate,C_hitrate]]
        '''
        assert len(first[0])==len(second[0]), "任务数量不一致"    
        self.train_batch_size = train_batch_size    # 训练批次
        self.num_tasks =num_tasks                   # 当前任务个数
        self.max_num_tasks = 25                     # [5,25]
        self.list_hit_rate = []       # 存储了来自真实数据采样和变形得到的曲线特征，即[a,b,c]
        # 用于生成类似曲线用
        self.b_min = float('inf')
        self.b_max = float('-inf')
        self.c_min = float('inf')
        self.c_max = float('-inf')
        # 前num_tasks为真实采样数据
        for i in range(num_tasks):
            # 分别获取两个点的 cache 和 hitrate 值
            cache1, hitrate1 = first[0][i], first[1][i]
            cache2, hitrate2 = second[0][i], second[1][i]
            # 确保cache值不同，以避免除零 -> predication_model.py
            assert cache1 != cache2, f"任务 {i} 的两个点的 cache 值不能相等"
            # 1.D_SEQUENTIAL分布，采样到的点list_hit_rate都为0
            if hitrate2 == 0 :
                self.list_hit_rate.append([0,0,0])
                continue
            # 2.采样点不精确，增大cache后list_hit_rate反而降低，做修改
            if (cache2 > cache1 and hitrate2 < hitrate1) or (cache2 < cache1 and hitrate2 > hitrate1):
                temp = cache1
                cache1 = cache2
                cache2 = temp
            # 3.其余使用sigmoid函数进行模拟
            a = 1       # 命中率最大值为1
            c = (hitrate1 - hitrate2) / (cache1 - cache2)   # 中值
            self.c_max = max(self.c_max, c)
            self.c_min = min(self.c_min, c)
            b = (1 / (cache2 - cache1)) * np.log((a / hitrate1 - 1) / (a / hitrate2 - 1))  # 根据公式计算 b
            self.b_max = max(self.b_max, b)
            self.b_min = min(self.b_min, b)
            self.list_hit_rate.append([a,b,c])
        # 生成模拟数据
        for i in range(num_tasks * 9):
            a = 1 if random.random() < 0.9 else 0
            if a:
                b = random.uniform(self.b_min * 0.9, self.b_max * 1.1)
                c = random.uniform(self.c_min * 0.9, self.c_max * 1.1)
                self.list_hit_rate.append([a,b,c])
            else:
                b = 0
                c = 0
                self.list_hit_rate.append([a,b,c])
        
        self.all_lines = self.gen_total_lines()         # 存储所有的list_hit_rate对应的line
        # 划分训练集和测试集
        self.train_dataset = self.gen_dataset(size_dataset=train_batch_size, num_tasks = num_tasks)
        self.validate_dataset = self.gen_dataset(size_dataset=validate_batch_size, num_tasks = num_tasks)
    
    def gen_total_lines(self):
        '''生成所有的曲线'''
        random_samples = torch.linspace(0, 1, 128) * TOTAL_CACHE_SIZE       # 随机生成128个浮点数,范围在0到TOTAL_CACHE_SIZE之间
        sorted_samples_x, indices = torch.sort(random_samples)

        y_list = []
        index_list = []
        for line_index in range(len(self.list_hit_rate)):
            y = self.predict_hitrate(sorted_samples_x, self.list_hit_rate[line_index])
            y_list.append(y)
            index_list
        

    def gen_dataset(self, size_dataset, num_tasks):
        '''生成 size_dataset 个 num_tasks 个任务的数据集'''
        dataset_state = []
        task_state_index = []       # 所有组的num_tasks任务状态下标
        for batch in range(size_dataset):
            # 随机选择num_tasks个任务为一组曲线
            choose_index = random.choices(range(len(self.list_hit_rate)), k=num_tasks)    # 选择的曲线下标

            
            y = self.prediction_line(sorted_samples_x, self.prediction_line[choose_index].unsqueeze(1).repeat(1, 128))
            choose_state = torch.tensor([self.list_hit_rate[i] for i in choose_index], dtype=torch.float32)  # 转为 Tensor
            
            task_state_index.append(choose_index)
            dataset_state.append(choose_state)

        dataset_state = torch.stack(dataset_state, dim=0)
        task_state_index = torch.tensor(task_state_index, dtype=torch.int64)
        return dataset_state,task_state_index
    def update_train_dataset(self):
        '''更新一批训练集'''
        self.train_dataset = self.gen_instances(size_dataset=self.train_batch_size, num_tasks=self.num_tasks)
    
    def predict_hitrate(self, cache_size, list_hit_rate_params):
        """
        传入一组分配的 cache 大小和对应预测曲线的参数 abc，返回在所分配 cache 大小下的命中率。
        :param cache_size: 分配的 cache 大小，可以是标量或一组值 (NumPy 数组或 PyTorch 张量)
        :param list_hit_rate_params: 对应预测曲线的参数 [a, b, c]，长度为 3
        :return: 计算得到的命中率，与 cache_size 的形状一致
        """
        a = list_hit_rate_params[0]
        b = list_hit_rate_params[1]
        c = list_hit_rate_params[2]
        # 使用广播机制批量计算
        return a / (1 + np.exp(-b * (cache_size - c)))
    def compete_hitrate(self, action, list_hit_rate_params):
        """
        计算当前动作下的命中率，并返回一个列表，列表中包含所有任务的命中率，用于验证集
        :param action: 当前动作，是一个列表，长度为任务数，每个元素表示当前任务的缓存大小
        :param list_hit_rate_params: 对应预测曲线的参数 [a, b, c]，长度为 3
        :return: 计算得到的命中率，与 action 的形状一致
        """
        if action.dim() > 1:
            batch_size, num_task = action.size()
        else:
            num_task = action.size(0)
            batch_size = 1
        allocate_size = action * TOTAL_CACHE_SIZE    # 总共可以分的CACHE量
        # 记录命中率曲线
        hitrate_line = torch.zeros((batch_size, num_task))
        for batch_idx in range(batch_size):
            for task_idx in range(num_task):
                hitrate_line[batch_idx, task_idx] = self.list_hit_rate[task_idx]
        
        list_total_hitrate = []
        for batch_idx in range(batch_size):
            hitrate = self.predict_hitrate(allocate_size[batch_idx], hitrate_line[batch_idx])
            list_total_hitrate.append(hitrate)
        list_total_hitrate = torch.stack(list_total_hitrate, dim = 0)
        return list_total_hitrate
    def step(self, action, index):
        '''根据action计算返回reward，单步决策，无state
        index : 对应训练曲线的索引
        '''
        if action.dim() > 1:
            batch_size, num_task = action.size()
        else:
            num_task = action.size(0)
            batch_size = 1
        allocate_size = action * TOTAL_CACHE_SIZE    # 总共可以分的CACHE量
        # 记录命中率曲线
        hitrate_line = torch.zeros((batch_size, num_task))
        for batch_idx in range(batch_size):
            for task_idx in range(num_task):
                hitrate_line[batch_idx, task_idx] = self.list_hit_rate[task_idx]

        # 减少分配空间，曲线上探
        sub_allocate_size = allocate_size - 128 * 0.05
        sub_allocate_size = torch.where(sub_allocate_size<0., torch.tensor(0.), sub_allocate_size)

        # 增大分配空间，曲线下探
        add_allocate_size = allocate_size + 128 * 0.05
        add_allocate_size = torch.where(add_allocate_size > 128.,torch.tensor(128.), add_allocate_size)
        
        list_hitrate = []
        list_sub_allocate_hitrate = []
        list_add_allocate_hitrate = []

        for batch_idx in range(batch_size):
            hitrate = self.predict_hitrate(allocate_size[batch_idx], hitrate_line[batch_idx])
            sub_allocate_delay = self.predict_hitrate(sub_allocate_size[batch_idx], hitrate_line[batch_idx])
            add_allocate_delay = self.predict_hitrate(add_allocate_size[batch_idx], hitrate_line[batch_idx])

            list_hitrate.append(hitrate)
            list_sub_allocate_hitrate.append(sub_allocate_delay)            
            list_add_allocate_hitrate.append(add_allocate_delay)
        
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
        mean_list_hitrate = torch.mean(hitrate, dim=-1,keepdim=True)

        # 每个任务的命中率 / 平均命中率 : 每个任务的命中率越大越好
        reward_hitrate = hitrate / (mean_list_hitrate + 1e-9)

        # 当增大相同分配大小时，相较于平均增大的命中率，任务增大的命中率越大，说明应该增大该任务的分配大小，该奖励越大
        mean_add_hitrate = torch.mean(add_hitrate, dim=-1,keepdim=True)

        reward_add_hitrate = add_hitrate / (mean_add_hitrate + 1e-9)

        mean_sub_hitrate = torch.mean(sub_hitrate, dim=-1,keepdim=True)
        reward_sub_hitrate =  sub_hitrate / (mean_sub_hitrate + 1e-9)

        # 奖励由三部分组成,当前分配比例下延迟小，如果增大分配大小延迟减少的多，如果减少分配大小延迟增大的多
        reward = (1.0 * reward_hitrate) + (0.5 * reward_add_hitrate) + (0.5 * reward_sub_hitrate)

        reward = (reward - reward.mean()) / (reward.std() + 1e-9)

        # 无下一个状态
        state = None
        # 一次即结束
        done = None
        return state, reward, done
    
        

if __name__ == '__main__':
    point1=[[16, 16, 16, 16, 16, 16, 16, 16, 16, 16],[0.0001 , 0.8601,  0.2398,  0.4711,  0.29215,   0.26915,  0.7311,  0,        0.5329,  0.2957]]
    point2=[[20, 20, 10, 21, 12, 14, 17, 9,  17, 20],[0,       0.9324,  0.0447,  0.7632,  0.297075,  0.1392,   0.7185,  0,        0.7132,  0.3795]]
    env=Env(point1,point2)
    
    print(env.train_dataset)

    # 训练
    # point3=[[20, 20, 10, 21, 12, 14, 17, 9,  17, 20],[0.385,   0,       0.8063,  0.0508,  0.804575,  0.2685,   0,       0.63,     0,       0.724]]
    # point4=[[18, 18, 12, 24, 16, 9,  18, 14, 16, 15],[0.3415,  0,       0.8841,  0.0003, 0.82305,   0.17145,  0,       0.6912,   0,       0.5854]]
    # env=Env(point3,point4)
    # list_hit_rate = env.list_hit_rate
    # print(list_hit_rate)
    # x = np.linspace(0, 80, 100)
    # for i in range(len(list_hit_rate)):
    #     line = sigmoid(x, list_hit_rate[i][0], list_hit_rate[i][1], list_hit_rate[i][2])
    #     plt.plot(x, line, label='line' + str(i + 1))
    # plt.legend()
    # plt.title("Function Mapping")
    # plt.xlabel("Original Values")
    # plt.ylabel("Mapped Values (0-1)")
    # plt.savefig('figures/activate_fun.png')