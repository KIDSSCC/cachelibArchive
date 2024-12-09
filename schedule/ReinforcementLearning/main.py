from util import *
from datetime import datetime
from predication_model import *
from agent import *
import threading
import os
import logging
import argparse

parser = argparse.ArgumentParser(description="命令行参数解析")
parser.add_argument('mode', type=int, help='0:baseline;1:schedule')

WARMTIME = 300
RUNTIME = 600           # 注意和backend/src/main.cpp保持一致
cache_model_path = '/home/md/SHMCachelib/schedule/ReinforcementLearning/train_dir/Cache_num_task_10_20241207_184235/model_T10_I126.pt'
bandwidth_model_path = '/home/md/SHMCachelib/schedule/ReinforcementLearning/train_dir/BandWidth_num_task_10_20241208_153300/model_T15_I294.pt'
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

# 默认只在每个阶段的开始获取当前正在运行的任务task，其各个阶段对应特征曲线已知
def for_RL_learning(file_path):
    logger = setup_logger(file_path)  # 使用 logging 记录日志
    logger.info('--------- main thread detects warm up -----------')
    time.sleep(WARMTIME)                     # 等待warmup结束
    logger.info('--------- main thread begin to work -----------')
    cm = ProtoSystemManagement()
    curr_config = cm.receive_config()   # 获取当前任务及cache分配信息
    all_app = curr_config.task_id       # 获取任务名称
    num_resources = [int(np.sum(x)) for x in curr_config.resource_allocation]       # 获取当前资源分配信息

    for i in range(3):
        logger.info("========== Main thread in Phase %d ==========", i + 1)
        start_time = time.time()
        time.sleep(10)                      # 等待10s确保进入该阶段
        curr_config = cm.receive_config()   # 获取当前任务及cache分配信息
        curr_performance = curr_config.performance
        # 1. 根据当前任务名称获取对应曲线
        cache_state,cache_features, bandwidth_state, bandwidth_features, cpu_allocation = MapTaskNameToLines(all_app,
                                                                                              i + 1,
                                                                                              total_cache_num=num_resources[0], 
                                                                                              total_cpu_num=num_resources[1],
                                                                                              total_bw_num=num_resources[2])
        assert sum(cpu_allocation) == num_resources[1], "CPU分配有误"
        # 2. 将cache-命中率曲线发送给cache_agent进行决策;latency - bandwidth曲线发送给bandwidth_agent进行决策
        cache_state_dict = torch.load(cache_model_path, weights_only=True)
        bandwidth_state_dict = torch.load(bandwidth_model_path, weights_only=True)
        cache_agent = Agent()
        bandwith_agent = Agent()
        cache_agent.model.load_state_dict(cache_state_dict)
        bandwith_agent.model.load_state_dict(bandwidth_state_dict)
        logger.info("load cache model from %s, bandwidth model from %s", cache_model_path, bandwidth_model_path)
        cache_action_probs = cache_agent.get_action(cache_state)
        bandwidth_action_probs = bandwith_agent.get_action(bandwidth_state)
        logger.info("强化学习算法决策cache分配为: %s", str(cache_action_probs * num_resources[0]))
        logger.info("强化学习算法决策bandwidth分配为: %s", str(bandwidth_action_probs * num_resources[2]))
        # 3. 模拟退火算法对cache分配和bandwidth分配进行优化
        best_cache_solution, best_hitrate = cache_simulated_annealing( len(all_app), 
                                                                num_resources[0],
                                                                get_curr_avg_hitrate, 
                                                                x0 = cache_action_probs * num_resources[0],
                                                                features=cache_features,
                                                                T_max=100, T_min=1e-3, 
                                                                L=30, 
                                                                max_stay_counter=10, 
                                                                precision=0.5,
                                                                cooling_rate=0.95,
                                                                lb=2,
                                                                ub=num_resources[0],
                                                                change_precision=10
                                                                )
        assert sum(best_cache_solution) == num_resources[0], "cache分配有误"
        temp_time = time.time()
        logger.info("模拟退火算法调优后cache分配为: %s,\n预测最佳命中率为: %.4f, 决策总用时: %.8f: ", str(best_cache_solution),
                                                                                            best_hitrate,
                                                                                            temp_time - start_time)
        best_bandwidth_solution, best_latency = bandwidth_simulated_annealing( len(all_app), 
                                                            num_resources[2],
                                                            get_curr_avg_latency, 
                                                            x0 = bandwidth_action_probs * num_resources[2],
                                                            features=bandwidth_features,
                                                            T_max=100, T_min=1e-3, 
                                                            L=30, 
                                                            max_stay_counter=10, 
                                                            precision=0.5,
                                                            cooling_rate=0.95,
                                                            lb = 5,
                                                            ub= num_resources[2],
                                                            change_precision=10
                                                            )
        assert sum(best_bandwidth_solution) == num_resources[2], "带宽分配有误"
        temp_time = time.time()
        logger.info("模拟退火算法调优后bandwidth分配为: %s,\n预测最佳尾延迟为: %.4f, 决策总用时: %.8f: ", str(best_bandwidth_solution),
                                                                                            best_latency,
                                                                                            temp_time - start_time)
        # 4. 发送最终决策
        new_config = []
        new_config = [curr_config.task_id, best_cache_solution, cpu_allocation, best_bandwidth_solution]
        # 5. 等待该阶段结束
        end_time = time.time()
        logger.info("发送强化学习决策配置为 %s\n决策总耗时为 %.6f", str(new_config), end_time - start_time)
        cm.send_config(new_config)
        temp_time = time.time()
        time.sleep(RUNTIME - (temp_time - start_time))         # 每个阶段剩余时间
        end_time = time.time()
        logger.info("阶段 %d 结束，总用时 %.6f 秒", i + 1, end_time - start_time)
        

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
        'tmdb_3',
        'tmdb_4',
        'tmdb_5',
        'leveldb_1',
        'leveldb_2',
        'leveldb_3',
        'leveldb_4',
        'leveldb_5',
        'mysql_1',
        'mysql_2',
        'mysql_3',
        'mysql_4',
        'mysql_5',
        'sqlite_1',
        'sqlite_2',
        'sqlite_3',
        'sqlite_4',
        'sqlite_5',
        'mongodb_1',
        'mongodb_2',
        'mongodb_3',
        'mongodb_4',
        'mongodb_5',
    ]
    logger.info('----- sample thread detacts warmup phase finished ------')
    for phase in range(3):
        logger.info(" =========== PHASE %d ===========", phase + 1)
        logger.info('----- sample thread in new phase waiting')
        time.sleep(60)          # 每一轮的开始60s不计算尾延迟 -> 在做决策
        logger.info('----- sample thread in new phase begin')
        # log_info = '{}Phase {} Start to Cal tail latency'.format(current_time, phase + 1)
        # file_.write(log_info)
        logger.info("Phase %d Start to Cal tail latency", phase + 1)
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
    args = parser.parse_args()

    # 测试强化学习用
    # str_time = time.strftime("%Y%m%d_%H%M%S", time.localtime(time.time()))
    # save_dir = f'./log/{str_time}'
    # os.makedirs(save_dir)
    # RL_file_path = os.path.join(save_dir, 'reinforce.log')
    # latency_file_path = os.path.join(save_dir, 'RL_latency_cat.log')
    # my_thread = threading.Thread(target=default_sample, args=(latency_file_path,))
    # my_thread.start()
    # for_RL_learning(RL_file_path)
    # my_thread.join()

    if args.mode==0:
        latency_file_path = './baseline_latency.log'
        my_thread = threading.Thread(target=default_sample, args=(latency_file_path,))
        my_thread.start()
        my_thread.join()
    if args.mode == 1:
        str_time = time.strftime("%Y%m%d_%H%M%S", time.localtime(time.time()))
        save_dir = f'./log/{str_time}'
        os.makedirs(save_dir)
        RL_file_path = os.path.join(save_dir, 'reinforce.log')
        latency_file_path = os.path.join(save_dir, 'RL_latency_cat.log')
        my_thread = threading.Thread(target=default_sample, args=(latency_file_path,))
        my_thread.start()
        for_RL_learning(RL_file_path)
        my_thread.join()

    
