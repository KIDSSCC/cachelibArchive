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
using namespace std;
//#define CACHELIB_LOCAL


double timeval_to_seconds(const timeval& t) {
    return t.tv_sec + t.tv_usec / 1000000.0;
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
string prefix (10, 'A');

/**
 * data insert
 *
 * @param int total insert total
 * @param char *df data file name
 */
int _db_insert_test(int total, char *df){
	
	char n[16];
	char *dat;
	int i = 0;
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
	
	clock_t start, finish;   
	double duration;   
	start=clock();
	
	for (i=0; i<total; i++){
		sprintf(n, "num%d", i);
		string value(10,char('A'+i%26));
		s = tdb_store(db, n, value.c_str(), TDB_INSERT);
		if (TDB_SUCCESS == s){
			success++;
		}
	}
	tdb_close(db);
	finish=clock();   
	duration=(double)(finish-start)/CLOCKS_PER_SEC;   
	printf("db insert data test success: %d, used time: %fs\n", success, duration);

	return success;
}

/**
 * data read
 *
 * @param int total read total
 */
int _db_read_test(int total, char *df){
	char n[16];
	char *dat = NULL;
	int i = 0;
	int success = 0;
	int s = 0;

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

	clock_t start, finish;   
	double duration;   
	start=clock();

	
	for (i=0; i<total; i++){
		int num=dis(gen);
		sprintf(n, "num%d", num);
		string value(10,char('A'+num%26));
		dat = tdb_fetch(db, n);
		if (dat == value){
			success++;
		}
	}
	tdb_close(db);

	finish=clock();   
	duration=(double)(finish-start)/CLOCKS_PER_SEC;   
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
void parallel_test(int argc,char*argv[])
{
	char* file_path=nullptr;
	int rand_seed=-1;
	int o;
	while((o=getopt(argc,argv,"-f:r:"))!=-1)
	{
		switch(o)
		{
			case 'f':file_path=optarg;break;
			case 'r':rand_seed=atoi(optarg);break;
			default:cout<<"wrong argument\n";break;
		}
	}
	if(!file_path||rand_seed<0)
	{
		cout<<"can get efficient argument\n";
		exit(0);
	}
	int total=512*1024;
	char mode[]="c";
	TDB* db=tdb_open(file_path,mode);
	if(!db)
	{
		cout<<"Error open: "<<file_path<<endl;
		exit(0);
	}
	//time to set
	timeval start,end;
	gettimeofday(&start,NULL);

	int success=0;
	for(int i=0;i<total;i++)
	{
		string key="num_"+to_string(i);
		string value(1023,char('A'+(i+rand_seed)%26));
		int s=tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
		if(s==TDB_SUCCESS)
			success++;
	}

	gettimeofday(&end,NULL);
	double start_seconds=timeval_to_seconds(start);
	double end_seconds=timeval_to_seconds(end);
	double elapsed_seconds=end_seconds-start_seconds;
	cout<<"insert success: "<<success<<" failed: "<<total-success<<" used time: "<<elapsed_seconds<<endl;
	tdb_close(db);

	char mode_r[]="w";
	TDB* db_r=tdb_open(file_path,mode_r);
	if(!db_r)
	{
		cout<<"Error open: "<<file_path<<endl;
		exit(0);
	}
	//time to get
	gettimeofday(&start,NULL);
	success=0;
	random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<int> dis(0,total-1);
	for(int i=0;i<5*total;i++)
	{
		int num=dis(gen);
		string key="num_"+to_string(num);
		//cout<<"in get test, key is: "<<key<<endl;
		string res_value(1023,char('A'+(num+rand_seed)%26));
		string get_value=tdb_fetch(db_r,key.c_str());
		if(res_value==get_value)
			success++;
	}
	gettimeofday(&end,NULL);
	start_seconds=timeval_to_seconds(start);
	end_seconds=timeval_to_seconds(end);
	elapsed_seconds=end_seconds-start_seconds;
	cout<<"get success: "<<success<<" failed: "<<5*total-success<<" used time: "<<elapsed_seconds<<endl;
	tdb_close(db_r);

}
	







void data_run(char*df)
{
	char mode[]="w";
	TDB *db = tdb_open(df, mode);
	if ( !db )
	{
		cout<<"open db "<<df<<" failed\n";
		exit(0);
	}
	//打开执行文件
	fstream fileRun("/home/md/testFile_1/cleanRun.txt");
	if (!fileRun.is_open()) 
	{ 	// 检查文件是否成功打开
        cout << "Error opening fileLoad!" << std::endl;
        exit(0);
    }

	timeval start, end;
    gettimeofday(&start, NULL);

	string line;
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
			tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
			//cout<<"set operation-----key is: "<<key<<" and value is: "<<value<<endl;
		}
		else if(line=="read")
		{
			//读取操作
			getline(fileRun,line);
			string key=line.substr(5);
			string get_value=tdb_fetch(db,key.c_str());
			//cout<<"get operation-----key is: "<<key<<" and get_value is: "<<get_value<<endl;
		}
	}

	gettimeofday(&end, NULL);
	double start_seconds = timeval_to_seconds(start);
    double end_seconds = timeval_to_seconds(end);
    double elapsed_seconds = end_seconds - start_seconds;

	cout<<"run test,used time: "<<elapsed_seconds<<endl;
	fileRun.close();
	tdb_close(db);
}


int main(int argc,char*argv[]){

	
	//insert&read test
	#ifdef CACHELIB_LOCAL
		facebook::cachelib_examples::initializeCache();
	#endif

	
	
	printf("============= performance test ===========\n");
	parallel_test(argc,argv);
	/*
	char df[] = "tdb_data_test";
	int total = 10;
	_db_insert_test(total, df);
	_db_read_test(total, df);
	*/

	/*char df[]="kidsscc_data_test";
	int total=1024*1024;
	kidsscc_db_insert_test(total, df);
	kidsscc_db_read_test(total, df);*/

	/*char df[]="ycsb_data";
	data_prepare(df);
	data_run(df);*/
	#ifdef CACHELIB_LOCAL
		facebook::cachelib_examples::destroyCache();
	#endif
	return 0;

}

