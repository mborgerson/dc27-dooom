#!/bin/bash
# set -x
set -e

XQEMU=./stuff/xqemu/i386-softmmu/qemu-system-i386
MCPX=./stuff/mcpx.bin
BIOS=./stuff/bios.bin
HDD=./stuff/xbox_hdd.qcow2
BOOTROM=,bootrom=$MCPX
export QEMU_AUDIO_DRV=none
SHORTANIM=,short-animation
EEPROM=./xbox_eeprom.bin
DISC=./agent/agent.iso
DISC=dcdoom.iso
ACCEL="-accel kvm"

rm -f log.txt core

WINDOW_POSITIONS=(
0,0,0,640,480
0,640,0,640,480
0,1280,0,640,480
0,1920,0,640,480
0,0,480,640,480
0,640,480,640,480
0,1280,480,640,480
0,1920,480,640,480
0,0,960,640,480
0,640,960,640,480
0,1280,960,640,480
0,1920,960,640,480
0,0,1440,640,480
0,640,1440,640,480
0,1280,1440,640,480
0,1920,1440,640,480
)

function launch_instance {
	INSTANCE_NUM=$1

	HDD_LOCAL=xbox_hdd_$INSTANCE_NUM.qcow2
	cp $HDD $HDD_LOCAL

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
		-debugcon stdio
		"
		#-debugcon stdio -s"

	POS=${WINDOW_POSITIONS[$INSTANCE_NUM]}

	echo "Launching instance $INSTANCE_NUM at $POS"
	$XQEMU $RUNARGS > log_$INSTANCE_NUM.txt 2>&1 &
	local PID=$!
	echo "--> $PID"

	sleep 2
	local WID=""
	while [ "$WID" == "" ]; do
	        WID=$(wmctrl -lp | grep $PID | cut "-d " -f1)
	done
	# Set the size and location of the window
	# See man wmctrl for more info
	wmctrl -i -r $WID -e $POS
}

# launch_instance 0
# exit 0

for i in {0..14}
do
	launch_instance $i
done
