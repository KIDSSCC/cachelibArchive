import copy
import math
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.distributions import Categorical

class MLP(nn.Module):
    def __init__(self, input_dim, hidden_dim, output_dim,dropout_prob):
        super(MLP, self).__init__()

        self.linear1 = nn.Linear(input_dim,hidden_dim)
        self.dropout1 = nn.Dropout(dropout_prob)
        self.linear2 = nn.Linear(hidden_dim, hidden_dim)
        self.dropout2 = nn.Dropout(dropout_prob)
        self.linear3 = nn.Linear(hidden_dim, int(hidden_dim / 4))
        self.dropout3 = nn.Dropout(dropout_prob)
        self.linear4 = nn.Linear(int(hidden_dim / 4), output_dim)

        self.activate_func = nn.Sigmoid()


    def forward(self, x):
        x =  self.activate_func(self.dropout1(self.linear1(x)))
        x =  self.activate_func(self.dropout2(self.linear2(x)))
        x =  self.activate_func(self.dropout3(self.linear3(x)))
        out = self.linear4(x)
        return out
class PolicyNetwork(nn.Module):

    def __init__(self):
        super().__init__()

        self.num_layers_actor = 4  # MLP 的层数
        self.num_layers_critic = 4

        self.input_dim_actor = 128  # 输入维度
        self.input_dim_critic = 128

        self.hidden_dim_actor = 256  # 隐藏层的维度
        self.hidden_dim_critic = 256

        self.output_dim_actor = 1  # 输出维度
        self.output_dim_critic = 1

        self.dropout_prob = 0.5

        self.actor = MLP(input_dim=self.input_dim_actor,
                         hidden_dim=self.hidden_dim_actor,
                         output_dim=self.output_dim_actor,
                         dropout_prob = self.dropout_prob)




class Agent:
    def __init__(self):
        # self.device = 'cpu'

        self.lr = 2e-4

        self.model = PolicyNetwork()
        self.optimizer = torch.optim.Adam(self.model.parameters(), lr=self.lr)
        self.Mseloss = nn.MSELoss()

    def get_action(self, state):
        scores = self.model.actor(state).squeeze()
        action_probs = F.softmax(scores, dim=-1)
        return action_probs

    def learn(self, reward, action_prob):

        # mean_reward = torch.mean(reward, dim=-1)
        # reward = mean_reward/reward
        # reward = (reward - reward.mean()) / (reward.std() + 1e-9)
        reward = reward.detach()
        log_action_probs = torch.log(action_prob)
        # 增大奖励大动作概率，即任务分配比例
        loss_actor = - torch.sum( log_action_probs * reward,dim=-1).sum()
        self.optimizer.zero_grad()
        loss_actor.backward()
        self.optimizer.step()


if __name__ == '__main__':

    from environment import Env
    from torch.utils.data import DataLoader, TensorDataset
    point1=[[16, 16, 16, 16, 16, 16, 16, 16, 16, 16],[0.0001 , 0.8601,  0.2398,  0.4711,  0.29215,   0.26915,  0.7311,  0,        0.5329,  0.2957]]
    point2=[[20, 20, 10, 21, 12, 14, 17, 9,  17, 20],[0,       0.9324,  0.0447,  0.7632,  0.297075,  0.1392,   0.7185,  0,        0.7132,  0.3795]]
    env = Env(point1, point2)
    state, index = env.train_dataset

    agent = Agent()

    # 合并state和index_tensor为一个数据集
    combined_dataset = TensorDataset(state, index)
    # 使用DataLoader从合并后的数据集中采样
    dataloader = DataLoader(combined_dataset, batch_size=200, shuffle=True)
    # 遍历dataloader获取每次的采样
    for batch_data in dataloader:
        sampled_state, sampled_index = batch_data
        print(f"Sampled state shape: {sampled_state.shape}")            # [200, 10, 3] batch_size, num_tasks, 曲线
        print(sampled_state)
        action_probs = agent.get_action(sampled_state)
        _, reward, _ = env.step(action_probs, sampled_index)
        print(reward)

