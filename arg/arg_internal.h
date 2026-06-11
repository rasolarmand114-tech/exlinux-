/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ARG Scheduler Layer — Internal Header
 * Version: 1.0
 * Target:  Linux 4.19 · ARM64 · Exynos 850
 *
 * NOTE: Policy/dispatch logic is written in C for Linux 4.19.
 *       When porting to Linux 6.1+, dispatch/ and policy/ are
 *       candidates for rewrite in Rust ("Rust decides, ASM executes").
 */
#ifndef _ARG_INTERNAL_H
#define _ARG_INTERNAL_H

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/percpu.h>
#include <linux/cpumask.h>
#include <linux/hashtable.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/ktime.h>

/* ------------------------------------------------------------------ */
/* Version                                                             */
/* ------------------------------------------------------------------ */
#define ARG_VERSION_MAJOR  1
#define ARG_VERSION_MINOR  0
#define ARG_VERSION_STR    "1.0"

/* ------------------------------------------------------------------ */
/* Tuning constants                                                    */
/* ------------------------------------------------------------------ */

/* task_util (0-1024 scale) above which task is treated as "large" */
#define ARG_LARGE_TASK_UTIL     512U

/* pack tasks onto a single CPU if their util is below this */
#define ARG_PACK_TASK_UTIL_MAX  102U   /* ~10 % */

/* stop filling a pack bin when aggregate util exceeds this */
#define ARG_PACK_BIN_UTIL_MAX   819U   /* ~80 % */

/* Minimum idle time (ns) before a CPU is considered truly idle */
#define ARG_CPU_IDLE_NS         500000ULL  /* 0.5 ms */

/* Hash table order for task-state lookup */
#define ARG_TASK_HT_BITS        8          /* 256 buckets */

/* ------------------------------------------------------------------ */
/* Runtime flags (set via /sys/module/arg_sched/parameters/)          */
/* ------------------------------------------------------------------ */
extern atomic_t arg_async_mode;       /* 1 = async (gaming), 0 = sync  */
extern atomic_t arg_pack_enabled;     /* 1 = task packing on           */
extern atomic_t arg_vthread_enabled;  /* 1 = virtual thread layer on   */
extern atomic_t arg_energy_mode;      /* 1 = energy-aware dispatch      */
extern atomic_t arg_fps_mode;         /* 1 = FPS-stability mode         */

/* ------------------------------------------------------------------ */
/* Per-CPU ARM64 feature cache                                         */
/* (detected once at module load, never re-evaluated at runtime)      */
/* ------------------------------------------------------------------ */
struct arg_cpu_features {
	bool has_lse;       /* Large System Extensions (atomics)  */
	bool has_fp;        /* Floating-point                     */
	bool has_sve;       /* Scalable Vector Extension          */
	bool has_crc32;     /* CRC32 instructions                 */
	bool has_aes;       /* AES crypto                         */
	bool has_sha1;
	bool has_sha2;
	bool has_pmull;
	bool detected;      /* set to true after first detection  */
};
DECLARE_PER_CPU(struct arg_cpu_features, arg_cpu_feat);

/* ------------------------------------------------------------------ */
/* Per-CPU scheduler state                                             */
/* ------------------------------------------------------------------ */
struct arg_cpu_state {
	unsigned int          capacity;       /* arch_scale_cpu_capacity  */
	unsigned int          util;           /* current CFS utilization  */
	bool                  is_big;         /* big core (big.LITTLE)    */
	bool                  is_idle;
	bool                  hotplugged_out;
	u64                   last_idle_ts;   /* ktime_get_ns()           */
	spinlock_t            lock;
};
DECLARE_PER_CPU(struct arg_cpu_state, arg_cpu_st);

/* ------------------------------------------------------------------ */
/* Task state — tracked alongside every task_struct                   */
/* ------------------------------------------------------------------ */
struct arg_task_state {
	struct task_struct   *task;
	unsigned int          util_avg;       /* snapshot from se.avg     */
	unsigned int          nr_migrations;
	bool                  is_large;
	u64                   last_enqueue_ns;
	u64                   last_wakeup_ns;
	struct hlist_node     ht_node;        /* in arg_task_ht           */
};

/* Global hash table: pid → arg_task_state */
extern DECLARE_HASHTABLE(arg_task_ht, ARG_TASK_HT_BITS);
extern spinlock_t arg_task_ht_lock;

/* ------------------------------------------------------------------ */
/* Task packing bin                                                    */
/* ------------------------------------------------------------------ */
struct arg_pack_bin {
	int         cpu;
	unsigned int total_util;   /* aggregate util of packed tasks    */
	int         nr_tasks;
	spinlock_t  lock;
};
extern struct arg_pack_bin arg_pack_bins[NR_CPUS];

