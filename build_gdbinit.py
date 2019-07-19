#!/usr/bin/env python

sections = {}
with open('addrs.txt', 'rb') as f:
	vaddr = None
	name = None
		
	for l in f.readlines():
		if l.startswith('Virtual address'):
			vaddr = int(l.split(':')[1].strip(),16)
		if l.startswith('Section name'):
			name = l.split('"')[1]
			sections[name] = vaddr

print("""
# Tell GDB that we are using 32-bit x86 architecture
set arch i386

# Tell GDB to load symbols from main.exe, with the addresses of all
# sections (e.g. .text section at virtual address 0x60000).
#
# Notice here that we are using the .exe file, not the .xbe file, as
# GDB does not understand the .xbe format.
#
""")

print('add-symbol-file main.exe ' + hex(sections['.text']) + ' ' \
	+ ' '.join(['-s %s 0x%x' % (n,sections[n]) for n in sections if n != '.text' and not n.startswith('/')]))

print("""
# Use a layout which shows source code
layout src
set substitute-path /work/ /home/mborgerson/Projects/dcdoom/dcdoom/

# Connect to the XQEMU GDB server
target remote 127.0.0.1:1234

# Stop execution at the beginning of the `main` function
#b I_InitJoystick
# b check_eeprom
b D_DoomLoop

# Let XQEMU run until the CPU tries to execute from the address
# we have placed our breakpoint(s)
c
""")