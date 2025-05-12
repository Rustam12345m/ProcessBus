#!/bin/sh

# 1024 * 2MB
echo 1024 > /proc/sys/vm/nr_hugepages

mkdir -p /mnt/bus_gen /mnt/bus_proc
mount -t hugetlbfs nodev /mnt/bus_gen
mount -t hugetlbfs nodev /mnt/bus_proc

modprobe uio
modprobe uio_pci_generic

cd /home/user/pbus/
./dpdk-devbind.py --bind=uio_pci_generic 0000:00:04.0 0000:00:05.0
./dpdk-devbind.py --status

# ./run_generator.sh --goose 1,1
# ./run_processor.sh --goose 1,1

