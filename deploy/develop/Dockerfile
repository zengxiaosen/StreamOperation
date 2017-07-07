FROM docker.intviu.cn:5000/ol_stream_server:base 

# install orbit 
ADD olive2.tar.gz /home/
ADD stream_service.tar.gz /home/repository/

ADD build/start.sh /sbin/start.sh
RUN chmod 755 /sbin/start.sh

