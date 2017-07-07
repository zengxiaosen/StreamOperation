#!/bin/bash

version=$1

docker pull docker.intviu.cn:5000/ol_orbit:${version}
docker stop ol_orbit_${version}
docker rm ol_orbit_${version}

docker run --name ol_orbit_${version} --net=host -d --privileged docker.intviu.cn:5000/ol_orbit:${version} /sbin/start.sh
