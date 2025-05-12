#!/bin/sh

NIC_PCI_ADDR="0000:00:04.0"

bin/bus_generator -l 0          \
    -a $NIC_PCI_ADDR            \
    --huge-dir=/mnt/bus_gen/    \
    --file-prefix=bus_gen       \
    -- $@

