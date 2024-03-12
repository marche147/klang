FROM ubuntu:focal
MAINTAINER i@shiki7.me 

RUN apt-get update && apt-get install -y \
    cmake \
    gcc \
    bison \
    flex

RUN mkdir /challenge/
ADD ./runtime/ /challenge/runtime/
ADD ./build/compiler/klang /challenge/klang 
ADD ./scripts/compile.sh /challenge/compile.sh
RUN chmod +x /challenge/compile.sh

ENTRYPOINT ["/bin/bash"]
