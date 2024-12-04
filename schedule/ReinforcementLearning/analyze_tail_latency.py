import matplotlib.pyplot as plt

# 文件路径
file_path = '/home/md/SHMCachelib/schedule/ReinforcementLearning/log/20241202_150737/RL_latency_cat.log'
baseline_filepath = '/home/md/SHMCachelib/schedule/ReinforcementLearning/baseline_latency.log'
fig_save_path = '/home/md/SHMCachelib/schedule/ReinforcementLearning/log/20241202_150737/RL_tail_latency.png'
# 初始化存储数据的列表
RL_epochs = []
RL_values = []
RL_phase = 0
RL_avg_value = []
RL_curr_phase_total_latency = 0
# 读取文件
with open(file_path, 'r') as file:
    for line in file:
        parts = line.strip().split(':')  # 以冒号分割
        epoch = int(parts[1])
        RL_epochs.append(epoch + RL_phase * 107)
        RL_values.append(float(parts[2]))
        RL_curr_phase_total_latency += float(parts[2])
        if epoch == 107:
            RL_phase += 1
            RL_avg_value.append(RL_curr_phase_total_latency / 107)
            RL_curr_phase_total_latency = 0
        

baseline_epochs = []
baseline_values = []
baseline_phase = 0
baseline_avg_value = []
baseline_curr_phase_total_latency = 0
with open(baseline_filepath, 'r') as file:
    for line in file:
        parts = line.strip().split(':')  # 以冒号分割
        epoch = int(parts[1])
        baseline_epochs.append(epoch + baseline_phase * 107)
        baseline_values.append(float(parts[2]))
        baseline_curr_phase_total_latency += float(parts[2])
        if epoch == 107:
            baseline_phase += 1
            baseline_avg_value.append(baseline_curr_phase_total_latency / 107)
            baseline_curr_phase_total_latency = 0
        

print(f'baseline_avg_value: {baseline_avg_value}')
print(f'RL_avg_value: {RL_avg_value}')
print(f'each epoch upgrade rate {[(baseline_avg_value[i] - RL_avg_value[i]) / baseline_avg_value[i] for i in range(len(RL_avg_value))]}')
# 绘制图形
plt.plot(RL_epochs, RL_values, marker='', color='b', label='RL_tail_latency')  # 绘制曲线
plt.plot(baseline_epochs, baseline_values, marker='', color='r', label='baseline_tail_latency')  # 绘制曲线
plt.xlabel('Epoch')  # x轴标签
plt.ylabel('Value')  # y轴标签
plt.title('Latency Over RL_epochs')  # 图表标题
plt.legend()  # 显示图例
# /home/md/SHMCachelib/schedule/ReinforcementLearning/log/
plt.savefig(fig_save_path)
