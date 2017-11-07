*********************
cs 6210 - project 3 - online store using grpc

Rakesh Belagali
Mekhala HS
**********************

build:
    make

clean:
    make clean

run:
    ./store 0.0.0.0:50001 10
    Two cmd args - 
    1. server address with port
    2. number of threads in threadpool

vendor address taken from a file in current folder "vendor_addresses.txt"


server logs - 
1.
--------------------------------------------
------Responded successfully for trimmer2 --------
--------------------------------------------

This is printed once a response has been sent back to user successfully.

2.
Whenever a thread on threadpool gets work following is logged - 
Got Job! Thread ID 6

3.
HandleRpcs:: inside while:: inside pool->Add:: tag->status = 2
This is printed when status changes of any query.

4.
Adding vendors from file - 
added to vendor list localhost:50052
added to vendor list localhost:50053
added to vendor list localhost:50054
added to vendor list localhost:50055