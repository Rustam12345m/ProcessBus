FROM alpine:3.21

RUN apk update && apk add --no-cache \
    ranger vim sudo bash \
    coreutils git gcc g++ make \
    musl-dev linux-headers \
    meson ninja cmake \
    pkgconfig libbsd libbsd-dev \
    pciutils python3 py3-pip py3-elftools \
    libatomic libpcap libpcap-dev \
    libxdp libxdp-dev libelf libbpf \
    elfutils-dev boost-dev \
    openssh sshpass

# Create a new user 'builder' and add to the sudo group
RUN adduser --disabled-password --gecos "" builder && \
    echo "builder:builder" | chpasswd && \
    echo "builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

#apk add numactl-dev libarchive-dev jansson-dev openssl-dev

WORKDIR /
CMD ["/bin/bash"]

