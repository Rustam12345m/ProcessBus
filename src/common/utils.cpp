#include "utils.hpp"

#include <iostream>

void set_thread_name(const std::string &name)
{
    pthread_setname_np(pthread_self(), name.c_str());
}

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

void display_packet_as_array(const uint8_t *packet, size_t packetSize)
{
    printf("const uint8_t packet[%zu] = {", packetSize);
    for (size_t i = 0; i < packetSize; ++i) {
        if (i % 16 == 0) {
            printf("\n    ");
        }
        printf("0x%02X", packet[i]);
        if (i < packetSize - 1) {
            printf(", ");
        }
    }
    printf("\n};\n");
}

int create_signalfd()
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int fd = signalfd(-1, &mask, SFD_NONBLOCK);
    if (fd < 0) {
        std::cerr << "Can't create Signal FD" << std::endl;
        abort();
    }
    return fd;
}

int create_timerfd(int periodSec)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (fd < 0) {
        std::cerr << "Can't create Timer FD" << std::endl;
        abort();
    }

    itimerspec its = {};
    its.it_interval.tv_sec = periodSec;
    its.it_value.tv_sec = periodSec;
    timerfd_settime(fd, 0, &its, NULL);
    return fd;
}

