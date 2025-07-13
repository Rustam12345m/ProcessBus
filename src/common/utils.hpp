#pragma once

#include <stdint.h>
#include <sched.h>
#include <pthread.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>

#include <string>

void set_thread_name(const std::string &name);
void set_thread_priority(int priority);
void pin_thread_to_cpu(int cpu, int priority);

void display_packet_as_array(const uint8_t *packet, size_t packetSize);

int create_signalfd();
int create_timerfd(int periodSec = 1);

