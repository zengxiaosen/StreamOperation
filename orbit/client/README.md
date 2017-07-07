# README

## USE THE TEST
1. copy data.pb to stream_service/orbit/client/
2. build client_test
3. Execute (id is the session_id of the room that you want to go in)

## DOWNLOAD PB FILE
you can mkdir a new directory to save the pb file.

git clone https://iamzhanghao@bitbucket.org/iamzhanghao/orbit_data.git

## GUIDE TO USE
bazel build stream_service/orbit/client/client_test

bazel-bin/stream_service/orbit/client/client_test --logtostderr --session_id=(id)

