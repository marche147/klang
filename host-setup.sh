#!/bin/sh

# install dependencies
apt install -y tcpdump

# make shared directory
mkdir /files/

# setup traffic capture
mkdir /root/pcap/
cp ./deploy/capture.sh /root/ && chmod +x /root/capture.sh
pushd .
cd /root/
nohup ./capture.sh & > /dev/null
popd