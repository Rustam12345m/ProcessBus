## IEC 61850 ProcessBus in Linux

This repository contains a proof-of-concept that combines five key ideas:

1. Using DPDK for IRQ-free Ethernet packet handling in Linux userspace.

2. Dedicating a core or multiple cores to handle huge ProcessBus/StationBus traffic.

3. Running Linux with a fully preemptible kernel (RT).

4. Splitting real-time-dependent traffic, such as IEC 61850 GOOSE & SV, from IP traffic for use in CPAC, vPAC, or Digital Fault Recorder systems.

5. Monitoring and measuring the state of ProcessBus/StationBuses, including signaling congestion.

## Applications

1. **bus_generator:** A frame generator for GOOSE & SV protocols.
    (support for R-GOOSE & R-SV are planned)

2. **bus_processor:** An example application for processing GOOSE & SV frames.

## Platforms

- QEMU scripts are provided for testing purposes.
- Embedded systems based on Intel Atom or ARM64 architectures.

## How to run

There are special scripts(qemu): `run_generator.sh` and `run_processor.sh`.

For example, generating packets:

1. `./run_generator.sh --sv80 500`  
   Generate 500 SV protocol according to 9.2LE 80 points.

2. `./run_generator.sh --sv256 500`  
   Generate 500 SV protocol according to 9.2LE 256 points.

3. `./run_generator.sh --goose 100,1000`  
   Generate 100 unique GOOSE messages with 1000 changes per second.

Processing packets:

1. `./run_processor.sh --sv80 100`  
   Expect 100 SV streams from a generator.

2. `./run_processor.sh --sv256 100`  
   Expect 100 SV streams from a generator.

3. `./run_processor.sh --goose 100`  
   Expect 100 GOOSE messages from a generator.

## Performance metrics  
Intel Atom 

1. Generating 1000 SV80 and Receiving Them Back via 10Gb/s SFP Module

   - Packet Rate: 4 million packets per second (PPS)
   - Data Rate: 3968 Mb/s

   Bus Generator (Core 1):

   | Metric | Min (µs) | Max (µs) | Load % | Wait % |
   |--------|----------|----------|--------|--------|
   | Main   | 94       | 217      | 47.016 | 52.984 |

   Bus Processor (Core 2):

   | Metric | Min (µs) | Max (µs) | Load %  | Wait %  |
   |--------|----------|----------|---------|---------|
   | Main   | 0        | 94       | 70.686  | 29.314  |

   Observations:  
   Stream Integrity: 1-2 samples were lost on some SV-streams after 5 minutes.

2. Generating 100 GOOSE Messages with 10,000 Changes Per Second

   - Packet Rate: 1 million packets per second (PPS)
   - Data Rate: 1560 Mb/s

   Bus Generator (Core 1):

   | Metric | Min (µs) | Max (µs) | Load % | Wait % |
   |--------|----------|----------|--------|--------|
   | Main   |12        | 85       | 15.169 | 84.831 |

   Bus Processor (Core 2):

   | Metric | Min (µs) | Max (µs) | Load %  | Wait %  |
   |--------|----------|----------|---------|---------|
   | Main   | 0        | 63       | 35.541  | 64.459  |

   Observations:  
   Stream Integrity: no messages were lost after 5 minutes.

3. Generating 200 GOOSE Messages with 10,000 Changes Per Second

   - Packet Rate: 2 million packets per second (PPS)
   - Data Rate: 3120 Mb/s

   Bus Generator (Core 1):

   | Metric | Min (µs) | Max (µs) | Load %  | Wait %  |
   |--------|----------|----------|---------|---------|
   | Main   | 27       | 115      | 35.892  | 64.108  |

   Bus Processor (Core 2):

   | Metric | Min (µs) | Max (µs) | Load %  | Wait %  |
   |--------|----------|----------|---------|---------|
   | Main   | 0        | 86       | 72.138  | 27.862  |

   Observations:  
   Stream Integrity: 1-2 GOOSE messages were lost after 5 minutes.

4. Generating 300 GOOSE Messages with 10,000 Changes Per Second

   - Packet Rate: 3 million packets per second (PPS)
   - Data Rate: 4680 Mb/s

   Bus Generator (Core 1):

   | Metric | Min (µs) | Max (µs) | Load %  | Wait %  |
   |--------|----------|----------|---------|---------|
   | Main   | 38       | 145      | 56.655  | 43.345  |

   Bus Processor (Core 2):

   | Metric | Min (µs) | Max (µs) | Load %  | Wait %  |
   |--------|----------|----------|---------|---------|
   | Main   | 0        | 74       | 53.423  | 46.577  |
   | LCore3 | 0        | 64       | 58.682  | 41.318  |
   | LCore4 | 0        | 97       | 72.205  | 27.795  |

   Observations:  
   Stream Integrity: no messages were lost after 5 minutes.

5. Generating 350 GOOSE Messages with 10,000 Changes Per Second

   - Packet Rate: 3.5 million packets per second (PPS)
   - Data Rate: 5460 Mb/s

   Bus Generator (Core 1):

   | Metric | Min (µs) | Max (µs) | Load %  | Wait %  |
   |--------|----------|----------|---------|---------|
   | Main   | 45       | 160      | 71.306  | 28.694  |

   Bus Processor (Core 2,3,4):

   | Metric | Min (µs) | Max (µs) | Load %  | Wait %  |
   |--------|----------|----------|---------|---------|
   | Main   | 0        | 82       | 38.934  | 61.066  |
   | LCore3 | 0        | 72       | 73.692  | 26.308  |
   | LCore4 | 0        | 109      | 89.894  | 10.106  |

   Observations:  
   Stream Integrity: no messages were lost after 5 minutes.

## License

GPL-3.0

