#include <stdio.h>
#include <string.h>
#include <time.h>
#include "tmdb.h"

#include<iostream>
#include<vector>
#include<random>
#include<fstream>


#include<thread>
#include<sys/time.h>
#include<unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <chrono>
using namespace std;
//#define CACHELIB_LOCAL

#define PRINT_LATENCY


double timeval_to_seconds(const timeval& t) {
    return t.tv_sec + t.tv_usec / 1000000.0;
}

double getUsedTime(const timeval& st,const timeval& en)
{
	double startTime=timeval_to_seconds(st);
	double endTime=timeval_to_seconds(en);
	return endTime-startTime;
}
long long int timeval_diff_nsec(const std::chrono::time_point<std::chrono::high_resolution_clock>& start, const std::chrono::time_point<std::chrono::high_resolution_clock>& end) {
    std::chrono::nanoseconds duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return duration.count();
}
//-----------------------------
//
//    test functions
//
//-----------------------------

int _db_insert_test(int total, char *df);
int _db_read_test(int total, char *df);

int kidsscc_db_insert_test(int total,char*df);
int kidsscc_db_read_test(int total,char*df);

using namespace std;
string prefix (1000, 'A');

/**
 * data insert
 *
 * @param int total insert total
 * @param char *df data file name
 */
int _db_insert_test(int total, char *df){
	int success = 0;
	int s = 0;

	if (total <= 0){
		return 0;
	}

	char mode[]="c";
	TDB *db = tdb_open(df, mode);
	if ( !db ){
		printf("db_open() %s fail.\n", df);
		return 0;
	}
	
	clock_t start, end;   
	double duration;   
	start=clock();
	
	for (int i=0; i<total; i++){
		string key = "num_" + std::to_string(i);
		string value(10,char('A'+i%26));
		s = tdb_store(db, key.c_str(), value.c_str(), TDB_INSERT);
		if (TDB_SUCCESS == s){
			std::cout<<"set: "<<key<<" --> "<<value<<std::endl;
			success++;
		}
	}
	tdb_close(db);
	end=clock();   
	duration=(double)(end-start)/CLOCKS_PER_SEC;   
	printf("db insert data test success: %d, used time: %fs\n", success, duration);
	return success;
}

/**
 * data read
 *
 * @param int total read total
 */
int _db_read_test(int total, char *df){
	int success = 0;

	if (total <= 0){
		return 0;
	}

	char mode[]="r";
	TDB *db = tdb_open(df, mode);
	if ( !db ){
		printf("db_open() %s fail.\n", df);
		return 0;
	}

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dis(0,total-1);

	clock_t start, end;   
	double duration;   
	start=clock();

	
	for (int i=0; i<total; i++){
		int num=dis(gen);
		string key = "num_" + std::to_string(num);
		string value(10,char('A'+num%26));
		string getValue = tdb_fetch(db, key.c_str());
		std::cout<<"key is: "<<key<<" get value is: "<<getValue<<std::endl;
		if (getValue == value){
			success++;
		}
	}
	tdb_close(db);

	end=clock();   
	duration=(double)(end-start)/CLOCKS_PER_SEC;   
	printf("db read data test success: %d, used time: %fs\n", success, duration);

	return success;
}

int kidsscc_db_insert_test(int total,char *df)
{
	char mode[]="c";
	TDB *db = tdb_open(df, mode);
	if ( !db )
	{
		cout<<"open db "<<df<<" failed\n";
		return 0;
	}
	cout<<"begin to set\n";
	timeval start, end;
    gettimeofday(&start, NULL);

	int success=0;
	for(int i=0;i<total;i++)
	{
		string key="num_"+to_string(i);
		string value(1023,char('A'+i%26));
		int s=tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
		if(s==TDB_SUCCESS)
			success++;
	}
	gettimeofday(&end, NULL);
	double start_seconds = timeval_to_seconds(start);
    double end_seconds = timeval_to_seconds(end);
    double elapsed_seconds = end_seconds - start_seconds;
	
	tdb_close(db);
	cout<<"db insert data test success: "<<success<<" failed: "<<total-success<<" use time:"<<elapsed_seconds<<endl;
	return success;
}

