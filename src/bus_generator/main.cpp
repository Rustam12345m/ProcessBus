#include "gen_application.hpp"

#include <rte_eal.h>
#include <rte_ethdev.h>

#include <poll.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>

#include <format>
#include <thread>
#include <iostream>

volatile bool g_doWork = true;

static int setup_signalfd()
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

static int setup_timerfd(int sec = 1)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (fd < 0) {
        std::cerr << "Can't create Timer FD" << std::endl;
        abort();
    }

    itimerspec its = {};
    its.it_interval.tv_sec = sec;
    its.it_value.tv_sec = sec;
    timerfd_settime(fd, 0, &its, NULL);
    return fd;
}

static void* auxiliary_thread()
{
    set_thread_name("signal_thread");

    int signalFD = setup_signalfd();
    int timerFD = setup_timerfd();

    struct pollfd fds[2] = {
        { signalFD, POLLIN, 0 },
        { timerFD, POLLIN, 0 }
    };

    while (g_doWork) {
        int res = poll(fds, sizeof(fds)/sizeof(fds[0]), 250);
        if (res < 0) {
            continue;
        }

        if (fds[0].revents & POLLIN) {
            signalfd_siginfo si;
            read(signalFD, &si, sizeof(si));

            std::cout << std::format("Received signal {0}, exiting", si.ssi_signo)
                      << std::endl;
            g_doWork = false;
        }

        if (fds[1].revents & POLLIN) {
            uint64_t expirations = 0;
            read(timerFD, &expirations, sizeof(expirations));

            // TODO: Display statistics
        }
    }

    close(signalFD);
    close(timerFD);
    return NULL;
}

int main(int argc, char *argv[])
{
    // Block signals in all threads
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);

    // Init DPDK
    int retval = rte_eal_init(argc, argv);
    if (retval < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }
    // Skip DPDK's options
    argc -= retval;
    argv += retval;

    if (rte_eth_dev_count_avail() == 0) {
        rte_exit(EXIT_FAILURE, "No available ports. Check port binding.\n");
    }
    if (rte_get_main_lcore() == 0) {
        rte_exit(EXIT_FAILURE, "You can't use core 0 to generate/process BUSes!\n");
    }

    // Thread identity, CPU core by DPDK's command
    set_thread_name("main");

    // Start auxiliary thread: signals & statistics
    std::thread auxThread(auxiliary_thread);

    // Packet generator
    try {
        GenApplication app(argc, argv);

        app.run(g_doWork);
    } catch (const std::exception &exp) {
        std::cerr << "Exception: " << exp.what() << std::endl;
        g_doWork = false;
    }
    if (auxThread.joinable()) {
        auxThread.join();
    }

    rte_eal_cleanup();
    return 0;
}

