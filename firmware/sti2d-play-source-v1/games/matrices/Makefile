# Matrices (IA hors ligne de la STI2D) : seule (output/matrices.nwa) ou dans STI2D-Play (module).
Q ?= @
CC = arm-none-eabi-gcc
NWLINK ?= npx --yes -- nwlink@1.0.0
BUILD_DIR = output
SRC = src/main.c
HEADERS = src/kb.h src/mini.h src/qr_data.h

CFLAGS = -std=c11 $(shell $(NWLINK) eadk-cflags-device)
CFLAGS += -Os -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -fno-math-errno
CFLAGS += -fno-tree-loop-distribute-patterns -flto -fno-fat-lto-objects -fwhole-program -fvisibility=internal
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level
LDFLAGS += -Wl,--gc-sections -flinker-output=nolto-rel

.PHONY: build module clean
build: $(BUILD_DIR)/matrices.nwa

$(BUILD_DIR)/main.o: $(SRC) $(HEADERS) | $(BUILD_DIR)
	$(Q) $(CC) $(CFLAGS) -c $(SRC) -o $@
$(BUILD_DIR)/icon.o: src/icon.png | $(BUILD_DIR)
	$(Q) $(NWLINK) png-icon-o $< $@
$(BUILD_DIR)/matrices.nwa: $(BUILD_DIR)/main.o $(BUILD_DIR)/icon.o
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
	$(Q) arm-none-eabi-strip --strip-unneeded $@

module: $(BUILD_DIR)/module.o
$(BUILD_DIR)/module-main.o: $(SRC) $(HEADERS) | $(BUILD_DIR)
	$(Q) $(CC) $(CFLAGS) -DNP_MODULE=1 -c $(SRC) -o $@
$(BUILD_DIR)/module.o: $(BUILD_DIR)/module-main.o
	$(Q) $(CC) $(CFLAGS) -Wl,--relocatable -nostartfiles -nostdlib -Wl,-e,main -flinker-output=nolto-rel $< -o $@

$(BUILD_DIR):
	$(Q) mkdir -p $@

clean:
	$(Q) rm -rf $(BUILD_DIR)
