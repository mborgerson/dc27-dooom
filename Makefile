DEBUG = y
XBE_TITLE = dcdoom
GEN_XISO = $(XBE_TITLE).iso
SRCS += $(wildcard $(CURDIR)/*.c)
NXDK_DIR = $(CURDIR)/nxdk
NXDK_SDL = y
NXDK_NET=y


SRCS += $(CURDIR)/sdl_net/SDLnet.c
SRCS += $(CURDIR)/sdl_net/SDLnetselect.c
SRCS += $(CURDIR)/sdl_net/SDLnetTCP.c
SRCS += $(CURDIR)/sdl_net/SDLnetUDP.c

include $(NXDK_DIR)/Makefile

DOOM_LIBS=\
    $(CURDIR)/doom/build/src/doom/libdoom.a \
    $(CURDIR)/doom/build/pcsound/libpcsound.a \
    $(CURDIR)/doom/build/textscreen/libtextscreen.a \
    $(CURDIR)/doom/build/src/libnxdk-doom.a \

main.exe: $(DOOM_LIBS)

.PHONY: $(DOOM_LIBS)

#
# Build main doom code (as a library)
#
$(CURDIR)/doom/build/src/doom/libdoom.a:
	mkdir -p $(CURDIR)/doom/build && cd $(CURDIR)/doom/build && cmake .. -DXBOX=1 && cd $(CURDIR) && make -C $(CURDIR)/doom/build

#
# Generate a GDB script from the final .exe that can be used
# to debug in XQEMU (run XQEMU with -s to host gdbserver)
#
.gdbinit: main.exe
	echo "Building gdbinit"
	./build_gdbinit.sh

all: .gdbinit

#
# Add 
#
$(XBE_TITLE).iso: $(OUTPUT_DIR)/freedm.wad # $(OUTPUT_DIR)/doom.wad

# Freedoom deathmatch wad
# This takes a while so we don't do it every time
$(OUTPUT_DIR)/freedm.wad: $(CURDIR)/freedoom/wads/freedm.wad
	cp $^ $@
