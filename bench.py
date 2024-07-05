import socket
import subprocess
import threading
import time
import os
import pickle
from RemoteSchedule.resource_management import set_cache_size, set_cpu_cores, set_bandwidth, clear_groups

def run_workload(name):
    process = subprocess.Popen(name, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return process

if __name__ == '__main__':
    server_path = ['/home/md/SHMCachelib/Build/Server']
    workload_prepare = ['/home/md/SHMCachelib/Build/YCSB_CPP', '--cache', '--prepare']
    workload_run = ['/home/md/SHMCachelib/Build/YCSB_CPP', '--cache', '--run']
    for i in range(-4, 97, 4):
        # start server
        server_proc = subprocess.Popen(server_path, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
        time.sleep(3)
        # start workload
        if i<0:
            workload = workload_prepare
        else:
            workload = workload_run
        process = run_workload(workload)
        time.sleep(3)

        if i>=0:
            pool_name = ['tmdb_uniform_1K']
            cache_size = [i]
            set_cache_size(pool_name, cache_size)

        log_name = 'tmdb_uniform_1K_latency.log'
        message = 'adjust config to: ' + str(i) + '\n'
        print(message)
        with open(log_name, 'a') as f:
                f.write(message)
        process.communicate()

        host = '127.0.0.1'
        port = 54000
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
        message = "E:"
        sock.sendall(message.encode())

        stdout, stderr = server_proc.communicate()
        server_log_name = 'server.log'
        with open(server_log_name, 'a') as f:
            f.write(stdout.decode())
        time.sleep(5)
        print('finish {}'.format(i))
