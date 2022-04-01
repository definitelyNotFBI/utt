#pragma once 

#include <memory>
#include "PrimitiveTypes.hpp"
#include "SimpleThreadPool.hpp"
#include "ReplicaImp.hpp"
#include "messages/ClientReplyMsg.hpp"

namespace bftEngine::impl {

class AsyncUTTPostExecution : public util::SimpleThreadPool::Job {
public:
AsyncUTTPostExecution(ReplicaImp* myReplica, std::unique_ptr<ClientReplyMsg> ReplyMsg, uint16_t client_id): ReplyMsg{std::move(ReplyMsg)} {
    this->me = myReplica;
    this->client_id = client_id;
    // TODO: get access to utt bank parameters here
}

virtual ~AsyncUTTPostExecution(){};

// Code to execute on committing
virtual void execute() override {
    me->send(this->ReplyMsg.get(), this->client_id);
}

// What to do on finishing
void release() override { delete this; }

private:
ReplicaImp* me;
std::unique_ptr<ClientReplyMsg> ReplyMsg = nullptr;
uint16_t client_id;
};

}