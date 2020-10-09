#!/bin/bash

g++ -O2 decode.cpp -o decode

./decode $1 >00-decoded.log
grep 'CONTROL_TRANSFER CONTROL SETUP' 00-decoded.log >00-setup.log
grep -v "GET_METERS" 00-setup.log > 00-clean.log
