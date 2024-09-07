PROJECT_ROOT ?= $(shell realpath ../../..)
BUILD_DIR ?= $(PROJECT_ROOT)/build/platforms/ma30
PLATFORMS_DIR ?= $(shell realpath "`dirname "$(BUILD_DIR)"`")
ASSETS_DIR := $(PROJECT_ROOT)/assets
PACKER_DIR := $(shell pwd)

ASSETS := $(ASSETS_DIR)/assets.bin $(ASSETS_DIR)/MiyooMini.layout

SD_ROOT := $(BUILD_DIR)/SD
BINARY_DIR_SD := $(SD_ROOT)/Emu/PORTS/dinojump
ASSET_DIR_SD := $(BINARY_DIR_SD)/assets
SHORTCUT_DIR_SD := $(SD_ROOT)/Roms/PORTS
ZIP_PACKAGE := $(PLATFORMS_DIR)/dino_jump_a30_spruce.zip

.PHONY: package

package: $(ZIP_PACKAGE)
	@echo Project root: $(PROJECT_ROOT)
	@echo Target path: $(BUILD_DIR)
	@echo ZIP package: $(ZIP_PACKAGE)

$(ZIP_PACKAGE): $(ASSETS) $(BUILD_DIR)/target/dino_jump $(PACKER_DIR)/Dino\ Jump.sh $(PACKER_DIR)/Dino\ Jump.png
	mkdir -p "$(BINARY_DIR_SD)"
	mkdir -p "$(ASSET_DIR_SD)"
	mkdir -p "$(SHORTCUT_DIR_SD)"
	mkdir -p "$(SHORTCUT_DIR_SD)/Imgs"
	@echo Assets: $(ASSETS)
	cp $(ASSETS) "$(ASSET_DIR_SD)/"
	cp "$(BUILD_DIR)/target/dino_jump" "$(BINARY_DIR_SD)/"
	cp "$(PACKER_DIR)/Dino Jump.sh" "$(SHORTCUT_DIR_SD)/"
	cp "$(PACKER_DIR)/Dino Jump.png" "$(SHORTCUT_DIR_SD)/Imgs/"
	(cd "$(SD_ROOT)"; zip -r $(ZIP_PACKAGE) *)
