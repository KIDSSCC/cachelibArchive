#include <stdio.h>
#include <string.h>
#include <time.h>
#include "tmdb.h"

#include<iostream>
#include<vector>
#include<random>


#include<thread>
using namespace std;
//#define CACHELIB_LOCAL

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

	int success=0;
	for(int i=0;i<total;i++)
	{
		string key="num_"+to_string(i);
		string value(1023,char('A'+i%26));
		int s=tdb_store(db,key.c_str(),value.c_str(),TDB_INSERT);
		if(s==TDB_SUCCESS)
			success++;
	}
	tdb_close(db);
	cout<<"db insert data test success: "<<success<<" failed: "<<total-success<<endl;
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
	for(int i=0;i<total;i++)
	{
		string key="num_"+to_string(i);
		string res_value(1023,char('A'+i%26));

		string get_value=tdb_fetch(db,key.c_str());
		if(get_value==res_value)
			success++;
	}
	tdb_close(db);
	cout<<"db fetch data test success: "<<success<<" failed: "<<total-success<<endl;
	return success;
}


/**
 * test main call
 *
 */
int main(){

	/**
	 * function test
	 */

	/* 
	int s;

	TDB *db = _db_open("./test/abc", "c");
	p_tdb(db, "1");

	s = _db_store(db, "aaa", "aaa_test", TDB_INSERT);
	printf("_db_store: aaa : aaa_test : %d\n", s);

	s = _db_store(db, "aaa", "aaaaaaaaaa", TDB_REPLACE);
	printf("_db_store: aaa : aaaaaaaaaa : %d\n", s);

	s = _db_store(db, "bbb", "bbbbbbbbbb", TDB_STORE);
	printf("_db_store: bbb : bbbbbbbbbb : %d\n", s);

	s = _db_delete(db, "bbb");
	printf("_db_delete: bbb : %d\n", s);

	s = _db_store(db, "ccc", "cccccccccc", TDB_STORE);
	printf("_db_store: ccc : cccccccccc : %d\n", s);

	s = _db_store(db, "ddd", "ddddddddd", TDB_STORE);
	printf("_db_store: %s : %s : %d\n", "ddd", "ddddddddd", s);

	char *dat = _db_fetch(db, "aaa");
	printf("_db_fetch: %s : %s (exists)\n", "aaa", dat);

	char *dat1 = _db_fetch(db, "ccc");
	printf("_db_fetch: %s : %s (exists)\n", "ccc", dat1);

	char *dat2 = _db_fetch(db, "bbb");
	printf("_db_fetch: %s : %s (deleted)\n", "bbb", dat2);

	//each all item
	_db_rewind(db);
	char *ndat, *nkey = (char *)calloc(64, 1);
	while ((ndat = _db_nextrec(db, nkey)) != NULL){
		printf("Each item key:%s, val:%s\n", nkey, ndat);
		bzero(nkey, strlen(nkey));
		bzero(ndat, strlen(ndat));
	}
	*/


	//insert&read test
	#ifdef CACHELIB_LOCAL
		facebook::cachelib_examples::initializeCache();
	#endif
	printf("============= performance test ===========\n");
	/*
	char df[] = "tdb_data_test";
	int total = 10;
	_db_insert_test(total, df);
	_db_read_test(total, df);
	*/

	char df[]="kidsscc_data_test";
	int total=100;
	kidsscc_db_insert_test(total, df);
	kidsscc_db_read_test(total, df);
	
	#ifdef CACHELIB_LOCAL
		facebook::cachelib_examples::destroyCache();
	#endif
	return 0;

}

