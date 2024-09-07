PROJECT_ROOT ?= $(shell realpath ../../..)
BUILD_DIR ?= $(PROJECT_ROOT)/build/platforms/mm
PLATFORMS_DIR ?= $(shell realpath "`dirname "$(BUILD_DIR)"`")
ASSETS_DIR := $(PROJECT_ROOT)/assets
PACKER_DIR := $(shell pwd)

ASSETS := $(ASSETS_DIR)/assets.bin $(ASSETS_DIR)/MiyooMini.layout

ZIP_PACKAGE := $(PLATFORMS_DIR)/dino_jump_mm_onion.zip

.PHONY: package

package: $(ZIP_PACKAGE)
	@echo Project root: $(PROJECT_ROOT)
	@echo Target path: $(BUILD_DIR)

$(ZIP_PACKAGE): $(ASSETS)
	mkdir -p "$(BUILD_DIR)/Roms/PORTS/Games/DinoJump/assets"
	mkdir -p "$(BUILD_DIR)/Roms/PORTS/Shortcuts/Action"
	@echo Assets: $(ASSETS)
	cp $(ASSETS) "$(BUILD_DIR)/Roms/PORTS/Games/DinoJump/assets/"
	cp "$(BUILD_DIR)/target/dino_jump" "$(BUILD_DIR)/Roms/PORTS/Games/DinoJump/"
	cp "$(PACKER_DIR)/Dino Jump.notfound" "$(BUILD_DIR)/Roms/PORTS/Shortcuts/Action"
	(cd "$(BUILD_DIR)"; zip -r "$(ZIP_PACKAGE)" Roms)
