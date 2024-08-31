PROJECT_ROOT ?= $(shell realpath ../../..)
BUILD_DIR ?= $(PROJECT_ROOT)/build/platforms/a32
ASSETS_DIR := $(PROJECT_ROOT)/assets
PACKER_DIR := $(shell pwd)

ASSETS := $(ASSETS_DIR)/assets.bin $(ASSETS_DIR)/RG35XX22B.layout

SD_ROOT := $(BUILD_DIR)/SD
BINARY_DIR_SD := $(SD_ROOT)/roms/ports/dinojump
ASSET_DIR_SD := $(BINARY_DIR_SD)/assets
SHORTCUT_DIR_SD := $(SD_ROOT)/roms/ports
ZIP_PACKAGE := $(BUILD_DIR)/dino_jump_rg35xx_blite.zip

.PHONY: package

package: $(ZIP_PACKAGE)
	@echo Project root: $(PROJECT_ROOT)
	@echo Target path: $(BUILD_DIR)
	@echo ZIP package: $(ZIP_PACKAGE)

$(ZIP_PACKAGE): $(ASSETS) $(BUILD_DIR)/target/dino_jump $(PACKER_DIR)/Dino\ Jump.sh
	mkdir -p "$(BINARY_DIR_SD)"
	mkdir -p "$(ASSET_DIR_SD)"
	mkdir -p "$(SHORTCUT_DIR_SD)"
	@echo Assets: $(ASSETS)
	cp $(ASSETS) "$(ASSET_DIR_SD)/"
	cp "$(BUILD_DIR)/target/dino_jump" "$(BINARY_DIR_SD)/"
	cp "$(PACKER_DIR)/Dino Jump.sh" "$(SHORTCUT_DIR_SD)/"
	(cd "$(SD_ROOT)"; zip -r $(ZIP_PACKAGE) *)
