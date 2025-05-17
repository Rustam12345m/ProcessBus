#!/bin/sh

NIC_PCI_ADDR="0B:00.0"

bin/bus_generator -l 1 \
    -a $NIC_PCI_ADDR \
    --huge-dir=/mnt/bus_gen/ \
    --file-prefix=bus_gen \
    -- $@

