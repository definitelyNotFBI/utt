# UTT-BFT: a Distributed Payment Infrastructure Implementation over Concord-BFT


<!-- ![Concored-bft Logo](TBD) -->

<!-- <img src="TODO.jpg" width="200" height="200" /> -->


## Overview of UTT-BFT

**UTT-BFT** is an implementation of decentralized ecash with accountable privacy. 

## Overview of Concord-BFT

**Concord-bft** is a generic state machine replication library that can handle malicious (byzantine) replicas.

Its implementation is based on the algorithm described in the paper [SBFT: a Scalable Decentralized Trust Infrastructure for
Blockchains](https://arxiv.org/pdf/1804.01626.pdf).

It is designed to be used as a core building block for replicated distributed data stores, and is especially suited to serve as the basis of permissioned Blockchain systems.

For a real-life integration example, please take a look at [Project Concord](https://github.com/vmware/concord), a highly scalable and energy-efficient distributed trust infrastructure for consensus and smart contract execution.

## Install and Build (Ubuntu Linux 18.04)

Concord-BFT supports two kinds of builds: native and docker.

The native build is **strongly recommended**.

### Native

```sh
git clone https://github.com/definitelyNotFBI/concord-bft
cd concord-bft
sudo ./install_deps.sh # Installs all dependencies and 3rd parties
make build -j $(nproc)
cd build && sudo make test
```

In order to turn on or off various options, you need to change your cmake configuration. This is
done by passing arguments to cmake with a `-D` prefix: e.g. `cmake -DBUILD_TESTING=OFF`. Note that
make must be run afterwards to build according to the configuration. Please see [CMakeLists.txt](CMakeLists.txt) for configurable options.

#### Select comm module
One option that is worth calling out explicitly is the communication (transport) library.

We support both UDP and TCP communication. UDP is the default. In order to
enable TCP communication, build with `-DBUILD_COMM_TCP_PLAIN=TRUE` in the cmake
instructions shown above.  If set, the test client will run using TCP. If you
wish to use TCP in your application, you need to build the TCP module as
mentioned above and then create the communication object using CommFactory and
passing PlainTcpConfig object to it.

We also support TCP over TLS communication. To enable it, change the
`BUILD_COMM_TCP_TLS` flag to `TRUE` in the main CMakeLists.txt file. When
running simpleTest using the testReplicasAndClient.sh - there is no need to create TLS certificates manually. The script will use the `create_tls_certs.sh` (located under the scripts/linux folder) to create certificates. The latter can be used to create TLS files for any number of replicas, e.g. when extending existing tests.

As we used pinned certificates for TLS, the user will have to manually provide these. THey can use the [create_tls_certs.sh](scripts/linux/create_tls_certs.sh) script as an example.


### C++ Linter

The C++ code is statically checked by `clang-tidy` as part of the [CI](https://github.com/vmware/concord-bft/actions?query=workflow%3Aclang-tidy).
<br>To check code before submitting PR, please run `make tidy-check`.

[Detailed information about clang-tidy checks](https://clang.llvm.org/extra/clang-tidy/checks/list.html).


#### (Optional) Python client

The python client is required for running tests. If you do not want to install python, you can
configure the build of concord-bft by running `cmake -DBUILD_TESTING=OFF ..` from the `build`
directory for native builds, and `CONCORD_BFT_CMAKE_BUILD_TESTING=TRUE make` for docker builds.

The python client requires python3(>= 3.5) and trio, which is installed via pip.

    python3 -m pip install --upgrade trio


#### Adding a new dependency or tool

## Apollo testing framework


The Apollo framework provides utilities and advanced testing scenarios for validating
Concord BFT's correctness properties, regardless of the running application/execution engine.
For the purposes of system testing, we have implemented a "Simple Key-Value Blockchain" (SKVBC)
test application which runs on top of the Concord BFT consensus engine.
<br>

Apollo enables running all test suites (without modification) against any supported BFT network
configuration (in terms of <i>n</i>, <i>f</i>, <i>c</i> and other parameters).
<br>

Various crash or byzantine failure scenarios are also covered
(including faulty replicas and/or network partitioning).
<br>

Apollo test suites run regularly as part of Concord BFT's continuous integration pipeline.

Please find more details about the Apollo framework [here](tests/apollo/README.md)

## Run examples


### Simple test application (4 replicas and 1 client on a single machine)

Tests are compiled into in the build directory and can be run from anywhere as
long as they aren't moved.

Run the following from the top level concord-bft directory:

   ./build/tests/simpleTest/scripts/testReplicasAndClient.sh

### Using simple test application via Python script

You can use the simpleTest.py script to run various configurations via a simple
command line interface.
Please find more information [here](./tests/simpleTest/README.md)

## Directory Structure


- [bftengine](./bftengine): concord-bft codebase
	- [include](./bftengine/include): external interfaces of concord-bft (to be used by client applications)
	- [src](./bftengine/src): internal implementation of concord-bft
    - [tests](./bftengine/tests): tests and usage examples
- [threshsign](./threshsign): crypto library that supports digital threshold signatures
	- [include](./threshsign/include): external interfaces of threshsign (to be used by client applications)
	- [src](./threshsign/src): internal implementation of threshsign
    - [tests](./threshsign/tests): tests and usage examples
- [scripts](./scripts): build scripts
- [tests](./tests): BFT engine system tests

## Contributing


The concord-bft project team welcomes contributions from the community. If you wish to contribute code and you have not
signed our contributor license agreement (CLA), our bot will update the issue when you open a Pull Request. For any
questions about the CLA process, please refer to our [FAQ](https://cla.vmware.com/faq). For more detailed information,
refer to [CONTRIBUTING.md](CONTRIBUTING.md).

## Notes
The library calls `std::terminate()` when it cannot continue in a safe manner.
In that way, users can install a handler that does something different than just calling `std::abort()`.

## Community


[Concord-BFT Slack](https://concordbft.slack.com/).

Request a Slack invitation via <concordbft@gmail.com>.

## License

concord-bft is available under the [Apache 2 license](LICENSE).
