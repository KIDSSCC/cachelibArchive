# YCSB-CPP

Reimplement the YCSB (Yahoo! Cloud Serving Benchmark) in C++.

This project is meant for experimenting with caching layers for the YCSB benchmark.

Currently supported databases:
- LevelDB
- MySQL
- MongoDB
- Sqlite3

The cache layers' implementations can be found at [KIDSSCC/cachelibArchive](https://github.com/KIDSSCC/cachelibArchive), which is based on cachelib.

## Requirements

### [LevelDB](https://github.com/google/leveldb)

build and install LevelDB
```bash
git clone --recurse-submodules https://github.com/google/leveldb.git
cd leveldb
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
cmake --install .
```

### [MySQL](https://dev.mysql.com/doc/refman/8.3/en/installing.html)

install MySQL server and mysql-connector-cpp

you can use apt-get to install
```bash
sudo apt-get install mysql-server libmysqlcppconn-dev
```

then configure MySQL server as you like

**note in CMakeLists.txt, the libmysqlcppconn is hardcoded to /usr/include/cppconn, you may need to change it to your own path**

### [MongoDB](https://docs.mongodb.com/manual/tutorial/install-mongodb-on-ubuntu/)

follow the instructions [here](https://www.mongodb.com/docs/manual/installation/) to install MongoDB, and instructions [here](https://www.mongodb.com/docs/languages/cpp/cpp-driver/current/installation/) to install the MongoDB C++ driver

### [Sqlite3](https://www.sqlite.org/download.html)

install sqlite3 and libsqlite3-dev using apt (note: multithreaded version is required)
```bash
sudo apt install sqlite3 libsqlite3-dev
```

### optional dependencies
    
this repository includes a fake header of cache layer, so you can build the project without the [KIDSSCC/cachelibArchive](https://github.com/KIDSSCC/cachelibArchive), to build the project with the cache layer, follow the instructions from the [KIDSSCC/cachelibArchive](https://github.com/KIDSSCC/cachelibArchive)

## Build

```bash
git clone https://github.com/SynodicMonth/YCSB-CPP.git
cd YCSB-CPP
mkdir -p build && cd build
cmake .. && cmake --build .
```

## Usage

Edit `src/benchmarks/config.h to configure the backend and benchmark settings, then compile the project.

```bash
cd build
cmake .. && cmake --build .
```

Then run the benchmark using command below, the benchmark will prepare the database and run the workload

```bash
./YCSB-CPP --threads <num_threads>
```

To enable the cache layer, use

```bash
./YCSB-CPP --threads <num_threads> --cache
```

If you want to seperate the prepare and run phase, use

```bash
./YCSB-CPP --prepare
./YCSB-CPP --run --threads <num_threads> --cache
```

**Note: if the benchmark inserts data in the runnning phase, it will automatically remove the data inserted when the program exits. But if its interrupted or crashed, the data will remain in the database, so you need to prepare the database again.**

## Config

The benchmark settings can be configured in `src/benchmarks/config.h`

You can change the backend by defining the `BACKEND` macro to the backend you want to use
```cpp
// #define BACKEND MySQLBackend
// #define BACKEND MongoDBBackend
#define BACKEND LevelDBBackend
// #define BACKEND SQLiteBackend
```

These macros control how the benchmark will run
```cpp
#define MAX_RECORDS 1000 // number of records in the database
#define MAX_FIELDS 10 // number of fields in each record
#define MAX_FIELD_SIZE 100 // size of each field (in chars)
#define MAX_QUERIES 100000 // number of queries to execute
#define QUERY_PROPORTION 0.98 // proportion of read queries, the rest are insert queries
```

There are 5 distributions for the queries, `DISTRIBUTION_ZIPFIAN`, `DISTRIBUTION_UNIFORM`, `DISTRIBUTION_SEQUENTIAL`, `DISTRIBUTION_HOTSPOT`, `DISTRIBUTION_EXPONENTIAL`, you can change the distribution by defining the `DISTRIBUTION` macro

```cpp
#define DISTRIBUTION DISTRIBUTION_ZIPFIAN // distribution of the keys
```

some distributions require additional parameters, you can change the parameters accordingly

```cpp
#if DISTRIBUTION == DISTRIBUTION_ZIPFIAN
    #define ZIPFIAN_SKEW 0.99
#elif DISTRIBUTION == DISTRIBUTION_HOTSPOT
    // p(x \in [0, HOTSPOT_PROPORTION * N)) = HOTSPOT_ALPHA
    // p(x \in [HOTSPOT_PROPORTION * N, N)) = (1 - HOTSPOT_ALPHA)
    #define HOTSPOT_PROPORTION 0.2
    #define HOTSPOT_ALPHA 0.8
#elif DISTRIBUTION == DISTRIBUTION_EXPONENTIAL
    #define EXPONENTIAL_LAMBDA 0.1
#endif
```

Note: We define hotspot as a piecewise function, where the first `HOTSPOT_PROPORTION` of the keys have a probability of `HOTSPOT_ALPHA` and the rest have a probability of `1 - HOTSPOT_ALPHA`. Which can be fomulated as:

$$
P(x) = \begin{cases}
    \alpha & \text{if } x \in [0, Prop \times N) \\
    1 - \alpha & \text{if } x \in [Prop \times N, N)
\end{cases}
$$
, where $N$ is the number of records, $Prop$ is the proportion of the keys, and $\alpha$ is the probability of the first $Prop$ keys.