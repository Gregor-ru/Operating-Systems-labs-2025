#!/bin/bash

rm -rf build
mkdir build
cd build

cmake ..
make

cd ..
echo "Build finished! Run with:"
echo "./build/host_mq    (Message Queues)"
echo "./build/host_fifo  (Named Pipes)"
echo "./build/host_sock  (Sockets)"