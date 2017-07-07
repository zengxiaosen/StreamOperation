#
# Need to clone the trunk and then set up the build environment.

# Run the system.
# The following command line specifies the $DISPLAY so that it can use the host's X server.
# To use this function, run xhost + command before starting the docker container.

sudo docker run --net=host --name dev_system -v $HOME/project:/home/project:rw -e DISPLAY=$DISPLAY --rm -v /tmp/.X11-unix:/tmp/.X11-unix -it --privileged orangelab/stream_server

# Exec the system

# sudo docker exec -it dev_system /bin/bash

