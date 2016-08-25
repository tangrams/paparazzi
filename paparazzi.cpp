//prime_server guts
#include <prime_server/prime_server.hpp>
#include <prime_server/http_protocol.hpp>
using namespace prime_server;

//nuts and bolts required
#include <functional>
#include <string>
#include <csignal>

class paparazzi_t {
 public:
  //initialize tangram objects
  paparazzi_t(/*TODO: parameters to initialize tangram objects*/) {
    //TODO: initialize tangram objects
  }

  //actually serve up content
  worker_t::result_t work(const std::list<zmq::message_t>& job, void* request_info) {
    //false means this is going back to the client, there is no next stage of the pipeline
    worker_t::result_t result{false};
    //this type differs per protocol hence the void* fun
    auto& info = *static_cast<http_request_t::info_t*>(request_info);
    http_response_t response;
    try {
      //TODO: actually use/validate the request parameters
      auto request = http_request_t::from_string(
        static_cast<const char*>(job.front().data()), job.front().size());
      //TODO:get your image bytes here
      std::string image(":p"); // = tangram.render(args...);
      response = http_response_t(200, "OK", image);
    }
    catch(const std::exception& e) {
      //complain
      response = http_response_t(400, "Bad Request", e.what());
    }
    //does some tricky stuff with headers and different versions of http
    response.from_info(info);
    //formats the response to protocal that the client will understand
    result.messages.emplace_back(response.to_string());
    return result;
  }

  void cleanup() {
    //TODO: clear tangram state between requests to avoid side effects
    //only need this if requests change the state of our tangram objects
    //in such a way that future requests are affected
  }

 protected:
  //TODO: keep some tangram objects here

};

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
  paparazzi_t paparazzi_worker{/*TODO: parameters to initialize tangram objects*/};
  worker_t worker(context, upstream_endpoint, "ipc:///dev/null", loopback_endpoint,
    std::bind(&paparazzi_t::work, std::ref(paparazzi_worker), std::placeholders::_1, std::placeholders::_2),
    std::bind(&paparazzi_t::cleanup, std::ref(paparazzi_worker)));
  worker.work();

  //listen for SIGINT and terminate if we hear it
  std::signal(SIGINT, [](int s){ exit(1); });

  return EXIT_SUCCESS;
}
