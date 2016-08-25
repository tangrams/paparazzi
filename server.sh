#!/bin/bash
#compile paparazzi
g++ -std=c++11 tangram.cpp -O2 -lprime_server -o paparazzi_worker
#start http server
prime_httpd tcp://*:8002 ipc:///tmp/proxy_in ipc:///tmp/loopback &
#start proxy
prime_proxyd ipc:///tmp/proxy_in ipc:///tmp/proxy_out
#start 3 tangram workers
for i in {0..2}; do
  ./paparazzi_worker ipc:///tmp/proxy_out ipc:///tmp/loopback &
done

#now 5 processes are running so kill them all before starting more ;)