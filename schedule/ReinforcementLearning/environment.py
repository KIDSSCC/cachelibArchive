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

def sigmoid(x, a, b, c):
        """定义 Sigmoid 函数"""
        return a / (1 + np.exp(-b * (x - c)))

class Env:

    def __init__(self,num_tasks=25,num_states=5,
                 train_batch_size=4096,validate_batch_size=100):
        '''
        num_tasks ： 任务个数
        num_states : 每个任务的状态，五种
        '''
        self.num_tasks = num_tasks          # 任务总数，个数在5-25之间
        self.num_states = num_states        # 任务状态
        self.train_batch_size = train_batch_size    # 训练批次
        # 每个任务的每一个状态对应了一种尾延迟曲线

        pass

    def step(self, action, index):
        # 共分256GB
        allocate_size = action * 256
        # TODO：根据index
        pass

    def gen_dataset():
        '''
        需要训练集和测试集
        '''
        pass
    
    def predict_line(self, first, second):
        '''
        根据采样的两个点预测分配与曲线之间的关系
        first/second传入参数形如：[[A_cache,B_cahce,C_cache],[A_hitrate,B_hitrate,C_hitrate]]
        认为
        '''
        assert len(first[0])==len(second[0]), "任务数量不一致"    
        num_tasks = len(first[0])  # 任务数量
        predictions = []

        for i in range(num_tasks):
            # 分别获取两个点的 cache 和 hitrate 值
            cache1, hitrate1 = first[0][i], first[1][i]
            cache2, hitrate2 = second[0][i], second[1][i]
            # 确保cache值不同，以避免除零
            assert cache1 != cache2, f"任务 {i} 的两个点的 cache 值不能相等"

            # 1.D_SEQUENTIAL分布，采样到的点hit_rate都为0
            if hitrate2 == 0 :
                predictions.append([0,0,0])
                continue
            # 2.采样点不精确，增大cache后hit_rate反而降低，暂定为直线
            if (cache2 > cache1 and hitrate2 < hitrate1) or (cache2 < cache1 and hitrate2 > hitrate1):
                predictions.append([hitrate1,0,0])
                continue
            # 3.其余使用sigmoid函数进行模拟
            a = 1       # 命中率最大值为1
            c = (hitrate1 - hitrate2) / (cache1 - cache2)   # 中值
            # numerator = (a / hitrate1 - 1)
            # denominator = (a / hitrate2 - 1)
            # print(f"i: {i}, numerator: {numerator}, denominator: {denominator}")
            # assert numerator > 0 and denominator > 0, "对数输入非法"
            b = (1 / (cache2 - cache1)) * np.log((a / hitrate1 - 1) / (a / hitrate2 - 1))  # 根据公式计算 b
            predictions.append([a,b,c])
        return predictions

if __name__ == '__main__':
    point1=[[16, 16, 16, 16, 16, 16, 16, 16, 16, 16],[0.0001 , 0.8601,  0.2398,  0.4711,  0.29215,   0.26915,  0.7311,  0,        0.5329,  0.2957]]
    point2=[[20, 20, 10, 21, 12, 14, 17, 9,  17, 20],[0,       0.9324,  0.0447,  0.7632,  0.297075,  0.1392,   0.7185,  0,        0.7132,  0.3795]]

    point3=[[20, 20, 10, 21, 12, 14, 17, 9,  17, 20],[0.385,   0,       0.8063,  0.0508,  0.804575,  0.2685,   0,       0.63,     0,       0.724]]
    point4=[[18, 18, 12, 24, 16, 9,  18, 14, 16, 15],[0.3415,  0,       0.8841,  0.0003, 0.82305,   0.17145,  0,       0.6912,   0,       0.5854]]
    env=Env()
    # predictions = env.predict_line(point1,point2)
    predictions = env.predict_line(point3,point4)
    x = np.linspace(0, 80, 100)
    for i in range(len(predictions)):
        line = sigmoid(x, predictions[i][0], predictions[i][1], predictions[i][2])
        plt.plot(x, line, label='line' + str(i + 1))
    plt.legend()
    plt.title("Function Mapping")
    plt.xlabel("Original Values")
    plt.ylabel("Mapped Values (0-1)")
    plt.savefig('figures/activate_fun.png')