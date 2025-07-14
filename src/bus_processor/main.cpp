#include "rx_application.hpp"

#include <rte_eal.h>
#include <rte_ethdev.h>

#include <poll.h>
#include <stdlib.h>

#include <format>
#include <atomic>
#include <thread>
#include <iostream>

volatile bool g_doWork = true;

static void* auxiliary_thread(RX_Application::ptr app)
{
    set_thread_name("auxiliary_thread");
    pin_thread_to_cpu(0, 1);

    const unsigned TIMER_PERIOD_SEC = 3;
    int signalFD = create_signalfd();
    int timerFD = create_timerfd(TIMER_PERIOD_SEC);

    struct pollfd fds[2] = {
        { signalFD, POLLIN, 0 },
        { timerFD, POLLIN, 0 }
    };

    while (g_doWork) {
        int res = poll(fds, sizeof(fds)/sizeof(fds[0]), 250);
        if (res < 0) {
            continue;
        }

        // Signals
        if (fds[0].revents & POLLIN) {
            signalfd_siginfo si;
            read(signalFD, &si, sizeof(si));

            std::cout << std::format("Received signal {0}, exiting", si.ssi_signo)
                      << std::endl;
            g_doWork = false;
        }

        // Timer
        if (fds[1].revents & POLLIN) {
            uint64_t expirations = 0;
            read(timerFD, &expirations, sizeof(expirations));

            app->DisplayStatistic(TIMER_PERIOD_SEC);
        }
    }

    close(signalFD);
    close(timerFD);
    return NULL;
}

int main(int argc, char *argv[])
{
    // block signals in all threads
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);

    int retval = rte_eal_init(argc, argv);
    if (retval < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }
    // Skip DPDK's options
    argc -= retval;
    argv += retval;

    if (rte_eth_dev_count_avail() == 0) {
        rte_exit(EXIT_FAILURE, "No available ports. Please, check port binding.\n");
    }
    if (rte_get_main_lcore() == 0) {
        rte_exit(EXIT_FAILURE, "You can't use core 0 to generate/process BUSes!\n");
    }

    // Thread identification
    set_thread_name("main");

    // Start auxiliary thread: signals & statistics
    std::thread auxThread;

    // Packet receiver
    RX_Application::ptr app;

    try {
        app = std::make_shared< RX_Application >(argc, argv);

        auxThread = std::thread(auxiliary_thread, app);

        app->Run(g_doWork);
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

