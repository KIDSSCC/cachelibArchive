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

int realtime_db_insert_test(int total,char*df);
int realtime_db_read_test(int total,char*df);

using namespace std;
string prefix (1000, 'A');

/**
 * data insert
 * 使用clock函数统计CPU时间，底层出现异步或阻塞操作时，时间的统计会存在偏差
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
		string value(10, char('A'+i%26));
		s = tdb_store(db, key.c_str(), value.c_str(), TDB_INSERT);
		if (TDB_SUCCESS == s){
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
 * 使用clock函数统计CPU时间，底层出现异步或阻塞操作时，时间的统计会存在偏差
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

/**
 * data insert
 * 使用gettimeofday函数统计系统真实时间
 * @param int total insert total
 * @param char *df data file name
 */
int realtime_db_insert_test(int total,char *df)
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

/**
 * data read，一遍正序，两遍随机，一遍逆序
 * 使用gettimeofday函数统计系统真实时间
 * @param int total insert total
 * @param char *df data file name
 */
int realtime_db_read_test(int total, char*df)
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

	//正序遍历
	for(int i=0;i < total;i++)
	{
		string key="num_"+to_string(i);
		string res_value(1023,char('A'+i%26));

		string get_value=tdb_fetch(db,key.c_str());
		if(get_value==res_value)
			success++;
	}

	for(int i=0;i<2*total;i++)
	{
		int num=dis(gen);
		string key="num_"+to_string(num);
		string res_value(1023,char('A'+num%26));
		string get_value=tdb_fetch(db,key.c_str());
		//cout<<"res value is: "<<res_value<<" and get value is: "<<get_value<<endl;
		if(get_value==res_value)
			success++;
	}

	//逆序遍历
	for(int i=total-1;i>=0;i--)
	{
		string key="num_"+to_string(i);
		string res_value(1023,char('A'+i%26));
		string get_value=tdb_fetch(db,key.c_str());
		if(get_value==res_value)
			success++;
	}

	gettimeofday(&end, NULL);
	double start_seconds = timeval_to_seconds(start);
    double end_seconds = timeval_to_seconds(end);
    double elapsed_seconds = end_seconds - start_seconds;

	tdb_close(db);
	cout<<"db fetch data test success: "<<success<<" failed: "<<4 * total-success<<" use time:"<<elapsed_seconds<<endl;
	return success;
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

int main(int argc,char*argv[]){
	
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

	correct_verification();

	
	// char df[] = "tdb_data_test";
	// int total = 10000;
	// realtime_db_insert_test(total, df);
	// realtime_db_read_test(total, df);
	return 0;
}

