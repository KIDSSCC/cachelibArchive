import logging
import time
import subprocess
import socket
import os

# 配置日志输出的基本格式和日志级别
logging.basicConfig(level=logging.INFO)

bin_path = '/home/md/cachelibArchive/Build/'
pool_size = 4096

def operation(args):
    process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return process

def get_binary():
    # # Build目录下获取可执行文件，随机打乱后从中选择5个任务
    bin_files = [
        ['bin_tmdb_1'],
        ['bin_tmdb_2'],
        ['bin_tmdb_3'],
        ['bin_tmdb_4'],
    ]

    # 默认每个任务4G缓存
    cache_size = pool_size * len(bin_files)

    logging.info('----- Target workloads:')
    for item in bin_files:
        logging.info(item)
    logging.info('----- Target workloads End')
    return bin_files, cache_size

def cache_server(cache_size, pool_size, default_pool, size_conv=None):
    args = ['./Build/Server']
    if cache_size is not None:
        args.append('-c')
        args.append(str(cache_size))
    if pool_size is not None:
        args.append('-p')
        args.append(str(pool_size))
    if size_conv is not None:
        args.append('-g')
        args.append(str(size_conv))
    if default_pool == 1:
        args.append('-d')
        args.append('1')
    
    out_filename = 'log/server_out.log'
    err_filename = 'log/server_err.log'
    with open(out_filename, 'w') as out_file, open(err_filename, 'w') as err_file:
        process = subprocess.Popen(args, stdout=out_file, stderr=err_file)
    logging.info('----- Start Cache Server')
    time.sleep(5)
    ret_code = process.poll()
    return process, ret_code

def prepare_phase(target_workloads):
    logging.info('----- Begin To Prepare')
    start_time = time.time()
    prepare_procs = []
    for wl in target_workloads:
        prepare_procs.append(operation([os.path.join(bin_path, wl[0]), '--prepare']))
    for index, p in enumerate(prepare_procs):
        logging.info('{} Prepare Start'.format(target_workloads[index]))
        stdout, _ = p.communicate()
        lines = stdout.decode('utf-8').strip().split('\n')
        if not any('Preparation done' in s for s in lines):
            logging.error('{} Preparation failed'.format(target_workloads[index]))
    end_time = time.time()
    logging.info('----- Prepare Time: {}'.format(end_time - start_time))
    logging.info('----- Prepare Done')

def run_phase(target_workloads):
    logging.info('----- Begin To Run')
    start_time = time.time()
    procs = []
    for wl in target_workloads:
        tmp = [os.path.join(bin_path, wl[0]), '--cache']
        tmp.extend(['--profile', 'log/'])
        tmp[-1] = tmp[-1] + wl[0]
        procs.append(operation(tmp))

    for index, p in enumerate(procs):
        logging.info('Waiting {}'.format(target_workloads[index][0]))
        stdout, stderr = p.communicate()
    end_time = time.time()
    logging.info('----- Run Time: {}'.format(end_time - start_time))
    logging.info('----- Run Done')

def close_server():
    host = '127.0.0.1'
    port = 54000
    message = "E:"
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    sock.sendall(message.encode())

if __name__ == '__main__':
    target_workloads, cache_size = get_binary()

    # prepare阶段
    # prepare_phase(target_workloads)

    # 启动cache server
    server_process, ret_code = cache_server(cache_size, pool_size, 0, 64)
    if ret_code is not None:
        logging.error('----- Cache Server Failed')
        exit(1)

    run_phase(target_workloads)
    close_server()
    server_process.communicate()