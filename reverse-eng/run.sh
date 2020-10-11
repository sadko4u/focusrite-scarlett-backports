#!/bin/bash

FILENAME=$1

if [ "$FILENAME" == "" ]; then
  echo "PCAP file as first argument is required";
  exit 1;
fi;

echo "Compiling toolchain..."
g++ -O2 decode.cpp -o decode
g++ -O2 checksum.cpp -o checksum

echo "Reading PCAP file..."
./decode "$FILENAME" "00-state.rom" >00-decoded.log
egrep ' CONTROL (SETUP|COMPLETE)' 00-decoded.log >00-setup.log
grep -v "GET_METERS" 00-setup.log > 00-clean.log

echo "Validating ROM checksum..."
./checksum 00-state.rom ec 1a70

echo "Execution has been completed"