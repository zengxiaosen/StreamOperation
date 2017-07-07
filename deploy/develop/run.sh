#!/bin/bash

if [ $# != 1 ] ; then 
    echo "USAGE: $0 <version>" 
    echo " e.g.: $0 v1" 
    exit 1; 
fi 

version=$1

## check projects if existed
codepath=$HOME/mygit
if [ -d $codepath/stream_service ]; then
    echo "project stream_service found"
else
    echo "[Error] project stream_service not found in $codepath/stream_service"
    exit
fi

if [ -d $codepath/olive2 ]; then
    echo "project olive2 found"
else
    echo "[Error] project stream_service not found in $codepath/stream_service"
fi

docker stop ol_orbit_${version}
docker rm ol_orbit_${version}

docker run --name ol_orbit_${version} --net=host -v $codepath/stream_service:/home/repository/stream_service \
    -v $codepath/olive2:/home/olive2 -d --privileged orangelab/orbit:${version} /sbin/start.sh
