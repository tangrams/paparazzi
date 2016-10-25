#!/bin/bash
SLACK_SNIP=''

paparazzi_worker ipc:///tmp/proxy_out ipc:///tmp/loopback &> worker_$$.log

curl -X POST --data-urlencode 'payload={"channel": "#paparazzi", "username": "worker_'$$'", "text": I just die.", "icon_emoji": ":ghost:"}' $SLACK_SNIP