int kidsscc_db_read_test(int total, char*df)
{
	char mode[]="r";
	TDB *db = tdb_open(df, mode);
	if ( !db )
	{
		cout<<"open db "<<df<<" failed\n";
		return 0;
	}
	int success=0;
	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<int> dis(0,total-1);

	timeval start, end;
    gettimeofday(&start, NULL);

	for(int i=0;i<2*total;i++)
	{
		int num=dis(gen);
		//int num=i;
		string key="num_"+to_string(num);
		string res_value(1023,char('A'+num%26));
		string get_value=tdb_fetch(db,key.c_str());
		//cout<<"res value is: "<<res_value<<" and get value is: "<<get_value<<endl;
		if(get_value==res_value)
			success++;
	}

	/*//正序遍历
	for(int i=0;i<total;i++)
	{
		string key="num_"+to_string(i);
		string res_value(1023,char('A'+i%26));

		string get_value=tdb_fetch(db,key.c_str());
		if(get_value==res_value)
			success++;
	}
	cout<<"second time\n";
	//逆序遍历
	for(int i=total-1;i>=0;i--)
	{
		string key="num_"+to_string(i);
		string res_value(1023,char('A'+i%26));
		string get_value=tdb_fetch(db,key.c_str());
		if(get_value==res_value)
			success++;
	}*/

	gettimeofday(&end, NULL);
	double start_seconds = timeval_to_seconds(start);
    double end_seconds = timeval_to_seconds(end);
    double elapsed_seconds = end_seconds - start_seconds;

	tdb_close(db);
	cout<<"db fetch data test success: "<<success<<" failed: "<<2*total-success<<" use time:"<<elapsed_seconds<<endl;
	return success;
}


void data_prepare(char*df)
{
	char mode[]="c";
	TDB *db = tdb_open(df, mode);
	if ( !db )
	{
		cout<<"open db "<<df<<" failed\n";
		exit(0);
	}
	//打开录入文件
	fstream fileLoad("/home/md/testFile_1/cleanLoad.txt");
	if (!fileLoad.is_open()) 
	{ 	// 检查文件是否成功打开
        cout << "Error opening fileLoad!" << std::endl;
        exit(0);
    }

	timeval start, end;
    gettimeofday(&start, NULL);

	string line;
	while(getline(fileLoad,line))
	{
		//读取某一行内容
		if(line[0]=='-')
			continue;
		if(line=="insert")
		{
			//插入操作
			getline(fileLoad,line);
			string key=line.substr(5);
			getline(fileLoad,line);
			string value=line.substr(8);
			tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
			//cout<<"set operation-----key is: "<<key<<" and value is: "<<value<<endl;
		}
	}
	gettimeofday(&end, NULL);
	double start_seconds = timeval_to_seconds(start);
    double end_seconds = timeval_to_seconds(end);
    double elapsed_seconds = end_seconds - start_seconds;

	cout<<"load test,used time: "<<elapsed_seconds<<endl;

	fileLoad.close();
	tdb_close(db);
}
/*
 * void just_load()
 * need complete file name
 * execute load phase
 * */
void just_load(string fileName,char* diskFile){
	char mode[] = "c";
	TDB* db=tdb_open(diskFile,mode);
	if(!db){
		cout<<"Error open: "<<diskFile<<endl;
		exit(0);
	}
	fstream fileLoad(fileName);
	if (!fileLoad.is_open()) { 	
		cout << "Error opening fileLoad!" << endl;
        	exit(0);
    	}
#ifdef PRINT_LATENCY
	timeval t_start, t_end;
	gettimeofday(&t_start, NULL);
#endif
	string line;
	while(getline(fileLoad,line)){
		//read a line from workload file
		if(line[0]=='-')
			continue;
		else if(line=="insert")
		{
			//insert operation
			getline(fileLoad,line);
			string key=line.substr(5);
			getline(fileLoad,line);
			string value=line.substr(8);
			tdb_store(db,key.c_str(),prefix.c_str(),TDB_INSERT);
			//cout<<"set operation-----key is: "<<key<<" and value is: "<<value<<endl;
		}
	}
#ifdef PRINT_LATENCY
	gettimeofday(&t_end,NULL);
	cout<<"Load phase, Used Time: "<<getUsedTime(t_start, t_end)<<endl;
#endif
	tdb_close(db);
	fileLoad.close();
	cout<<"finish load phase\n";

}
/*
 * void just_run
 * need complete file name
 * execute run phase and print log per 1000operations
 * */
