#!/bin/bash
# Setup the development environment and repo
# @chengxu
# Usage:
#   setup_repo.sh [REPO_PATH]
# The REPO_PATH should be a relative path from $HOME/project/

mkdir $HOME/project/$1
cd $HOME/project/$1
git clone https://github.com/mzhaom/trunk && cd trunk
echo "It will takes a few hours..."
git submodule update --init

cd $HOME/project/$1/trunk
git clone https://chengxu@bitbucket.org/chengxu/stream_service.git

# The CROSSTOOL needs updated.
cp stream_service/CROSSTOOL tools/cpp/CROSSTOOL

