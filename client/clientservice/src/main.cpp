// Concord
//
// Copyright (c) 2021 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <boost/program_options.hpp>
#include <yaml-cpp/yaml.h>

#include "client/clientservice/client_service.hpp"
#include "client/clientservice/configuration.hpp"
#include "client/concordclient/concord_client.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"

using concord::client::clientservice::ClientService;
using concord::client::clientservice::configureSubscription;
using concord::client::clientservice::parseConfigFile;

using concord::client::concordclient::ConcordClient;
using concord::client::concordclient::ConcordClientConfig;

namespace po = boost::program_options;

po::variables_map parseCmdLine(int argc, char** argv) {
  po::options_description desc;
  // clang-format off
  desc.add_options()
    ("config", po::value<std::string>()->required(), "YAML configuration file for the RequestService")
    ("host", po::value<std::string>()->default_value("0.0.0.0"), "Clientservice gRPC service host")
    ("port", po::value<int>()->default_value(50505), "Clientservice gRPC service port")
    ("bft-batching", po::value<bool>()->default_value(false), "Enable batching requests before sending to replicas")
    ("tr-id", po::value<std::string>()->required(), "ID used to subscribe to replicas for data/hashes")
    ("tr-insecure", po::value<bool>()->default_value(false), "Testing only: Allow insecure connection with TRS on replicas")
    ("tr-tls-path", po::value<std::string>()->default_value(""), "Path to thin replica TLS certificates")
  ;
  // clang-format on
  po::variables_map opts;
  po::store(po::parse_command_line(argc, argv, desc), opts);
  po::notify(opts);

  return opts;
}

int main(int argc, char** argv) {
  // TODO: Use config file and watch thread for logger
  auto logger = logging::getLogger("concord.client.clientservice.main");

  auto opts = parseCmdLine(argc, argv);

  ConcordClientConfig config;
  try {
    auto yaml = YAML::LoadFile(opts["config"].as<std::string>());
    parseConfigFile(config, yaml);
    configureSubscription(
        config, opts["tr-id"].as<std::string>(), opts["tr-insecure"].as<bool>(), opts["tr-tls-path"].as<std::string>());
  } catch (std::exception& e) {
    LOG_ERROR(logger, "Failed to configure ConcordClient: " << e.what());
    return 1;
  }
  LOG_INFO(logger, "ConcordClient configured");

  auto concord_client = std::make_unique<ConcordClient>(config);
  auto metrics = std::make_shared<concordMetrics::Aggregator>();
  concord_client->setMetricsAggregator(metrics);
  ClientService service(std::move(concord_client));

  auto server_addr = opts["host"].as<std::string>() + ":" + std::to_string(opts["port"].as<int>());
  LOG_INFO(logger, "Starting clientservice at " << server_addr);
  service.start(server_addr);

  return 0;
}
