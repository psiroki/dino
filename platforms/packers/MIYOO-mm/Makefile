PROJECT_ROOT ?= $(shell realpath ../../..)
BUILD_DIR ?= $(PROJECT_ROOT)/build/platforms/mm
ASSETS_DIR := $(PROJECT_ROOT)/assets
PACKER_DIR := $(shell pwd)

ASSETS := $(ASSETS_DIR)/assets.bin $(ASSETS_DIR)/MiyooMini.layout

.PHONY: package

package: $(BUILD_DIR)/dino_jump_mm_onion.zip
	@echo Project root: $(PROJECT_ROOT)
	@echo Target path: $(BUILD_DIR)

$(BUILD_DIR)/dino_jump_mm_onion.zip: $(ASSETS)
	mkdir -p "$(BUILD_DIR)/Roms/PORTS/Games/DinoJump/assets"
	mkdir -p "$(BUILD_DIR)/Roms/PORTS/Shortcuts/Action"
	@echo Assets: $(ASSETS)
	cp $(ASSETS) "$(BUILD_DIR)/Roms/PORTS/Games/DinoJump/assets/"
	cp "$(BUILD_DIR)/target/dino_jump" "$(BUILD_DIR)/Roms/PORTS/Games/DinoJump/"
	cp "$(PACKER_DIR)/Dino Jump.notfound" "$(BUILD_DIR)/Roms/PORTS/Shortcuts/Action"
	(cd "$(BUILD_DIR)"; zip -r dino_jump_mm_onion.zip Roms)
