#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

#include <asio.hpp>
#include <asio/buffer.hpp>
#include <asio/basic_waitable_timer.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>
#include <vector>

#include "Logger.hpp"
#include "Logging4cplus.hpp"
#include "communication/CommDefs.hpp"
#include "common.hpp"
#include "histogram.hpp"
#include "quickpay/TesterClient/conn.hpp"
#include "client/Params.hpp"

namespace quickpay::client {

struct MatchedResponse {
    std::vector<std::vector<uint8_t>> responses;
    std::vector<uint16_t> ids;

    std::set<uint16_t> responses_from;

    void add(uint16_t id, uint8_t* data_ptr, size_t bytes) {
        if (responses_from.count(id)) {
            LOG_WARN(GL, "Got duplicate repsonse from "<< id);
            return;
        }
        responses_from.insert(id);
        std::vector<uint8_t> data(bytes);
        memcpy(data.data(), data_ptr, bytes);
        responses.push_back(data);
        ids.push_back(id);
    }

    void clear() {
        responses_from.clear();
        ids.clear();
        responses.clear();
    }

    size_t size() const {
        return ids.size();
    }
};


/*
 * The quickpay protocol for the replicas
 */
class protocol : public std::enable_shared_from_this<protocol> {
    typedef asio::ip::tcp::socket sock_t;
    typedef asio::ip::tcp::endpoint endpoint_t;
    typedef asio::io_context io_ctx_t;
    typedef asio::ip::tcp::resolver resolver_t;

    static logging::Logger logger;

friend class conn_handler;

// ASIO related stuff
private:
    io_ctx_t& m_io_ctx_;
    asio::basic_waitable_timer<std::chrono::steady_clock> timer;
    asio::basic_waitable_timer<std::chrono::steady_clock> tx_timer;

private:
    // Mutex to guard m_conn_
    std::mutex m_conn_mutex_;
    // WARNING: Access m_conn_ only after acquiring m_conn_mutex_
    std::vector<conn_handler_ptr> m_conn_;
    // Mutex to guard responses
    std::mutex m_resp_mtx_;
    // WARNING: Access m_conn_ only after acquiring m_conn_mutex_
    MatchedResponse matched_response;

// BFT data
private:
    std::unordered_map<uint16_t, asio::ip::tcp::endpoint> m_node_map_;
    std::unique_ptr<utt_bft::client::Params> m_params_ = nullptr;
    std::vector<std::tuple<libutt::CoinSecrets, libutt::CoinComm, libutt::CoinSig>> my_initial_coins;
    libutt::LTSK my_ltsk;
    libutt::LTPK my_ltpk;

// Other data
private:
    uint16_t experiment_idx;

// Metrics
private:
    std::mutex m_metric_mtx_;
    concordUtils::Histogram hist;
    uint64_t begin, throughput_begin;


// Functions
public:
    protocol(asio::io_context& io_ctx, bft::communication::NodeMap& node_map,
                   utt_bft::client::Params &&m_params_);

    // Start the protocol loop
    void start();

    // Add a connection
    void add_connection(conn_handler_ptr conn_ptr);

    // Adds a response
    void add_response(uint8_t* ptr, size_t data, uint16_t id);

    // Start the experiment
    void start_experiments();

    // Send the transaction
    void send_tx();
    void on_timeout(const asio::error_code& err);
    void on_tx_timeout(const asio::error_code err);
};

} // namespace quickpay::replica