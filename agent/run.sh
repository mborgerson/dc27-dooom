#!/bin/bash
# set -x
set -e

MCPX=../stuff/mcpx.bin
BIOS=../stuff/bios.bin
HDD=../stuff/xbox_hdd.qcow2
BOOTROM=,bootrom=$MCPX
export QEMU_AUDIO_DRV=none
SHORTANIM=,short-animation
#MACADDR=$(../eeprom_tool/xbeeprom $EEPROM | grep MAC | tr - : | cut -d ' ' -f 3)
EEPROM=./xbox_eeprom.bin
DISC=agent.iso

# DISC=/home/mborgerson/Projects/xbox/junk/frosty.iso

#NET="-net nic,model=nvnet,netdev=network0,macaddr=$MACADDR -netdev tap,id=network0,ifname=tap3,script=no,downscript=no"
#NET="-net nic,model=nvnet,netdev=network0 -netdev tap,id=network0,ifname=tap0,script=no,downscript=no"
#NET="-net nic,model=nvnet -net user,hostfwd=udp:127.0.0.1:5555-192.168.1.2:5555"
# NET="-net nic,model=nvnet -net user,hostfwd=tcp:127.0.0.1:2121-:21"
NET="-net nic,model=nvnet -net user,hostfwd=tcp:127.0.0.1:5555-:6660,net=10.13.37.0/8,dhcpstart=10.2.105.10"
# NET="-net nic,macaddr=00:50:f2:4f:65:52,model=nvnet -net socket,listen=:1236"
# NET="-net nic,macaddr=00:50:f2:4f:65:56,model=nvnet -net socket,connect=127.0.0.1:1236"
# NET="-net nic,model=nvnet -net socket,connect=47.14.106.97:12345"

ACCEL="-accel tcg"
# ACCEL="-accel kvm"

rm -f log.txt core

HDD_LOCAL=xbox_hdd.qcow2
# cp $HDD $HDD_LOCAL

RUNARGS=" \
	-cpu pentium3 \
	-machine xbox$BOOTROM$SHORTANIM,kernel_irqchip=off -m 64 \
	-bios $BIOS \
	$NET \
	-drive index=0,media=disk,file=$HDD_LOCAL,locked \
	-drive index=1,media=cdrom,file=$DISC \
	-device usb-xbox-gamepad,port=3 \
	$ACCEL \
	-display sdl \
	-debugcon stdio -s
	"

/home/mborgerson/Projects/xbox/xqemu_master/i386-softmmu/qemu-system-i386 $RUNARGS
