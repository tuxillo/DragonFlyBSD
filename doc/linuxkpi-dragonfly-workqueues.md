## Appendix A: Taskqueue Implementation Investigation (DragonFly vs FreeBSD)

**Investigation Date:** 2026-02-01
**Purpose:** Determine whether DragonFly needs FreeBSD's multi-task tracking mechanism for taskqueue

### A.1 Executive Summary

Both DragonFly and FreeBSD taskqueue implementations share a common origin (Doug Rabson's 2000 FreeBSD code), but have diverged significantly. **The most critical difference is in the "busy" tracking mechanism**, which is essential for proper task synchronization in multi-worker taskqueues.

**Key Finding:** DragonFly's native kernel code uses **only single-worker taskqueues**. The multi-worker requirement comes **exclusively from LinuxKPI workqueues** used by DRM/GPU drivers.

---

### A.2 Data Structure Comparison

#### A.2.1 DragonFly `struct taskqueue` (sys/kern/subr_taskqueue.c:49-63)

```c
struct taskqueue {
    STAILQ_ENTRY(taskqueue) tq_link;
    STAILQ_HEAD(, task)     tq_queue;
    const char              *tq_name;
    taskqueue_enqueue_fn    tq_enqueue;
    void                    *tq_context;

    struct task             *tq_running;    /* Single pointer - ONE task only */
    struct spinlock         tq_lock;
    struct thread           **tq_threads;
    int                     tq_tcount;
    int                     tq_flags;
    int                     tq_callouts;
};
```

#### A.2.2 FreeBSD `struct taskqueue` (sys/kern/subr_taskqueue.c:63-79)

```c
struct taskqueue {
    STAILQ_HEAD(, task)     tq_queue;
    LIST_HEAD(, taskqueue_busy) tq_active;  /* LIST of busy tasks - MULTIPLE */
    struct task             *tq_hint;       /* Optimization hint */
    u_int                   tq_seq;         /* Sequence number for ordering */
    int                     tq_callouts;
    struct mtx_padalign     tq_mutex;
    taskqueue_enqueue_fn    tq_enqueue;
    void                    *tq_context;
    char                    *tq_name;
    struct thread           **tq_threads;
    int                     tq_tcount;
    int                     tq_spin;
    int                     tq_flags;
    taskqueue_callback_fn   tq_callbacks[TASKQUEUE_NUM_CALLBACKS];
    void                    *tq_cb_contexts[TASKQUEUE_NUM_CALLBACKS];
};
```

#### A.2.3 FreeBSD `struct taskqueue_busy` (sys/kern/subr_taskqueue.c:56-61)

```c
struct taskqueue_busy {
    struct task             *tb_running;    /* Currently executing task */
    u_int                    tb_seq;        /* Sequence number when started */
    bool                     tb_canceling;  /* Task is being canceled */
    LIST_ENTRY(taskqueue_busy) tb_link;     /* Link in tq_active list */
};
```

**DragonFly does NOT have this structure.**

#### A.2.4 Critical Architectural Difference

| Aspect | DragonFly | FreeBSD |
|--------|-----------|---------|
| Running task tracking | Single `tq_running` pointer | `LIST_HEAD tq_active` of `taskqueue_busy` |
| Concurrent task support | **ONE task only** | **Multiple tasks** |
| Sequence numbers | Not present | `tq_seq` for drain ordering |
| Hint optimization | Not present | `tq_hint` for priority insertion |
| Cancel tracking | Not present | `tb_canceling` flag per task |

---

### A.3 API Function Comparison

#### A.3.1 Functions Present in Both

| Function | DragonFly | FreeBSD |
|----------|-----------|---------|
| `taskqueue_create()` | ✓ | ✓ |
| `taskqueue_free()` | ✓ | ✓ |
| `taskqueue_start_threads()` | ✓ | ✓ |
| `taskqueue_enqueue()` | ✓ | ✓ |
| `taskqueue_enqueue_flags()` | ✓ | ✓ |
| `taskqueue_enqueue_timeout()` | ✓ | ✓ |
| `taskqueue_cancel()` | ✓ | ✓ |
| `taskqueue_cancel_timeout()` | ✓ | ✓ |
| `taskqueue_drain()` | ✓ | ✓ |
| `taskqueue_drain_timeout()` | ✓ | ✓ |
| `taskqueue_drain_all()` | ✓ | ✓ |
| `taskqueue_poll_is_busy()` | ✓ | ✓ |
| `taskqueue_block()` | ✓ | ✓ |
| `taskqueue_unblock()` | ✓ | ✓ |
| `taskqueue_run()` | ✓ | ✓ |
| `taskqueue_member()` | ✗ | ✓ |

#### A.3.2 DragonFly-Only Extensions

| Function | Purpose |
|----------|---------|
| `taskqueue_find()` | Find taskqueue by name |
| `taskqueue_enqueue_optq()` | Allow task migration between queues |
| `taskqueue_cancel_simple()` | Cancel without knowing queue |
| `taskqueue_drain_simple()` | Drain without knowing queue |

#### A.3.3 FreeBSD-Only Features

| Function | Purpose |
|----------|---------|
| `taskqueue_create_fast()` | Create with spin mutex |
| `taskqueue_start_threads_cpuset()` | CPU affinity |
| `taskqueue_start_threads_in_proc()` | Specific process |
| `taskqueue_enqueue_timeout_sbt()` | sbintime precision |
| `taskqueue_quiesce()` | Repeated drain until truly empty |
| `taskqueue_set_callback()` | Thread init/shutdown callbacks |

---

### A.4 Implementation Comparison: Critical Functions

#### A.4.1 `taskqueue_drain_all()`

**FreeBSD Implementation** (sys/kern/subr_taskqueue.c:623-634):

```c
void
taskqueue_drain_all(struct taskqueue *queue)
{
    if (!queue->tq_spin)
        WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, __func__);

    TQ_LOCK(queue);
    (void)taskqueue_drain_tq_queue(queue);   /* Step 1: Drain pending queue */
    (void)taskqueue_drain_tq_active(queue);  /* Step 2: Wait for active tasks */
    TQ_UNLOCK(queue);
}
```

**FreeBSD `taskqueue_drain_tq_queue()`** (lines 400-427):

```c
static int
taskqueue_drain_tq_queue(struct taskqueue *queue)
{
    struct task t_barrier;

    if (STAILQ_EMPTY(&queue->tq_queue))
        return (0);

    /*
     * Enqueue barrier task at TAIL with highest priority (UCHAR_MAX).
     * This prevents new tasks from jumping ahead while ensuring all
     * currently queued tasks execute first.
     */
    TASK_INIT(&t_barrier, UCHAR_MAX, taskqueue_task_nop_fn, &t_barrier);
    STAILQ_INSERT_TAIL(&queue->tq_queue, &t_barrier, ta_link);
    queue->tq_hint = &t_barrier;
    t_barrier.ta_pending = 1;

    /* Wait until barrier has been executed */
    while (t_barrier.ta_pending != 0)
        TQ_SLEEP(queue, &t_barrier, "tq_qdrain");
    return (1);
}
```

**FreeBSD `taskqueue_drain_tq_active()`** (lines 434-461):

```c
static int
taskqueue_drain_tq_active(struct taskqueue *queue)
{
    struct taskqueue_busy *tb;
    u_int seq;

    if (LIST_EMPTY(&queue->tq_active))
        return (0);

    /* Block taskq_terminate() during drain */
    queue->tq_callouts++;

    /* Capture current sequence number */
    seq = queue->tq_seq;

restart:
    /* Wait for any task with sequence <= our captured seq */
    LIST_FOREACH(tb, &queue->tq_active, tb_link) {
        if ((int)(tb->tb_seq - seq) <= 0) {
            TQ_SLEEP(queue, tb->tb_running, "tq_adrain");
            goto restart;  /* List may have changed, restart */
        }
    }

    /* Release taskqueue_terminate() */
    queue->tq_callouts--;
    if ((queue->tq_flags & TQ_FLAGS_ACTIVE) == 0)
        wakeup_one(queue->tq_threads);
    return (1);
}
```

**Current DragonFly Implementation** (sys/kern/subr_taskqueue.c:522-550):

```c
void
taskqueue_drain_all(struct taskqueue *queue)
{
    struct task *task;

    TQ_LOCK(queue);

    while ((task = STAILQ_FIRST(&queue->tq_queue)) != NULL ||
        queue->tq_running != NULL) {
        if (task != NULL) {
            TQ_SLEEP(queue, queue, "tqdall");
        } else {
            TQ_SLEEP(queue, queue, "tqdall");
        }
    }

    TQ_UNLOCK(queue);
}
```

**Analysis:**

| Aspect | FreeBSD | DragonFly (Current) |
|--------|---------|---------------------|
| Barrier task pattern | Yes - ensures ordering | No |
| Multi-task tracking | Yes - via `tq_active` list | No - single `tq_running` |
| Sequence numbers | Yes - ignores tasks started after drain began | No |
| Handles concurrent workers | Yes - correctly waits for ALL | **No - only waits for ONE** |

#### A.4.2 `taskqueue_poll_is_busy()`

**FreeBSD Implementation** (sys/kern/subr_taskqueue.c:541-551):

```c
int
taskqueue_poll_is_busy(struct taskqueue *queue, struct task *task)
{
    int retval;

    TQ_LOCK(queue);
    retval = task->ta_pending > 0 || task_get_busy(queue, task) != NULL;
    TQ_UNLOCK(queue);

    return (retval);
}
```

**FreeBSD `task_get_busy()` helper** (lines 126-137):

```c
static struct taskqueue_busy *
task_get_busy(struct taskqueue *queue, struct task *task)
{
    struct taskqueue_busy *tb;

    TQ_ASSERT_LOCKED(queue);
    LIST_FOREACH(tb, &queue->tq_active, tb_link) {
        if (tb->tb_running == task)
            return (tb);
    }
    return (NULL);
}
```

**Current DragonFly Implementation** (sys/kern/subr_taskqueue.c:555-565):

```c
int
taskqueue_poll_is_busy(struct taskqueue *queue, struct task *task)
{
    int busy;

    TQ_LOCK(queue);
    busy = task->ta_pending > 0 || task == queue->tq_running;
    TQ_UNLOCK(queue);

    return (busy);
}
```

**Analysis:**

| Aspect | FreeBSD | DragonFly (Current) |
|--------|---------|---------------------|
| Checks pending | Yes | Yes |
| Checks running | Yes - searches `tq_active` LIST | Yes - compares to single `tq_running` |
| Multi-worker support | Yes - finds task in any worker | **No - only finds if it's THE one running** |

#### A.4.3 `taskqueue_run()` / `taskqueue_run_locked()`

**FreeBSD Implementation** (sys/kern/subr_taskqueue.c:483-525):

```c
static void
taskqueue_run_locked(struct taskqueue *queue)
{
    struct epoch_tracker et;
    struct taskqueue_busy tb;      /* Stack-allocated busy tracker */
    struct task *task;
    bool in_net_epoch;
    int pending;

    KASSERT(queue != NULL, ("tq is NULL"));
    TQ_ASSERT_LOCKED(queue);
    tb.tb_running = NULL;
    LIST_INSERT_HEAD(&queue->tq_active, &tb, tb_link);  /* Register tracker */
    in_net_epoch = false;

    while ((task = STAILQ_FIRST(&queue->tq_queue)) != NULL) {
        STAILQ_REMOVE_HEAD(&queue->tq_queue, ta_link);
        if (queue->tq_hint == task)
            queue->tq_hint = NULL;
        pending = task->ta_pending;
        task->ta_pending = 0;
        tb.tb_running = task;           /* Mark which task is running */
        tb.tb_seq = ++queue->tq_seq;    /* Assign sequence number */
        tb.tb_canceling = false;
        TQ_UNLOCK(queue);

        KASSERT(task->ta_func != NULL, ("task->ta_func is NULL"));
        /* Network epoch handling omitted for brevity */
        task->ta_func(task->ta_context, pending);

        TQ_LOCK(queue);
        wakeup(task);  /* Wake anyone waiting on this specific task */
    }
    /* Network epoch cleanup omitted */
    LIST_REMOVE(&tb, tb_link);  /* Unregister tracker */
}
```

**Current DragonFly Implementation** (sys/kern/subr_taskqueue.c:409-437):

```c
static void
taskqueue_run(struct taskqueue *queue, int lock_held)
{
    struct task *task;
    int pending;

    if (lock_held == 0)
        TQ_LOCK(queue);
    while (STAILQ_FIRST(&queue->tq_queue)) {
        task = STAILQ_FIRST(&queue->tq_queue);
        STAILQ_REMOVE_HEAD(&queue->tq_queue, ta_link);
        pending = task->ta_pending;
        task->ta_pending = 0;
        queue->tq_running = task;      /* Single pointer assignment */

        TQ_UNLOCK(queue);
        task->ta_func(task->ta_context, pending);
        queue->tq_running = NULL;      /* Clear single pointer */
        wakeup(task);
        wakeup(queue);  /* Wake drain waiters */
        TQ_LOCK(queue);
    }
    if (lock_held == 0)
        TQ_UNLOCK(queue);
}
```

**Key Difference:** FreeBSD allocates a `taskqueue_busy` structure **on the stack** of each worker thread and inserts it into `tq_active`. This allows tracking **multiple concurrent running tasks**. DragonFly uses a single `tq_running` pointer, which can only track **one task at a time**.

---

### A.5 Locking Mechanism Comparison

| Aspect | DragonFly | FreeBSD |
|--------|-----------|---------|
| Lock type | Always spinlock (`struct spinlock`) | Configurable: sleep mutex (`MTX_DEF`) or spin mutex (`MTX_SPIN`) |
| Lock init | `spin_init()` | `mtx_init()` |
| Sleep with lock | `ssleep()` | `msleep()` or `msleep_spin()` |
| Lock primitives | `spin_lock()`/`spin_unlock()` | `mtx_lock()`/`mtx_unlock()` |
| Fast (spin) option | Always spin | `taskqueue_create_fast()` for spin |

---

### A.6 Complete Taskqueue Usage Audit in DragonFly

#### A.6.1 All `taskqueue_start_threads()` Callers

| File | Thread Count | Uses `drain_all`? | Uses `poll_is_busy`? |
|------|--------------|-------------------|----------------------|
| `sys/compat/linuxkpi/common/src/linux_work.c:679` | **`cpus` (multi-worker)** | **Yes (via macro)** | **Yes** |
| `sys/compat/linuxkpi/common/src/linux_work.c:793` | 1 | Yes | No |
| `sys/netproto/802_11/wlan/ieee80211.c:351` | 1 | No | No |
| `sys/kern/uipc_usrreq.c:1697` | 1 | No | No |
| `sys/kern/subr_firmware.c:493` | 1 | No | No |
| `sys/net/wg/if_wg.c:2968` | 1 | No | No |
| `sys/dev/netif/iwn/if_iwn.c:735` | 1 | **Commented out for DFly** | No |
| `sys/dev/netif/alc/if_alc.c:1565` | 1 | No | No |
| `sys/dev/netif/bwn/bwn/if_bwn.c:854` | 1 | No | No |
| `sys/dev/netif/ath/ath/if_ath.c:758` | 1 | No | No |
| `sys/dev/netif/iwm/if_iwm.c:6089` | 1 | **Commented out for DFly** | No |
| `sys/dev/netif/oce/oce_if.c:714` | 1 | No | No |
| `sys/dev/raid/mpr/mpr_sas.c:795` | 1 | No | No |
| `sys/dev/raid/mps/mps_sas.c:725` | 1 | No | No |
| `sys/dev/raid/mrsas/mrsas_cam.c:146` | 1 | No | No |
| `sys/dev/virtual/amazon/ena/ena.c:795` | 1 | No | No |
| `sys/dev/virtual/amazon/ena/ena.c:3763` | 1 | No | No |
| `sys/dev/virtual/vmware/vmxnet3/if_vmx.c:997` | **`nthreads` (variable)** | No | No |
| `sys/kern/subr_taskqueue.c:738` (per-CPU) | 1 | No | No |

#### A.6.2 vmxnet3 Thread Count Analysis

```c
/* sys/dev/virtual/vmware/vmxnet3/if_vmx.c:985-997 */
static void
vmxnet3_start_taskqueue(struct vmxnet3_softc *sc)
{
    device_t dev;
    int nthreads, error;

    dev = sc->vmx_dev;

    /*
     * The taskqueue is typically not frequently used, so a dedicated
     * thread for each queue is unnecessary.
     */
    nthreads = MAX(1, sc->vmx_ntxqueues / 2);

    error = taskqueue_start_threads(&sc->vmx_tq, nthreads, PI_NET,
        "%s taskq", device_get_nameunit(dev));
```

**vmxnet3 can create multiple workers** (`MAX(1, ntxqueues/2)`), but it **only uses `taskqueue_drain()` for specific tasks**, not `taskqueue_drain_all()`.

#### A.6.3 All `taskqueue_drain_all()` Callers

| File | Line | Context |
|------|------|---------|
| `sys/compat/linuxkpi/common/src/linux_work.c:807` | IRQ work init | LinuxKPI - **multi-worker queue** |
| `sys/kern/subr_gtaskqueue.c:415` | gtaskqueue drain | Group taskqueue (separate implementation) |
| `sys/kern/subr_gtaskqueue.c:815` | Group drain all | Group taskqueue |
| `sys/kern/subr_taskqueue.c:524` | Implementation | N/A |
| `sys/dev/netif/iwn/if_iwn.c:1466` | Driver cleanup | **Commented out for DragonFly** |
| `sys/dev/netif/iwm/if_iwm.c:6673` | Driver cleanup | **Commented out for DragonFly** |

**iwn and iwm drivers explicitly skip `taskqueue_drain_all()` on DragonFly:**

```c
/* sys/dev/netif/iwn/if_iwn.c:1462-1468 */
#if defined(__DragonFly__)
        /* doesn't exist for DFly, DFly drains tasks on free */
#else
        taskqueue_drain_all(sc->sc_tq);
#endif
        taskqueue_free(sc->sc_tq);
```

#### A.6.4 All `taskqueue_poll_is_busy()` Callers

| File | Line | Function | Context |
|------|------|----------|---------|
| `sys/compat/linuxkpi/common/src/linux_work.c:592` | `linux_flush_work()` | LinuxKPI - **multi-worker** |
| `sys/compat/linuxkpi/common/src/linux_work.c:621` | `linux_flush_delayed_work()` | LinuxKPI - **multi-worker** |
| `sys/compat/linuxkpi/common/src/linux_work.c:657` | `work_busy()` | LinuxKPI - **multi-worker** |
| `sys/kern/subr_taskqueue.c:556` | Implementation | N/A |

---

### A.7 LinuxKPI Workqueue Analysis

#### A.7.1 Workqueue Creation (linux_work.c:670-687)

```c
struct workqueue_struct *
alloc_workqueue(const char *name, unsigned flags, int cpus)
{
    struct workqueue_struct *wq;

    if (cpus == 0)
        cpus = linux_default_wq_cpus;  /* mp_ncpus + 1, minimum 4 */

    wq = kmalloc(sizeof(*wq), M_WAITOK | M_ZERO);
    wq->taskqueue = taskqueue_create(name, M_WAITOK,
        taskqueue_thread_enqueue, &wq->taskqueue);
    atomic_set(&wq->draining, 0);
#ifdef __DragonFly__
    taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, -1, "%s", name);
#else
    taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, "%s", name);
#endif
    TAILQ_INIT(&wq->exec_head);
    mtx_init(&wq->exec_mtx, "linux_wq_exec", NULL, MTX_DEF);

    return (wq);
}
```

**Key:** LinuxKPI workqueues create **`cpus` worker threads** (typically `mp_ncpus + 1`).

#### A.7.2 Default Worker Count (linux_work.c:717-727)

```c
static void
linux_work_init(void *arg)
{
    int max_wq_cpus = mp_ncpus + 1;

    /* avoid deadlock when there are too few threads */
    if (max_wq_cpus < 4)
        max_wq_cpus = 4;

    /* set default number of CPUs */
    linux_default_wq_cpus = max_wq_cpus;

    linux_system_short_wq = alloc_workqueue("linuxkpi_short_wq", 0, max_wq_cpus);
    linux_system_long_wq = alloc_workqueue("linuxkpi_long_wq", 0, max_wq_cpus);
    /* ... */
}
```

**Example:** On a 4-core system:
- `linux_system_short_wq` → 5 worker threads
- `linux_system_long_wq` → 5 worker threads

#### A.7.3 flush_workqueue and drain_workqueue Macros (workqueue.h:171-178)

```c
#define flush_workqueue(wq) \
    taskqueue_drain_all((wq)->taskqueue)

#define drain_workqueue(wq) do {        \
    atomic_inc(&(wq)->draining);        \
    taskqueue_drain_all((wq)->taskqueue);   \
    atomic_dec(&(wq)->draining);        \
} while (0)
```

**These macros call `taskqueue_drain_all()` on multi-worker taskqueues.**

---

### A.8 Multi-Worker Problem Illustration

#### A.8.1 Scenario: 5-Worker Taskqueue with Concurrent Tasks

```
Worker Thread 1: Executing task A
Worker Thread 2: Executing task B  
Worker Thread 3: Executing task C
Worker Thread 4: Executing task D
Worker Thread 5: Executing task E  ← This one is tq_running
```

#### A.8.2 DragonFly `taskqueue_poll_is_busy(task A)` - WRONG

```c
busy = task->ta_pending > 0 || task == queue->tq_running;
/* task A != queue->tq_running (which is task E) */
/* Returns: FALSE (WRONG - task A is actually running!) */
```

#### A.8.3 FreeBSD `taskqueue_poll_is_busy(task A)` - CORRECT

```c
retval = task->ta_pending > 0 || task_get_busy(queue, task) != NULL;
/* task_get_busy() searches tq_active list and finds task A */
/* Returns: TRUE (CORRECT) */
```

#### A.8.4 DragonFly `taskqueue_drain_all()` - WRONG

```c
while ((task = STAILQ_FIRST(&queue->tq_queue)) != NULL ||
    queue->tq_running != NULL) {
    /* ... */
}
/* Only waits for queue->tq_running (task E) */
/* Tasks A, B, C, D may still be executing when function returns! */
```

#### A.8.5 FreeBSD `taskqueue_drain_all()` - CORRECT

```c
(void)taskqueue_drain_tq_queue(queue);   /* Wait for pending tasks */
(void)taskqueue_drain_tq_active(queue);  /* Wait for ALL active tasks */
/* Iterates through tq_active list, waits for A, B, C, D, E */
```

---

### A.9 Recent Commit History for DragonFly Taskqueue

```
3f8958c72a Fix taskqueue_drain_all() - use queue wakeup
7466ae8e13 Fix taskqueue_drain_all() - wait on tasks, not queue
7c7fd30e63 Fix race condition in taskqueue_drain_all()
80f0e9f718 Add taskqueue_drain_all() and taskqueue_poll_is_busy() implementations
```

**These commits attempted iterative fixes but did not address the fundamental architectural issue: DragonFly lacks multi-task tracking.**

#### A.9.1 Commit 80f0e9f718 (Original Addition)

Added `taskqueue_drain_all()` and `taskqueue_poll_is_busy()` to DragonFly kernel. Removed stubs from `dragonfly_compat.h`.

#### A.9.2 Commits 7c7fd30e63, 7466ae8e13, 3f8958c72a (Fix Attempts)

Multiple attempts to fix wakeup/sleep race conditions in `taskqueue_drain_all()`. **None addressed the multi-worker tracking issue.**

**Recommendation:** Revert to commit 80f0e9f718 or earlier before implementing proper fix.

---

### A.10 Conclusions

#### A.10.1 Native DragonFly Usage

| Finding | Details |
|---------|---------|
| All native drivers use single-worker | Every `taskqueue_start_threads()` call uses count=1 (except vmxnet3 variable) |
| vmxnet3 multi-worker safe | Uses only `taskqueue_drain()` for specific tasks, not `drain_all` |
| iwn/iwm skip drain_all | Explicitly commented out for DragonFly |
| Current tq_running sufficient | For all native code |

#### A.10.2 LinuxKPI Usage

| Finding | Details |
|---------|---------|
| Creates multi-worker queues | `cpus = mp_ncpus + 1` (minimum 4) |
| Uses drain_all | Via `flush_workqueue()` and `drain_workqueue()` macros |
| Uses poll_is_busy | Via `flush_work()`, `flush_delayed_work()`, `work_busy()` |
| Current implementation broken | Will cause races, use-after-free |

#### A.10.3 Required for LinuxKPI/DRM

To properly support LinuxKPI workqueues with multi-worker taskqueues:

1. **Add `struct taskqueue_busy`** - Per-worker task tracking structure
2. **Add `tq_active` list** - Track all currently executing tasks
3. **Add `tq_seq`** - Sequence number for drain ordering
4. **Implement `task_get_busy()`** - Search active list for task
5. **Modify `taskqueue_run()`** - Use stack-allocated `taskqueue_busy`
6. **Implement `taskqueue_drain_tq_queue()`** - Barrier task pattern
7. **Implement `taskqueue_drain_tq_active()`** - Sequence-based wait

#### A.10.4 Alternative Approaches

| Approach | Pros | Cons |
|----------|------|------|
| Full FreeBSD port | Complete compatibility | More invasive changes |
| Minimal tq_active tracking | Fixes multi-worker issue | Missing barrier optimization |
| Force single-worker for LinuxKPI | No kernel changes | **Major GPU performance penalty** |
| Keep current (broken) | No work | **LinuxKPI will crash/corrupt** |

---

### A.11 Source File References

#### A.11.1 DragonFly Files

- `sys/kern/subr_taskqueue.c` - Main taskqueue implementation (744 lines)
- `sys/sys/taskqueue.h` - Public header (179 lines)
- `sys/kern/subr_gtaskqueue.c` - Group taskqueue (separate implementation)
- `sys/compat/linuxkpi/common/src/linux_work.c` - LinuxKPI workqueue wrapper
- `sys/compat/linuxkpi/common/include/linux/workqueue.h` - Workqueue macros

#### A.11.2 FreeBSD Files (~/s/freebsd)

- `sys/kern/subr_taskqueue.c` - Main taskqueue implementation
- `sys/sys/taskqueue.h` - Public header
- `sys/sys/_task.h` - Task structure definition

---

### A.12 Raw Data: All taskqueue_start_threads Calls

```
sys/compat/linuxkpi/common/src/linux_work.c:679:	taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, -1, "%s", name);
sys/compat/linuxkpi/common/src/linux_work.c:681:	taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, "%s", name);
sys/compat/linuxkpi/common/src/linux_work.c:793:	taskqueue_start_threads(&linux_irq_work_tq, 1, PWAIT, -1,
sys/compat/linuxkpi/common/src/linux_work.c:796:	taskqueue_start_threads(&linux_irq_work_tq, 1, PWAIT,
sys/netproto/802_11/wlan/ieee80211.c:351:	taskqueue_start_threads(&ic->ic_tq, 1, TDPRI_KERN_DAEMON, -1,
sys/netproto/802_11/wlan/ieee80211.c:354:	taskqueue_start_threads(&ic->ic_tq, 1, PI_NET, "%s net80211 taskq",
sys/kern/uipc_usrreq.c:1697:	taskqueue_start_threads(&unp_taskqueue, 1, TDPRI_KERN_DAEMON,
sys/kern/subr_firmware.c:493:		(void) taskqueue_start_threads(&firmware_tq, 1, TDPRI_KERN_DAEMON,
sys/net/wg/if_wg.c:2968:		taskqueue_start_threads(&wg_taskqueues[i], 1,
sys/dev/netif/iwn/if_iwn.c:735:	error = taskqueue_start_threads(&sc->sc_tq, 1, TDPRI_KERN_DAEMON,
sys/dev/netif/iwn/if_iwn.c:738:	error = taskqueue_start_threads(&sc->sc_tq, 1, 0, "iwn_taskq");
sys/dev/netif/alc/if_alc.c:1565:	taskqueue_start_threads(&sc->alc_tq, 1, TDPRI_KERN_DAEMON, -1, "%s taskq",
sys/dev/netif/bwn/bwn/if_bwn.c:854:	taskqueue_start_threads(&sc->sc_tq, 1, TDPRI_KERN_DAEMON,
sys/dev/netif/bwn/bwn/if_bwn.c:859:	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET,
sys/dev/netif/ath/ath/if_ath.c:758:	taskqueue_start_threads(&sc->sc_tq, 1, TDPRI_KERN_DAEMON, -1,
sys/dev/netif/ath/ath/if_ath.c:763:	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET, "%s taskq",
sys/dev/netif/iwm/if_iwm.c:6089:	error = taskqueue_start_threads(&sc->sc_tq, 1, TDPRI_KERN_DAEMON, -1, "iwm_taskq");
sys/dev/netif/oce/oce_if.c:714:	taskqueue_start_threads(&ii->tq, 1, TDPRI_KERN_DAEMON, -1, "%s taskq",
sys/dev/raid/mpr/mpr_sas.c:795:	taskqueue_start_threads(&sassc->ev_tq, 1, TDPRI_KERN_DAEMON,
sys/dev/raid/mps/mps_sas.c:725:	taskqueue_start_threads(&sassc->ev_tq, 1, TDPRI_KERN_DAEMON,
sys/dev/raid/mrsas/mrsas_cam.c:146:    taskqueue_start_threads(&sc->ev_tq, 1, TDPRI_KERN_DAEMON,
sys/dev/virtual/amazon/ena/ena.c:795:	taskqueue_start_threads_cpuset(&rx_ring->cmpl_tq, 1, PI_NET, &cpu_mask,
sys/dev/virtual/amazon/ena/ena.c:799:	taskqueue_start_threads(&rx_ring->cmpl_tq, 1, TDPRI_KERN_DAEMON, -1,
sys/dev/virtual/amazon/ena/ena.c:3763:	taskqueue_start_threads(&adapter->reset_tq, 1, TDPRI_KERN_DAEMON, -1,
sys/dev/virtual/vmware/vmxnet3/if_vmx.c:997:	error = taskqueue_start_threads(&sc->vmx_tq, nthreads, PI_NET,
sys/kern/subr_taskqueue.c:738:		taskqueue_start_threads(&taskqueue_thread[cpu], 1,
```

---

### A.13 Raw Data: All drain_all and poll_is_busy Calls

```
sys/compat/linuxkpi/common/src/linux_work.c:592:		retval = taskqueue_poll_is_busy(tq, &work->work_task);
sys/compat/linuxkpi/common/src/linux_work.c:621:		retval = taskqueue_poll_is_busy(tq, &dwork->work.work_task);
sys/compat/linuxkpi/common/src/linux_work.c:657:		return (taskqueue_poll_is_busy(tq, &work->work_task));
sys/compat/linuxkpi/common/src/linux_work.c:807:	taskqueue_drain_all(linux_irq_work_tq);
sys/kern/subr_gtaskqueue.c:415:gtaskqueue_drain_all(struct gtaskqueue *queue)
sys/kern/subr_gtaskqueue.c:815:		gtaskqueue_drain_all(q);
sys/kern/subr_taskqueue.c:524:taskqueue_drain_all(struct taskqueue *queue)
sys/kern/subr_taskqueue.c:556:taskqueue_poll_is_busy(struct taskqueue *queue, struct task *task)
sys/dev/netif/iwn/if_iwn.c:1466:		taskqueue_drain_all(sc->sc_tq);
sys/dev/netif/iwm/if_iwm.c:6673:		taskqueue_drain_all(sc->sc_tq);
```

**Note:** iwn and iwm calls are inside `#if !defined(__DragonFly__)` blocks.

---

## Appendix B: LinuxKPI Per-CPU Workqueue Implementation Plan

**Date:** 2026-02-01
**Status:** Planned
**Goal:** Fix LinuxKPI workqueue drain/flush operations by using per-CPU single-worker taskqueues instead of one multi-worker taskqueue.

### B.1 Background and Rationale

#### B.1.1 The Problem

Current LinuxKPI workqueues create a single taskqueue with multiple workers (`mp_ncpus + 1`, minimum 4). This breaks `taskqueue_drain_all()` and `taskqueue_poll_is_busy()` because DragonFly's taskqueue only tracks ONE running task via `tq_running`.

#### B.1.2 The Solution

Transform LinuxKPI workqueue implementation from:
- **Current:** 1 taskqueue × N workers (broken with multi-worker)

To:
- **New:** N taskqueues × 1 worker each (works correctly)

This approach:
- Keeps existing LinuxKPI state machine (tested by FreeBSD)
- Preserves rcu_work, irq_work, current_work() features
- Uses well-tested taskqueue infrastructure
- drain_all/poll_is_busy work correctly with single worker per queue
- Provides similar parallelism (work distributed across CPUs)

#### B.1.3 Historical Reference

Commit `c17dd299cec907bb472a1f824300cad3290019b3` (François Tigeot/Matthew Dillon, 2020-11-11) implemented a similar per-CPU model for the old DRM workqueue code. That implementation:
- Used custom lwkt threads (one per CPU)
- Each worker had its own queue + lock
- Bypassed taskqueue entirely

Our approach keeps taskqueue but adopts the per-CPU topology.

---

### B.2 Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Work re-queue target | Same CPU | Consistency with Linux behavior; work stays on same CPU unless explicitly moved |
| `system_unbound_wq` | Separate single-queue | Linux uses separate pools for unbound work |
| `WQ_HIGHPRI` handling | Future enhancement | Can use different thread priority later |
| `irq_work` changes | None | Already uses single-thread taskqueue (correct) |

---

### B.3 Data Structure Changes

#### B.3.1 Update `struct workqueue_struct`

**File:** `sys/compat/linuxkpi/common/include/linux/workqueue.h`

**Current:**
```c
struct workqueue_struct {
    struct taskqueue *taskqueue;
    struct mtx exec_mtx;
    TAILQ_HEAD(, work_exec) exec_head;
    atomic_t draining;
};
```

**New:**
```c
struct workqueue_struct {
    int num_queues;                    /* Number of taskqueues */
    struct taskqueue **taskqueues;     /* Array of per-CPU taskqueues */
    struct mtx exec_mtx;
    TAILQ_HEAD(, work_exec) exec_head;
    atomic_t draining;
    unsigned int flags;                /* WQ_UNBOUND, WQ_HIGHPRI, etc. */
};
```

#### B.3.2 Update `struct work_struct`

**Current:**
```c
struct work_struct {
    struct task work_task;
    struct workqueue_struct *work_queue;
    work_func_t func;
    atomic_t state;
};
```

**New:**
```c
struct work_struct {
    struct task work_task;
    struct workqueue_struct *work_queue;
    int queue_index;                   /* Index into taskqueues array */
    work_func_t func;
    atomic_t state;
};
```

---

### B.4 Implementation Details

#### B.4.1 Helper Functions

```c
/* Select which taskqueue to use for a work item */
static inline int
linux_select_queue(struct workqueue_struct *wq, int cpu)
{
    if (wq->num_queues == 1)
        return 0;
    
    if (cpu == WORK_CPU_UNBOUND || cpu < 0 || cpu >= wq->num_queues)
        return mycpuid % wq->num_queues;
    
    return cpu;
}

/* Get taskqueue by index */
static inline struct taskqueue *
linux_get_taskqueue(struct workqueue_struct *wq, int queue_index)
{
    KASSERT(queue_index >= 0 && queue_index < wq->num_queues,
        ("invalid queue index %d", queue_index));
    return wq->taskqueues[queue_index];
}
```

#### B.4.2 Workqueue Creation (`linux_create_workqueue_common`)

```c
struct workqueue_struct *
linux_create_workqueue_common(const char *name, int cpus)
{
    struct workqueue_struct *wq;
    int i, num_queues;
    char tq_name[32];

    wq = kmalloc(sizeof(*wq), M_WAITOK | M_ZERO);
    
    /* Determine number of queues */
    if (cpus == 1) {
        /* Ordered/single-threaded workqueue */
        num_queues = 1;
    } else {
        /* Per-CPU queues */
        num_queues = ncpus;
    }
    
    wq->num_queues = num_queues;
    wq->taskqueues = kmalloc(sizeof(struct taskqueue *) * num_queues, 
                             M_WAITOK | M_ZERO);
    
    for (i = 0; i < num_queues; i++) {
        if (num_queues > 1)
            ksnprintf(tq_name, sizeof(tq_name), "%s/%d", name, i);
        else
            ksnprintf(tq_name, sizeof(tq_name), "%s", name);
        
        wq->taskqueues[i] = taskqueue_create(tq_name, M_WAITOK,
            taskqueue_thread_enqueue, &wq->taskqueues[i]);
        taskqueue_start_threads(&wq->taskqueues[i], 1, PWAIT, -1, 
            "%s", tq_name);
    }
    
    atomic_set(&wq->draining, 0);
    TAILQ_INIT(&wq->exec_head);
    mtx_init(&wq->exec_mtx, "linux_wq_exec", NULL, MTX_DEF);
    
    return (wq);
}
```

#### B.4.3 Workqueue Destruction (`linux_destroy_workqueue`)

```c
void
linux_destroy_workqueue(struct workqueue_struct *wq)
{
    int i;
    
    atomic_inc(&wq->draining);
    
    /* Drain all taskqueues */
    for (i = 0; i < wq->num_queues; i++) {
        taskqueue_drain_all(wq->taskqueues[i]);
    }
    
    /* Free all taskqueues */
    for (i = 0; i < wq->num_queues; i++) {
        taskqueue_free(wq->taskqueues[i]);
    }
    
    mtx_destroy(&wq->exec_mtx);
    kfree(wq->taskqueues);
    kfree(wq);
}
```

#### B.4.4 Queue Work (`linux_queue_work_on`)

Key change: Select appropriate taskqueue and store index.

```c
bool
linux_queue_work_on(int cpu, struct workqueue_struct *wq,
    struct work_struct *work)
{
    static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
        [WORK_ST_IDLE] = WORK_ST_TASK,
        [WORK_ST_TIMER] = WORK_ST_TIMER,
        [WORK_ST_TASK] = WORK_ST_TASK,
        [WORK_ST_EXEC] = WORK_ST_TASK,
        [WORK_ST_CANCEL] = WORK_ST_TASK,
    };
    struct taskqueue *tq;
    int queue_index;

    if (atomic_read(&wq->draining) != 0)
        return (!work_pending(work));

    switch (linux_update_state(&work->state, states)) {
    case WORK_ST_EXEC:
    case WORK_ST_CANCEL:
        if (linux_work_exec_unblock(work) != 0)
            return (true);
        /* FALLTHROUGH */
    case WORK_ST_IDLE:
        work->work_queue = wq;
        queue_index = linux_select_queue(wq, cpu);
        work->queue_index = queue_index;
        tq = linux_get_taskqueue(wq, queue_index);
        taskqueue_enqueue(tq, &work->work_task);
        return (true);
    default:
        return (false);
    }
}
```

#### B.4.5 Flush Workqueue (`linux_flush_workqueue`)

New function to iterate over all per-CPU taskqueues:

```c
void
linux_flush_workqueue(struct workqueue_struct *wq)
{
    int i;
    
    for (i = 0; i < wq->num_queues; i++) {
        taskqueue_drain_all(wq->taskqueues[i]);
    }
}
```

#### B.4.6 Work-Specific Functions

Functions that operate on specific work items use the stored `queue_index`:

```c
bool
linux_flush_work(struct work_struct *work)
{
    struct taskqueue *tq;
    bool retval;

    WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
        "linux_flush_work() might sleep");

    switch (atomic_read(&work->state)) {
    case WORK_ST_IDLE:
        return (false);
    default:
        tq = linux_get_taskqueue(work->work_queue, work->queue_index);
        retval = taskqueue_poll_is_busy(tq, &work->work_task);
        taskqueue_drain(tq, &work->work_task);
        return (retval);
    }
}
```

---

### B.5 Header Macro Updates

**File:** `sys/compat/linuxkpi/common/include/linux/workqueue.h`

#### B.5.1 INIT_WORK Macro

Add `queue_index` initialization:

```c
#define INIT_WORK(work, fn)                         \
do {                                                \
    (work)->func = (fn);                            \
    (work)->work_queue = NULL;                      \
    (work)->queue_index = -1;                       \
    atomic_set(&(work)->state, 0);                  \
    TASK_INIT(&(work)->work_task, 0, linux_work_fn, (work)); \
} while (0)
```

#### B.5.2 Flush/Drain Macros

```c
#define flush_workqueue(wq) \
    linux_flush_workqueue(wq)

#define drain_workqueue(wq) do {            \
    atomic_inc(&(wq)->draining);            \
    linux_flush_workqueue(wq);              \
    atomic_dec(&(wq)->draining);            \
} while (0)

#define flush_scheduled_work() \
    linux_flush_workqueue(system_wq)
```

---

### B.6 System Workqueue Initialization

**File:** `sys/compat/linuxkpi/common/src/linux_work.c`

```c
static void
linux_work_init(void *arg)
{
    /* 
     * Create system workqueues.
     * - system_wq: per-CPU for parallelism
     * - system_unbound_wq: single queue (unbound work can run anywhere)
     * - system_highpri_wq: per-CPU (same as system_wq for now)
     * - system_long_wq: per-CPU for long-running work
     * - system_power_efficient_wq: alias to system_wq
     */
    linux_system_short_wq = alloc_workqueue("linuxkpi_short_wq", 0, 0);
    linux_system_long_wq = alloc_workqueue("linuxkpi_long_wq", 0, 0);
    
    /* Create truly unbound workqueue with single thread */
    linux_system_unbound_wq = alloc_workqueue("linuxkpi_unbound_wq", 
        WQ_UNBOUND, 1);

    system_long_wq = linux_system_long_wq;
    system_wq = linux_system_short_wq;
    system_power_efficient_wq = linux_system_short_wq;
    system_unbound_wq = linux_system_unbound_wq;
    system_highpri_wq = linux_system_short_wq;
}
```

---

### B.7 Functions Requiring Updates

| Function | File | Change Required |
|----------|------|-----------------|
| `linux_create_workqueue_common()` | linux_work.c | Complete rewrite (per-CPU queues) |
| `linux_destroy_workqueue()` | linux_work.c | Iterate and free all queues |
| `linux_queue_work_on()` | linux_work.c | Select queue, store index |
| `linux_queue_delayed_work_on()` | linux_work.c | Select queue, store index |
| `linux_flush_work()` | linux_work.c | Use stored queue_index |
| `linux_flush_delayed_work()` | linux_work.c | Use stored queue_index |
| `linux_work_busy()` | linux_work.c | Use stored queue_index |
| `linux_cancel_work()` | linux_work.c | Use stored queue_index |
| `linux_cancel_work_sync()` | linux_work.c | Use stored queue_index |
| `linux_cancel_delayed_work()` | linux_work.c | Use stored queue_index |
| `linux_cancel_delayed_work_sync_int()` | linux_work.c | Use stored queue_index |
| `linux_delayed_work_enqueue()` | linux_work.c | Use stored queue_index |
| `linux_work_init()` | linux_work.c | Create separate unbound queue |
| `linux_work_uninit()` | linux_work.c | Destroy unbound queue |
| `INIT_WORK` macro | workqueue.h | Initialize queue_index |
| `flush_workqueue` macro | workqueue.h | Call linux_flush_workqueue() |
| `drain_workqueue` macro | workqueue.h | Call linux_flush_workqueue() |
| `flush_scheduled_work` macro | workqueue.h | Call linux_flush_workqueue() |

---

### B.8 Files to Modify

| File | Type of Change |
|------|----------------|
| `sys/compat/linuxkpi/common/include/linux/workqueue.h` | Struct changes, macro updates |
| `sys/compat/linuxkpi/common/src/linux_work.c` | Implementation changes |

---

### B.9 Testing Plan

#### B.9.1 Build Test

```bash
cd /usr/src
make -j$(sysctl -n hw.ncpu) buildkernel KERNCONF=X86_64_GENERIC
```

#### B.9.2 Boot Test

1. Install new kernel
2. Verify system boots without panics
3. Check dmesg for LinuxKPI initialization messages

#### B.9.3 Workqueue Functional Tests

Create test module that:
1. Creates workqueue with multiple CPUs
2. Queues work items to different CPUs
3. Verifies `flush_workqueue()` waits for all items
4. Verifies `flush_work()` waits for specific item
5. Verifies `cancel_work_sync()` properly cancels
6. Tests work re-queueing from callback

#### B.9.4 DRM/GPU Validation

After implementation:
1. Build drm-kmod against modified kernel
2. Load DRM modules in QEMU
3. Verify no workqueue-related panics or hangs

---

### B.10 Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| API compatibility | Medium | Keep same function signatures; changes are internal |
| Performance | Low | Per-CPU model similar to old DRM; may improve cache locality |
| Work migration | Low | Decided to keep work on same CPU (consistent with Linux) |
| Unbound workqueue | Low | Created separate single-thread queue |
| irq_work impact | None | Already uses single-thread; no changes needed |

---

### B.11 Implementation Phases

| Phase | Description | Files | Status |
|-------|-------------|-------|--------|
| B.11.1 | Update data structures | workqueue.h | ✅ DONE |
| B.11.2 | Implement helper functions | linux_work.c | ✅ DONE |
| B.11.3 | Rewrite workqueue creation/destruction | linux_work.c | ✅ DONE |
| B.11.4 | Update queue_work functions | linux_work.c | ✅ DONE |
| B.11.5 | Update flush/drain functions | linux_work.c | ✅ DONE |
| B.11.6 | Update cancel functions | linux_work.c | ✅ DONE |
| B.11.7 | Update header macros | workqueue.h | ✅ DONE |
| B.11.8 | Update system workqueue init | linux_work.c | ✅ DONE |
| B.11.9 | Build and test | - | PENDING |

---

### B.12 Implementation Summary (2026-02-01)

#### Problem Solved
DragonFly's taskqueue tracks only ONE running task via `tq_running`, but LinuxKPI creates multi-worker taskqueues. This broke `taskqueue_drain_all()` and `taskqueue_poll_is_busy()` used by `flush_workqueue()`, `drain_workqueue()`, `flush_work()`, and `work_busy()`.

#### Solution Implemented
Transformed LinuxKPI workqueue from **1 taskqueue × N workers** to **N taskqueues × 1 worker each** (per-CPU model).

#### Key Changes

**Data Structures (workqueue.h):**
1. `struct workqueue_struct`: Changed `taskqueue` pointer → `taskqueues` array + `num_queues` + `flags`
2. `struct work_struct`: Added `queue_index` field to track which queue holds the work
3. `INIT_WORK` macro: Initializes `queue_index` to -1

**New Functions (linux_work.c):**
1. `linux_select_queue(wq, cpu)`: Selects appropriate queue index (CPU-based or round-robin)
2. `linux_get_taskqueue(wq, index)`: Returns taskqueue pointer from array with bounds check
3. `linux_flush_workqueue(wq)`: Iterates all per-CPU queues and drains each

**Updated Functions (linux_work.c):**
- `linux_create_workqueue_common()` - Creates N single-worker taskqueues
- `linux_destroy_workqueue()` - Frees all per-CPU queues
- `linux_queue_work_on()` - Selects queue, stores index
- `linux_queue_delayed_work_on()` - Selects queue, stores index
- `linux_delayed_work_enqueue()` - Uses stored queue_index
- `linux_flush_work()` - Uses stored queue_index
- `linux_flush_delayed_work()` - Uses stored queue_index
- `linux_work_busy()` - Uses stored queue_index
- `linux_cancel_work()` - Uses stored queue_index
- `linux_cancel_work_sync()` - Uses stored queue_index
- `linux_cancel_delayed_work()` - Uses stored queue_index
- `linux_cancel_delayed_work_sync_int()` - Uses stored queue_index
- `linux_work_init()` - Creates separate `system_unbound_wq` with single queue
- `linux_work_uninit()` - Destroys all three system workqueues

**Updated Macros (workqueue.h):**
- `flush_workqueue()` → calls `linux_flush_workqueue()`
- `drain_workqueue()` → calls `linux_flush_workqueue()`
- `flush_scheduled_work()` → calls `linux_flush_workqueue(system_wq)`

---

### B.13 Success Criteria

- [ ] Kernel builds without errors
- [ ] System boots without panics
- [ ] `flush_workqueue()` correctly waits for all work items across all CPUs
- [ ] `flush_work()` correctly waits for specific work item
- [ ] `work_busy()` correctly reports busy status
- [ ] `cancel_work_sync()` properly cancels and waits
- [ ] Work items execute on expected CPUs
- [ ] No race conditions in work re-queueing
- [ ] drm-kmod builds and loads without workqueue-related errors
