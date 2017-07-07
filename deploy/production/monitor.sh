#!/bin/bash
# author stephen 2015-09-29

public_ip=`curl -s http://whatismyip.akamai.com/`

# send mail. 
report()
{
    logs=${1}
    curl -s --user 'api:key-da0c25d63e0e6d51737203d2639d9aba' \
	https://api.mailgun.net/v3/office.intviu.cn/messages \
        -F from='orbit live <mailgun@office.intviu.cn>' \
        -F to=eric@orangelab.cn \
        -F to=zhihan@orangelab.cn \
        -F to=qingyong@orangelab.cn \
        -F to=stephen@orangelab.cn \
        -F to=chenteng@orangelab.cn \
        -F subject="[ERROR] orbit live ${public_ip}" \
        -F text="${logs}" 
    echo "Mail send done."
}

# check memory used. example like: checkMem janus 50
checkMem() {
    # $1=<target process name>, $2=<set limit used percent of memory>
    target=$1
    limit=$2
    now=$(date +"%Y%m%d%H%M%S")
    result=`ps aux|grep ${target}|grep -v grep|awk '{print $4}'|awk -F. '{print $1}'`
    mem=`echo $result | awk -F. '{print $1}'`
    if [ "${mem}" -gt "${limit}" ]; then
        error_message="[$now] ${target} used ${result}% of MEM greater than ${limit}%."
        report "${error_message}"
    fi
}

_restartOrbit() {
    #part=$(date +"%Y%m%d")
    part=oribt
    node_port=3800
    orbit_port=10002
    tmp_log=`pwd`/tmp_monitor.log

    # kill node server and bazel
    ps aux|grep server.js|grep -v grep |awk '{print $2}' | xargs kill -9 
    ps aux|grep bazel|grep -v grep |awk '{print $2}' | xargs kill -9 

    # start orbit
    cd /home/repository
    echo "restart at "$(date +"%Y-%m-%d %H:%M:%S") >> log_${part}.txt
    echo "========== orbit log =========" >> ${tmp_log} 
    tail -n 50 log_${part}.txt >> ${tmp_log} 
    #nohup bazel-bin/stream_service/orbit/server/orbit_stream_server --only_public_ip=true --port=${orbit_port} >> log_${part}.txt 2>&1 &
    nohup bazel-bin/stream_service/orbit/server/orbit_stream_server --only_public_ip=false --port=${orbit_port} >> log_${part}.txt 2>&1 &
    
    echo "" >> ${tmp_log} 

    # start node server
    cd /home/olive/kurento_media_server/server
    echo "restart at "$(date +"%Y-%m-%d %H:%M:%S") >> log_${part}.txt
    echo "========== node log =========" >> ${tmp_log} 
    tail -n 50 log_${part}.txt >> ${tmp_log}
    nohup nodejs ./server.js --orbithost=localhost:${orbit_port} --port=${node_port} >> log_${part}.txt 2>&1  &

    # report error
    error_message=`cat ${tmp_log}`
    rm ${tmp_log}
    report "${error_message}"

    # statuz html 
    cd /home/statusz_htmls/
    cp tmp.html statusz_$(date +"%Y%m%d%H%M%S").html
}

checkOrbitLive() {
    # check Orbit Live 
    ps auxw | grep orbit_stream_server | grep -v grep > /dev/null
    if [ $? != 0 ]
    then
        _restartOrbit
    fi
    # check node 
    ps auxw | grep server.js | grep -v grep > /dev/null
    if [ $? != 0 ]
    then
        _restartOrbit
    fi
}

curdir=`pwd`
${curdir}/make_statuz.sh tmp
echo ${curdir}/make_statuz.sh tmp
last_minute=0
while [ true ] ; do
    minute=$(date +"%M")
    # job 1: do things below per minute
    if [ "${last_minute}" -ne "${minute}" ]
    then
        last_minute=${minute}        
        echo "do job 1 at minute:"${minute}
        checkMem orbit_stream_server 50
        # statuz html
        ${curdir}/make_statuz.sh tmp
    fi
    # job 2: do things below per 2 seconds 
    checkOrbitLive  
    sleep 2 
done
