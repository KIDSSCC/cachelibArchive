#include <iostream>

#include <vector>
#include <time.h>

#include "cachelibHeader.h"
#include "clientAPI.h"

using namespace std;


void remoteTest(string appName)
{
    CachelibClient* client=new CachelibClient();

    int cases = 10000;

    cout<<"-------- Test for registerPool --------\n";
    client->addpool(appName);

    vector<std::string> keys;
    vector<std::string> values;
    for(int i=0;i<cases;i++)
    {
        keys.push_back("key_" + std::to_string(i));
        values.push_back(appName + "value_" + std::to_string(i));
    }

    cout<<"-------- Test for set --------\n";
    clock_t start_s, finish_s;   
	double duration_s;   
	start_s=clock();
    int count = 0;
    for(int i=0;i<cases;i++)
    {
        if(client->setKV(keys[i], values[i]))
            count++;
    }
    finish_s=clock();   
	duration_s=(double)(finish_s-start_s)/CLOCKS_PER_SEC; 
    cout<<"set test result: success "<<count<<" ,failed "<<cases-count<<" time is: "<<duration_s<<std::endl;

    cout<<"-------- Test for get --------\n";
    clock_t start_g, finish_g;   
	double duration_g;   
	start_g=clock();
    count = 0;
    for(int i=0;i<cases;i++)
    {
        if(client->getKV(keys[i])==values[i])
            count++;
    }
    finish_g=clock();   
	duration_g=(double)(finish_g-start_g)/CLOCKS_PER_SEC; 
    cout<<"get test result: success "<<count<<" ,failed "<<cases-count<<" time is: "<<duration_g<<std::endl;

    
    cout<<"-------- Test for del --------\n";
    clock_t start_d, finish_d;   
	double duration_d;   
	start_d=clock();
    for(int i=0;i<cases;i++)
    {
        client->delKV(keys[i]);
    }
    finish_d=clock();   
	duration_d=(double)(finish_d-start_d)/CLOCKS_PER_SEC; 
    count = 0;
    for(int i=0;i<cases;i++)
    {
        if(client->getKV(keys[i])=="")
            count++;
    }
    cout<<"del test result: success "<<count<<" ,failed "<<cases-count<<" time is: "<<duration_d<<std::endl;
    
}

using namespace facebook::cachelib_examples;
void localTest(std::string appName)
{
    initializeCache();
    cout<<"local access cachelib\n";

    int cases = 10000;
    cout<<"-------- Test for registerPool --------\n";
    int pid = addpool_(appName);
    cout<<"receive the reply from the server: "<<pid<<std::endl;

    vector<std::string> keys;
    vector<std::string> values;
    for(int i=0;i<cases;i++)
    {
        keys.push_back("key_" + std::to_string(i));
        values.push_back(appName + "value_" + std::to_string(i));
    }

    cout<<"-------- Test for set --------\n";
    clock_t start_s, finish_s;   
	double duration_s;   
	start_s=clock();
    int count = 0;
    for(int i=0;i<cases;i++)
    {
        if(set_(pid, keys[i], values[i]))
            count++;
    }
    finish_s=clock();   
	duration_s=(double)(finish_s-start_s)/CLOCKS_PER_SEC; 
    cout<<"set test result: success "<<count<<" ,failed "<<cases-count<<" time is: "<<duration_s<<std::endl;

    cout<<"-------- Test for get --------\n";
    clock_t start_g, finish_g;   
	double duration_g;   
	start_g=clock();
    count = 0;
    for(int i=0;i<cases;i++)
    {
        if(get_(keys[i])==values[i])
            count++;
    }
    finish_g=clock();   
	duration_g=(double)(finish_g-start_g)/CLOCKS_PER_SEC; 
    cout<<"get test result: success "<<count<<" ,failed "<<cases-count<<" time is: "<<duration_g<<std::endl;

    cout<<"-------- Test for del --------\n";
    clock_t start_d, finish_d;   
	double duration_d;   
	start_d=clock();
    for(int i=0;i<cases;i++)
    {
        del_(keys[i]);
    }
    finish_d=clock();   
	duration_d=(double)(finish_d-start_d)/CLOCKS_PER_SEC; 
    count = 0;
    for(int i=0;i<cases;i++)
    {
        if(get_(keys[i])=="")
            count++;
    }
    cout<<"del test result: success "<<count<<" ,failed "<<cases-count<<" time is: "<<duration_d<<std::endl;


    destroyCache();
}

int main()
{
    remoteTest("tmdb");
    //localTest("tmdb");
}