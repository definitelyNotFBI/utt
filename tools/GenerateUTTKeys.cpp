#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <sys/param.h>
#include <string>
#include <cstring>
#include <getopt.h>
#include <unistd.h>
#include <sstream>

#include "utt/Bank.h"
#include "utt/Utt.h"

using namespace libutt;

std::string dirname = "./";
std::string fileNamePrefix = "utt_pvt_replica_";
std::string clientFileName = "utt_pub_client.dat";
size_t n = 4 ;
size_t t = 1 ;

static struct option longOptions[] = {
    {"key-file-prefix", required_argument, 0, 'k'},
    {"node-num", required_argument, 0, 'n'},
    {"fault-num", required_argument, 0, 't'},
    {"output-dir", required_argument, 0, 'o'},
    {"client-file-prefix", required_argument, 0, 'c'}
};

int main(int argc, char **argv)
{
    libutt::initialize(nullptr, 0);
    int o = 0;
    int optionIndex = 0;
    while((o = getopt_long(argc, argv, "k:n:t:o:c:", longOptions, &optionIndex)) != -1) {
        switch (o) {
            case 'k': {
                fileNamePrefix = optarg;
            } break;
            case 'c': {
                clientFileName = optarg;
            } break;
            case 'n': {
                n = atol(optarg);
            } break;
            case 't': {
                t = atol(optarg);
            } break;
            case 'o': {
                dirname = optarg;
            } break;
            case '?': {
                throw std::runtime_error("invalid arguments");
            } break;
            default:
                break;
        }
    }

    if ( n <= 3*t ) {
        std::ostringstream ss;
        ss << "Invalid values for n and t" 
            << std::endl 
            << "n:" << n
            << "t:" << t
            << std::endl 
            << "n must be at least 3t+1"; 
        throw std::runtime_error(ss.str());
    }

    // Delete the files if they already exist to prevent libutt from complaining about and failing other builds
    for(size_t i=0; i < n; i++) {
        std::string to_delete = dirname + fileNamePrefix + std::to_string(i); 
        remove(to_delete.c_str());
    }

    libutt::Params p;

    BankThresholdKeygen keys(p, t, n);
    keys.writeToFiles(dirname, fileNamePrefix);

    std::ofstream cliFile(dirname + clientFileName);
    cliFile << p;

    for(size_t i=0; i< n;i++) {
        cliFile << keys.getShareSK(i).toSharePK(p);
    }
    cliFile.close();

    std::cout << "Created replica files in "
        << dirname + fileNamePrefix
        << std::endl;
    std::cout << "Created client files in "
        << dirname + clientFileName
        << std::endl;

    return 0;
}