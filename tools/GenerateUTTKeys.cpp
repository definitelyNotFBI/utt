#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <bits/getopt_core.h>
#include <sys/param.h>
#include <string>
#include <cstring>
#include <getopt.h>
#include <unistd.h>
#include <sstream>

#include "utt/Bank.h"
#include "utt/Utt.h"

using namespace libutt;

std::string dirname = ".";
std::string fileNamePrefix = "utt_pvt_replica_";
std::string clientFileName = "utt_pub_client.dat";
std::string genesis_prefix = "genesis_";
size_t n = 4 ;
size_t t = 1 ;
size_t number_of_genesis_files = 0;
bool debug = false;

static struct option longOptions[] = {
    {"key-file-prefix", required_argument, 0, 'k'},
    {"node-num", required_argument, 0, 'n'},
    {"fault-num", required_argument, 0, 't'},
    {"output-dir", required_argument, 0, 'o'},
    {"client-file-prefix", required_argument, 0, 'c'},
    {"num-genesis-coins", required_argument, 0, 'g'},
    {"genesis-prefix", required_argument, 0, 'p'},
    {"debug", no_argument, 0, 'd'},
};

int main(int argc, char **argv)
{
    libutt::initialize(nullptr, 0);
    int o = 0;
    int optionIndex = 0;
    while((o = getopt_long(argc, argv, "k:n:t:o:c:g:p:d", longOptions, &optionIndex)) != -1) {
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
            case 'g': {
                number_of_genesis_files = atol(optarg);
                break;
            }
            case 'p': {
                genesis_prefix = optarg;
                break;
            }
            case 'd': {
                debug = true;
                break;
            }
            case '?': {
                throw std::runtime_error("invalid arguments");
            } break;
            default:
                break;
        }
    }

    if ( n <= 3*(t-1) ) {
        std::ostringstream ss;
        ss << "Invalid values for n and t" 
            << std::endl 
            << "n:" << n
            << "t:" << t
            << std::endl 
            << "n must be at least 3t+1"; 
        throw std::runtime_error(ss.str());
    }
    if (debug)
    std::cout 
        << "Output directory:" << dirname << std::endl
        << "Key file prefix:" << fileNamePrefix << std::endl
        << "Client prefix:" << clientFileName << std::endl
        << "Number of nodes:" << n << std::endl
        << "Number of faults:" << t << std::endl
        << "Genesis:" << number_of_genesis_files << std::endl
        << "Genesis Prefix:" << genesis_prefix << std::endl
        ;

    // Delete the files if they already exist to prevent libutt from complaining about and failing other builds
    for(size_t i=0; i < n; i++) {
        std::string to_delete = dirname + "/" + fileNamePrefix + std::to_string(i); 
        remove(to_delete.c_str());
    }

    libutt::Params p;

    BankThresholdKeygen keys(p, t, n);
    keys.writeToFiles(dirname, fileNamePrefix);

    std::ofstream cliFile(dirname + "/"+ clientFileName);
    cliFile << p;
    auto skShares = keys.getAllShareSKs();
    std::vector<BankSharePK> pkShares;
    for (auto& sk: skShares) {
        pkShares.push_back(sk.toSharePK(p));
    }

    for(size_t i=0; i< n;i++) {
        cliFile << pkShares.at(i);
    }
    cliFile << keys.getPK(p);
    cliFile.close();

    // Generate Genesis configs
    for (auto i=0ul; i< number_of_genesis_files; i++) {
        // Make a new genesis file
        std::ofstream genfile(dirname + genesis_prefix + std::to_string(i));
        
        // Put 2 coins with value 1 each, so they can keep paying themselves for the benchmarking
        // TODO

        // Close the file
        genfile.close(); 
    }

    std::cout << "Created replica files in "
        << dirname + fileNamePrefix
        << std::endl;
    std::cout << "Created client files in "
        << dirname + clientFileName
        << std::endl;

    return 0;
}