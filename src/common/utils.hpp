#pragma once

#include <sched.h>
#include <pthread.h>
#include <string>
#include <cstdint>

void set_thread_name(const std::string &name);

void set_thread_priority(int priority);

void pin_thread_to_cpu(int cpu, int priority);

void displayPacketAsArray(const uint8_t *packet, size_t packetSize);

