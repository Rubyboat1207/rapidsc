CC      = zig cc
AR      = zig ar
CFLAGS  = -Wall -g -fvisibility=hidden
RELEASE_CFLAGS = -Wall -O2 -fvisibility=hidden -DNDEBUG

BUILD_ROOT = build

# --- Detect host OS/arch so the native build lands in the same
# build/<platform>/ layout the cross-compile targets use. ---
ifeq ($(OS),Windows_NT)
    NATIVE_DIR   = windows
    EXE_EXT      =.exe
    SHARED_NAME  = rapids.dll
else
    UNAME_S := $(shell uname -s)
    UNAME_M := $(shell uname -m)
    ifeq ($(UNAME_S),Darwin)
        EXE_EXT     =
        SHARED_NAME = librapids.dylib
        ifeq ($(UNAME_M),arm64)
            NATIVE_DIR = macos-arm64
        else
            NATIVE_DIR = macos
        endif
    else
        NATIVE_DIR   = linux
        EXE_EXT      =
        SHARED_NAME  = librapids.so
    endif
endif

BUILD_DIR  = $(BUILD_ROOT)/$(NATIVE_DIR)
OBJ_DIR    = $(BUILD_DIR)/obj
EXE        = $(BUILD_DIR)/rapidsc$(EXE_EXT)
STATIC_LIB = $(BUILD_DIR)/librapids.a
SHARED_LIB = $(BUILD_DIR)/$(SHARED_NAME)

SRCS     := $(shell find src -name '*.c')
OBJS     := $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS:.c=.o)))
LIB_OBJS := $(filter-out $(OBJ_DIR)/main.o, $(OBJS))

vpath %.c $(sort $(dir $(SRCS)))

# Cross-compile targets: name -> zig target triple
XPLAT_windows     = x86_64-windows-gnu
XPLAT_macos       = x86_64-macos
XPLAT_macos_arm64 = aarch64-macos

.PHONY: build build_lib build_exe run compile_example clean asan release \
        cross-windows cross-macos cross-macos-arm64 cross-all \
        cross-windows-release cross-macos-release cross-macos-arm64-release cross-all-release \
        compile_commands

build: build_lib build_exe

build_exe: $(EXE)

build_lib: $(STATIC_LIB) $(SHARED_LIB)

$(EXE): $(OBJS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(STATIC_LIB): $(LIB_OBJS) | $(OBJ_DIR)
	$(AR) rcs $@ $^

$(SHARED_LIB): $(LIB_OBJS) | $(OBJ_DIR)
	$(CC) -shared -o $@ $^

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

compile_example:
	rapids example.rpd -o $(BUILD_DIR)/example --ds

run: build compile_example
	$(EXE) $(BUILD_DIR)/example.rpdb

asan: clean
	$(MAKE) build compile_example CFLAGS="-Wall -g -fsanitize=address -fno-omit-frame-pointer"
	$(EXE) $(BUILD_DIR)/example.rpdb

release: clean
	$(MAKE) build CFLAGS="$(RELEASE_CFLAGS)"
	strip $(EXE) $(SHARED_LIB)

# --- Cross-compilation: exe + static + dynamic libs for Windows and macOS ---
# Each target builds into build/<platform>/ using zig cc/ar as a cross
# compiler+linker, driven by a single reusable recipe (see cross-build).
# (If a target's platform matches the host, it just rebuilds the native
# output in place using an explicit -target instead of the host default.)

# $(5) carries the CFLAGS to use (defaults to $(CFLAGS) when omitted), so
# the *-release targets can drive the same recipe with RELEASE_CFLAGS + -s
# (strip at link time — cross-arch binaries can't be stripped by the host's
# native `strip`, but the linker can drop symbols for any target just fine).
define cross-build
	mkdir -p $(BUILD_ROOT)/$(1)/obj
	for src in $(SRCS); do \
		obj=$(BUILD_ROOT)/$(1)/obj/$$(basename $$src .c).o; \
		$(CC) -target $(2) $(or $(5),$(CFLAGS)) -fPIC -c $$src -o $$obj || exit 1; \
	done
	$(AR) rcs $(BUILD_ROOT)/$(1)/librapids.a $$(find $(BUILD_ROOT)/$(1)/obj -name '*.o' ! -name 'main.o')
	$(CC) -target $(2) -shared -o $(BUILD_ROOT)/$(1)/$(3) $$(find $(BUILD_ROOT)/$(1)/obj -name '*.o' ! -name 'main.o')
	$(CC) -target $(2) $(or $(5),$(CFLAGS)) $(BUILD_ROOT)/$(1)/obj/*.o -o $(BUILD_ROOT)/$(1)/$(4)
endef

cross-windows:
	$(call cross-build,windows,$(XPLAT_windows),rapids.dll,rapidsc.exe)

cross-macos:
	$(call cross-build,macos,$(XPLAT_macos),librapids.dylib,rapidsc)

cross-macos-arm64:
	$(call cross-build,macos-arm64,$(XPLAT_macos_arm64),librapids.dylib,rapidsc)

cross-all: cross-windows cross-macos cross-macos-arm64

cross-windows-release:
	$(call cross-build,windows,$(XPLAT_windows),rapids.dll,rapidsc.exe,$(RELEASE_CFLAGS) -s)

cross-macos-release:
	$(call cross-build,macos,$(XPLAT_macos),librapids.dylib,rapidsc,$(RELEASE_CFLAGS) -s)

cross-macos-arm64-release:
	$(call cross-build,macos-arm64,$(XPLAT_macos_arm64),librapids.dylib,rapidsc,$(RELEASE_CFLAGS) -s)

cross-all-release: cross-windows-release cross-macos-release cross-macos-arm64-release

# --- Generate compile_commands.json for tooling (clangd, etc.) using a
# real clang instead of zig cc, since bear needs to intercept actual
# compiler invocations. ---
compile_commands: clean
	bear -- $(MAKE) build CC=clang AR=ar

clean:
	rm -rf $(BUILD_ROOT)
