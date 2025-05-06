# syntax=docker/dockerfile:1.4
FROM --platform=linux/amd64 ubuntu:18.04

ENV TZ=Asia/Seoul \
    LANG=ko_KR.UTF-8 \
    LANGUAGE=ko_KR.UTF-8

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    locales tzdata \
    build-essential gdb vim curl git wget cmake make gcc sudo valgrind \
    gcc-multilib g++-multilib libc6-dev-i386 \
    && locale-gen ko_KR.UTF-8 \
    && update-locale LANG=ko_KR.UTF-8 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
# jungle 사용자 생성, sudo 권한 부여
RUN useradd -m -s /bin/bash jungle \
    && echo "jungle ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/jungle

EXPOSE 12345

# 이후부터는 jungle 사용자로
USER jungle
WORKDIR /home/jungle

CMD ["bash"]