void just_run(string fileName, char* diskFile){
	char mode[] = "w";
	TDB* db=tdb_open(diskFile,mode);
	if(!db){
		cout<<"Error open: "<<diskFile<<endl;
		exit(0);
	}
	fstream fileLoad(fileName);
	string logFileName = string(diskFile)+"_performance.log";
	string lockFileName = string(diskFile)+".lock";
	timeval logStart,logEnd;
	//time part
	timeval t_start,t_end;
	gettimeofday(&t_start,NULL);
	string line;
	int operCount = 0;
	gettimeofday(&logStart, NULL);
	while(getline(fileLoad,line)){
		//read a line from workload file
		if(line[0]=='-')
			continue;
		else if(line=="insert"||line=="update")
		{
			//insert operation or update operation
			getline(fileLoad,line);
			string key=line.substr(5);
			getline(fileLoad,line);
			string value=line.substr(8);
			tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
			//cout<<line<<" operation-----key is: "<<key<<" and value is: "<<value<<endl;
			//operCount++;
		}
		else if(line=="read")
		{
			//read operation
			getline(fileLoad,line);
			string key=line.substr(5);
			string get_value=tdb_fetch(db,key.c_str());
			//cout<<"get operation-----key is: "<<key<<" and get_value is: "<<get_value<<endl;
			//operCount++;
		}
#ifdef PRINT_LATENCY
		if(operCount>100000){
			//print log
			gettimeofday(&logEnd, NULL);
			ofstream lockFile(lockFileName);
			lockFile.close();

			ofstream logFile(logFileName, ios::app);
			if(logFile.is_open()){
				double usedTime = getUsedTime(logStart, logEnd);
				string log = "Used Time: " + to_string(usedTime);
				logFile<<log<<endl;
				logFile.close();
			}
			remove(lockFileName.c_str());
			
			operCount = 0;
			gettimeofday(&logStart, NULL);
		}
#endif
	}
	
	gettimeofday(&t_end,NULL);
	cout<<"Run phase, Used Time: "<<getUsedTime(t_start,t_end)<<endl;
	tdb_close(db);
	fileLoad.close();
}
/*
 * just_addPool()
 * need diskFile name,which is also poolName
 * just addpool through tdb_open,and wait to resize pool
 * */
void just_addPool(char* diskFile){
	char mode[] = "c";
	TDB* db=tdb_open(diskFile,mode);
	if(!db){
		cout<<"Error open: "<<diskFile<<endl;
		exit(0);
	}
	tdb_close(db);
}
/*
 * void just_load_TL()
 * need complete file name
 * execute load phase
 * */
void just_load_TL(string fileName,char* diskFile){
	char mode[] = "c";
	TDB* db=tdb_open(diskFile,mode);
	if(!db){
		cout<<"Error open: "<<diskFile<<endl;
		exit(0);
	}
	fstream fileLoad(fileName);
	if (!fileLoad.is_open()) { 	
		cout << "Error opening fileLoad!" << endl;
        	exit(0);
    	}
	string line;
	TailLatency _999tl(1000);
	while(getline(fileLoad,line)){
		//read a line from workload file
		if(line[0]=='-')
			continue;
		else if(line=="insert")
		{
			//insert operation
			getline(fileLoad,line);
			string key=line.substr(5);
			getline(fileLoad,line);
			string value=line.substr(8);
			auto start = chrono::high_resolution_clock::now();
			tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
			auto end = chrono::high_resolution_clock::now();
			_999tl.push(timeval_diff_nsec(start, end));
			//cout<<"set operation-----key is: "<<key<<" and value is: "<<value<<endl;
		}
	}
	_999tl.getResult();
	tdb_close(db);
	fileLoad.close();
	cout<<"finish load phase\n";

}
/*
 * void just_run_TL
 * need complete file name
 * execute run phase and print log per 1000operations
 * */
