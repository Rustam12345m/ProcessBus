#!/bin/bash
# ./run_generator.sh --goose 10,100000
# ./run_processor.sh --goose 10
#

echo 4096 > /proc/sys/vm/nr_hugepages

mkdir -p /mnt/bus_gen /mnt/bus_proc
mount -t hugetlbfs nodev /mnt/bus_gen
mount -t hugetlbfs nodev /mnt/bus_proc

modprobe vfio
modprobe vfio-pci

cd /home/pulo/pbus/
./dpdk-devbind.py --bind=vfio-pci 0000:0c:00.0 0000:0b:00.0 0000:08:00.0
./dpdk-devbind.py --status

