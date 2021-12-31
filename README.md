# icmptunnel
## icmptunnel
The code is implements TCP session over ICMP packets.
It allows the users to connect with TCP using ICMP echo request and reply packet.
## Creators
Yannay Mizrachi
Benel Tayar
## Installation
```bash
cmake -DCMAKE_BUILD_TYPE=Release -G "CodeBlocks - Unix Makefiles" .
make
```
### server
```bash
./server
```
### client
```bash
./client <proxy_address> <local_port> <destination_address> <destination_port>
```
