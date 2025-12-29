# cachelibArchive
## 2024/04/13
- 目前完成系统基本功能实现
- 服务端提供多线程支持
- 统计缓存命中率

## 2024/09/17
- workload 批量编译

backend/src/config下保存的多个配置文件，将在编译期间依次进行编译，生成工作负载

## 一.项目编译
项目根目录下执行：

~~~bash
./script/build.sh
~~~

## 二. 服务端启动
Server端参数配置可通过配置文件或命令行参数两种方式实现。

- 配置文件：clientserver/config.h
    - CACHE_SIZE：默认缓存实例大小
    - POOL_SIZE：默认缓存池大小
    - CREATE_DEFAULT_POOL：是否启用全局缓存
    - SIZE_CONV：缓存调度粒度
    - MAX_WAIT：Server端自旋锁最长等待时间
- 命令行参数：
    - -c：默认缓存实例大小，单位MB
    - -p：默认缓存池大小，单位MB
    - -d：是否启用全局缓存
    - -g：缓存调度粒度

Server启动，在根目录下执行：
~~~bash
./Build/Server -c 20480 -p 2048
~~~



