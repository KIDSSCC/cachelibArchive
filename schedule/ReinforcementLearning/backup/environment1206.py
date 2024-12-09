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

def exponential(x, a , b):
    """命中率为指数函数时"""
    res = a * (1 - torch.exp(-b * (x - 0)))
    res = torch.minimum(res, torch.tensor(1))  # 将结果限制在最大值 1
    return res
def log_func(x, a, b):
    res = a * np.log(b * x + 1)
    res = torch.minimum(res, torch.tensor(1))  # 将结果限制在最大值 1
    return res
def Cache_Simulated_Line(x, params):
    '''
    flag = 0:线性增长
    flag = 1:指数型增长
    params : []'''
    # if params[2] == 0:          # y1 = y2 = 1.0，假设线性增长
    #     res = params[0] * x
    #     res = torch.minimum(res, torch.tensor(1))  # 将结果限制在最大值 1
    #     return res
    # if params[2] == 1 :          # y1 != y2
    #     res = params[0] * (1 - torch.exp(-params[1] * (x - 0)))
    #     res = torch.minimum(res, torch.tensor(1))  # 将结果限制在最大值 1
    #     return res
    if params[0] == 0:          # sequential
        res = torch.where(x > params[1], torch.tensor(1), torch.tensor(0))
        return res
    if params[0] == 1 or params[0] == 3:
        res = params[2] * (1 - torch.exp(-params[3] * (x - 0)))
        res = torch.minimum(res, torch.tensor(1))  # 将结果限制在最大值 1
        return res
    if params[0] == 2:
        res = params[2] * x
        res = torch.minimum(res, torch.tensor(1))  # 将结果限制在最大值 1
        return res
    print("should not be here, params : ", params)

def Calculate_features(x1, x2, y1, y2):
    # 1. y1 == y2 == 1
    flag = 1    # 0 横线为0， 1，exponential
    a = 1
    b = 1
    # 1. 两次采样命中率都是1
    if y1 == 1 and y2 == 1:     
        flag = 0
        a = 1 / min(x1, x2)         # a为斜线斜率
        return [a, b, flag, 0]
    # 2. 第二次采样的命中率低于0.05 且分配的资源量是比较多的，认为当前是sequence    -> 4可以改
    if y2 <= 0.05 and x2 >= 4:                   
        flag = 0
        a = 0                               # a为斜线斜率
        return [a, b, flag, 0]
    # 3. 其余都用exponential
    # 采样点不精确，cache增大，结果反而缩小
    if (x2 > x1 and y2 < y1) or (x2 < x1 and y2 > y1):
        temp = x1
        x1 = x2
        x2 = temp
    params_solution = fsolve(exp_growth, [1, 0.1], args=(x1, y1, x2, y2), maxfev=200)
    a = params_solution[0]
    b = params_solution[1]
    return [a, b, flag]

# Cache分配和命中率之间的关系
def exp_growth(params, x1, y1, x2, y2):
    '''
    params : 初始猜想解
    '''
    A, B = params
    eq1 = A * (1 - np.exp(-B * x1)) - y1
    eq2 = A * (1 - np.exp(-B * x2)) - y2
    return [eq1, eq2]

# 带宽分配和99尾延迟之间的关系
def exp_decay(params, bw1, latency1, bw2, latency2):
    A, B = params
    eq1 = A * (np.exp(-B * bw1)) -latency1 # 对应第一个点的方程
    eq2 = A * (np.exp(-B * bw2)) -latency2 # 对应第二个点的方程
    return [eq1, eq2]