void just_run_TL(string fileName, char* diskFile){
	char mode[] = "w";
	TDB* db=tdb_open(diskFile,mode);
	if(!db){
		cout<<"Error open: "<<diskFile<<endl;
		exit(0);
	}
	fstream fileLoad(fileName);
	TailLatency _999tl(1000);
	string logFileName = string(diskFile)+"_performance.log";
	string line;
	int operCount = 0;
	while(getline(fileLoad,line)){
		//read a line from workload file
		if(line[0]=='-')
			continue;
		else if(line=="insert"||line=="update")
		{
			//insert operation or update operation
			getline(fileLoad,line);
			string key=line.substr(5);
			getline(fileLoad,line);
			string value=line.substr(8);

			auto start = chrono::high_resolution_clock::now();
			tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
			auto end = chrono::high_resolution_clock::now();
			_999tl.push(timeval_diff_nsec(start, end));

			//cout<<line<<" operation-----key is: "<<key<<" and value is: "<<value<<endl;
			operCount++;
		}
		else if(line=="read")
		{
			//read operation
			getline(fileLoad,line);
			string key=line.substr(5);

			//cout<<"get operation-----key is: "<<key<<endl;
			auto start = chrono::high_resolution_clock::now();
			string get_value=tdb_fetch(db,key.c_str());
			auto end = chrono::high_resolution_clock::now();
			_999tl.push(timeval_diff_nsec(start, end));
			
			//cout<<"get operation-----key is: "<<key<<" and get_value is: "<<get_value<<endl;
			operCount++;
		}
		if(operCount>1000000){
			//print log
			ofstream logFile(logFileName, ios::app);
			if(logFile.is_open()){
				long long tailLatency = _999tl.getResult();
				string log = "99.9\% tail latency is: " + to_string(tailLatency);
				logFile<<log<<endl;
				logFile.close();
			}
			operCount = 0;
			_999tl.clear();
		}
	}
	tdb_close(db);
	fileLoad.close();
}
/*
 * traverse_data: scan the load file and check the data info in *.tdb
 * 
 * */
void traverse_data(string fileName, char* diskFile){
	char mode[] = "w";
	TDB* db = tdb_open(diskFile, mode);
	if(!db){
		cout<<"Error open: "<<diskFile<<endl;
		exit(0);
	}
	fstream fileLoad(fileName);
	string line;
	while(getline(fileLoad, line)){
		if(line[0] == '-'){
			continue;
		}else if(line=="insert"){
			getline(fileLoad, line);
			string key = line.substr(5);
			getline(fileLoad, line);
			tdb_fetch(db, key.c_str());
		}
	}
	cout<<"finish scan\n";
	tdb_close(db);
	fileLoad.close();
}

/*
 * correct_verification:execute simple set and get
 * */
