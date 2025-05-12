.PHONY: getfiles hard install run interfaces

getfiles:
	sudo apt install qemu-system-x86 bridge-utils
	wget -c https://dl-cdn.alpinelinux.org/alpine/v3.21/releases/x86_64/alpine-standard-3.21.3-x86_64.iso

hard:
	qemu-img create -f qcow2 alpine-disk.qcow2 4G

interfaces:
	@{ for i in 0 1 2 3; do \
		NAME=vm_tap$$i; \
		sudo ip tuntap add dev $$NAME mode tap ; \
		sudo ip link set $$NAME up; \
	done; \
	}
	ip addr add 192.168.21.1/24 dev vm_tap0 

install:
	qemu-system-x86_64 \
		-m 4096 \
		-smp 4 \
		-cdrom alpine-standard-3.21.3-x86_64.iso \
		-hda alpine-dpdk-harddrive.qcow2 \
		-boot d \
		-netdev user,id=net0 -device virtio-net-pci,netdev=net0

# KERN_DRIVER="virtio-net-pci"
KERN_DRIVER="igb"
run:
	qemu-system-x86_64 \
		-enable-kvm \
		-cpu Nehalem,+sse3 \
		-machine q35 \
		-smp 4 \
		-m 4096 \
		-hda alpine-disk.qcow2 \
		-netdev tap,id=net0,ifname=vm_tap0,script=no,downscript=no -device ${KERN_DRIVER},netdev=net0,mac=10:01:00:00:00:01 \
		-netdev tap,id=net1,ifname=vm_tap1,script=no,downscript=no -device ${KERN_DRIVER},netdev=net1,mac=20:02:00:00:00:02 \
		-netdev tap,id=net2,ifname=vm_tap2,script=no,downscript=no -device ${KERN_DRIVER},netdev=net2,mac=30:03:00:00:00:03 \
		-netdev tap,id=net3,ifname=vm_tap3,script=no,downscript=no -device ${KERN_DRIVER},netdev=net3,mac=40:04:00:00:00:04

