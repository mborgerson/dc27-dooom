DEBUG=y
XBE_TITLE=agent
GEN_XISO = $(XBE_TITLE).iso
SRCS += $(wildcard $(CURDIR)/*.c)
SRCS += $(CURDIR)/xxHash/xxhash.c
NXDK_DIR = $(CURDIR)/../nxdk
NXDK_NET=y
LDFLAGS = /OPT:REF
include $(NXDK_DIR)/Makefile