void correct_verification(){
	char mode[] = "c";
	char cv_db[] = "cv_db";
	TDB *db = tdb_open(cv_db, mode);
	if(!db){
		cout<<"open db: "<<cv_db<<" failed!"<<endl;
		exit(0);
	}
	cout<<"begin to set\n";
	int success = 0;
	for(int i=0;i<1000;i++){
		string key = "num_" + to_string(i);
		string value(1000, char('A' + i % 26));
		int res = tdb_store(db, key.c_str(), value.c_str(), TDB_INSERT);
		if(res==TDB_SUCCESS)
			success++;
	}
	tdb_close(db);
	cout<<"finish set test, success: "<<success<<" in "<<1000<<" total\n";

	mode[0] = 'r';
	TDB *db_r = tdb_open(cv_db, mode);
	if(!db_r){
		cout<<"open db: "<<cv_db<<" failed!"<<endl;
		exit(0);
	}
	cout<<"begin to get\n";
	success = 0;
	for(int i=0;i<1000;i++){
		string key = "num_" + to_string(i);
		string value(1000, char('A' + i % 26));
		string getValue = tdb_fetch(db_r, key.c_str());
		if(getValue==value)
			success++;
	}
	tdb_close(db_r);
	cout<<"finish get test, success: "<<success<<" in "<<1000<<" total\n";
}
void parallel_test(int argc,char*argv[])
{
	char* input_file=nullptr;
	char* output_file=nullptr;
	int operationType = -1;
	int o;
	// bool printLog = false;
	while((o=getopt(argc,argv,"-i:o:lr"))!=-1)
	{
		switch(o)
		{
			case 'i':input_file=optarg;break;
			case 'o':output_file=optarg;break;
			case 'l':operationType = 1;break;
			case 'r':
				 operationType = 2;
				//  printLog = true;
				 break;
			default:cout<<"wrong argument\n";break;
		}
	}
	if(!input_file||!output_file)
	{
		cout<<"can get efficient argument\n";
		exit(0);
	}
	char mode[] = "c";
	if(operationType == 1){
		//load operation
		mode[0] = 'w';
	}
	TDB* db=tdb_open(output_file,mode);
	if(!db)
	{
		cout<<"Error open: "<<output_file<<endl;
		exit(0);
	}
	//open the target file
	string inputFileName = input_file;
	fstream fileLoad(inputFileName);
	if (!fileLoad.is_open()) 
	{ 	
		cout << "Error opening fileLoad!" << endl;
        	exit(0);
    	}

	//time to set
	timeval start,end;
	gettimeofday(&start,NULL);

	string line;
	while(getline(fileLoad,line)){
		//read a line from workload file
		if(line[0]=='-')
			continue;
		else if(line=="insert")
		{
			//insert operation
			getline(fileLoad,line);
			string key=line.substr(5);
			getline(fileLoad,line);
			string value=line.substr(8);
			tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
			//cout<<"set operation-----key is: "<<key<<" and value is: "<<value<<endl;
		}
		else if(line=="read")
		{
			//read operation
			getline(fileLoad,line);
			string key=line.substr(5);
			string get_value=tdb_fetch(db,key.c_str());
			//cout<<"get operation-----key is: "<<key<<" and get_value is: "<<get_value<<endl;
		}
		else if(line=="update")
		{
			//update operation
			getline(fileLoad,line);
			string key=line.substr(5);
			getline(fileLoad,line);
			string value=line.substr(8);
			tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
			//cout<<"update operation-----key is: "<<key<<" and value is: "<<value<<endl;
		}


	}
	
	gettimeofday(&end,NULL);
	if(operationType == 1){
		cout<<"Load phase,Used Time: "<<getUsedTime(start,end)<<endl;
	}else{
		cout<<"Run phase, Used Time: "<<getUsedTime(start,end)<<endl;
	}
	tdb_close(db);
	fileLoad.close();
}

/*
 * void loadAndRun()
 * accept uncomplete file name
 * execute both load and run phase
 * */	
