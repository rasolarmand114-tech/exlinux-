# SPDX-License-Identifier: GPL-2.0
# ARG Scheduler Module — Top-level Makefile

obj-$(CONFIG_SCHAD_ARG_KMM) += arg_core.o

arg_core-y += arg_core.o

# Hook layer
arg_core-y += hook/enqueue.o
arg_core-y += hook/wakeup.o
arg_core-y += hook/picknext.o

# Policy layer
arg_core-y += policy/packing.o
arg_core-y += policy/async.o
arg_core-y += policy/energy.o

# Dispatch layer
arg_core-y += dispatch/dispatch.o

# Topology layer
arg_core-y += topology/topology.o

# State layer
arg_core-y += state/state.o

# Virtual Thread layer
arg_core-y += vthread/vthread.o

# Runtime parameters
arg_core-y += runtime/runtime.o

# ARM64 architecture layer
arg_core-y += arch/arm64/barriers.o
arg_core-y += arch/arm64/cache.o
arg_core-y += arch/arm64/atomics.o
arg_core-y += arch/arm64/regs.o
arg_core-y += arch/arm64/detect.o

# Assembly flags
AFLAGS_barriers.o := -march=armv8-a
AFLAGS_cache.o    := -march=armv8-a
AFLAGS_regs.o     := -march=armv8-a
AFLAGS_atomics.o  := -march=armv8-a+lse
