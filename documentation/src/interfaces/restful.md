---
layout: default
title: RESTful
parent: Interfaces
nav_order: 2
---
# RESTful Interface
Use the RESTful interface for easy and quick setup. It is the default interface that supports all commands and uses GET requests only. The [Quick Start Guide](/interfaces/quickstart.html) uses this interface extensively. For bash scripting, it is recommended to use `cURL` to issue the commands, and `jq` to parse the return codes and status.

## Examples
```bash
HOST="wsx100.local" # or <ip>
TARGET="0" # Only when daisy-chain enabled (default 0)
QUEUE="0" # Needed only for motor control commands (default 0)
curl -sS "http://${HOST}/api/<COMMAND>?arg1=<ARG1>&arg2=<ARG2>&target=${TARGET}&queue=${QUEUE}"


# Eample using Execution Queue (Queue 0, the default) and no daisy-chaining
curl -sS "http://${HOST}/api/motor/run?dir=forward&stepss=100"

# Example adding to Queue 3
curl -sS "http://${HOST}/api/goto?pos=800&queue=3"

# Example that gets the current motor state of
# target 1 (on daisy-chain) and pipes to jq for parsing
curl -sS "http://${HOST}/api/motor/state?target=${TARGET}" | jq
```