void loadAndRun(int argc,char*argv[])
{
	char* input_file=nullptr;
	char* output_file=nullptr;
	int o;
	while((o=getopt(argc,argv,"-i:o:"))!=-1)
	{
		switch(o)
		{
			case 'i':input_file=optarg;break;
			case 'o':output_file=optarg;break;
			default:cout<<"wrong argument\n";break;
		}
	}
	if(!input_file||!output_file)
	{
		cout<<"can get efficient argument\n";
		exit(0);
	}
	string fileLoad_name=string(input_file)+"_load.txt";
	string fileRun_name=string(input_file)+"_run.txt";
	char mode[]="c";
	TDB* db=tdb_open(output_file,mode);
	if(!db)
	{
		cout<<"Error open: "<<output_file<<endl;
		exit(0);
	}
	//open the load file
	fstream fileLoad(fileLoad_name);
	if (!fileLoad.is_open()) 
	{ 	
		cout << "Error opening fileLoad!" << endl;
        	exit(0);
    	}

	//time to set
	timeval start,end;
	gettimeofday(&start,NULL);

	string line;
	while(getline(fileLoad,line))
	{
		//读取某一行内容
		if(line[0]=='-')
			continue;
		if(line=="insert")
		{
			//插入操作
			getline(fileLoad,line);
			string key=line.substr(5);
			getline(fileLoad,line);
			string value=line.substr(8);
			tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
			//cout<<"set operation-----key is: "<<key<<" and value is: "<<value<<endl;
		}
	}
	
	gettimeofday(&end,NULL);
	cout<<"Load phase,Used Time: "<<getUsedTime(start,end)<<endl;
	tdb_close(db);
	fileLoad.close();
	
	cout<<"finish load"<<endl;

	char mode_r[]="w";
	TDB* db_r=tdb_open(output_file,mode_r);
	if(!db_r)
	{
		cout<<"Error open: "<<output_file<<endl;
		exit(0);
	}
	fstream fileRun(fileRun_name);
	if (!fileRun.is_open()) 
	{ 	
		cout << "Error opening fileRun!" << endl;
        	exit(0);
    	}
	//time to get
	gettimeofday(&start,NULL);

	line="";
	while(getline(fileRun,line))
	{
		//读取某一行内容
		if(line[0]=='-')
			continue;
		else if(line=="insert")
		{
			//插入操作
			getline(fileRun,line);
			string key=line.substr(5);
			getline(fileRun,line);
			string value=line.substr(8);
			tdb_store(db_r,key.c_str(),value.c_str(),TDB_INSERT);
			//cout<<"set operation-----key is: "<<key<<" and value is: "<<value<<endl;
		}
		else if(line=="read")
		{
			//读取操作
			getline(fileRun,line);
			string key=line.substr(5);
			string get_value=tdb_fetch(db_r,key.c_str());
			//cout<<"get operation-----key is: "<<key<<" and get_value is: "<<get_value<<endl;
		}
		else if(line=="update")
		{
			//update operation
			getline(fileRun,line);
			string key=line.substr(5);
			getline(fileRun,line);
			string value=line.substr(8);
			tdb_store(db_r,key.c_str(),value.c_str(),TDB_INSERT);
			//cout<<"update operation-----key is: "<<key<<" and value is: "<<value<<endl;
		}


	}
	cout<<"finish Run"<<endl;
	gettimeofday(&end,NULL);
	cout<<"Run phase,Used Time: "<<getUsedTime(start,end)<<endl;
	tdb_close(db_r);
	fileRun.close();

}

int main(int argc,char*argv[]){
	//correct_verification();
	//return 0;
	/*
	char* df = "one_test";
	TDB *db = tdb_open(df, "c");
	string k_key = "kidsscc_key";
	string k_value = "kidsscc_value";
	tdb_store(db, k_key.c_str(), k_value.c_str(), TDB_INSERT);
	tdb_close(db);

	cout<<"finish set KV\n";

	TDB *db_r = tdb_open(df, "r");
	string getValue = tdb_fetch(db_r, k_key.c_str());
	cout<<"get value is: "<<getValue<<endl;
	tdb_close(db_r);
	
	return 0;
	*/

	
	char df[] = "tdb_data_test";
	int total = 10;
	_db_insert_test(total, df);
	_db_read_test(total, df);
	return 0;
	

	//insert&read test
	#ifdef CACHELIB_LOCAL
		facebook::cachelib_examples::initializeCache();
	#endif
	char* input_file=nullptr;
	char* output_file=nullptr;
	int operationType = -1;
	int o;
	while((o=getopt(argc,argv,"-i:o:lra"))!=-1)
	{
		switch(o)
		{
			case 'i':input_file=optarg;break;
			case 'o':output_file=optarg;break;
			case 'l':operationType = 1;break;
			case 'r':operationType = 2;break;
			case 'a':operationType = 3;break;
			default:cout<<"wrong argument\n";break;
		}
	}
	if(!input_file||!output_file||operationType<0)
	{
		cout<<"can get efficient argument\n";
		exit(0);
	}
	string fileName = input_file;
	if(operationType == 1){
		just_load(fileName, output_file);
	}else if(operationType == 2){
		just_run(fileName, output_file);
		// just_run_TL(fileName, output_file);
	}else{
		just_addPool(output_file);
	}
	

	#ifdef CACHELIB_LOCAL
		facebook::cachelib_examples::destroyCache();
	#endif
	return 0;

}

