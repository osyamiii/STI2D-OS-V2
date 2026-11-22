# STI2D-OS userland: the STI2D-OS shell (launcher + V2 layer, C) compiled into
# the userland, and its host (host.cpp). play/ and v2/ point to the project's
# firmware sources (firmware/sti2d-play-source-v1, firmware/v2): the same
# sources build the .nwa app and this userland (STI_USERLAND=1).

_sti2d_launcher := apps.c compat.c flash.c font_data.c games.c gfx.c home.c \
  icons.c inflate.c live.c main.c matrices.c platform.c settings.c sti2d.c \
  sti2d_assets.c storage.c sys.c ui.c xfont.c
_sti2d_v2 := calendar.c clock.c config.c control_center.c crypto.c gallery.c \
  gfx_adapt.c home_os.c input_pi.c maths.c pin.c pin_screen.c stimg.c terminal.c

_sti2d_c := $(addprefix $(PATH_apps)/sti2d/play/launcher/src/,$(_sti2d_launcher)) \
  $(addprefix $(PATH_apps)/sti2d/v2/src/,$(_sti2d_v2))

STI_TARGET_n0110 := 110
STI_TARGET_n0115 := 115
STI_TARGET_n0120 := 120
STI2D_VERSION ?= 2.2.0

ifeq ($(PLATFORM_TYPE),device)
SOURCES_apps += $(addsuffix :-test,$(PATH_apps)/sti2d/host.cpp $(_sti2d_c))

$(call all_objects_for,$(_sti2d_c)): CFLAGS += -std=gnu11 -DSTI_USERLAND=1 \
  -DSTI_TARGET=$(or $(STI_TARGET_$(PLATFORM)),0) -DNP_VERSION='"$(STI2D_VERSION)"' \
  -I$(PATH_apps)/sti2d/play/launcher/src -I$(PATH_apps)/sti2d/v2/include \
  -I$(PATH_apps)/../eadk/include/eadk -include sti_libc_compat.h \
  -Wno-unused-parameter -Wno-unused-function -Wno-sign-compare -Wno-missing-field-initializers
$(call all_objects_for,$(PATH_apps)/sti2d/host.cpp): CXXFLAGS += \
  -I$(PATH_apps)/sti2d -I$(PATH_apps)/../eadk/include/eadk
endif
