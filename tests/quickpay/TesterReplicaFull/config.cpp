#include "config.hpp"
#include <string>

ReplicaConfig* ReplicaConfig::m_rconfig= nullptr;;

/**
 * Static methods should be defined outside the class.
 */
ReplicaConfig* ReplicaConfig::Get() {
    /**
     * This is a safer way to create an instance. instance = new Singleton is
     * dangeruous in case two instance threads wants to access at the same time
     */
    if(m_rconfig==nullptr){
        m_rconfig = new ReplicaConfig();
    }
    return m_rconfig;
}