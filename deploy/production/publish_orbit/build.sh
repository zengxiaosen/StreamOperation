#!/bin/bash

currentpath=`pwd`
coderoot=/home/repository
output=$currentpath/orbit

mkdir -p $output 
# remove orbit_master and orbit_slave if existed
rm -rf $output/orbit_*  

## build master,slave and make dependence files  
cd $coderoot
bazel build stream_service/orbit/master_server/orbit_master_server
bazel build stream_service/orbit/server/orbit_stream_server
tar czvf stream_service.tar.gz \
      stream_service/orbit/http_server/html \
      stream_service/orbit/master_server/html \
      stream_service/orbit/server/certs \
      stream_service/orbit/olivedata/production

## move to output
cd $output
cp $coderoot/bazel-bin/stream_service/orbit/master_server/orbit_master_server orbit_master
cp $coderoot/bazel-bin/stream_service/orbit/server/orbit_stream_server orbit_slave
mv $coderoot/stream_service.tar.gz stream_service.tar.gz
tar zxvf stream_service.tar.gz
rm stream_service.tar.gz

## statuz cache
cd $currentpath
mkdir -p statusz_cache
cp $output/stream_service/orbit/http_server/html/*.css statusz_cache/ 
cp $output/stream_service/orbit/http_server/html/*.js statusz_cache/ 
