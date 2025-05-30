#include "utils.hpp"

void set_thread_priority(int priority)
{
    sched_param param = {
        .sched_priority = priority
    };
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
}

void pin_thread_to_cpu(int cpu, int priority)
{
    pthread_t thread = pthread_self();

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    sched_param param = {
        .sched_priority = priority
    };
    pthread_setschedparam(thread, SCHED_FIFO, &param);
}

