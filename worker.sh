#!/bin/bash

paparazzi_worker ipc:///tmp/proxy_out ipc:///tmp/loopback &> worker_$$.log
