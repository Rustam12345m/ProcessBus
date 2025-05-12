#pragma once

#include <sched.h>
#include <pthread.h>

void pin_thread_to_cpu(int cpu, int priority);

