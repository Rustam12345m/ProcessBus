## ProcessBus

This repository contains a proof-of-concept that combines five key ideas:

1. Using DPDK for IRQ-free Ethernet packet handling at near wire-speed.

2. Dedicating a core or multiple cores to handle ProcessBus/StationBus traffic.

3. Running Linux with a fully preemptible kernel (RT).

4. Splitting real-time-dependent traffic, such as IEC 61850 GOOSE & SV, from IP traffic for use in CPAC or Digital Fault Recorder systems.

5. Monitoring and controlling the state of ProcessBus/StationBuses, including signaling congestion.

## Applications

1. **bus_generator:** A frame generator for GOOSE & SV protocols. Support for R-GOOSE & R-SV is planned.

2. **bus_processor:** An example application for processing GOOSE & SV frames.

## Platforms

- QEMU scripts are provided for testing purposes.
- Embedded systems based on Intel Atom or ARM64 architectures.

## License

GPL-3.0

