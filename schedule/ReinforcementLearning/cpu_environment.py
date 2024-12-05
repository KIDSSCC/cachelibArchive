import copy
import torch
import torch.nn.functional as F
from torch.utils.data import DataLoader, TensorDataset
import matplotlib.pyplot as plt
import numpy as np
import random
from torch.utils.data import random_split

class CPUEnv:
    def __init__(self, first, second, num_tasks = 10):
        '''
        根据采样的两个点预测CPU分配与曲线之间的关系
        first/second传入参数形如：[[A_cpu_num,B_cpu_num,C_cpu_num],[A_cpu_utilization,B_cpu_utilization,C_cpu_utilization]]
        '''
        self.num_tasks = num_tasks

if __name__ == '__main__':
    first =[[2, 1, 1, 2, 1, 1, 1, 1, 1, 1]]