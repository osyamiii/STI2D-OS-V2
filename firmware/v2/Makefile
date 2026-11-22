# STI2D-OS V2 layer.
#
# The V2 modules are not a separate library any more: they are compiled into
# the STI2D-Play launcher by ../sti2d-play-source-v1/Makefile (LAUNCHER_SRC,
# targets sti2dos-n0110 / sti2dos-n0115 / sti2dos-n0120). This Makefile runs the
# host tests and a quick ARM compile check of the modules.
#
#   make test        host build of the real launcher + every V2 screen, run the
#                    tests, screenshots in build/host-shots (PPM, and PNG when
#                    Python's Pillow is there)
#   make arm-check   compile every V2 module with the calculator's flags
#   make clean

LAUNCHER = ../sti2d-play-source-v1/launcher/src
NWLINK ?= npx --yes -- nwlink@1.0.0
EADK_INC ?= $(shell $(NWLINK) eadk-cflags-simulator 2>/dev/null | sed -e 's/-fPIC//')
EADK_DEV := $(shell $(NWLINK) eadk-cflags-device 2>/dev/null)
PY ?= python3
B = build

V2_SRC = $(wildcard src/*.c)
HOST_LAUNCHER = $(LAUNCHER)/main.c $(LAUNCHER)/sti2d.c $(LAUNCHER)/sti2d_assets.c $(LAUNCHER)/gfx.c \
  $(LAUNCHER)/ui.c $(LAUNCHER)/inflate.c $(LAUNCHER)/live.c $(LAUNCHER)/icons.c $(LAUNCHER)/font_data.c \
  $(LAUNCHER)/platform.c $(LAUNCHER)/sys.c $(LAUNCHER)/apps.c $(LAUNCHER)/xfont.c
HOST_CFLAGS = -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-deprecated-declarations -Wno-comment \
  -DNP_SIMULATOR=1 -DNP_VERSION='"host"' -DSTI_TARGET=0 $(EADK_INC) -Iinclude -I$(LAUNCHER) -Itest/host \
  -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all test test-userland arm-check clean
all: test

$(B)/test_v2: $(V2_SRC) $(HOST_LAUNCHER) test/host/mock.c test/host/test_v2.c test/host/mock.h include/*.h
	@mkdir -p $(B)
	cc $(HOST_CFLAGS) -c $(LAUNCHER)/main.c -Dmain=launcher_main -o $(B)/host_main.o
	cc $(HOST_CFLAGS) $(filter-out $(LAUNCHER)/main.c,$(HOST_LAUNCHER)) $(V2_SRC) test/host/mock.c test/host/test_v2.c \
	  $(B)/host_main.o -lm -o $@

test: $(B)/test_v2
	@mkdir -p $(B)/host-shots
	./$(B)/test_v2 $(B)/host-shots
	@$(PY) test/host/ppm2png.py $(B)/host-shots 2>/dev/null || echo "(Pillow absent: screenshots stay in PPM)"

# the shell as the STI2D-OS userland runs it (STI_USERLAND=1, see userland/)
UL_LAUNCHER = $(filter-out $(LAUNCHER)/main.c,$(HOST_LAUNCHER)) $(LAUNCHER)/main.c
$(B)/test_userland: $(V2_SRC) $(HOST_LAUNCHER) test/host/mock.c test/host/test_userland.c test/host/mock.h include/*.h
	@mkdir -p $(B)
	cc $(HOST_CFLAGS) -DSTI_USERLAND=1 -include include/sti_libc_compat.h $(UL_LAUNCHER) $(V2_SRC) \
	  test/host/mock.c test/host/test_userland.c -lm -o $@

test-userland: $(B)/test_userland
	@mkdir -p $(B)/userland-shots
	./$(B)/test_userland $(B)/userland-shots
	@$(PY) test/host/ppm2png.py $(B)/userland-shots 2>/dev/null || echo "(Pillow absent: screenshots stay in PPM)"

# every module, as the calculator build compiles it (-Werror: no warning allowed)
ARM_FLAGS = -std=gnu11 $(EADK_DEV) -Os -Wall -Wextra -Werror -Wno-unused-parameter -ffunction-sections \
  -fdata-sections -fno-math-errno -Iinclude -I$(LAUNCHER)
arm-check:
	@mkdir -p $(B)/arm
	@for f in $(V2_SRC); do for t in 110 115 120; do \
	  arm-none-eabi-gcc $(ARM_FLAGS) -DSTI_TARGET=$$t -c $$f -o $(B)/arm/$$(basename $$f .c)-$$t.o || exit 1; done; done
	@arm-none-eabi-size -t $(B)/arm/*-120.o | tail -1 | awk '{print "V2 modules (N0120 build): text " $$1 " B, data " $$2 " B, bss " $$3 " B"}'

clean:
	rm -rf $(B)
