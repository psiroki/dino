PROJECT_ROOT ?= $(shell realpath ../../..)
BUILD_DIR ?= $(PROJECT_ROOT)/build/platforms/a64
ASSETS_DIR := $(PROJECT_ROOT)/assets
PACKER_DIR := $(shell pwd)

ASSETS := $(ASSETS_DIR)/assets.bin $(ASSETS_DIR)/RG35XX.layout

.PHONY: package

package: $(BUILD_DIR)/dino_jump_rg35xx_muos.zip
	@echo Project root: $(PROJECT_ROOT)
	@echo Target path: $(BUILD_DIR)

$(BUILD_DIR)/dino_jump_rg35xx_muos.zip: $(ASSETS)
	mkdir -p "$(BUILD_DIR)/ports/dinojump/assets"
	mkdir -p "$(BUILD_DIR)/ROMS/PORTS"
	@echo Assets: $(ASSETS)
	cp $(ASSETS) "$(BUILD_DIR)/ports/dinojump/assets/"
	cp "$(BUILD_DIR)/target/dino_jump" "$(BUILD_DIR)/ports/dinojump/"
	cp "$(PACKER_DIR)/Dino Jump.sh" "$(BUILD_DIR)/ROMS/PORTS/"
	(cd "$(BUILD_DIR)"; zip -r dino_jump_rg35xx_muos.zip ROMS ports)
