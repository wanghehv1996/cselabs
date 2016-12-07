#!/bin/sh

./stop.sh
./stop.sh

./start.sh
./test-lab-6 ./yfs1
./stop.sh
./stop.sh

rm log
