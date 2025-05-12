#!/bin/sh

NIC_PCI_ADDR="0000:00:05.0"

bin/bus_processor -l 0          \
    -a $NIC_PCI_ADDR            \
    --huge-dir=/mnt/bus_proc/   \
    --file-prefix=buc_proc      \
    -- $@

