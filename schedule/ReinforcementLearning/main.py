from util import *
from datetime import datetime
from predication_model import *
import threading

WARMTIME = 200
RUNTIME = 200           # 注意和backend/src/main.cpp保持一致

def for_RL_learning():
    print('---------warm up-----------')
    time.sleep(WARMTIME + 2)                     # 等待warmup结束，多2s确保进入phase1
    print('begin to work')
    cm = ProtoSystemManagement()
    curr_config = cm.receive_config()   # 获取当前任务及cache分配信息
    all_app = curr_config.task_id       # 获取任务名称
    num_resources = [int(np.sum(x)) for x in curr_config.resource_allocation]       # 获取当前资源分配信息
    online_profiling = OnlineProfile(all_app, num_resources)            # 在线采样

    file_ = open('reinforce.log', 'w', newline='')

    for i in range(3):
        log_info = '----------- Phase {} -----------\n'.format(i + 1)
        file_.write(log_info)
        start_time = time.time()
        curr_config = cm.receive_config()   # 获取当前任务及cache分配信息
        # 1.第一个采样点
        performance = curr_config.performance                          # 获取当前性能指标
        print('performance: ', performance)
        context_info = curr_config.context
        chosen_arm = [dict(zip(all_app, i)) for i in curr_config.resource_allocation]   # 获取当前分配资源信息
        utilization_dict = dict(zip(all_app, curr_config.cpu_utilization))              # 获取当前cpu使用率
        print('utilization: ', utilization_dict)
        hitrate_dict = dict(zip(all_app, curr_config.hitrate))                          # 获取当前命中率
        print('hitrate: ', hitrate_dict)
        hitrates = [float(value) for value in hitrate_dict.values()]                    # 确保是数值类型
        first_point = zip(curr_config.resource_allocation, hitrates)                    # 第一个点的信息
        aver_hit = sum(hitrates) / len(hitrates)
        th_reward, aver_latency= online_profiling.get_now_reward(performance, context_info) # 记录一下采集到的第一个点的相关信息
        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")                     # write to log
        log_info = '{} get first allocation: {} ,\n avg_hitrate is {}, avg_latency is {}\n'.format(current_time, str(chosen_arm), aver_hit, aver_latency)
        file_.write(log_info)
        # 2.进行随机扰动，生成第二个采样点
        online_profiling.update([th_reward, utilization_dict, hitrate_dict], chosen_arm)    # 会进行一个扰动，生成一个新的分配
        new_arm = online_profiling.select_arm()
        new_config = [curr_config.task_id]
        new_config.extend(new_arm)
        cm.send_config(new_config)
        time.sleep(20)      # 等待20s使得扰动生效
        curr_config = cm.receive_config()
        performance = curr_config.performance                          # 获取当前性能指标
        context_info = curr_config.context
        chosen_arm = [dict(zip(all_app, i)) for i in curr_config.resource_allocation]   # 获取当前分配资源信息
        utilization_dict = dict(zip(all_app, curr_config.cpu_utilization))              # 获取当前cpu使用率
        hitrate_dict = dict(zip(all_app, curr_config.hitrate))                          # 获取当前命中率
        hitrates = hitrate_dict.values()
        second_point = zip(curr_config.resource_allocation, hitrates)                   # 第二个点的信息
        aver_hit = sum(hitrates) / len(hitrates)
        th_reward, aver_latency= online_profiling.get_now_reward(performance, context_info) # 记录一下采集到的第二个点的相关信息
        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")                     # write to log
        log_info = '{} get second allocation: {} ,\n avg_hitrate is {}, avg_latency is {}\n'.format(current_time, str(chosen_arm), aver_hit, aver_latency)
        file_.write(log_info)
        # 3.进行模拟退火，生成最终方案
        best_cache_solution, best_hitrate = simulated_annealing2( len(all_app), num_resources[0],
                                                            get_curr_avg_hitrate, 
                                                            x0=point1[0],features=workload_features,
                                                            T_max=100, T_min=1e-3, L=150, max_stay_counter=200, precision=0.5,
                                                            cooling_rate=0.95,lb=2,ub=total_cache,change_precision=10
                                                          )
        temp_time = time.time()
        log_info = 'simulated_annealing2 cal best cache solution: {},\n best hitrate: {}, use time: {}\n'.format(best_cache_solution,
                                                                                                                best_hitrate, 
                                                                                                                temp_time - start_time)
        # 4.使得方案生效 ->暂时只改了cache
        new_config = [curr_config.task_id, best_cache_solution, curr_config.resource_allocation[1], curr_config.resource_allocation[2]]
        cm.send_config(new_config)

        temp_time = time.time()
        time.sleep(RUNTIME - (temp_time - start_time))         # 每个阶段剩余时间
        end_time = time.time()
        print('Total used time :{}'.format(end_time - start_time))
    file_.close()


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

def default_sample():
    '''作为一个单独的线程挂载在后端，统计每个工作负载变换后尾延迟表现情况，计算平均值并记录在log/cat_latency.log'''
    time.sleep(200)         # 等待warmup结束
    epoch = 10
    start_time = time.time()
    file_ = open('log/cat_latency.log', 'w', newline='')
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
    print('----- finish warmup phase')
    for _ in range(3):
        print('----- new phase waiting')
        time.sleep(200)
        print('----- new phase begin')
        for i in range(epoch):
            time.sleep(20)          # 等待20s ->扰动采样？
            tail_latency = []
            log_files = ['/home/md/SHMCachelib/log/bin_' + x + '_meta.log' for x in tasklist]
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
            log_info = 'epoch:{}: {} \n'.format(i, aver_latency)
            file_.write(log_info)
            print(log_info)
    end_time = time.time()
    print('used time :{}'.format(end_time - start_time))
    file_.close()

if __name__ == '__main__':
    # for_epsilon_greedy()
    my_thread = threading.Thread(target=default_sample)
    my_thread.start()
    # for_reinforcement_learning()
    # my_thread.join()
    # default_sample()
    for_RL_learning()
    my_thread.join()


    
