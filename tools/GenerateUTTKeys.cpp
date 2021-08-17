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
#include <tuple>

#include "assertUtils.hpp"
#include "utt/Bank.h"
#include "utt/Coin.h"
#include "utt/Utt.h"
#include "ThresholdParamGen.hpp"

using namespace libutt;

std::string dirname = ".";
std::string fileNamePrefix = "utt_pvt_replica_";
std::string clientFileName = "utt_pub_client.dat";
std::string genesis_prefix = "genesis_";
size_t n = 4 ;
size_t f = 1 ;
size_t number_of_genesis_files = 0;
size_t number_of_coins_to_generate = 2;
bool debug = false;

static struct option longOptions[] = {
    {"key-file-prefix", required_argument, 0, 'k'},
    {"node-num", required_argument, 0, 'n'},
    {"fault-num", required_argument, 0, 'f'},
    {"output-dir", required_argument, 0, 'o'},
    {"client-file-prefix", required_argument, 0, 'c'},
    {"num-genesis-coins", required_argument, 0, 'g'},
    {"genesis-prefix", required_argument, 0, 'p'},
    {"debug", no_argument, 0, 'd'},
    {"coins", no_argument, 0, 'C'},
};

int main(int argc, char **argv)
{
    libutt::initialize(nullptr, 0);

    int o = 0;
    int optionIndex = 0;
    while((o = getopt_long(argc, argv, "k:n:f:o:c:g:p:dC:", longOptions, &optionIndex)) != -1) {
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
            case 'f': {
                f = atol(optarg);
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
            case 'C': {
                number_of_coins_to_generate = atol(optarg);
                break;
            }
            case '?': {
                throw std::runtime_error("invalid arguments");
            } break;
            default:
                break;
        }
    }

    if ( n <= 3*f ) {
        std::ostringstream ss;
        ss << "Invalid values for n and f" 
            << std::endl 
            << "n:" << n
            << "f:" << f
            << std::endl 
            << "n must be at least 3f+1"; 
        throw std::runtime_error(ss.str());
    }
    if (debug)
    std::cout 
        << "Output directory:" << dirname << std::endl
        << "Key file prefix:" << fileNamePrefix << std::endl
        << "Client prefix:" << clientFileName << std::endl
        << "Number of nodes:" << n << std::endl
        << "Number of faults:" << f << std::endl
        << "Genesis:" << number_of_genesis_files << std::endl
        << "Genesis Prefix:" << genesis_prefix << std::endl
        ;

    auto p = libutt::Params::Random();

    utt_bft::ThresholdParams tparams(p, n, f);
    auto replica_params = tparams.ReplicaParams();
    auto client_params = tparams.ClientParams();

    for(size_t i=0; i<n;i++) {
        // Write replica config for all the replicas
        std::ofstream rfile(dirname + "/" + fileNamePrefix + std::to_string(i));
        ConcordAssert(!rfile.fail());
        rfile << replica_params[i];
    }

    std::ofstream cliFile(dirname + "/"+ clientFileName);
    cliFile << client_params;
    cliFile.close();

    // Generate Genesis configs
    for (auto i=0ul; i< number_of_genesis_files; i++) {
        auto filename = dirname + "/" + genesis_prefix + std::to_string(i);
        // Make a new genesis file
        std::ofstream genfile(filename);
        if(!genfile.is_open()) {
            printf("Failed to open genesis file\n");
            return 1;
        }

        std::cout << "Writing Genesis " << filename << std::endl;
        
        // Put 2 coins with value 1024 (default v for CoinSecrets constructor) each, so they can keep paying themselves for the benchmarking
        // std::vector<std::tuple<CoinSecrets, CoinComm, CoinSig>> c;
        // std::vector<std::tuple<LTPK, Fr>> recv;

        libutt::CoinSecrets cs[2] = {
            CoinSecrets{p},
            CoinSecrets{p}
        };

        for(auto j=0ul; j<2;j++) {
            auto cc = CoinComm(p, cs[j]);
            auto csign = tparams.getSK().thresholdSignCoin(p, cc, tparams.getU());
            // c.push_back(std::make_tuple(cs[j], cc, csign));

            genfile << cs[j];
            genfile << cc;
            genfile << csign;
        }
        genfile.close();
        // libutt::Tx::create(p, c, recv);
    //     // TODO

        // Close the file
    //     genfile.close(); 
    }

    std::cout << "Created replica files in "
        << dirname + "/" + fileNamePrefix
        << std::endl;
    std::cout << "Created client files in "
        << dirname + "/" + clientFileName
        << std::endl;

    return 0;
}