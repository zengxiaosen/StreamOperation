#!/bin/bash
set -e

while true
do
   ./record_replay.sh /home/records   
   echo run ./record_replay.sh /home/records   
   sleep 60
done
