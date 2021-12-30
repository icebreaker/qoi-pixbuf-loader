CFLAGS += -Wall -Wextra -Werror -O2 -Isrc

#ifdef MAX_COLORS
#CFLAGS += -DMRT_MAX_COLORS=$(MAX_COLORS)
#endif

TARGET ?= qoi-pixbuf-loader.so
TARGET_TEST ?= qoi-pixbuf-loader-test

PREFIX ?= /usr
SHARE_DIR ?= $(PREFIX)/share
THUMBNAILER_DIR ?= $(SHARE_DIR)/thumbnailers

BUILD_DIR = build
BUILD_TARGET = $(BUILD_DIR)/$(TARGET)
BUILD_TARGET_TEST = $(BUILD_DIR)/$(TARGET_TEST)
BUILD_TARGET_TEST_LOADER_CACHE = $(BUILD_DIR)/loader.cache

PKG_CONFIG_LIBS = gdk-pixbuf-2.0
PKG_CONFIG_FLAGS = $(shell pkg-config --cflags --libs $(PKG_CONFIG_LIBS))
PKG_CONFIG_LOADERS_DIR ?= $(shell pkg-config $(PKG_CONFIG_LIBS) --variable=gdk_pixbuf_moduledir)

TEST_RUNNER=G_MESSAGES_DEBUG=all \
	GDK_PIXBUF_MODULE_FILE=$(CURDIR)/$(BUILD_TARGET_TEST_LOADER_CACHE) $(BUILD_TARGET_TEST)

HEADERS = src/qoi.h
SOURCES = src/qoi-pixbuf-loader.c
SOURCES_TEST = src/qoi-pixbuf-loader-test.c

all: $(BUILD_TARGET) $(BUILD_TARGET_TEST)

$(BUILD_TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(PKG_CONFIG_FLAGS) -shared $(SOURCES) -o $@

$(BUILD_TARGET_TEST): $(BUILD_TARGET) $(SOURCES_TEST)
	$(CC) $(CFLAGS) $(LDFLAGS) $(PKG_CONFIG_FLAGS) $(SOURCES_TEST) -o $@

$(BUILD_TARGET_TEST_LOADER_CACHE): $(BUILD_TARGET)
	gdk-pixbuf-query-loaders $^ > $@

test: $(BUILD_TARGET_TEST) $(BUILD_TARGET_TEST_LOADER_CACHE)
	$(TEST_RUNNER) qoi/test.qoi qoi/test_out.qoi qoi/test_callback_out.qoi
	$(TEST_RUNNER) qoi/test_alpha.qoi qoi/test_alpha_out.qoi qoi/test_alpha_callback_out.qoi
	$(TEST_RUNNER) qoi/test_large.qoi qoi/test_large_out.qoi qoi/test_large_callback_out.qoi
	$(TEST_RUNNER) qoi/test_alpha_large.qoi qoi/test_alpha_large_out.qoi qoi/test_alpha_large_callback_out.qoi
	md5sum -c qoi/test.md5sum
	md5sum -c qoi/test_alpha.md5sum
	md5sum -c qoi/test_large.md5sum
	md5sum -c qoi/test_alpha_large.md5sum

install:
ifeq ($(PKG_CONFIG_LOADERS_DIR),)
	@echo "Could not auto-detect the pixbuf loaders directory."
	@echo "Please run 'make install PKG_CONFIG_LOADERS_DIR=/path/to/loaders' again."
else
	cp $(BUILD_TARGET) $(PKG_CONFIG_LOADERS_DIR)/$(TARGET)
	gdk-pixbuf-query-loaders --update-cache
	xdg-mime install --mode system --novendor res/qoi.xml
	cp res/qoi.thumbnailer $(THUMBNAILER_DIR)/qoi.thumbnailer
endif

uninstall:
	$(RM) $(THUMBNAILER_DIR)/qoi.thumbnailer
	$(RM) $(PKG_CONFIG_LOADERS_DIR)/$(TARGET)
	gdk-pixbuf-query-loaders --update-cache
	xdg-mime uninstall --mode system res/qoi.xml

clean:
	$(RM) $(BUILD_TARGET)
	$(RM) $(BUILD_TARGET_TEST)
	$(RM) $(BUILD_TARGET_TEST_LOADER_CACHE)
	$(RM) qoi/*_out.qoi

.PHONY: all test install uninstall clean