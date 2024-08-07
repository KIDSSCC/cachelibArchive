import socket
import subprocess
import threading
import time
import os
import pickle
from util import *



host = '127.0.0.1'
port = 54000
executable_path = '/home/md/SHMCachelib/Build/tmdb_test'
data_path = '/home/md/workloadData/'

sig_end = 0


# '''
# send the current config to the scheduler and wait the new config
# '''
# def resource_schedule(config):
#     client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#     client_socket.connect(('127.0.0.1', 1412))
#     serialized_config = pickle.dumps(config)
#     client_socket.send(serialized_config)
#     client_socket.close()
    

#     # wait the response
    
#     server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#     server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
#     server_socket.bind(('127.0.0.1', 1413))
#     server_socket.listen(1)

#     response_socket, response_address = server_socket.accept()
#     serialized_new_config = response_socket.recv(1024)
#     new_config = pickle.loads(serialized_new_config)
#     server_socket.close()
#     response_socket.close()
#     #new_partition = [config[1][0]-2, config[1][1], config[1][2]+2]
#     #new_config = [config[0], new_partition]
#     return new_config
    
# '''
# construct all partitions
# '''
# def get_all_partitions():
#     all_partitions = []
#     # for i in range(0, 97, 8):
#     #     for j in range(1, 5, 1):
#     #         for k in range(1, 31, 3):
#     #             all_partitions.append([i, j, k])
#     # for i in range(80, 104, 8):
#     # for j in range(1, 5, 1):
#     for k in range(1, 31, 3):
#         all_partitions.append([96, 4, k])
#     return all_partitions

# def print_1(line):
#     fileName = 'global_record.log'
#     print(line)
#     return
#     with open(fileName, 'a') as logFile:
#         print(line, file=logFile)


# def execute_tmdb_load(workload):
#     complete_workload = data_path + workload + '_load.txt'
#     executable_with_args = [executable_path, '-i', complete_workload, '-o', workload, '-l']
#     process = subprocess.Popen(executable_with_args, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
#     return process


# def execute_tmdb_run(wl):
#     file_run = data_path + wl + '_run.txt'
#     command_order = [executable_path, '-i', file_run, '-o', wl, '-r']

#     itea = 1
#     while sig_end == 0:
#         process = subprocess.Popen(command_order, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
#         std_out, _ = process.communicate()
#         # print_1('itea is:'+str(itea))
#         # print_bytes(wl, std_out)
#         # itea = itea+1
#         #if itea>2:
#         #    break
        
# def capture_log(pool_names, pids):
#     clear_groups()
#     # wait for prepare
#     prepare_file = 'tmdb_hotspot_1K_02prepare.log'
#     while not os.path.isfile(prepare_file):
#         time.sleep(5)
#     print('prepare finish')

#     # begin to schedule
#     all_partitions = get_all_partitions()
#     for i in range(len(all_partitions)):
#         # wait for output
#         log_name = 'tmdb_hotspot_1K_02_latency.log'
#         last_line = get_last_line(log_name)
#         while last_line=='' or last_line.split()[1] != 'Hitrate:':
#             time.sleep(30)
#             last_line = get_last_line(log_name)

#         # set new config
#         curr_config = []
#         curr_config.append(pool_names)
#         curr_config.append(pids)
#         # cache size
#         curr_config.append([all_partitions[i][0], 96 - all_partitions[i][0]])
#         # cpu cores
#         curr_config.append([all_partitions[i][1], all_partitions[i][1]])
#         # IO bandwidth
#         curr_config.append([all_partitions[i][2], 30 - all_partitions[i][2]])
#         set_pool_stats(curr_config)
#         # write to log
        
#         message = 'adjust config to: ' + str(all_partitions[i]) + '\n'
#         print(message)
#         with open(log_name, 'a') as f:
#                 f.write(message)

#         os.makedirs('scheduleMangement/', exist_ok=True)
#         schedule = 'scheduleMangement/findOptimal_' + str(i+1) + '_' + str(len(all_partitions)) + '.log'
#         file = open(schedule, 'w')
#         file.close()


# def print_bytes(name, result):
#     text = result.decode('utf-8')
#     logFile = 'time_record.log'
#     print_1('workload is: '+ name)
#     for line in text.splitlines():
#         print_1('\t'+ line)

def prepare(name):
    args = ['--prepare']
    process = subprocess.Popen([name] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return process

def run(name):
    warmup_times = 5
    run_times = 6
    profile_file = name
    args = ['--cache', '--warmup', str(warmup_times), '--run', str(run_times), '--profile', profile_file]
    process = subprocess.Popen([name] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return process

if __name__ == '__main__':
    par_path = 'bin/'
    app = [
        'mysql_5G_hotspot',
        'mysql_5G_uniform',
        'tmdb_5G_hotspot',
        'tmdb_10G_sequential'
    ]
    app = [par_path + wl for wl in app]

    # server process
    # server_process = subprocess.Popen(f'./Build/Server', shell=True)
    # time.sleep(5)
    # # prepare
    # prepare_procs = []
    # for wl in app:
    #     prepare_procs.append(prepare(wl))
    # for p in prepare_procs:
    #     p.wait()

    

    # run
    run_procs = []
    for wl in app:
        run_procs.append(run(wl))

    # TODO: cache size schedule
    time.sleep(5)
    print('begin to schedule')
    pool = [
        'mysql_hotspot_5G',
        'mysql_uniform_5G',
        'tmdb_hotspot_5G',
        'tmdb_sequential_10G'
    ]
    allocate = [90, 100, 2, 0]
    # p_map = get_pool_stats()
    # for key in p_map.keys():
    #     print('{}->{}'.format(key, p_map[key]))
    set_cache_size(pool, allocate)
    print('end to schedule')
    for p in run_procs:
        p.wait()

    # server_process.wait()
