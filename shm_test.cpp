#include <iostream>
#include <vector>
#include <time.h>

#include "sharedMem/ClientAPI/clientAPI.h"

using namespace std;

void remoteTest(string appName)
{
    CachelibClient* client=new CachelibClient();
    int cases=10;
    cout<<"-------- Test for registerPool --------\n";
    client->addpool(appName);
    cout<<"pid is: "<<client->getPid()<<endl;
    
    
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
        client->setKV(keys[i], values[i]);
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

    /*cout<<"-------- Test for del --------\n";
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
    */
    
}

int main()
{
    // remoteTest("tmdb");
    CachelibClient* client_1=new CachelibClient();
    client_1->addpool("pool1");
    CachelibClient* client_2=new CachelibClient();
    client_2->addpool("pool2");
    CachelibClient* client_3=new CachelibClient();
    client_3->addpool("pool3");
    
}
