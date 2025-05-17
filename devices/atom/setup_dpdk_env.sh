#!/bin/bash

echo 4096 > /proc/sys/vm/nr_hugepages

mkdir -p /mnt/bus_gen /mnt/bus_proc
mount -t hugetlbfs nodev /mnt/bus_gen
mount -t hugetlbfs nodev /mnt/bus_proc

modprobe vfio
modprobe vfio-pci

cd /home/pulo/pbus/
./dpdk-devbind.py --bind=vfio-pci 0000:0c:00.0 0000:0b:00.0 0000:08:00.0
./dpdk-devbind.py --status

# ./run_generator.sh --goose 10,100000
# ./run_processor.sh --goose 10

echo -e "\tTune Linux scheduler"
echo -1 > /proc/sys/kernel/sched_rt_runtime_us
echo 2 > /proc/sys/kernel/sched_rr_timeslice_ms

# Workqueue's threads to CPU0
find /sys/devices/virtual/workqueue -name cpumask  -exec sh -c 'echo 1 > {}' ';'

# All kernel's threads move to CPU0
ps -A | grep -F "[" | awk '{ print $1 }' | xargs -i sh -c '{ taskset -pc 0 {} >/dev/null 2>&1; }'

# ABB: Limit interrput handling to cpu0
echo 1 > /proc/irq/default_smp_affinity > /dev/null 2>&1

# ABB: Limit kworkers to cpu0
for q in /sys/devices/virtual/workqueue/*/cpumask; do
    echo 1 > "$q";
done

for C in {1..8}; do
    echo performance > /sys/devices/system/cpu/cpu$C/cpufreq/scaling_governor
done

