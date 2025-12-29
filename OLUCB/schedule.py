from OLUCB import *
from EpsilonGreedy import *
from util import *
from datetime import datetime
import os

log_path = 'schedule.log'

def for_OLUCB(log_id='1.log'):
    time.sleep(20)

    cm = ProtoSystemManagement()
    curr_config, _ = cm.receive_config()
    curr_config.print()

    all_app = curr_config.task_id
    num_resources = [int(np.sum(x)) for x in curr_config.resource_allocation]
    print('all app is: ', str(all_app))
    print('num_resource is ', str(num_resources))

    alpha = 0.95
    factor_alpha = 0.98
    n_features = 10
    ucb_cache = OLUCB(all_app, num_resources, alpha, factor_alpha, n_features, True)

    file_ = open(log_path, 'w', newline='')
    i = -1

    terminate_state = False
    finish = False
    new_arm = [[]]
    while True:
        i = i + 1
        performance = curr_config.performance
        throughput = curr_config.throughoput

        chosen_arm = [dict(zip(all_app, i)) for i in curr_config.resource_allocation]
        hitrate_dict = dict(zip(all_app, curr_config.hitrate))
        
        if i > 5:
            finish, workload_change= ucb_cache.update([hitrate_dict], chosen_arm)
            if workload_change:
                terminate_state = False

            new_arm = ucb_cache.select_arm()
            if not terminate_state and len(new_arm) !=0:
                # prepare new config
                new_config = [curr_config.task_id]
                new_config.extend(new_arm)
                cm.send_config(new_config)
        
        # write to log
        current_time = datetime.now().strftime("%H:%M:%S")
        hitrates = hitrate_dict.values()
        hitrates = [float(x) for x in hitrates]
        performance = [float(x) for x in performance]
        throughput = [float(x) for x in throughput]


        aver_hit = sum(hitrates) / len(hitrates)
        aver_latency = sum(performance) / len(performance)
        aver_throughput = sum(throughput) / len(throughput)
        log_info_1 = '{}, epoch:{}, hitrate: {:.3%}, latency: {:.2f}, throughput: {:.2f}\n'.format(current_time, i, aver_hit, aver_latency, aver_throughput)

        file_.write(log_info_1)
        file_.flush()
        print(log_info_1)
        if finish:
            terminate_state = True
            # print('{}, finished schedule'.format(current_time))
            # print('----- Resource Allocation -----')
            # print('task_id: {}'.format(curr_config.task_id))
            # print('cache allocation: {}'.format(new_arm[0]))
            # print('----- Resource allocation end -----')

        # waiting for result
        time.sleep(20)
        curr_config, stop = cm.receive_config()
        if stop:
            break

    print('OLUCB finished!')
    file_.close()

def default_sample(log_id='1.log'):
    time.sleep(300)
    print('----- finish warmup phase')
    log_name = 'log/' + str(log_id)
    file_ = open(log_name, 'a', newline='')
    start_time = time.time()

    # 获取当前正在运行的所有任务
    tasklist = []
    for root, _, files in os.walk('/home/md/SHMCachelib/log'):
        for file in files:
            if file.endswith('subItem.log'):
                tasklist.append(os.path.join(root, file))
    print('----- num of task is {}'.format(len(tasklist)))
    epoch = 60
    for _ in range(3):
        current_time = datetime.now().strftime("%H:%M:%S")
        print('{}, new phase waiting'.format(current_time))
        time.sleep(300)
        print('----- new phase begin')
        for i in range(epoch):
            time.sleep(5)
            tail_latency = []
            for log in tasklist:
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
            aver_latency = round(sum(tail_latency) / len(tail_latency), 3)
            log_info = 'epoch:{}: {} \n'.format(i, aver_latency)
            file_.write(log_info)
            file_.flush()
            # print(log_info)
    end_time = time.time()
    print('used time :{}'.format(end_time - start_time))
    file_.close()

        


if __name__ == '__main__':
    for_OLUCB()
