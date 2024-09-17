#!/bin/bash

# 检查是否提供了两个参数
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <A> <B>"
    exit 1
fi

# 提取命令行参数
A=$1
B=$2

# 执行 pqos 命令
sudo pqos -R
sudo pqos -e "mba@0:1=$A"
sudo pqos -a "core:1=$B"
sudo pqos -s

