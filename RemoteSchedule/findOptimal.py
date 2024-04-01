import socket
import subprocess
import threading
import time
import os
import pickle

host = '127.0.0.1'
port = 54000

data_path = '/home/md/MicroData2/'
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


def print_bytes(name, result):
    text = result.decode('utf-8')
    logFile = 'time_record.log'
    print_1('workload is: '+ name)
    for line in text.splitlines():
        print_1('\t'+ line)

def print_1(line):
    fileName = 'global_record.log'
    print(line)
    return
    with open(fileName, 'a') as logFile:
        print(line, file=logFile)


    

if __name__ == '__main__':
    example = [17, 1, 12]
    all_partition = list(range(3))
    '''
    all_partition = [[10, 10, 10], [6, 18, 6], [6, 17, 7], [4, 13, 13], [6, 16, 8], [10, 13, 7], 
                    [10, 10, 10], [6, 18, 6], [6, 17, 7], [4, 13, 13], [6, 16, 8], [10, 13, 7], 
                    [10, 10, 10], [6, 18, 6], [6, 17, 7], [4, 13, 13], [6, 16, 8], [10, 13, 7]]
    all_partition = []
    for x in range(0, 31):
        for y in range(0, 30-x+1):
            pari = [x, y, 30-x-y]
            all_partition.append(pari)
    '''
    #test_partition = [[6, 8, 10]]
    workloads = ['wlzipfian', 'wluniform', 'wlhotspot']
    server_path = ['/home/md/SHMCachelib/Build/Server']

    # run server
    server_proc = subprocess.Popen(server_path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    time.sleep(1)

    for i  in range(len(all_partition)):
        print_1('--------------------------------------------------')
        print_1(all_partition[i])
                
        # add all pool
        for wl in workloads:
            proc = execute_tmdb_addpool(wl)
            proc.communicate()

        # resize the pool
        # set_pool_stats([workloads, all_partition[i]])
        
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

        
        schedule = 'scheduleMangement/findOptimal_' + str(i+1) + '_' + str(len(all_partition)) + '.log'
        file = open(schedule, 'w')
        file.close()
    # stop server
    server_proc.terminate()
