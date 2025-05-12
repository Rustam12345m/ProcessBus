#!/bin/sh

NIC_PCI_ADDR="0B:00.0"

bin/bus_generator -l 0 \
    -a $NIC_PCI_ADDR \
    --huge-dir=/mnt/huge1/ \
    --file-prefix=bus_gen \
    -- $@

