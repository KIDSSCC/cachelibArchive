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

OffLine_Sample_Analyzed_Path_Cache = '/home/md/SHMCachelib/schedule/ReinforcementLearning/Offline_Sample/20241207_152445_cache_analyzed.log'
OffLine_Sample_Analyzed_Path_Bw = '/home/md/SHMCachelib/schedule/ReinforcementLearning/Offline_Sample/20241207_152445_bw_analyzed.log'

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

    def train(self,save_dir, num_tasks, Train_Opt, OffLine_Sample_Analyzed_Path):
        '''传入模拟点和任务数量'''
        max_epoch =  15             # 训练轮数 10就够了，之后就开始下降
        maxlen_best_model = 1       # Save the best model
        last_best_model_path = None
        count_iters = 0
        list_mean_hitrate = []      # 记录训练过程中的平均命中率
        list_mean_latency = []      # 记录训练过程中的平均尾延迟
        agent = Agent()
        Offline_Sample_Analyzed_Path = ''
        self.Train_Opt = Train_Opt
        if self.Train_Opt == 0:
            TOTAL_RESOURCE = num_tasks * 16
            makespan_best =float('-inf')
        if self.Train_Opt == 1:
            TOTAL_RESOURCE = num_tasks * 20
            makespan_best =float('inf')
        # 训练批次为200，验证集批次为1
        env = Env(num_tasks = num_tasks, TOTAL_RESOURCE=TOTAL_RESOURCE, 
                    train_batch_size = 1024 * 20, validate_batch_size = 4, 
                    Offline_Sample_Analyzed_Path = OffLine_Sample_Analyzed_Path, 
                    Train_Opt = Train_Opt)     
        
        self.PrintValidateDataset(save_dir=save_dir, env=env)
        
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
                    val_reward += env.CompeteReward(action_probs[i] * TOTAL_RESOURCE, val_index[i])
                val_reward = val_reward / len(val_index)
                print(f'epoch {count_iters} get val_reward : ', val_reward)
                if Train_Opt == 0:          # cache
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
                elif Train_Opt == 1:        # 带宽
                    mean_latency = torch.mean(val_reward).item()
                    list_mean_latency.append(mean_latency)
                    print(f'epoch {count_iters} get validate mean latency : ', mean_latency)
                    if mean_latency < makespan_best:
                        makespan_best = mean_latency
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

        if Train_Opt == 0:
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
        elif Train_Opt == 1:
            with open('{0}/list.txt'.format(save_dir),'w') as f:
                f.write('valid_list_mean_latency' + str(list_mean_latency) + '\n\n')
            import matplotlib.pyplot as plt
            # plt.switch_backend('Agg')
            plt.title(f'mean latency with task num {num_tasks}')
            plt.figure(figsize=(12, 6))
            x_data = list(range(1,len(list_mean_latency)+1))
            plt.plot( x_data, list_mean_latency, label='mean hitrate')
            plt.xlabel('iterations')
            plt.ylabel('mean hitrate')
            plt.legend()
            plt.grid()  # 网格
            plt.tight_layout()  # 去白边
            plt.savefig(save_dir+f'/list_mean_latency with task num {num_tasks}.png', dpi=200)
            plt.show()

    def PrintValidateDataset(self, save_dir, env):
        # 打印所选择的验证集
        import matplotlib.pyplot as plt
        _, validate_dataset_index = env.validate_dataset
        x = torch.linspace(0, 40, 100)
        for validate_batch in range(len(validate_dataset_index)):
            # print(f'validate_dataset_index batch[{validate_batch}] : {validate_dataset_index[validate_batch]}')
            for i in range(len(validate_dataset_index[validate_batch])):
                # print(f'validate_dataset_index[{validate_batch}][{i}] : {validate_dataset_index[validate_batch][i]}')
                # print('validate_dataset_index[validate_batch][i][0].item()', validate_dataset_index[validate_batch][i][0].item())
                line = env.PredictReward(x, env.TotalFeatures[validate_dataset_index[validate_batch][i][0].item()][validate_dataset_index[validate_batch][i][1].item()])
                plt.plot(x, line, label='line' + str(i + 1), alpha=0.7)
            plt.legend()
            plt.axvline(x=16, color='red', linestyle='--', linewidth=2, label='x = 16')
            plt.title("Validate Data")
            plt.xlabel("Cache Allocation")
            plt.ylabel("Hit Rate")
            file_path = os.path.join(save_dir, f'validate_data_lines{validate_batch}.png')
            plt.savefig(file_path)
            plt.clf()

if __name__ == '__main__':
    str_time = time.strftime("%Y%m%d_%H%M%S", time.localtime(time.time()))
    # save_dir = f'./train_dir/Cache_num_task_10_{str_time}'
    save_dir = f'./train_dir/BandWidth_num_task_10_{str_time}'
    os.makedirs(save_dir)
    t = TrainManager()
    t.train(save_dir=save_dir, num_tasks=15, Train_Opt = 1, OffLine_Sample_Analyzed_Path= OffLine_Sample_Analyzed_Path_Bw)     # 1 : 先练带宽
    # t.train(save_dir=save_dir, num_tasks=10, Train_Opt = 0, OffLine_Sample_Analyzed_Path= OffLine_Sample_Analyzed_Path_Cache)     # 0 ： 先练cache
    # 20个任务，80个单位资源