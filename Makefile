# PocketMage app build recipe for PocketMuse.
#
# Point at the SDK checkout first:
#   export PM_SDK_ROOT=/path/to/PocketMage_SDK
# or pass it per-invocation:
#   make SDK_ROOT=/path/to/PocketMage_SDK

SDK_ROOT ?= $(PM_SDK_ROOT)
APP_NAME ?= $(notdir $(CURDIR))

# The pack target ships $(APP_NAME)_ICON.bin; regenerate it from the PNG
# asset whenever the name or the artwork changes. Plain `make` (the ELF only)
# needs no Pillow; `make pack` does.
$(APP_NAME)_ICON.bin: assets/icon.png tools/make_icon.py
	python3 tools/make_icon.py assets/icon.png $@

APP_SRCS := \
	src/main.cpp \
	src/muse.cpp \
	src/library.cpp \
	src/playlist.cpp \
	src/ui_player.cpp \
	src/ui/ui_browser.cpp \
	src/ui/ui_nowplaying.cpp \
	src/ui/ui_playlist.cpp \
	src/ui/ui_helpers.cpp

APP_CPPFLAGS += -Iinclude

include $(SDK_ROOT)/tools/app.mk

icon: $(APP_NAME)_ICON.bin
