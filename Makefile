# STI2D-OS V2 — every build and test from one place (see BUILD.md).
#
#   make n0110        firmware app for the N0110            -> release/n0110/STI2D-OS.nwa
#   make n0115        firmware app for the N0115            -> release/n0115/STI2D-OS.nwa
#   make n0120        userland app for the N0120 (Epsilon)  -> release/n0120/STI2D-OS.nwa
#   make firmware     the three
#   make host-test    host build of the real launcher + V2, unit and key-driven tests, screenshots
#   make arm-check    every V2 module compiled for the three models with -Werror
#   make emu-test     the ARM builds run in the project's Cortex-M7 emulator (Unicorn)
#   make portal       web manifests and covers rebuilt from library/, JavaScript syntax check
#   make bundle-test  resources, manifests, links, portal and releases checked
#   make test         host-test + arm-check + stimg cross-check + bundle-test (+ emu-test if Unicorn is there)
#   make serve        the portal on http://localhost:8000/web/ (and the phone address)
#
# Tools: arm-none-eabi-gcc (Arm GNU Toolchain), Node.js (npx nwlink@1.0.0), Python 3 with Pillow
# (and unicorn + pyelftools for emu-test). PY selects the Python that has them:
#   python3 -m venv .venv && .venv/bin/pip install Pillow pyelftools unicorn
#   make test PY=.venv/bin/python

PY ?= python3
NWLINK ?= npx --yes -- nwlink@1.0.0
# The checked-in EADK headers are enough for host tests; device builds still use nwlink.
EADK_INC ?= -I$(abspath userland/epsilon-src/epsilon/eadk/include/eadk)
FW = firmware/sti2d-play-source-v1
V2 = firmware/v2
MODELS = n0110 n0115 n0120

.PHONY: firmware $(MODELS) host-test arm-check emu-test portal bundle-test stimg-check messaging-test test serve clean

firmware: $(MODELS)

$(MODELS):
	$(MAKE) -C $(FW) sti2dos-$@ PY="$(abspath $(PY))" NWLINK="$(NWLINK)"
	mkdir -p release/$@
	cp $(FW)/build/sti2dos/$@/STI2D-OS.nwa release/$@/STI2D-OS.nwa

host-test:
	$(MAKE) -C $(V2) test test-userland PY="$(PY)" NWLINK=true EADK_INC="$(EADK_INC)"

arm-check:
	$(MAKE) -C $(V2) arm-check NWLINK="$(NWLINK)"

emu-test: firmware
	$(PY) $(V2)/test/emu_scenarios.py $(FW) auto $(V2)/build/emu

stimg-check:
	mkdir -p $(V2)/build/stimg
	node tests/stimg_crosscheck/encode.js $(V2)/build/stimg
	cc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -DNP_SIMULATOR=1 \
	  $(EADK_INC) -DNP_SIMULATOR=1 -I$(V2)/include -I$(FW)/launcher/src \
	  tests/stimg_crosscheck/decode.c $(V2)/src/stimg.c -o $(V2)/build/stimg/decode
	$(V2)/build/stimg/decode $(V2)/build/stimg gradient16 gradient_dither flat2 noise16

portal:
	$(PY) scripts/rebuild_manifests.py
	for f in web/assets/js/*.js web/sw.js; do node --check $$f || exit 1; done
	@echo "portail : manifestes, couvertures et syntaxe JavaScript OK"

messaging-test:
	$(PY) -m unittest discover -s messaging/tests -v


bundle-test:
	$(PY) tests/validate_bundle.py
	$(PY) -m unittest discover -s tests -p 'test_*.py' -v

test: host-test arm-check stimg-check bundle-test
	@if $(PY) -c "import unicorn, elftools" 2>/dev/null; then $(MAKE) emu-test PY="$(PY)"; \
	else echo "emu-test ignoré : installe unicorn et pyelftools pour l'exécuter"; fi

serve:
	$(PY) scripts/serve.py 8000

clean:
	$(MAKE) -C $(V2) clean
	rm -rf $(FW)/build/sti2dos
