/* SPDX-License-Identifier: GPL-2.0 */
/*
 * arg/include/arg_abi.h
 *
 * C ↔ Rust ABI for the ARG Scheduler Module.
 *
 * Rust exports  →  called from hook/*.c
 * Rust imports  →  defined in hook/shim.c  (kernel accessor shims)
 *
 * All pointers crossing the boundary are opaque void *.
 * Rust NEVER dereferences kernel struct pointers directly.
 */
#ifndef _ARG_ABI_H
#define _ARG_ABI_H

#include <linux/types.h>

/* ------------------------------------------------------------------ */
/* Rust → C exports  (Rust provides, C calls)                         */
/* ------------------------------------------------------------------ */

/* Lifecycle */
int  arg_rust_init(void);
void arg_rust_exit(void);

/* Scheduler hooks — called from hook/{picknext,enqueue,wakeup,load}.c */
void *arg_rust_pick_next(void *rq);
void  arg_rust_enqueue(void *rq, void *p);
void  arg_rust_wakeup(void *rq, void *p);
void  arg_rust_update_load(void);

/* CPU hotplug — called from glue in hook/load.c */
void arg_rust_cpu_online(int cpu);
void arg_rust_cpu_offline(int cpu);

/* Runtime parameter setters/getters */
void arg_rust_set_async(int v);
void arg_rust_set_pack(int v);
void arg_rust_set_vthread(int v);
void arg_rust_set_energy(int v);
void arg_rust_set_fps(int v);
int  arg_rust_get_async(void);
int  arg_rust_get_pack(void);
int  arg_rust_get_vthread(void);
int  arg_rust_get_energy(void);
int  arg_rust_get_fps(void);

/* ------------------------------------------------------------------ */
/* C → Rust exports  (shim.c provides, Rust declares as extern "C")  */
/* All functions listed here are implemented in hook/shim.c           */
/* ------------------------------------------------------------------ */

/* Task accessors */
u32  arg_shim_task_util(void *p);
int  arg_shim_task_cpu(void *p);
u32  arg_shim_task_policy(void *p);
int  arg_shim_task_pid(void *p);
bool arg_shim_task_is_rt(void *p);
bool arg_shim_task_is_dl(void *p);
int  arg_shim_task_cpu_allowed(void *p, int cpu);

/* Runqueue accessors */
void *arg_shim_cpu_rq(int cpu);
int   arg_shim_rq_cpu(void *rq);
u32   arg_shim_rq_nr_running(void *rq);
u32   arg_shim_rq_rt_nr(void *rq);

/* CPU topology/util */
u32  arg_shim_cpu_util(int cpu);
u32  arg_shim_cpu_capacity(int cpu);
bool arg_shim_cpu_idle(int cpu);
bool arg_shim_cpu_online(int cpu);
int  arg_shim_nr_cpus(void);
int  arg_shim_this_cpu(void);

/* Memory */
void *arg_shim_kmalloc(size_t size, u32 flags);
void  arg_shim_kfree(void *ptr);
u64   arg_shim_ktime_ns(void);

/* Task-state hash table (opaque handle) */
int   arg_shim_ts_init(void);
void  arg_shim_ts_destroy(void);
void *arg_shim_ts_get(void *task);
void *arg_shim_ts_create(void *task);
void  arg_shim_ts_remove(void *task);
u32   arg_shim_ts_get_util(void *ts);
void  arg_shim_ts_set_util(void *ts, u32 v);
u8    arg_shim_ts_get_large(void *ts);
void  arg_shim_ts_set_large(void *ts, u8 v);
u32   arg_shim_ts_get_mig(void *ts);
void  arg_shim_ts_inc_mig(void *ts);
u64   arg_shim_ts_get_enq(void *ts);
void  arg_shim_ts_set_enq(void *ts, u64 v);
u64   arg_shim_ts_get_wake(void *ts);
void  arg_shim_ts_set_wake(void *ts, u64 v);

/* Pack bins (per-CPU utilization totals) */
u32  arg_shim_pack_get(int cpu);
void arg_shim_pack_add(int cpu, u32 util);
void arg_shim_pack_sub(int cpu, u32 util);
void arg_shim_pack_reset(int cpu);

/* kthread helpers for vthread workers */
void *arg_shim_kthread_create_cpu(int cpu, const char *name);
void  arg_shim_kthread_stop(void *task);
void  arg_shim_kthread_wake(void *task);

/* SMP */
void arg_shim_smp_call(int cpu, void (*fn)(void *), void *arg, int wait);

/* Logging (ptr is valid UTF-8, NOT null-terminated; len bytes) */
void arg_shim_log_info(const u8 *ptr, size_t len);
void arg_shim_log_debug(const u8 *ptr, size_t len);

/* synchronize_sched() — safe to call from Rust exit path */
void arg_shim_sync_sched(void);

#endif /* _ARG_ABI_H */
