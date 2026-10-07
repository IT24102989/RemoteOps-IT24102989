# RemoteOps - IT24102989

## Personalisation
- Registration Number: IT24102989
- Agent Port: 9410 (= 7000 + 2410)
- SID: 9892 (last 4 digits 2989 reversed)
- Auth Token: OPS-2989
- Source Files: agent_989.c, controller_989.c, Makefile_989
- Log File: remoteops_IT24102989.log
- Storage Path: ./agentfiles/IT24102989/

## Build Instructions
    make -f Makefile_989

## Run Instructions
Terminal 1: ./agent_989
Terminal 2: ./controller_989

## Concurrency Model
Thread-per-client (one POSIX thread per Controller connection).
