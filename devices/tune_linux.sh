#/bin/bash
# Kernel options:
# intel_iommu=on iommu=pt isolcpus=1-12 rcu_nocbs=1-12 nohz_full=1-12
# housekeeping=0 rcutree.kthread_prio=95 irqaffinity=0 irqpoll idle=poll nosoftlockup
# processor.max_cstate=0 intel_idle.max_cstate=0 intel_pstate=disable mce=ignore_ce
# enforcing=0 skew_tick=1 transparent_hugepage=never tsc=reliable

# Scheduler
echo -1 > /proc/sys/kernel/sched_rt_runtime_us
echo  4 > /proc/sys/kernel/sched_rr_timeslice_ms

# Workqueues to CPU0
for q in /sys/devices/virtual/workqueue/*/cpumask; do
    echo 1 > "$q";
done

# Move all IRQ to CPU0
for i in /proc/irq/*/smp_affinity; do
    echo 1 > "$i" 2>/dev/null
done
# Set default affinity to CPU0
echo 1 > /proc/irq/default_smp_affinity 2>/dev/null

# Disable non-RT features in CPU
for C in 1 2 3 4 5 6 7 8; do
    if [ -e /sys/devices/system/cpu/cpu$C/cpufreq/scaling_governor ]; then
        echo performance > /sys/devices/system/cpu/cpu$C/cpufreq/scaling_governor
    fi
    if [ -e /sys/devices/system/cpu/cpu$C/cpuidle/state0/disable ]; then
        echo 1 > /sys/devices/system/cpu/cpu$C/cpuidle/state0/disable
    fi
done

# Prevent lockup detections
echo 0 > /proc/sys/kernel/watchdog
echo 0 > /proc/sys/kernel/nmi_watchdog
echo 0 > /proc/sys/kernel/softlockup_panic
echo 0 > /proc/sys/kernel/softlockup_all_cpu_backtrace
#echo 0 > /proc/sys/kernel/hung_task_timeout_secs
#echo 0 > /proc/sys/kernel/hung_task_warnings

# Define the list of kernel thread names and the desired priority and policy
declare -A kernel_threads=(
    ["ktimers"]="99 fifo"
    ["irq_work"]="98 fifo"
    ["rcuog"]="95 rr"
    ["rcuop"]="95 rr"
    ["rcuc"]="95 rr"
)
for thread_name in "${!kernel_threads[@]}"; do
    priority_policy=(${kernel_threads[$thread_name]})
    priority=${priority_policy[0]}
    policy=${priority_policy[1]}

    # Find the PIDs of the kernel threads
    pids=$(pgrep -f "$thread_name")

    # Set the priority and policy for each PID
    for pid in $pids; do
        chrt --$policy -p $priority $pid

        echo "Set priority for: $thread_name($pid) to $priority($policy)"
    done
done

