#include "config.hpp"
#include <string>

ClientConfig* ClientConfig::m_rconfig= nullptr;;

/**
 * Static methods should be defined outside the class.
 */
ClientConfig* ClientConfig::Get() {
    /**
     * This is a safer way to create an instance. instance = new Singleton is
     * dangeruous in case two instance threads wants to access at the same time
     */
    if(m_rconfig==nullptr){
        m_rconfig = new ClientConfig();
    }
    return m_rconfig;
}