class Env:
    def __init__(self, first, second, simulate_num = 4, num_tasks = 25, train_batch_size = 10, validate_batch_size = 1, TOTAL_RESOURCE = 1024, predict_opt = 0):
        '''
        simulate_num : 模拟曲线量
        根据采样的两个点预测分配与曲线之间的关系
        first/second传入参数形如：[[A_cache,B_cahce,C_cache],[A_hitrate,B_hitrate,C_hitrate]]
        '''
        assert len(first[0])==len(second[0]), "任务数量不一致"    
        self.train_batch_size = train_batch_size    # 训练批次
        self.num_tasks =num_tasks                   # 当前任务个数
        self.max_num_tasks = 25                     # [5,25]
        self.total_list_hit_rate = []       # 存储了来自真实数据采样和变形得到的曲线特征，即[类型，对应参数]
        self.TOTAL_RESOURCE = TOTAL_RESOURCE
        self.PredicetOpt = predict_opt      # 当前模拟的是哪一维资源 0 : Cache  1：bandwidth
        
        # # 前num_tasks为真实采样数据
        # for i in range(len(first[0])):
        #     # 分别获取两个点的 cache 和 hitrate 值
        #     x1, y1 = first[0][i], first[1][i]
        #     x2, y2 = second[0][i], second[1][i]
        #     # 确保cache值不同，以避免除零 -> predication_model.py
        #     assert x1 != x2, f"任务 {i} 的两个点的 cache 值不能相等"
        #     res = self.Calculate_features(x1, x2, y1, y2)
        #     print('line ', i + 1, res)
        #     self.total_list_hit_rate.append(res)
        # # 生成模拟数据
        # for i in range(len(first[0]) * simulate_num):
        #     a = self.total_list_hit_rate[i % len(first[0])][0]
        #     b = self.total_list_hit_rate[i % len(first[0])][1]
        #     flag = self.total_list_hit_rate[i % len(first[0])][2]
        #     a_new = a * (1 + np.random.uniform(-0.1, 0.1))
        #     b_new = b * (1 + np.random.uniform(-0.1, 0.1))
        #     self.total_list_hit_rate.append([a_new,b_new, flag])
        # self.all_lines, _ = self.gen_total_lines()         # 存储所有的total_list_hit_rate对应的line的128个值
        # # 划分训练集和测试集，同时记录了其采样下标
        # self.train_dataset = self.gen_dataset(size_dataset=train_batch_size, num_tasks = num_tasks, is_train=True)
        # self.validate_dataset = self.gen_dataset(size_dataset=validate_batch_size, num_tasks = num_tasks, is_train=False)
        
        self.Gen_Total_Features()       #生成每个任务每个阶段的特征值
        # print("total_list_hit_rate:", self.total_list_hit_rate)
        self.all_lines, _ = self.gen_total_lines()
        self.train_dataset = self.gen_dataset(size_dataset=train_batch_size, num_tasks = num_tasks, is_train=True)
        self.validate_dataset = self.gen_dataset(size_dataset=validate_batch_size, num_tasks = num_tasks, is_train=False)

    def Gen_Total_Features(self):
        for backend_index in range(len(database_phase_line)):
            backend_phase_features = []
            for phase_index in range(len(database_phase_line[backend_index])):
                if database_phase_line[backend_index][phase_index][0] == 0:
                    # Sequential
                    arg1 = 0    # 线性
                    arg2 = database_phase_line[backend_index][phase_index][1]   # 256
                    arg3 = 0
                    arg4 = 0
                if database_phase_line[backend_index][phase_index][0] == 1:
                    # Hotspot
                    arg1 = 1
                    arg2 = database_phase_line[backend_index][phase_index][1]   # 64 / 32 / 16
                    arg3 = 1            # a = 1
                    arg4 = 4 / database_phase_line[backend_index][phase_index][1]
                if database_phase_line[backend_index][phase_index][0] == 2:
                    # Uniform
                    arg1 = 2
                    arg2 = database_phase_line[backend_index][phase_index][1]   # 16 / 32 / 64
                    arg3 = 1
                    arg4 = 1 / arg2     # k斜率
                if database_phase_line[backend_index][phase_index][0] == 3:
                    # Zipfian
                    arg1 = 3
                    arg2 = database_phase_line[backend_index][phase_index][1]   # 16 / 32 / 64
                    arg3 = 1
                    arg4 = 4 / database_phase_line[backend_index][phase_index][1]   # 也用exponential
                self.total_list_hit_rate.append([arg1, arg2, arg3, arg4])
            # self.total_list_hit_rate.append(backend_phase_features)

    def Calculate_features(self, x1, x2, y1, y2):
        if self.PredicetOpt == 0:
            # 1. y1 == y2 == 1
            flag = 1    # 0 线性， 1，exponential
            a = 1
            b = 1
            # 1. 两次采样命中率都是1
            if y1 == 1 and y2 == 1:     
                flag = 0
                a = 1 / min(x1, x2)         # a为斜线斜率
                return [a, b, flag]
            # 2. 第二次采样的命中率低于0.05 且分配的资源量是比较多的，认为当前是sequence    -> 4可以改
            if y2 <= 0.05 and x2 >= 4:                   
                flag = 0
                a = 0                               # a为斜线斜率
                return [a, b, flag]
            # 3. 其余都用exponential
            # 采样点不精确，cache增大，结果反而缩小
            if (x2 > x1 and y2 < y1) or (x2 < x1 and y2 > y1):
                temp = x1
                x1 = x2
                x2 = temp
            params_solution = fsolve(exp_growth, [1, 0.1], args=(x1, y1, x2, y2), maxfev=100)
            a = params_solution[0]
            a = max(1, params_solution[0])
            b = params_solution[1]
            return [a, b, flag]
        # 带宽资源
        if self.PredicetOpt == 1:
            initial_guess = [max(y1, y2), 0.1] 
            params_solution = fsolve(exp_decay, initial_guess, args=(x1, y1, x2, y2), maxfev=100)
            

    def gen_total_lines(self):
        '''生成所有的曲线'''
        if self.PredicetOpt == 0:
            random_samples = torch.linspace(0, 1, 128) * self.TOTAL_RESOURCE       # 随机生成128个浮点数,范围在0到TOTAL_RESOURCE之间
            x, _ = torch.sort(random_samples)
            y_list = []
            index_list = []
            for line_index in range(len(self.total_list_hit_rate)):
                y = self.predict_hitrate(x, self.total_list_hit_rate[line_index])
                y_list.append(y)
                index_list.append(torch.tensor([line_index]))
            # print("y_list : ",y_list)
            y = torch.stack(y_list, dim=0)
            index = torch.stack(index_list, dim=0)
            # # 找到曲线采样矩阵中的最小值和最大值
            # min_val = torch.min(y)
            # max_val = torch.max(y)
            # # 缩放矩阵到[0,1]范围内
            # y = (y - min_val) / (max_val - min_val)
            return y, index
        if self.PredicetOpt == 1:           # 带宽
            random_samples = torch.linspace(0, 1, 128) * self.TOTAL_RESOURCE       # 随机生成128个浮点数,范围在0到TOTAL_RESOURCE之间
            x, _ = torch.sort(random_samples)
            y_list = []
            index_list = []
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
        if self.PredicetOpt == 0:
            res = Cache_Simulated_Line(cache_size, action_hitrate_index)
            return res
        else:
            print("--------------------------------------!!!")

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
        return torch.tensor(list_total_hitrate, dtype=torch.float64)
    
    def step(self, action, index):
        '''根据action计算返回reward，单步决策，无state'''
        if action.dim() > 1:
            batch_size, num_task = action.size()
        else:
            num_task = action.size(0)
            batch_size = 1
        # Cache划分
        if self.PredicetOpt == 0:       
            allocate_size = action * self.TOTAL_RESOURCE
            # 减少分配空间
            sub_allocate_size = allocate_size - 128 * 0.05
            sub_allocate_size = torch.where(sub_allocate_size < 0., torch.tensor(0.), sub_allocate_size)
            # 增大分配空间
            add_allocate_size = allocate_size + 128 * 0.05
            add_allocate_size = torch.where(add_allocate_size > 128., torch.tensor(128.), add_allocate_size)
            list_hitrate = []
            list_sub_allocate_hitrate = []
            list_add_allocate_hitrate = []
            for batch_idx in range(batch_size):
                hitrate_line = self.compete_hitrate(allocate_size[batch_idx], index[batch_idx])
                sub_allocate_hitrate = self.compete_hitrate(sub_allocate_size[batch_idx], index[batch_idx])
                add_allocate_hitrate = self.compete_hitrate(add_allocate_size[batch_idx], index[batch_idx])
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

    

