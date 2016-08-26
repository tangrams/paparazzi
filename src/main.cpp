//prime_server guts
#include <prime_server/prime_server.hpp>
#include <prime_server/http_protocol.hpp>
using namespace prime_server;

//nuts and bolts required
#include <functional>
#include <string>
#include <csignal>

// Paparazzi
#include "paparazzi.h"

int main(int argc, char* argv[]) {
    //we need the location of the proxy and the loopback
    if(argc < 3)
        return EXIT_FAILURE;

    //gets requests from the http server
    auto upstream_endpoint = std::string(argv[1]);
    //or returns just location information back to the server
    auto loopback_endpoint = std::string(argv[2]);

    //listen for requests
    zmq::context_t context;
    Paparazzi paparazzi_worker();
    worker_t worker(context, upstream_endpoint, "ipc:///dev/null", loopback_endpoint,
        std::bind(&Paparazzi::work, std::ref(paparazzi_worker), std::placeholders::_1, std::placeholders::_2),
        std::bind(&Paparazzi::cleanup, std::ref(paparazzi_worker)));
    worker.work();

    //listen for SIGINT and terminate if we hear it
    std::signal(SIGINT, [](int s){ exit(1); });

    return EXIT_SUCCESS;
}
