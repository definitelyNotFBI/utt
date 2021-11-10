#include <iostream>

#include "quickpay/TesterReplica/conn.hpp"

namespace quickpay::replica {

void conn_handler::on_new_conn() {
    std::cout << "New client connection" << std::endl;
}

} // namespace quickpay::replica