if __name__ == '__main__':
    point1=[[20, 20, 10, 21, 12, 14, 17, 9,  17, 20],[0.385,   0,       0.8063,  0.0508,  0.804575,  0.2685,   0,       0.63,     0,       0.724]]
    point2=[[18, 18, 12, 24, 16, 9,  18, 14, 16, 15],[0.3415,  0,       0.8841,  0.0003, 0.82305,   0.17145,  0,       0.6912,   0,       0.5854]]
    env=Env(point1,point2, simulate_num=0, predict_opt=0)
    total_list_hit_rate = env.total_list_hit_rate
    x = torch.linspace(0, 32, 100)
    # print("len(env.total_list_hit_rate) : ", len(env.total_list_hit_rate))      # 75 = 5 * 5 * 3
    for i in range(0,10):
        # line = exponential(x, env.total_list_hit_rate[i][0], env.total_list_hit_rate[i][1])
        line = Cache_Simulated_Line(x, env.total_list_hit_rate[i])
        plt.plot(x, line, label='line' + str(i + 1))
    plt.legend()
    plt.axvline(x=16, color='red', linestyle='--', linewidth=2, label='x = 16')
    plt.title("Function Mapping")
    plt.xlabel("Original Values")
    plt.ylabel("Mapped Values (0-1)")
    plt.savefig('figures/Sampled_simulation.png')
    plt.clf()

    # x = torch.linspace(0, 70, 100)
    # line = log_func(x, 1, (math.e - 1) / 32)
    # plt.plot(x, line, label='line')
    # plt.legend()
    # plt.axvline(x=32, color='red', linestyle='--', linewidth=2, label='x = 32')
    # plt.title("Function Mapping")
    # plt.xlabel("Original Values")
    # plt.ylabel("Mapped Values (0-1)")
    # plt.savefig('figures/Simulated_temp.png')

    
