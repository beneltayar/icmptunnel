# icmptunnel
## icmptunnel
The code is inplement TCP session over ICMP packets.
It allows the users to connect with TCP using ICMP echo request and reply packet.
## creators
Yannay Mizrachi and Benel Tayar
## instalation
cmake -DCMAKE_BUILD_TYPE=Release -G "CodeBlocks - Unix Makefiles" .
make
### server
./server
### client
./client <proxy_address> <local_port> <destination_address> <destination_port>
