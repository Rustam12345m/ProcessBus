#!/bin/bash

echo "System settings for DPDK"

echo 4096 > /proc/sys/vm/nr_hugepages
mkdir -p /dev/hugepages
mount -t hugetlbfs nodev /dev/hugepages

modprobe vfio
modprobe vfio-pci

export LD_LIBRARY_PATH=/home/pulo/dpdk/libs

cd /home/pulo/dpdk/bin/
./dpdk-devbind.py --bind=vfio-pci 0000:0c:00.0
./dpdk-devbind.py --bind=vfio-pci 0000:0b:00.0
./dpdk-devbind.py --bind=vfio-pci 0000:08:00.0
./dpdk-devbind.py --status


mkdir -p /mnt/huge1 /mnt/huge2
mount -t hugetlbfs nodev /mnt/huge1
mount -t hugetlbfs nodev /mnt/huge2

#./dpdk-testpmd -l 0-1 -n4
#./bus_processor -l3 -a 0c:00.0 --huge-dir=/mnt/huge2/ --file-prefix=app2
#./bus_generator -l4 -a 0b:00.0 --huge-dir=/mnt/huge1/ --file-prefix=app1
