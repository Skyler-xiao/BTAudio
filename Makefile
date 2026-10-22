#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

export BLOOPAIR_TOP_DIR := $(CURDIR)

# release builds unless asked otherwise (DEBUG=1 adds the commit hash and debug output)
ifeq ($(DEBUG),)
	export DEBUG := 0
endif

ifeq ($(DEBUG), 1)
	export BLOOPAIR_COMMIT_HASH := $(or $(shell git rev-parse HEAD),"ffffffffffffffffffffffffffffffffffffffff")
endif

VERSION ?= 1.0.0

.PHONY: all clean dist ios_kernel ios_usb ios_pad libbloopair loader koopair plugin

all: loader koopair plugin
	@echo -e "\033[92mDone!\033[0m"

# dist/sd is laid out like the SD card, dist/BTAudio_v$(VERSION).zip contains the same
dist: all
	rm -rf dist
	mkdir -p dist/sd/wiiu/environments/aroma/modules/setup
	mkdir -p dist/sd/wiiu/environments/aroma/plugins
	mkdir -p dist/sd/wiiu/apps/Koopair
	cp loader/30_bloopair.rpx dist/sd/wiiu/environments/aroma/modules/setup/
	mkdir -p dist/sd/wiiu/btaudio/module
	cp loader/30_bloopair.rpx dist/sd/wiiu/btaudio/module/
	cp plugin/BTAudio.wps dist/sd/wiiu/environments/aroma/plugins/
	cp koopair/Koopair.wuhb dist/sd/wiiu/apps/Koopair/
	cd dist/sd && zip -qr ../BTAudio_v$(VERSION).zip wiiu

ios_kernel: ios_usb ios_pad
	@echo -e "\033[92mBuilding $@...\033[0m"
	@$(MAKE) --no-print-directory -C $(CURDIR)/ios/ios_kernel

ios_usb:
	@echo -e "\033[92mBuilding $@...\033[0m"
	@$(MAKE) --no-print-directory -C $(CURDIR)/ios/ios_usb

ios_pad:
	@echo -e "\033[92mBuilding $@...\033[0m"
	@$(MAKE) --no-print-directory -C $(CURDIR)/ios/ios_pad

libbloopair:
	@echo -e "\033[92mBuilding $@...\033[0m"
	@$(MAKE) --no-print-directory -C $(CURDIR)/libbloopair

loader: ios_kernel libbloopair
	@echo -e "\033[92mBuilding $@...\033[0m"
	@$(MAKE) --no-print-directory -C $(CURDIR)/loader

koopair: libbloopair
	@echo -e "\033[92mBuilding $@...\033[0m"
	@$(MAKE) --no-print-directory -C $(CURDIR)/koopair

plugin: libbloopair
	@echo -e "\033[92mBuilding $@...\033[0m"
	@$(MAKE) --no-print-directory -C $(CURDIR)/plugin

clean:
	@$(MAKE) --no-print-directory -C $(CURDIR)/ios/ios_kernel clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/ios/ios_usb clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/ios/ios_pad clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/loader clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/libbloopair clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/koopair clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/plugin clean
	rm -rf dist
