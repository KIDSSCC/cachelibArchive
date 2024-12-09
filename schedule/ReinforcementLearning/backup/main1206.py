from util import *
from datetime import datetime
from predication_model import *
from agent import *
import threading
import os
import logging

WARMTIME = 100
RUNTIME = 200           # 注意和backend/src/main.cpp保持一致
cache_model_path = './train_dir/num_task_10_20241128_120715/model_T10_I295.pt'

class OfflineProfile(ScheduleFrame):
    def __init__(self, all_apps: List[str],  n_resources: List[int]):
        """
        all_apps ： 所有任务名称
        n_resources : 当前资源分配信息
        """
        super().__init__()
        self.all_apps = all_apps
        self.num_apps = len(all_apps)       # 应用个数

        # cache可以分0，但cpu和bandwidth不可，此处统一arm的数量，生成分配方案时再进行限制，
        assert len(n_resources) == 3, "The dimension of resources should be 3"
        self.cpu_n_arms = n_resources[1] + 1
        self.bandwidth_n_arms = n_resources[2] + 1
        
        # 采样相关
        self.times = 0

        self.history_reward_window = 4
        self.history_reward = []
        
        self.cache_mode = True              # True为
        self.cache_perturbed = None         # 缓存扰动采样点
        self.cpu_pertubed = None            # cpu绕从采样点
        self.bandwidth_perturbed = None     # 带宽扰动采样点
        self.n_cache = n_resources[0]       # cache分配情况
        self.n_cpu = n_resources[1]         # cpu分配情况
        self.n_bandwidth = n_resources[2]   
        self.cache_history_arm = []         # 缓存分配历史
        self.hitrate_history = []           # 对应命中率
        self.cpu_history_arm = []           # cpu分配历史
        self.cpu_utilization_history = []           # 对应利用率
        self.bandwidth_history_arm = []     # 带宽分配历史
        self.latency_history = []           # 对应尾延迟
        self.cache_hitrate = []

        self.new_arm = []
        return

    def select_arm(self):
        return self.new_arm

    def perturb_cpu(self, cpu_utilization, cpu_allocation):
        """把当前CPU利用率最低的几个多核任务分配核心数 - 1，分给CPU利用率最高的进程"""
        total_cpu_cores = sum(cpu_allocation)
        cpu_util_arr = np.array(cpu_utilization)
        indices = np.argsort(cpu_util_arr)[:len(cpu_allocation) // 2]    # 获取利用率最低的前1 / 2进程的索引
        cpu_free_num = 0
        for i in indices:
            if cpu_allocation[i] > 1: 
                cpu_allocation[i] -= 1
                cpu_free_num += 1
        indices = np.argsort(cpu_util_arr)[-len(cpu_allocation) // 2:]   # 获取利用率最高的前1 / 2进程的索引
        for i in indices:
            if cpu_free_num <= 0:
                break
            cpu_allocation[i] += 1
            cpu_free_num -= 1
        assert sum(cpu_allocation) == total_cpu_cores, '扰动前后总核心数不一致！'
        return cpu_allocation


    def update(self, reward, chosen_arm):
        """
        离线采样，全部扰动
            chosen_arm 调整为长度为3的list, 每个元素为一个dict, 对应该资源在每个任务上的分配
            reward 调整为一个长度为3的list, [0]为延迟对应的reward值, [1]为dict, 每个任务对应的cpu利用率，[2]为dict，每个任务对应的cache命中率
        """
        self.new_arm = []
        # 动态负载变化监测
        if not self.cache_mode:
            if len(self.history_reward) < self.history_reward_window:
                self.history_reward.append(sum(map(float, reward[2].values())))
            else:
                half_window = self.history_reward_window // 2
                first_half = self.history_reward[:half_window]
                second_half = self.history_reward[half_window:]
                first_aver = sum(first_half) / len(first_half)
                second_aver = sum(second_half) / len(second_half)
                print('{} --> {},  {}'.format(first_aver, second_aver, abs(second_aver - first_aver) / first_aver))
                if (first_aver - second_aver) / first_aver > 0.10:
                    print('----- test workload change -----')
                    self.reset()
                else:
                    self.history_reward.pop(0)

        # 调整缓存资源
        if self.cache_mode:
            # 当前分配的值
            cache_arm = list(chosen_arm[0].values())
            cpu_arm = list(chosen_arm[1].values())
            bw_arm = list(chosen_arm[2].values())
            self.cache_history_arm.append(cache_arm)
            self.hitrate_history.append(list(reward[2].values()))
            self.cpu_history_arm.append(cpu_arm)
            self.cpu_utilization_history.append(list(reward[1].values()))
            self.bw_history_arm.append(bw_arm)
            self.latency_history.append(reward[0])
            self.times += 1
            # 进行扰动，要求每次扰动后的新值和已经出现过的值不同
            cache_perturbed = self.perturb(cache_arm)
            if self.times < 20:
                # 扰动
                self.cache_perturbed = perturb_list_integers_no_same(cache_arm, self.n_cache)
                self.bw_pertubed = perturb_list_integers_no_same(bw_arm, self.n_bandwidth, epsilon = 3)
                self.cpu_pertubed = self.perturb_cpu(reward[1],cpu_arm)
                self.new_arm = [self.cache_perturbed, self.cpu_pertubed , self.bw_pertubed]
        self.times += 1
def setup_logger(file_path):
    # 创建日志记录器
    logger = logging.getLogger(file_path)
    logger.setLevel(logging.INFO)
    
    # 创建文件处理器并设置日志文件名
    file_handler = logging.FileHandler(file_path)
    file_handler.setLevel(logging.INFO)

    # 创建日志格式化器
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    file_handler.setFormatter(formatter)

    # 创建控制台处理器
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)
    console_handler.setFormatter(formatter)

    # 将文件处理器和控制台处理器添加到日志记录器
    logger.addHandler(file_handler)
    logger.addHandler(console_handler)
    
    return logger

def for_RL_learning(file_path):
    logger = setup_logger(file_path)  # 使用 logging 记录日志
    logger.info('--------- main thread detects warm up -----------')
    time.sleep(WARMTIME + 2)                     # 等待warmup结束，多2s确保进入phase1
    logger.info('--------- main thread begin to work -----------')
    cm = ProtoSystemManagement()
    curr_config = cm.receive_config()   # 获取当前任务及cache分配信息
    all_app = curr_config.task_id       # 获取任务名称
    num_resources = [int(np.sum(x)) for x in curr_config.resource_allocation]       # 获取当前资源分配信息
    online_profiling = OnlineProfile(all_app, num_resources)            # 在线采样

    for i in range(3):
        logger.info("========== Main thread in Phase %d ==========", i + 1)
        start_time = time.time()
        curr_config = cm.receive_config()   # 获取当前任务及cache分配信息
        # 1.第一个采样点
        performance = curr_config.performance                          # 获取当前性能指标
        # print('performance: ', performance)
        context_info = curr_config.context
        chosen_arm = [dict(zip(all_app, i)) for i in curr_config.resource_allocation]   # 获取当前分配资源信息
        utilization_dict = dict(zip(all_app, curr_config.cpu_utilization))              # 获取当前cpu使用率
        utilizations = [float(value) for value in utilization_dict.values()]            # 确保是数值类型
        hitrate_dict = dict(zip(all_app, curr_config.hitrate))                          # 获取当前命中率
        hitrates = [float(value) for value in hitrate_dict.values()]                    # 确保是数值类型
        first_cache_point = [[int(value) for value in chosen_arm[0].values()], hitrates]                    # 第一个点的信息
        first_cpu_point = [[int(value) for value in chosen_arm[1].values()], utilizations]                  # 认为cpu利用率过高的话 ->?
        first_bandwidth_point = [[int(value) for value in chosen_arm[2].values()], curr_config.cpu_utilization]
        aver_hit = sum(hitrates) / len(hitrates)
        th_reward, aver_latency= online_profiling.get_now_reward(performance, context_info) # 记录一下采集到的第一个点的相关信息
        logger.info("采样到第一个点的cache分配为 ：%s\n命中率为%s\ncpu分配为%s\ncpu利用率为%s\n带宽分配为%s\ncpu利用率为%s\n任务平均命中率为: %.4f, 平均尾延迟为: %.4f\n",
                     str(first_cache_point[0]), hitrates, 
                     str(first_cpu_point[0]), utilizations,
                     str(first_bandwidth_point[0]), curr_config.cpu_utilization,
                     aver_hit,aver_latency)
        # 2.进行随机扰动，生成第二个采样点
        online_profiling.update([th_reward, utilization_dict, hitrate_dict], chosen_arm)    # 会进行一个扰动，生成一个新的分配
        new_arm = online_profiling.select_arm()
        new_config = [curr_config.task_id]
        new_config.extend(new_arm)
        logger.info("发送扰动后配置为: %s", new_config)
        cm.send_config(new_config)
        time.sleep(30)      # 等待30s使得扰动生效
        curr_config = cm.receive_config()
        logger.info("第二个采样点信息为: %s", curr_config)
        performance = curr_config.performance                                           # 
        context_info = curr_config.context
        chosen_arm = [dict(zip(all_app, i)) for i in curr_config.resource_allocation]   # 获取当前分配资源信息
        utilization_dict = dict(zip(all_app, curr_config.cpu_utilization))              # 获取当前cpu使用率
        utilizations = [float(value) for value in utilization_dict.values()]            # 确保是数值类型
        hitrate_dict = dict(zip(all_app, curr_config.hitrate))                          # 获取当前命中率
        hitrates = [float(value) for value in hitrate_dict.values()]                    # 确保是数值类型
        second_cache_point = [[int(value) for value in chosen_arm[0].values()], hitrates]
        second_cpu_point = [[int(value) for value in chosen_arm[1].values()], utilizations]                  # 认为cpu利用率过高的话 ->?
        second_bandwidth_point = [[int(value) for value in chosen_arm[2].values()], curr_config.cpu_utilization]
        aver_hit = sum(hitrates) / len(hitrates)
        th_reward, aver_latency= online_profiling.get_now_reward(performance, context_info) # 记录一下采集到的第二个点的相关信息
        logger.info("采样到第二个点的cache分配为 ：%s\n命中率为%s\ncpu分配为%s\ncpu利用率为%s\n带宽分配为%s\ncpu利用率为%s\n任务平均命中率为: %.4f, 平均尾延迟为: %.4f\n",
                     str(second_cache_point[0]), hitrates, 
                     str(second_cpu_point[0]), utilizations,
                     str(second_bandwidth_point[0]), curr_config.cpu_utilization,
                     aver_hit,aver_latency)
        # 3. agent决策
        state_dict = torch.load(cache_model_path, weights_only=True)
        agent = Agent()
        agent.model.load_state_dict(state_dict)
        logger.info("load cache model from %s", cache_model_path)
        # 3.1 根据采样的两个点进行曲线模拟
        cache_state,features = do_simulation(first_cache_point, second_cache_point)
        logger.info("任务曲线模拟特征为 %s", str(features))
        # 3.2 agent决策
        action_probs = agent.get_action(cache_state)
        logger.info("强化学习算法决策cache分配为: %s", str(action_probs * num_resources[0]))
        # 4. 进行模拟退火，生成最终方案
        best_cache_solution, best_hitrate = simulated_annealing2( len(all_app), 
                                                                num_resources[0],
                                                                get_curr_avg_hitrate, 
                                                                x0=action_probs * num_resources[0],
                                                                features=features,
                                                                T_max=100, T_min=1e-3, 
                                                                L=30, 
                                                                max_stay_counter=10, 
                                                                precision=0.5,
                                                                cooling_rate=0.95,
                                                                lb=2,
                                                                ub=num_resources[0],
                                                                change_precision=10
                                                                )
        temp_time = time.time()
        logger.info("模拟退火算法调优后cache分配为: %s,\n预测最佳命中率为: %.4f, 决策总用时: %.8f: ", str(best_cache_solution),
                                                                                            best_hitrate,
                                                                                            temp_time - start_time)
        # 4.使得方案生效 ->暂时只改了cache
        new_config = [curr_config.task_id, best_cache_solution, curr_config.resource_allocation[1], curr_config.resource_allocation[2]]
        end_time = time.time()
        logger.info("发送强化学习决策配置为 %s\n采样 + 决策总耗时为 %.6f", str(new_config), end_time - start_time)
        cm.send_config(new_config)

        temp_time = time.time()
        time.sleep(RUNTIME - (temp_time - start_time))         # 每个阶段剩余时间
        end_time = time.time()
        online_profiling.reset()
        logger.info("阶段 %d 结束，总用时 %.6f 秒", i + 1, end_time - start_time)
        


def for_reinforcement_learning():
    print('Hello, world')
    time.sleep(300)                     # 等待warmup结束
    print('begin to work')

    time.sleep(20)                      
    cm = ProtoSystemManagement()
    curr_config = cm.receive_config()   # 获取当前任务及cache分配信息

    all_app = curr_config.task_id       # 获取任务名称
    num_resources = [int(np.sum(x)) for x in curr_config.resource_allocation]       # 获取资源分配信息

    epochs = 30                         # 训练轮数 为什么是30？
    online_profiling = OnlineProfile(all_app, num_resources)            # 在线采样

    file_ = open('log/reinforce.log', 'w', newline='')
    start_time = time.time()
    for i in range(epochs):
        if i%10 == 0:                   # 每隔10轮，判断是否需要重新采样 -> 工作负载可能发生变化，是根据history_reward来判断的
            print("maybe workload change")
        performance = curr_config.performance                          # 获取当前性能指标
        context_info = curr_config.context

        chosen_arm = [dict(zip(all_app, i)) for i in curr_config.resource_allocation]   # 获取当前分配资源信息
        utilization_dict = dict(zip(all_app, curr_config.cpu_utilization))              # 获取当前cpu使用率
        hitrate_dict = dict(zip(all_app, curr_config.hitrate))                          # 获取当前命中率
            
        th_reward, aver_latency= online_profiling.get_now_reward(performance, context_info) # get preformance
        online_profiling.update([th_reward, utilization_dict, hitrate_dict], chosen_arm)    # 会进行一个扰动
        new_arm = online_profiling.select_arm()

        # prepare new config
        if len(new_arm) != 0:
            new_config = [curr_config.task_id]
            new_config.extend(new_arm)
            cm.send_config(new_config)
        # write to log
        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        hitrates = hitrate_dict.values()
        hitrates = [float(x) for x in hitrates]
        aver_hit = sum(hitrates) / len(hitrates)
        log_info = '{}, epoch:{}: {} {} {}\n'.format(current_time, i, str(new_arm), aver_hit, aver_latency)     # 记录的是扰动前点的信息
        file_.write(log_info)
        print(log_info)


        # waiting for result
        time.sleep(20)
        curr_config = cm.receive_config()
        # curr_config.print()

    end_time = time.time()
    print('used time :{}'.format(end_time - start_time))
    file_.close()

def default_sample(file_path):
    '''作为一个单独的线程挂载在后端，统计每个工作负载变换后尾延迟表现情况，计算平均值并记录在log/cat_latency.log'''
    # file_ = open(file_path, 'w', newline='')
    logger = setup_logger(file_path)  # 使用 logging 记录日志
    time.sleep(WARMTIME)         # 等待warmup结束
    epoch = 108                     # 600(RUNTIME) - 60(决策)，5s采样一次尾延迟
    start_time = time.time()
    tasklist = [
        'tmdb_1',
        'tmdb_2',
        'leveldb_1',
        'leveldb_2',
        'mysql_1',
        'mysql_2',
        'sqlite_1',
        'sqlite_2',
        'mongodb_1',
        'mongodb_2',
    ]
    logger.info('----- sample thread detacts warmup phase finished ------')
    for phase in range(3):
        logger.info(" =========== PHASE %d ===========\n", phase + 1)
        logger.info('----- sample thread in new phase waiting\n')
        time.sleep(60)          # 每一轮的开始60s不计算尾延迟 -> 在做决策
        logger.info('----- sample thread in new phase begin\n')
        # log_info = '{}Phase {} Start to Cal tail latency'.format(current_time, phase + 1)
        # file_.write(log_info)
        logger.info("Phase %d Start to Cal tail latency\n", phase + 1)
        for i in range(epoch):
            time.sleep(5)          # 每5s计算一次尾延迟
            tail_latency = []
            log_files = ['/home/md/SHMCachelib/log/bin_' + x + '_subItem.log' for x in tasklist]
            for log in log_files:
                last_line = None
                while last_line is None or last_line == '':
                    if last_line is None:
                        last_line = get_last_line(log)
                    else:
                        print('Error: {} log error'.format(log))
                        time.sleep(5)
                        last_line = get_last_line(log)
                tail_latency.append(get_last_line(log))
            
            tail_latency = [float(x) for x in tail_latency]
            aver_latency = sum(tail_latency) / len(tail_latency)
            logger.info(" epoch:%d: %.4f", i, aver_latency)
    end_time = time.time()
    logger.info('used time :{}'.format(end_time - start_time))
    # file_.close()

if __name__ == '__main__':
    # default_sample('./baseline_latency.log')
    str_time = time.strftime("%Y%m%d_%H%M%S", time.localtime(time.time()))
    save_dir = f'./log/{str_time}'
    os.makedirs(save_dir)
    RL_file_path = os.path.join(save_dir, 'reinforce.log')
    latency_file_path = os.path.join(save_dir, 'RL_latency_cat.log')
    my_thread = threading.Thread(target=default_sample, args=(latency_file_path,))
    my_thread.start()
    for_RL_learning(RL_file_path)
    my_thread.join()


    
