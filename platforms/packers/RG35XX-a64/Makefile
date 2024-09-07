PROJECT_ROOT ?= $(shell realpath ../../..)
BUILD_DIR ?= $(PROJECT_ROOT)/build/platforms/a64
PLATFORMS_DIR ?= $(shell realpath "`dirname "$(BUILD_DIR)"`")
ASSETS_DIR := $(PROJECT_ROOT)/assets
PACKER_DIR := $(shell pwd)

ASSETS := $(ASSETS_DIR)/assets.bin $(ASSETS_DIR)/RG35XX.layout
ZIP_PACKAGE := $(PLATFORMS_DIR)/dino_jump_rg35xx_muos.zip

.PHONY: package

package: $(ZIP_PACKAGE)
	@echo Project root: $(PROJECT_ROOT)
	@echo Target path: $(BUILD_DIR)

$(ZIP_PACKAGE): $(ASSETS)
	mkdir -p "$(BUILD_DIR)/ports/dinojump/assets"
	mkdir -p "$(BUILD_DIR)/ROMS/PORTS"
	@echo Assets: $(ASSETS)
	cp $(ASSETS) "$(BUILD_DIR)/ports/dinojump/assets/"
	cp "$(BUILD_DIR)/target/dino_jump" "$(BUILD_DIR)/ports/dinojump/"
	cp "$(PACKER_DIR)/Dino Jump.sh" "$(BUILD_DIR)/ROMS/PORTS/"
	(cd "$(BUILD_DIR)"; zip -r "$(ZIP_PACKAGE)" ROMS ports)
