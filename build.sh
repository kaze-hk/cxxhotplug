#!/bin/bash
set -e

g++ -fPIC -shared -o score_op_v1.so score_op_v1.cpp
g++ -fPIC -shared -o score_op_v2.so score_op_v2.cpp
g++ -std=c++11 -o demo main.cpp -ldl -pthread
echo "Build done. Run with: ./demo"
