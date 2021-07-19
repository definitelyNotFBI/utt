#include <cstdlib>
#include <stdexcept>
#include <sys/param.h>
#include <string>
#include <cstring>
#include <getopt.h>
#include <unistd.h>

#include "utt/Bank.h"
#include "utt/Utt.h"

using namespace libutt;

std::string dirname = "./";
std::string fileNamePrefix = "utt_pvt_replica_";
size_t n = 4 ;
size_t t = 1 ;

static struct option longOptions[] = {
    {"key-file-prefix", required_argument, 0, 'k'},
    {"node-num", required_argument, 0, 'n'},
    {"fault-num", required_argument, 0, 't'},
    {"output-dir", required_argument, 0, 'o'}
};

int main(int argc, char **argv)
{
    libutt::initialize(nullptr, 0);
    int o = 0;
    int optionIndex = 0;
    while((o = getopt_long(argc, argv, "k:n:t:o:", longOptions, &optionIndex)) != -1) {
        switch (o) {
            case 'k': {
                fileNamePrefix = optarg;
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

    libutt::Params p;

    BankThresholdKeygen keys(p, t, n);
    keys.writeToFiles(dirname, fileNamePrefix);
}