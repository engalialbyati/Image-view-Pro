# Build the macOS port of Image Viewer Pro.
# Requires macOS 11+ with Xcode Command Line Tools (clang++).
#
#   cd macos && make
#   open build/ImageViewerPro.app
#
SDK        ?= $(shell xcrun --sdk macosx --show-sdk-path)
CXX        ?= clang++
CXXFLAGS   ?= -std=c++17 -O2 -fobjc-arc -Wall -I../core $(CXXFLAGS_EXTRA)
LDFLAGS    ?= -framework AppKit -framework CoreGraphics -framework CoreFoundation \
              -framework ImageIO -framework Foundation
OUT_DIR    := build
APP        := $(OUT_DIR)/ImageViewerPro.app
BIN        := $(APP)/Contents/MacOS/ImageViewerPro

SRCS := main.mm IVPDocument.mm IVPImageView.mm IVPWindow.mm
OBJS := $(SRCS:%.mm=$(OUT_DIR)/%.o)

.PHONY: all clean run
all: $(APP)

$(APP): $(BIN) Info.plist
	@mkdir -p $(APP)/Contents/MacOS
	@mkdir -p $(APP)/Contents/Resources
	cp Info.plist $(APP)/Contents/
	/bin/echo -n 'APPL????' > $(APP)/Contents/PkgInfo

$(BIN): $(OBJS)
	@mkdir -p $(APP)/Contents/MacOS
	$(CXX) $(OBJS) -o $(BIN) $(LDFLAGS)

$(OUT_DIR)/%.o: %.mm
	@mkdir -p $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -isysroot $(SDK) -c $< -o $@

run: $(APP)
	open $(APP)

clean:
	rm -rf $(OUT_DIR)
