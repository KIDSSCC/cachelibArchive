# from jhn
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

        # self.gamma = 1.0  # 折扣率

        self.lr = 2e-4

        self.model = PolicyNetwork()
        self.optimizer = torch.optim.Adam(self.model.parameters(), lr=self.lr)
        self.Mseloss = nn.MSELoss()

    def get_action(self, state):
        scores = self.model.actor(state).squeeze()
        action_probs = F.softmax(scores, dim=-1)

        # dist_action = Categorical(action_probs)

        # action_index = dist_action.sample()
        # log_prob = dist_action.log_prob(action_index)

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

    env = Env()
    state, index = env.train_dataset

    agent = Agent()

    # 合并state和index_tensor为一个数据集
    combined_dataset = TensorDataset(state, index)
    # 使用DataLoader从合并后的数据集中采样
    dataloader = DataLoader(combined_dataset, batch_size=200, shuffle=True)
    # 遍历dataloader获取每次的采样
    for batch_data in dataloader:
        sampled_state, sampled_index = batch_data

        action_probs = agent.get_action(sampled_state)
        _, reward, _ = env.step(action_probs, sampled_index)
        print(reward)
        # agent.learn(state,reward=torch.tensor(10.),action_prob=action_probs)

