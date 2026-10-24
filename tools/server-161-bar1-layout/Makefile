# SPDX-License-Identifier: GPL-2.0-only
obj-m := bar1_layout.o
KDIR ?= /lib/modules/$(shell uname -r)/build
all:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" modules
