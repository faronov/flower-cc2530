# SPDX-License-Identifier: BSD-3-Clause
# FlowerX firmware on the cc2530-zigbee stack (git submodule cc2530-zigbee).
.DEFAULT_GOAL := test
STACK := cc2530-zigbee
PYTHON ?= python3
LLVM_TOOLS ?= ../llvm-mcs51-build/bin
LLVM_SRC ?= ../llvm-project
FIRMWARE_OUT ?= build/firmware
HOST_DEFINES := -DCC2530_ZDO_DESCRIPTORS -DCC2530_APP_QUIET

.PHONY: test firmware clean

$(STACK)/Makefile:
	git submodule update --init $(STACK)

# Host tests (plain and ASan/UBSan) of the application against the stack.
test: | $(STACK)/Makefile
	$(MAKE) -C $(STACK) test-app-host APP_DIR=$(CURDIR) BUILD=$(CURDIR)/build/host \
		APP_HOST_TEST=tests/test_zcl_flower.c APP_HOST_DEFINES="$(HOST_DEFINES)"

# LLVM MCS-51 firmware (Intel HEX in $(FIRMWARE_OUT)/firmware.hex).
firmware: | $(STACK)/Makefile
	$(PYTHON) $(STACK)/tools/llvm/firmware.py --tools $(LLVM_TOOLS) --llvm-src $(LLVM_SRC) \
		--output $(FIRMWARE_OUT) --board external --image join --variant app --app $(CURDIR)

clean:
	rm -rf build
