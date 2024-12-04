import copy
import os
import pandas as pd
import torch
import random
import time
import numpy as np
from collections import deque

from environment import  *
from agent import Agent
from torch.utils.data import DataLoader, TensorDataset

def setup_seed(seed):
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)    # 为所有的GPU设置相同的种子
    np.random.seed(seed)
    random.seed(seed)
    torch.backends.cudnn.deterministic = True   # 设置 CuDNN 的确定性模式，以获得可重复的结果

setup_seed(300)

class TrainManager:

    def __init__(self):

        device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
        # device = torch.device('cpu')

        if device.type == 'cuda':
            torch.cuda.set_device(device)
            torch.set_default_tensor_type('torch.cuda.FloatTensor')
        else :
            torch.set_default_tensor_type('torch.FloatTensor')
            num_cpus = torch.get_num_threads()
            print(f"PyTorch默认使用的CPU核个数为：{num_cpus}")
        # print("Pytorch device: ",device.type)
        print('Using PyTorch version:', torch.__version__, ' Device:', device)
        # 设置打印选项  threshold: 控制打印的张量元素数量的阈值， np.inf 表示无限制，可以打印所有元素
        # torch.set_printoptions(precision=None, threshold=np.inf, edgeitems=None, linewidth=None, profile=None,
        #                        sci_mode=False)

    def cahce_train(self, first, second, num_tasks, TOTAL_RESOURCE):
        '''传入模拟点和任务数量'''
        max_epoch =  15             # 训练轮数 10就够了，之后就开始下降
        maxlen_best_model = 1       # Save the best model
        makespan_best =float('-inf')
        last_best_model_path = None
        count_iters = 0
        list_mean_hitrate = []      #记录训练过程中的平均命中率
        agent = Agent()
        # 训练批次为200，验证集批次为1
        env = Env(first, second, num_tasks=num_tasks, 
                  train_batch_size = 1024 * 20, validate_batch_size = 4,
                  simulate_num = 4, TOTAL_CACHE_SIZE=TOTAL_RESOURCE)     
        str_time = time.strftime("%Y%m%d_%H%M%S", time.localtime(time.time()))
        save_dir = f'./train_dir/num_task_{num_tasks}_{str_time}'
        os.makedirs(save_dir)
        # 打印所选择的验证集
        import matplotlib.pyplot as plt
        validate_dataset = env.validate_dataset
        x = torch.linspace(0, 30, 100)
        # print('len(validate_dataset[0])',len(validate_dataset[0]))
        for validate_dataset_batch in range(len(validate_dataset[0])):
            # print('len(env.validate_dataset[1][validate_dataset_batch])', len(env.validate_dataset[1][validate_dataset_batch]))
            for i in range(len(env.validate_dataset[1][validate_dataset_batch])):
                # print('env.total_list_hit_rate[env.validate_dataset[1][validate_dataset_batch][i]][0]',env.total_list_hit_rate[env.validate_dataset[1][validate_dataset_batch][i]][0])
                line = env.predict_hitrate(x, env.total_list_hit_rate[env.validate_dataset[1][validate_dataset_batch][i]])
                plt.plot(x, line, label='line' + str(i + 1), alpha=0.7)
            plt.legend()
            plt.axvline(x=16, color='red', linestyle='--', linewidth=2, label='x = 16')
            plt.title("Validate Data")
            plt.xlabel("Cache Allocation")
            plt.ylabel("Hit Rate")
            file_path = os.path.join(save_dir, f'validate_data_lines{validate_dataset_batch}.png')
            plt.savefig(file_path)
            plt.clf()
        start_train_time = time.time()
        for i in range(1,max_epoch + 1):
            print(f'epoch: {i}')
            state, index = env.train_dataset
            if i % 1000 ==0:         # 每100个epoch更新一次数据集
                env.update_train_dataset(num_tasks=num_tasks)
                state, index = env.train_dataset
            combined_dataset = TensorDataset(state, index)
            # 使用DataLoader从合并后的数据集中采样
            dataloader = DataLoader(combined_dataset, batch_size=1024, shuffle=True)
            # 遍历dataloader获取每次的采样
            for batch_data in dataloader:
                sampled_state, sampled_index = batch_data
                action_probs = agent.get_action(sampled_state)
                _, reward, _ = env.step(action_probs, sampled_index)
                # 模型更新
                agent.learn( reward, action_probs)
                count_iters +=1

                # 验证集验证
                val_state, val_index = env.validate_dataset
                action_probs = agent.get_action(val_state)
                # print('action_probs : ',action_probs * TOTAL_CACHE_SIZE)
                val_reward = 0.0
                # print('action_probs)',action_probs)
                # print('val_index', val_index)
                # print('len(action_probs)', len(action_probs))
                for i in range(len(action_probs)):
                    # print('val_index', val_index[i])
                    # print('train action',action_probs * TOTAL_RESOURCE)
                    val_reward += env.compete_hitrate(action_probs[i] * TOTAL_RESOURCE, val_index[i], True)
                val_reward = val_reward / len(val_index)
                print(f'epoch {count_iters} get val_reward : ', val_reward)
                mean_hitrate = torch.mean(val_reward).item()
                list_mean_hitrate.append(mean_hitrate)
                print(f'epoch {count_iters} get validate mean hitrate : ',mean_hitrate)

                if mean_hitrate > makespan_best:
                    makespan_best = mean_hitrate
                    save_new_model_path = '{0}/model_T{1}_I{2}.pt'.format(save_dir,num_tasks, count_iters)
                    if last_best_model_path != None:
                        os.remove(last_best_model_path)
                    last_best_model_path = save_new_model_path

                    torch.save(agent.model.state_dict(), save_new_model_path)
        seconds = time.time() - start_train_time
        hours = seconds // 3600
        minutes = (seconds % 3600) // 60
        remaining_seconds = int(seconds % 60)
        print(f"total_train_time: H:{hours}-M:{minutes}-S:{remaining_seconds}")
        # print(list_mean_hitrate)

        with open('{0}/list.txt'.format(save_dir),'w') as f:
            f.write('valid_list_mean_hitrate' + str(list_mean_hitrate) + '\n\n')

        import matplotlib.pyplot as plt

        # plt.switch_backend('Agg')
        plt.title(f'mean hitrate with task num {num_tasks}')
        plt.figure(figsize=(12, 6))
        x_data = list(range(1,len(list_mean_hitrate)+1))
        plt.plot( x_data, list_mean_hitrate, label='mean hitrate')

        plt.xlabel('iterations')
        plt.ylabel('mean hitrate')
        plt.legend()

        plt.grid()  # 网格
        plt.tight_layout()  # 去白边
        plt.savefig(save_dir+f'/mean_hitrate with task num {num_tasks}.png', dpi=200)
        plt.show()

    def cpu_train(self, first, second, num_tasks, TOTAL_RESOURCE):

        pass
if __name__ == '__main__':
    point1=[[20, 20, 10, 21, 12, 14, 17, 9,  17, 20],
            [0.385,   0,       0.8063,  0.0508,  0.804575,  0.2685,   0,       0.63,     0,       0.724]]
    point2=[[18, 18, 12, 24, 16, 9,  18, 14, 16, 15],
            [0.3415,  0,       0.8841,  0.0003, 0.82305,   0.17145,  0,       0.6912,   0,       0.5854]]
    t = TrainManager()
    t.cahce_train(point1, point2, num_tasks = 10, TOTAL_RESOURCE = 160)
    # 20个任务，80个单位资源