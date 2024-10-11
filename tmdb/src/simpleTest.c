#include <stdio.h>
#include <string.h>
#include "tmdb.h"
//#include "tmdb_cp.h"
#include "cachelibClientAPI.h"


typedef struct
{
    int num;
    std::string name;
    std::unique_ptr<CachelibClient> client;
} myStruct;

void func(const char *path, char *mode)
{
    myStruct object;
    object.num=1412;
    object.name="kidsscc";
    object.client=std::make_unique<CachelibClient>(grpc::CreateChannel("localhost:50051",grpc::InsecureChannelCredentials()));
    std::string user("SFTest");
    object.client->SayHello(user);
    object.client->registerPool(user);
}
int main()
{
    char *df = "simple_test";
    //TDB *db = tdb_open(df, "c");
    func(df, "c");
}
