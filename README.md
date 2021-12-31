# icmptunnel
## icmptunnel
The code is inplement TCP session over ICMP packets.
It allows the users to connect with TCP using ICMP echo request and reply packet.
## creators
Yannay Mizrachi and Benel Tayar
## instalation
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
