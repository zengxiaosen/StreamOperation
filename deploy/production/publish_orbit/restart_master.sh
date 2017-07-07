#!/bin/bash

work_path=`pwd`
orbit_bin=/home/publish_orbit/orbit
olive_path=/home/olive/kurento_media_server/server/

logs_path=${work_path}/logs
mkdir -p ${logs_path} 
last_master_log=${logs_path}/master_first.log
last_olive_log=${logs_path}/olive_first.log
monitor_master_log=${logs_path}/monitor_master.log

ps aux|grep bazel|grep -v grep| awk '{print $2}' | xargs kill -9
ps aux|grep monitor_master|grep -v grep| awk '{print $2}' | xargs kill -9
ps aux|grep server.js|grep -v grep |awk '{print $2}' | xargs kill -9 
ps aux|grep orbit_master|grep -v grep |awk '{print $2}' | xargs kill -9 

# start orbit master
echo "########" >> ${last_master_log}
cd ${orbit_bin} 
nohup ./orbit_master --logtostderr >> ${last_master_log} 2>&1 &

# start olive 
echo "########" >> ${last_olive_log}
cd ${olive_path}
nohup nodejs ./server.js --report=true --port=3800 >> ${last_olive_log} 2>&1  &

# start montior
cd ${work_path} 
nohup ./monitor_master.sh >> ${monitor_master_log} 2>&1 &

