#!/bin/sh

NIC_PCI_ADDR="0C:00.0"

bin/bus_processor -l 0 -a $NIC_PCI_ADDR \
    --huge-dir=/mnt/huge2/ \
    --file-prefix=buc_proc \
    -- $@

