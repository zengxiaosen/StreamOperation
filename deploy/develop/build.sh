#!/bin/bash

version=$1

docker pull docker.intviu.cn:5000/ol_stream_server:base

cp -rf ~/mygit/olive2 olive2 
cp -rf ~/mygit/stream_service stream_service

tar czvf olive2.tar.gz olive2 
tar czvf stream_service.tar.gz stream_service

docker build -t orangelab/orbit:${version} .

rm -rf olive2* stream_service*  
