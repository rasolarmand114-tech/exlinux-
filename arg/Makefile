# SPDX-License-Identifier: GPL-2.0
# arg/Makefile — ARG Scheduler Module

obj-$(CONFIG_SCHAD_ARG_KMM) += arg_sched.o

# ── objectها ─────────────────────────────────────────────────────────
# حتماً -y نه -m  (kbuild همیشه -y برای لیست object استفاده میکنه)
arg_sched-m :=                \
	arg_core.o            \
	hook/picknext.o       \
	hook/enqueue.o        \
	hook/wakeup.o         \
	hook/load.o           \
	hook/shim.o           \
	arg_rust.o            \
	arch/arm64/barriers.o \
	arch/arm64/cache.o    \
	arch/arm64/atomics.o  \
	arch/arm64/regs.o     \
	arch/arm64/prefetch.o
# C flags
ccflags-y += -I$(src)/include

# Assembly flags
AFLAGS_barriers.o  := -march=armv8-a
AFLAGS_cache.o     := -march=armv8-a
AFLAGS_regs.o      := -march=armv8-a
AFLAGS_prefetch.o  := -march=armv8-a
AFLAGS_atomics.o   := -march=armv8-a+lse

# Rust compilation
RUSTC      ?= rustc
RUST_TARGET := aarch64-unknown-none

RUST_FLAGS := \
	--edition 2021 \
	--crate-type staticlib \
	--crate-name arg_sched \
	--target $(RUST_TARGET) \
	-C panic=abort \
	-C opt-level=2 \
	-C code-model=kernel \
	-C relocation-model=pic \
	--emit obj \
	-A dead_code \
	-A unused_variables

RUST_SRCS := \
	src/lib.rs \
	src/packing.rs \
	src/async_mode.rs \
	src/fps.rs \
	src/energy.rs \
	src/virtual_thread.rs \
	src/topology.rs \
	src/hotplug.rs \
	src/features.rs

# Custom Rust build rule
$(obj)/arg_rust.o: $(addprefix $(src)/, $(RUST_SRCS)) FORCE
	@echo "  RUSTC  $(notdir $@)"
	$(RUSTC) $(RUST_FLAGS) -o $@ $(src)/src/lib.rs

FORCE:

.PHONY: FORCE
