#!/bin/bash
# author stephen 2015-09-29

## define
public_ip=`curl -s http://whatismyip.akamai.com/`
current_path=`pwd`

statusz_cache_url=http://${public_ip}:7777
statusz_cache_path=${current_path}/statusz_cache
http_port=12000
orbit_port=3800

orbit_bin=${current_path}/orbit
orbit_master=${orbit_bin}/orbit_master
logs_path=${current_path}/logs
last_orbit_log=${logs_path}/master_first.log
last_olive_log=${logs_path}/olive_first.log

olive_path=/home/olive/kurento_media_server/server/

## init
mkdir -p ${statusz_cache_path}
mkdir -p ${logs_path}

## start statuzs cache webservice
ps aux|grep python|grep -v grep |awk '{print $2}' | xargs kill -9
cd ${statusz_cache_path}
nohup python -m SimpleHTTPServer 7777 &

# send mail. 
report()
{
    logs=${1}
    nohup curl -s --user 'api:key-da0c25d63e0e6d51737203d2639d9aba' \
	https://api.mailgun.net/v3/office.intviu.cn/messages \
        -F from='master orbit <mailgun@office.intviu.cn>' \
        -F to=stephen@orangelab.cn \
        -F to=eric@orangelab.cn \
        -F to=zhihan@orangelab.cn \
        -F to=qingyong@orangelab.cn \
        -F to=chenteng@orangelab.cn \
        -F subject="[ERROR] tt master orbit ${public_ip}" \
        -F text="${logs}" &
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
    echo mem:${mem}-limit:${limit}
    if [ "${mem}" -gt "${limit}" ]; then
        error_message="[$now] ${target} used ${result}% of MEM greater than ${limit}%."
        report "${error_message}"
    fi
}

# make a statusz snapshot 
makeStatuzSnapshot() {
   cd ${statusz_cache_path}
   wget --no-check-certificate https://${public_ip}:${http_port}/mstatus
   mv mstatus master_index.html
   sed -i "s/\/file?file=Chart.min.js/Chart.min.js/g" master_index.html
   sed -i "s/\/file?file=statusz.css/statusz.css/g" master_index.html
}

# restart orbit
_restartOrbit() {
    part=$(date +"%Y%m%d%H%M%S")
    tmp_log=${logs_path}/tmp_monitor_${part}.log
    new_orbit_log=${logs_path}/master_${part}.log
    new_olive_log=${logs_path}/olive_${part}.log

    # statusz cache 
    statuz_cache_html=slave_${part}.html
    cp ${statusz_cache_path}/master_index.html ${statusz_cache_path}/${statuz_cache_html}  

    # kill node server and bazel
    ps aux|grep server.js|grep -v grep |awk '{print $2}' | xargs kill -9 
    ps aux|grep orbit_master|grep -v grep |awk '{print $2}' | xargs kill -9 

    # report error
    echo "restart at "$(date +"%Y-%m-%d %H:%M:%S") >> ${last_orbit_log} 
    echo "restart at "$(date +"%Y-%m-%d %H:%M:%S") >> ${last_olive_log} 
    echo "========== statusz cache  =========" >> ${tmp_log} 
    echo ${statusz_cache_url}/${statuz_cache_html} >> ${tmp_log}  
    echo "========== master orbit log =========" >> ${tmp_log} 
    tail -n 50 ${last_orbit_log} >> ${tmp_log} 
    echo "========== olive log =========" >> ${tmp_log} 
    tail -n 50 ${last_olive_log} >> ${tmp_log}
    error_message=`cat ${tmp_log}`
    rm ${tmp_log}

    # start slave orbit
    cd ${orbit_bin}
    nohup ./orbit_master --logtostderr > ${new_orbit_log} 2>&1 & 

    # start olive 
    cd ${olive_path}
    nohup nodejs ./server.js --report=true --port=${orbit_port} >> ${new_olive_log} 2>&1  &
    
    # setting last log
    last_orbit_log=${new_orbit_log}
    last_olive_log=${new_olive_log}

    report "${error_message}"

}

checkOrbitLive() {
    # check Orbit 
    ps auxw | grep orbit_master | grep -v grep > /dev/null
    if [ $? != 0 ]
    then
        ## do restart
        _restartOrbit
    fi
    
    # check Olive 
    ps auxw | grep server.js | grep -v grep > /dev/null
    if [ $? != 0 ]
    then
        ## do restart
        _restartOrbit
    fi

}

last_minute=0
while [ true ] ; do
    minute=$(date +"%M")
    # job 1: do things below per minute
    if [ "${last_minute}" -ne "${minute}" ]
    then
        last_minute=${minute}        
        echo "do job checkMem at minute:"${minute}
        checkMem orbit_stub 50
        echo "do job makeStatuzSnapshot at minute:"${minute}
        makeStatuzSnapshot
    fi
    # job 2: do things below per 2 seconds 
    checkOrbitLive  
    sleep 2 
done
