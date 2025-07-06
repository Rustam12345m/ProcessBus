#/bin/sh

# Scheduler
echo -1 > /proc/sys/kernel/sched_rt_runtime_us
echo  4 > /proc/sys/kernel/sched_rr_timeslice_ms

# Workqueues to CPU0
find /sys/devices/virtual/workqueue -name cpumask  -exec sh -c 'echo 1 > {}' ';'
for q in /sys/devices/virtual/workqueue/*/cpumask; do
    echo 1 > "$q";
done

# Move all IRQ to CPU0
for i in /proc/irq/*/smp_affinity; do
    echo 1 > $i > /dev/null 2>&1
done
echo 1 > /proc/irq/default_smp_affinity

# RT
for C in 1 2 3 4 5 6 7 8; do
    echo performance > /sys/devices/system/cpu/cpu$C/cpufreq/scaling_governor
    echo 1 > /sys/devices/system/cpu/cpu$C/cpuidle/state0/disable
done

# Prevent lockup detections
echo 0 > /proc/sys/kernel/watchdog
echo 0 > /proc/sys/kernel/nmi_watchdog
echo 0 > /proc/sys/kernel/softlockup_panic
echo 0 > /proc/sys/kernel/softlockup_all_cpu_backtrace
#echo 0 > /proc/sys/kernel/hung_task_timeout_secs
#echo 0 > /proc/sys/kernel/hung_task_warnings

