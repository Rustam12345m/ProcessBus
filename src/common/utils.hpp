#pragma once

#include <sched.h>
#include <pthread.h>

void set_thread_priority(int priority);

void pin_thread_to_cpu(int cpu, int priority);