/* ------------------------------------------------------------------ */
/* Virtual Thread                                                      */
/* One Thread → One CPU At A Time is always preserved.                */
/* A vthread represents a *logical* subdivision of a large task;      */
/* each worker runs on its own dedicated CPU.                         */
/* ------------------------------------------------------------------ */
#define ARG_VT_IDLE    0
#define ARG_VT_RUNNING 1
#define ARG_VT_DEAD    2

struct arg_vthread {
	int               id;
	int               target_cpu;
	struct task_struct *worker;
	unsigned int      load;          /* share of parent task load    */
	atomic_t          state;         /* ARG_VT_* above               */
	struct list_head  node;          /* in arg_vthread_pool          */
};

/* ------------------------------------------------------------------ */
/* Function prototypes — Hook layer (called from kernel/sched/)       */
/* ------------------------------------------------------------------ */
void arg_do_enqueue(struct rq *rq, struct task_struct *p);
void arg_do_wakeup(struct rq *rq, struct task_struct *p);
struct task_struct *arg_do_pick_next(struct rq *rq);
void arg_do_update_load(void);

/* ------------------------------------------------------------------ */
/* Policy layer                                                        */
/* ------------------------------------------------------------------ */
int  arg_policy_pack_task(struct task_struct *p, int *target_cpu);
int  arg_policy_select_cpu_async(struct task_struct *p);
int  arg_policy_select_cpu_sync(struct task_struct *p);
int  arg_policy_energy_cpu(struct task_struct *p);
int  arg_policy_fps_cpu(struct task_struct *p);

/* ------------------------------------------------------------------ */
/* Dispatch layer                                                      */
/* ------------------------------------------------------------------ */
int  arg_dispatch_select_cpu(struct task_struct *p);
int  arg_dispatch_migrate(struct task_struct *p, int dst_cpu);
void arg_dispatch_balance(void);

/* ------------------------------------------------------------------ */
/* Topology layer                                                      */
/* ------------------------------------------------------------------ */
int  arg_topology_init(void);
void arg_topology_exit(void);
void arg_topology_cpu_up(int cpu);
void arg_topology_cpu_down(int cpu);
unsigned int arg_topology_cpu_capacity(int cpu);
bool arg_topology_is_big_cpu(int cpu);

/* ------------------------------------------------------------------ */
/* State layer                                                         */
/* ------------------------------------------------------------------ */
int  arg_state_init(void);
void arg_state_exit(void);
void arg_state_task_enqueue(struct task_struct *p);
void arg_state_task_dequeue(struct task_struct *p);
struct arg_task_state *arg_state_get(struct task_struct *p);

/* ------------------------------------------------------------------ */
/* Virtual Thread layer                                                */
/* ------------------------------------------------------------------ */
int  arg_vthread_init(void);
void arg_vthread_exit(void);
struct arg_vthread *arg_vthread_create(struct task_struct *parent, int cpu);
void arg_vthread_destroy(struct arg_vthread *vt);

/* ------------------------------------------------------------------ */
/* Runtime parameters                                                  */
/* ------------------------------------------------------------------ */
void arg_runtime_init(void);
void arg_runtime_exit(void);
bool arg_runtime_is_async(void);
bool arg_runtime_is_pack_enabled(void);
bool arg_runtime_is_vthread_enabled(void);
bool arg_runtime_is_energy_mode(void);
bool arg_runtime_is_fps_mode(void);

/* ------------------------------------------------------------------ */
/* ARM64 arch layer (implemented in arch/arm64/*.S)                   */
/* Assembly executes.  Never contains decision logic.                 */
/* ------------------------------------------------------------------ */
void arg_arm64_dmb_ish(void);
void arg_arm64_dsb_ish(void);
void arg_arm64_isb(void);
void arg_arm64_prefetch_read(const void *addr);
void arg_arm64_prefetch_write(const void *addr);
void arg_arm64_dcache_clean_range(void *start, unsigned long len);
void arg_arm64_dcache_inval_range(void *start, unsigned long len);
void arg_arm64_tlb_flush_all(void);
unsigned long arg_arm64_read_pmccntr(void);
unsigned long arg_arm64_read_mpidr(void);

/* Feature detection — reads ID_AA64ISAR0_EL1 / ID_AA64PFR0_EL1 */
void arg_detect_cpu_features(int cpu);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static inline unsigned int arg_task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned int arg_cpu_util(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long util = READ_ONCE(rq->cfs.avg.util_avg);
	unsigned long capacity = arch_scale_cpu_capacity(NULL, cpu);

	if (util >= capacity)
		return (unsigned int)capacity;
	return (unsigned int)util;
}

static inline bool arg_task_is_rt(struct task_struct *p)
{
	return p->policy == SCHED_FIFO || p->policy == SCHED_RR;
}

#endif /* _ARG_INTERNAL_H */
