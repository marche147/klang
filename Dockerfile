FROM ubuntu:focal
MAINTAINER i@shiki7.me 

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get upgrade -y && apt-get install -y \
    cmake \
    gcc \
    bison \
    flex \
    python3 \
    python3-pip \
    xinetd
RUN pip3 install -U pip requests

RUN mkdir /home/ctf/ \
  && groupadd -g 1001 ctf \
  && useradd -g ctf -u 1001 -d /home/ctf/ ctf \
  && chown ctf:ctf /home/ctf/

ADD flag /flag
RUN chown root:root /flag && chmod 644 /flag

RUN mkdir /workdir && chown root:root /workdir && chmod 755 /workdir

RUN mkdir /challenge/
ADD runtime/ /challenge/runtime/
ADD build/compiler/klang /challenge/klang 
ADD deploy/server.py /challenge/server.py
ADD deploy/api.py /challenge/api.py
ADD deploy/run.sh /challenge/run.sh
RUN chmod 700 /challenge/ /challenge/runtime 
RUN chmod -R 640 /challenge/runtime/* && chmod 750 /challenge/klang && chmod 750 /challenge/server.py && chmod 640 /challenge/api.py && chmod 750 /challenge/run.sh

ADD deploy/klang /etc/xinetd.d/

EXPOSE 9999 
ENTRYPOINT ["/usr/sbin/xinetd", "-dontfork"]
