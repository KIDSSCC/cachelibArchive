import socket
import subprocess
import threading
import time
import os
import pickle

host = '127.0.0.1'
port = 54000

data_path = '/home/md/workloadData/'
executable_path = '/home/md/SHMCachelib/Build/tmdb_test'

def execute_tmdb_addpool(workload):
    complete_workload = data_path + workload + '_load.txt'
    executable_with_args = [executable_path, '-i', complete_workload, '-o', workload, '-a']
    process = subprocess.Popen(executable_with_args, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    return process

def execute_tmdb_load(workload):
    complete_workload = data_path + workload + '_load.txt'
    executable_with_args = [executable_path, '-i', complete_workload, '-o', workload, '-l']
    process = subprocess.Popen(executable_with_args, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    return process


def execute_tmdb_run(workload):
    complete_workload = data_path + workload + '_run.txt'
    executable_with_args = [executable_path, '-i', complete_workload, '-o', workload, '-r']
    process = subprocess.Popen(executable_with_args, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    return process

def execute_tmdb_run_allthetime(wl):
    file_run = data_path + wl + '_run.txt'
    command_order = [executable_path, '-i', file_run, '-o', wl, '-r']

    itea = 1
    while(True):
        process = subprocess.Popen(command_order, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        std_out, _ = process.communicate()
        print_1('itea is:'+str(itea))
        print_bytes(wl, std_out)
        itea = itea+1
        if itea>2:
            break
 

'''
set pool stat
'''
def set_pool_stats(new_config):
    # link the target program
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    serialized_data = '\n'.join([' '.join(map(str, row)) for row in new_config])
    serialized_data = 'S:' + serialized_data
    sock.sendall(serialized_data.encode())
    sock.close()
'''
get the last line from a file
'''
def get_last_line(fileName):
    with open(fileName, 'rb') as file:
        file.seek(-2,2)
        while file.read(1)!=b'\n':
            file.seek(-2, 1)
        return file.readline().decode().strip()

def capture_log(workloads):
    itea = 1
    while itea < 5:
        time.sleep(10)
        for wl in workloads:
            hitrate_log_name = wl + '_CacheHitRate.log'
            hitrate_last_line = get_last_line(hitrate_log_name)
            print(wl, hitrate_last_line)
        itea = itea + 1


def print_bytes(name, result):
    text = result.decode('utf-8')
    logFile = 'time_record.log'
    print_1('workload is: '+ name)
    for line in text.splitlines():
        print_1('\t'+ line)

def print_1(line):
    fileName = 'global_record.log'
    # print(line)
    # return
    with open(fileName, 'a') as logFile:
        print(line, file=logFile)


    

def run_partition():
    all_partition = []
    for i in range(0, 65):
        all_partition.append([i, 64-i])
    workloads = ['F', 'G', 'H']
    server_path = ['/home/md/SHMCachelib/Build/Server']

    # run server
    

    for i  in range(len(all_partition)):
        server_proc = subprocess.Popen(server_path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        time.sleep(3)
        print_1('--------------------------------------------------')
        print_1(all_partition[i])
                
        # add all pool
        for wl in workloads:
            proc = execute_tmdb_addpool(wl)
            proc.communicate()

        # resize the pool
        set_pool_stats([workloads, all_partition[i]])
        
        # load phase
        all_load_proc = []
        for wl in workloads:
            all_load_proc.append(execute_tmdb_load(wl))
            time.sleep(0.2)
        for wl, proc in zip(workloads, all_load_proc):
            stdout, stderr = proc.communicate()
            print_bytes(wl, stdout)

        # run phase
        all_run_proc = []
        for wl in workloads:
            all_run_proc.append(execute_tmdb_run(wl))
            time.sleep(0.2)
        for wl, proc in zip(workloads, all_run_proc):
            stdout, stderr = proc.communicate()
            print_bytes(wl, stdout)

        for wl in workloads:
            hitrate_log_name = wl + '_CacheHitRate.log'
            hitrate_last_line = get_last_line(hitrate_log_name)
            print_1(wl)
            print_1(hitrate_last_line)
            

        
        schedule = 'scheduleMangement/findOptimal_' + str(i+1) + '_' + str(len(all_partition)) + '.log'
        file = open(schedule, 'w')
        file.close()
        # stop server
        server_proc.terminate()
        time.sleep(3)

def run_together():
    workloads = ['D', 'E']
    server_path = ['/home/md/SHMCachelib/Build/Server']
    server_proc = subprocess.Popen(server_path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    time.sleep(3)
    print_1('--------------------------------------------------')


    print_1('-----Load Phase-----')
    # load phase
    all_load_proc = []
    for wl in workloads:
        all_load_proc.append(execute_tmdb_load(wl))
        time.sleep(0.2)
    for wl, proc in zip(workloads, all_load_proc):
        stdout, stderr = proc.communicate()
        print_bytes(wl, stdout)
    # run phase
    all_run_thread = []
    for wl in workloads:
        all_run_thread.append(threading.Thread(target = execute_tmdb_run_allthetime, args = (wl,)))
        all_run_thread[-1].start()

    capture_latency = threading.Thread(target = capture_log, args = (workloads,))
    capture_latency.start()
    capture_latency.join()

    for t in all_run_thread:
        t.join()
    server_proc.terminate()
    time.sleep(3)


if __name__ == '__main__':
    # run_together()
    run_partition()
