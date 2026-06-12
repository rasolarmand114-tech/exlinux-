# SPDX-License-Identifier: GPL-2.0
# arg/Makefile — ARG Scheduler Module
#
# Build flow:
#   1. rustc compiles src/lib.rs → arg_rust.o  (all Rust modules inlined)
#   2. ARM64 assembly → barriers.o cache.o atomics.o regs.o prefetch.o
#   3. C hook layer   → picknext.o enqueue.o wakeup.o load.o shim.o
#   4. arg_core.o     → module init/exit + module_param
#   5. Kernel linker merges all → arg_core.ko

obj-$(CONFIG_SCHAD_ARG_KMM) += arg_core.o

# ── Object list ─────────────────────────────────────────────────────
arg_core-y :=				\
	arg_core.o			\
	hook/picknext.o			\
	hook/enqueue.o			\
	hook/wakeup.o			\
	hook/load.o			\
	hook/shim.o			\
	arg_rust.o			\
	arch/arm64/barriers.o		\
	arch/arm64/cache.o		\
	arch/arm64/atomics.o		\
	arch/arm64/regs.o		\
	arch/arm64/prefetch.o

# ── Rust compilation ─────────────────────────────────────────────────
RUSTC      ?= rustc
RUST_TARGET := aarch64-unknown-none

RUST_FLAGS :=				\
	--edition 2021			\
	--crate-type staticlib		\
	--crate-name arg_sched		\
	--target $(RUST_TARGET)		\
	-C panic=abort			\
	-C opt-level=2			\
	-C code-model=kernel		\
	-C relocation-model=pic		\
	--emit obj			\
	-Z unstable-options		\
	-A dead_code			\
	-A unused_variables

RUST_SRCS  := $(shell find $(src)/src -name '*.rs' 2>/dev/null)
RUST_ENTRY := $(src)/src/lib.rs

# Custom rule: compile entire Rust crate to arg_rust.o
$(obj)/arg_rust.o: $(RUST_SRCS) FORCE
	$(RUSTC) $(RUST_FLAGS) -o $@ $(RUST_ENTRY)

# ── Assembly flags ───────────────────────────────────────────────────
AFLAGS_barriers.o  := -march=armv8-a
AFLAGS_cache.o     := -march=armv8-a
AFLAGS_regs.o      := -march=armv8-a
AFLAGS_prefetch.o  := -march=armv8-a
AFLAGS_atomics.o   := -march=armv8-a+lse

# ── C include paths ──────────────────────────────────────────────────
ccflags-y += -I$(src)/include

subdir-ccflags-y += -I$(src)/include
