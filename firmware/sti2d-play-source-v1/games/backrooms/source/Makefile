# Compilation de Backrooms '96 pour NumWorks (.nwa)
CC = arm-none-eabi-gcc
BUILD_DIR = output
CFLAGS = -std=c99 -mthumb -mfloat-abi=hard -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -Iinclude -Isrc
CFLAGS += -O2 -Wall -ffast-math -fdata-sections -ffunction-sections -fno-common
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level -Wl,--gc-sections

$(BUILD_DIR)/backrooms96.nwa: src/main.c src/assets.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) src/main.c -lm -o $@

$(BUILD_DIR):
	mkdir -p $@

sim/game: src/main.c src/assets.h sim/eadk_sim.c
	gcc -O2 -Wall -Iinclude -Isrc src/main.c sim/eadk_sim.c -lm -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
