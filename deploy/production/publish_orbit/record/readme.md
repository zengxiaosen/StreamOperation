## 客户端运行同步脚本

(1) 运行脚本
ps aux|grep run_client|grep -v grep| awk '{print $2}' | xargs kill -9
nohup ./run_client.sh &
注：依赖目录/tmp/records

## 服务器端启动脚本

(1) 安装bos客户端
# install env
cd upload/bce-python-sdk-0.8.14
python setup.py install

(2) 运行脚本
ps aux|grep run_replay|grep -v grep| awk '{print $2}' | xargs kill -9
nohup ./run_replay.sh &
注：依赖目录/home/records

