#include <memory>

namespace fullpay::tworound::client {

/* VERY DELICATE: Do not remove this from here.
 * protocol and conn are tightly coupled to each other, so a way to break the coupling is to use forward declaration here, that both of them can include.
 */
class conn_handler;
class protocol;
typedef std::shared_ptr<conn_handler> conn_handler_ptr;

}
