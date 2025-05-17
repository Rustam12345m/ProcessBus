#!/bin/sh

NIC_PCI_ADDR="0C:00.0"

bin/bus_processor -l 2,3,4 \
    -a $NIC_PCI_ADDR \
    --huge-dir=/mnt/bus_proc/ \
    --file-prefix=bus_proc \
    -- $@

