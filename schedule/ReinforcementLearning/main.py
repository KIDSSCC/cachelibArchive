from util import *
from datetime import datetime
from predication_model import *


def for_reinforcement_learning():
    print('Hello, world')
    time.sleep(200)
    print('begin to work')

    time.sleep(20)
    cm = ProtoSystemManagement()
    curr_config = cm.receive_config()

    all_app = curr_config.task_id
    num_resources = [int(np.sum(x)) for x in curr_config.resource_allocation]

    epochs = 30
    online_profiling = OnlineProfile(all_app, num_resources)

    file_ = open('linucb.log', 'w', newline='')
    start_time = time.time()
    for i in range(epochs):
        if i%10 == 0:
            print("maybe workload change")
        performance = curr_config.performance
        context_info = curr_config.context

        chosen_arm = [dict(zip(all_app, i)) for i in curr_config.resource_allocation]
        utilization_dict = dict(zip(all_app, curr_config.cpu_utilization))
        hitrate_dict = dict(zip(all_app, curr_config.hitrate))
            
        th_reward, aver_latency= online_profiling.get_now_reward(performance, context_info)
        online_profiling.update([th_reward, utilization_dict, hitrate_dict], chosen_arm)
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
        log_info = '{}, epoch:{}: {} {} {}\n'.format(current_time, i, str(new_arm), aver_hit, aver_latency)
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
    '''作为一个单独的线程挂载在后端，统计每个工作负载变换后尾延迟表现情况，计算平均值并记录'''
    time.sleep(200)         # 等待warmup结束
    epoch = 10
    file_ = open('log/cat_latency.log', 'w', newline='')
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
    print('----- finish warmup phase')
    for _ in range(3):
        print('----- new phase waiting')
        time.sleep(60)     # 只统计后九分钟
        print('----- new phase begin')
        for i in range(epoch):
            time.sleep(20)  # 等待20s新配置生效
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
            log_info = 'epoch:{}: {} \n'.format(i, aver_latency)
            file_.write(log_info)
            print(log_info)
    end_time = time.time()
    print('used time :{}'.format(end_time - start_time))
    file_.close()

        


if __name__ == '__main__':
    # for_epsilon_greedy()
    # my_thread = threading.Thread(target=default_sample)
    # my_thread.start()
    for_reinforcement_learning()
    # my_thread.join()
    # default_sample()

    
