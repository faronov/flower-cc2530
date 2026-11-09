# SPDX-License-Identifier: BSD-3-Clause
# FlowerX firmware on the cc2530-zigbee stack (git submodule cc2530-zigbee).
.DEFAULT_GOAL := test
STACK := cc2530-zigbee
PYTHON ?= python3
LLVM_TOOLS ?= ../llvm-mcs51-build/bin
LLVM_SRC ?= ../llvm-project
FIRMWARE_OUT ?= build/firmware
HOST_DEFINES := -DCC2530_ZDO_DESCRIPTORS -DCC2530_APP_QUIET -DCC2530_TX_POWER -DCC2530_APP_SETTINGS

.PHONY: test test-gp firmware clean

$(STACK)/Makefile:
	git submodule update --init $(STACK)

# Host tests (plain and ASan/UBSan) of the application against the stack.
test: test-gp | $(STACK)/Makefile
	$(MAKE) -C $(STACK) test-app-host APP_DIR=$(CURDIR) BUILD=$(CURDIR)/build/host \
		APP_HOST_TEST=tests/test_zcl_flower.c APP_HOST_DEFINES="$(HOST_DEFINES)"

# Green Power frame builder (include/gpd_frame.h), ASan/UBSan.
test-gp:
	mkdir -p build/host
	$(CC) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
		-Iinclude tests/test_gpd_frame.c -o build/host/test-gpd-frame
	build/host/test-gpd-frame

# LLVM MCS-51 firmware (Intel HEX in $(FIRMWARE_OUT)/firmware.hex).
firmware: | $(STACK)/Makefile
	$(PYTHON) $(STACK)/tools/llvm/firmware.py --tools $(LLVM_TOOLS) --llvm-src $(LLVM_SRC) \
		--output $(FIRMWARE_OUT) --board external --image join --variant app --app $(CURDIR)

clean:
	rm -rf build
