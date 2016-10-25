#include "prime_server.hpp"
#include "http_protocol.hpp"
using namespace prime_server;
#include "logging.hpp"

int main(int argc, char** argv) {
    if(argc < 3) {
        LOG_ERROR("Usage: " + std::string(argv[0]) + " [tcp|ipc]://upstream_endpoint[:tcp_port] [tcp|ipc]://downstream_endpoint[:tcp_port]");
        return EXIT_FAILURE;
    }

    //TODO: validate these
    std::string upstream_endpoint(argv[1]);
    std::string downstream_endpoint(argv[2]);
    if(upstream_endpoint.find("://") != 3)
        LOG_ERROR("bad upstream endpoint");
    if(downstream_endpoint.find("://") != 3)
        LOG_ERROR("bad downstream endpoint");

    //start it up
    zmq::context_t context;
    proxy_t proxy(context, upstream_endpoint, downstream_endpoint, 
        [](const std::list<zmq::message_t>& heart_beats, const std::list<zmq::message_t>& job) -> const zmq::message_t* {
            //have a look at each heartbeat
            for(const auto& heart_beat : heart_beats) {
                //so the heartbeat is a single char either A or B
                const auto& beat_type = static_cast<const char*>(heart_beat.data())[0];
                //but the job is in netstring format, so either 1:A, or 1:B,
                const auto& job_type = static_cast<const char*>(job.front().data())[2];
                //do we like this heartbeat
                if(beat_type == job_type)
                return &heart_beat;
            }
            //all of the heartbeats sucked so pick whichever
          return nullptr;
        }
    );

    //TODO: catch SIGINT for graceful shutdown
    proxy.forward();

    return EXIT_SUCCESS;
}