#!/bin/bash

if [ $# != 1 ] ; then
    echo "USAGE: ${0} <master host>" 
    echo " e.g.: ${0} orbit.intviu.cn:12350" 
    exit 1;
fi

master_server=${1}

work_path=`pwd`
orbit_bin=/home/publish_orbit/orbit

logs_path=${work_path}/logs
mkdir -p ${logs_path} 
logs_path=`pwd`/logs
new_log=${logs_path}/slave_first.log
monitor_slave_log=${logs_path}/monitor_slave.log

orbit_port=10010
http_port=11100

nice_min_port=50000
nice_max_port=60000

ps aux|grep bazel|grep -v grep| awk '{print $2}' | xargs kill -9
ps aux|grep orbit_slave|grep -v grep| awk '{print $2}' | xargs kill -9
ps aux|grep monitor_slave|grep -v grep| awk '{print $2}' | xargs kill -9

# start orbit
cd /home/publish_orbit
echo "########" >> ${new_log} 
cd orbit
nohup ./orbit_slave --nice_min_port=${nice_min_port} --nice_max_port=${nice_max_port} \
	--only_public_ip=true --slave_mode=true --master_server=${master_server} \
	--port=${orbit_port} --http_port=${http_port}  \
	--logtostderr >> ${new_log} 2>&1 &

# start montior
cd ${work_path} 
nohup ./monitor_slave.sh ${master_server} >> ${monitor_slave_log} 2>&1 &

