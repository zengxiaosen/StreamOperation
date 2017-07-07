README
-------------------------------------------
In production envrionment:

* Start the master server.
Command line:
bazel-bin/stream_service/orbit/master_server/orbit_master_server


* Start the stream_server in slave mode. And point them to the master server.
Command line:

bazel-bin/stream_service/orbit/server/orbit_stream_server --slave_mode=true --master_server="master.intviu.cn:10000"

if the machine is not a public IP machine, set the --set_local_address=192.168.10.1 etc. as the local address.

One example test setting for my desktop:

Start slave using command:
-----
bazel-bin/stream_service/orbit/server/orbit_stream_server --slave_mode=true --master_server="192.168.1.110:12350" --logtostderr --port=13000 --set_local_address=192.168.1.110
-----

Start master using command:
-----
bazel-bin/stream_service/orbit/master_server/orbit_master_server --logtostderr
-----



To test the registry:

Start the master server:

bazel-bin/stream_service/orbit/master_server/orbit_master_server


and then
bazel-bin/stream_service/orbit/master_server/registry_test_client --master_server="master.intviu.cn:10000" --test_case="register"

run another program in other console, make the test_case is query:
bazel-bin/stream_service/orbit/master_server/registry_test_client --master_server="master.intviu.cn:10000" --test_case="query"

And it will list all the registered slave servers.

Start rocks!


