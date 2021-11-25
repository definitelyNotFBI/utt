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
#include "utt/Coin.h"
#include "ThresholdParamGen.hpp"
#include "utt/Wallet.h"

using namespace libutt;

std::string dirname = ".";
std::string fileNamePrefix = "utt_pvt_replica_";
std::string clientFileName = "utt_pub_client.dat";
std::string wallet_prefix = "wallet_";
size_t n = 4 ;
size_t f = 1 ;
size_t number_of_wallet_files = 0;
size_t number_of_coins_to_generate = 2;
bool debug = false;

static struct option longOptions[] = {
    // Prefix for utt_pub_client.dat
    {"client-file",         required_argument,  0, 'c'},
    // Number of coins per wallet
    {"coins",               required_argument,  0, 'C'},
    // Print debug info when generating the coins
    {"debug",               no_argument,        0, 'd'},
    // Number of faults
    {"fault-num",           required_argument,  0, 'f'},
    // Prefix for utt_pvt_replica
    {"replica-file-prefix", required_argument,  0, 'k'},
    // Number of nodes
    {"node-num",            required_argument,  0, 'n'},
    // Folder to write the wallets and params
    {"output-dir",          required_argument,  0, 'o'},
    // Prefix for the wallet in the output folder: DEFAULT: wallet_
    {"wallet-prefix",       required_argument,  0, 'p'},
    // Number of wallets to generate
    {"num-wallets",         required_argument,  0, 'w'},
};

int main(int argc, char **argv)
{
    libutt::initialize(nullptr, 0);

    int o = 0;
    int optionIndex = 0;
    while((o = getopt_long(argc, argv, "c:C:df:k:n:o:p:w:", 
                                longOptions, &optionIndex)) != -1) 
    {
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
            case 'w': {
                number_of_wallet_files = atol(optarg);
                break;
            }
            case 'p': {
                wallet_prefix = optarg;
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
        << "Output directory: " << dirname << std::endl
        << "Key file prefix: " << fileNamePrefix << std::endl
        << "Client prefix: " << clientFileName << std::endl
        << "Number of nodes: " << n << std::endl
        << "Number of faults: " << f << std::endl
        << "Wallet: " << number_of_wallet_files << std::endl
        << "Wallet Prefix: " << wallet_prefix << std::endl
        ;

    utt_bft::ThresholdParams tparams{n, f};
    auto replica_params = tparams.ReplicaParams();
    auto client_params = tparams.ClientParams();

    for(size_t i=0; i<n;i++) {
        // Write replica config for all the replicas
        std::string replica_file_name("");
        replica_file_name += dirname + "/";
        replica_file_name += fileNamePrefix + std::to_string(i);
        std::ofstream rfile(replica_file_name);
        ConcordAssert(rfile.good());
        rfile << replica_params[i];
        rfile.close();
    }

    std::string client_file_name("");
    client_file_name += dirname + "/" + clientFileName;
    std::ofstream cliFile(client_file_name);
    ConcordAssert(cliFile.good());
    cliFile << client_params;
    cliFile.close();

    // Generate Genesis configs
    auto wallets = tparams.randomWallets(2*number_of_wallet_files, 
                        number_of_coins_to_generate, 100, 100000000);
    // WARNING: Ensure sufficient budget

    for (auto i=0ul; i<number_of_wallet_files; i+=1) {
        auto filename = dirname + "/" + wallet_prefix + std::to_string(i);
        // Make a new genesis file
        std::ofstream walfile(filename);
        if(!walfile.is_open()) {
            printf("Failed to open wallet file\n");
            return 1;
        }

        auto idx = 2*i;
        std::cout << "Writing Wallet to file: " << filename << std::endl;
        walfile << wallets[idx] << std::endl;
        walfile << wallets[idx+1] << std::endl;
        
        walfile.close();
    }

    std::cout << "Created replica files in "
        << dirname + "/" + fileNamePrefix
        << std::endl;
    std::cout << "Created client files in "
        << dirname + "/" + clientFileName
        << std::endl;

    return 0;
}