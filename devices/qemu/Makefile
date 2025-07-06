DISRT_ISO_FILE=debian-testing-amd64-netinst.iso
LINUX_DISTR_ISO=https://cdimage.debian.org/cdimage/weekly-builds/amd64/iso-cd/debian-testing-amd64-netinst.iso
QEMU_HARDDRIVE=qemu-debian-harddrive.qcow2

.PHONY: fetch hard interfaces install run

all: fetch hard interfaces install run

fetch:
	sudo apt install qemu-system-x86 bridge-utils
	wget -c ${LINUX_DISTR_ISO}

hard:
	qemu-img create -f qcow2 ${QEMU_HARDDRIVE} 4G

interfaces:
	@{ for i in 0 1 2 3; do \
		NAME=vm_tap$$i; \
		sudo ip tuntap add dev $$NAME mode tap ; \
		sudo ip link set $$NAME up; \
	done; \
	}
	sudo ip addr add 192.168.21.1/24 dev vm_tap0
	sudo ip link set dev vm_tap0 up

install: ${DISRT_ISO_FILE} ${QEMU_HARDDRIVE}
	sudo qemu-system-x86_64 \
		-enable-kvm \
		-smp 4 \
		-m 4096 \
		-cpu Nehalem,+sse3 \
		-machine q35 \
		-cdrom ${DISRT_ISO_FILE} \
		-hda ${QEMU_HARDDRIVE} \
		-boot d \
		-device intel-iommu,intremap=off,caching-mode=on \
		-netdev tap,id=net0,ifname=vm_tap0,script=no,downscript=no \
		-device virtio-net-pci,netdev=net0,mac=10:01:00:00:00:01

# KERN_DRIVER="igb"
# KERN_DRIVER="virtio-net-pci"
KERN_DRIVER="virtio-net-pci,disable-legacy=on,iommu_platform=on"
run: ${QEMU_HARDDRIVE}
	sudo qemu-system-x86_64 \
		-enable-kvm \
		-smp 4 \
		-m 4096 \
		-cpu Nehalem,+sse3 \
		-machine q35 \
		-hda ${QEMU_HARDDRIVE} \
		-device intel-iommu,intremap=off,caching-mode=on \
		-netdev tap,id=net0,ifname=vm_tap0,script=no,downscript=no \
		-device ${KERN_DRIVER},netdev=net0,mac=10:01:00:00:00:01 \
		-netdev tap,id=net1,ifname=vm_tap1,script=no,downscript=no \
		-device ${KERN_DRIVER},netdev=net1,mac=20:02:00:00:00:02 \
		-netdev tap,id=net2,ifname=vm_tap2,script=no,downscript=no \
		-device ${KERN_DRIVER},netdev=net2,mac=30:03:00:00:00:03 \
		-netdev tap,id=net3,ifname=vm_tap3,script=no,downscript=no \
		-device ${KERN_DRIVER},netdev=net3,mac=40:04:00:00:00:04

