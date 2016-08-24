#pragma once

// ZeroMQ
#include <zmq.h>
#include "utils.h"

void *context;
void *requester;

void zmqConnect (int _port) {
    context = zmq_ctx_new();
    requester = zmq_socket(context, ZMQ_REQ);
    zmq_connect (requester, std::string("tcp://localhost:"+toString(_port)).c_str());
}

bool zmqRecv (std::string &_buffer) {
    char buffer[140];
    bool rta = zmq_recv(requester, buffer, 140, 0);
    _buffer = std::string(buffer);
    return rta;
}

bool zmqSend (std::string _buffer) {
    return zmq_send(requester, _buffer.c_str(), _buffer.size(), 0);
}

void zmqClose() {
    zmq_close(requester);
    zmq_ctx_destroy(context);
}
