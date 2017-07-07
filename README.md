# README

## Directory and structure
-------------------------
* orbit/        the main source code repository
* proto/        the directory of all the protocol buffers files.
* third_party/  the directory to hold all the libraries with source codes.
* deploy/       the production deployment scripts. 
* scripts/      the scripts to setup/install repo or something
* docker/       the docker file to generate the development dependencies library
* deprecated/   the deprecated code

Note that all the compiler and libraries are in the docker container, not in the host machine.

## INSTALLATION:

Steps to use the developer env for Olive.

1. Install the docker 
2. Use the script to build or fetch the docker
3. Execute the script in script/setup_repo
4. Start programming.

## GUIDE TO START:

use the bazel build system to compile and build the binary code. Run the code using the command line:

bazel build stream_service/orbit/server/orbit_stream_server

### Run the binary:

bazel-bin/stream_service/orbit/server/orbit_stream_server --logtostderr

