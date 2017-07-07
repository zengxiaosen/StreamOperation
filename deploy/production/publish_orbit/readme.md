For master-slave

1. enter to the docker
```
docker exec -it ol_orbit_v1 /bin/bash
```
2. git pull the newest code   
```
cd /home/repository/stream_service
git pull 
cd /home/olive
git pull
```
3. setup workspace 
```
cp -rf /home/repository/stream_service/deploy/production/publish_orbit /home/public_orbit
```
4. build master and slave
```
cd /home/public_orbit
./build.sh
```
5. run master 
```
./restart_master.sh
```
6. run slave
```
./restart_slave.sh orbit.intviu.cn:12350
```

