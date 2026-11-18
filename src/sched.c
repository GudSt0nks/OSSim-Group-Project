/*
 * sched.c: CPU schedulers.
 *
 * FCFS and RR are given. TODO checkpoint 1: sjf, srtf, prio, mlfq.
 * Each one is a scheduler_t with the same hooks as FCFS and RR. There are
 * notes above each stub. Functions you will probably need:
 *   proc_burst_remaining(p)   ticks left in p's current cpu burst
 *   proc_total_remaining(p)   cpu ticks left in the whole process
 *   q_pop_min(&q, key)        remove the element with the smallest key
 *   q_min_key(&q, key)        the smallest key, without removing (for preemption)
 */
#include <string.h>
#include "sched.h"
#include "queue.h"

scheduler_t *sched = NULL;

/* FCFS */
static queue_t fcfs_q;

static void   fcfs_init(void)                { q_init(&fcfs_q); }
static void   fcfs_enqueue(pcb_t *p)         { q_push(&fcfs_q, p); }
static bool   fcfs_should_preempt(pcb_t *r)  { (void)r; return false; }
static pcb_t *fcfs_pick(void)                { return q_pop(&fcfs_q); }
static void   fcfs_on_dispatch(pcb_t *p)     { (void)p; }
static void   fcfs_on_tick(pcb_t *r)         { (void)r; }
static int    fcfs_ready_count(void)         { return q_size(&fcfs_q); }

static scheduler_t sched_fcfs = {
    "fcfs", fcfs_init, fcfs_enqueue, fcfs_should_preempt, fcfs_pick,
    fcfs_on_dispatch, fcfs_on_tick, fcfs_ready_count
};

/* RR */
static queue_t rr_q;

static void   rr_init(void)            { q_init(&rr_q); }
static void   rr_enqueue(pcb_t *p)     { q_push(&rr_q, p); }
static pcb_t *rr_pick(void)            { return q_pop(&rr_q); }
static void   rr_on_dispatch(pcb_t *p) { p->quantum_used = 0; }
static void   rr_on_tick(pcb_t *r)     { r->quantum_used++; }
static int    rr_ready_count(void)     { return q_size(&rr_q); }

/* Preempt when the quantum is used up and someone else is ready. If nobody
 * is ready, the process keeps the CPU and starts a new quantum, so a process
 * that arrives later waits until that quantum is over. */
static bool rr_should_preempt(pcb_t *r)
{
    if (r->quantum_used < cfg.quantum) return false;
    r->quantum_used = 0;
    return !q_empty(&rr_q);
}

static scheduler_t sched_rr = {
    "rr", rr_init, rr_enqueue, rr_should_preempt, rr_pick,
    rr_on_dispatch, rr_on_tick, rr_ready_count
};

/* SJF
 * TODO: shortest job first, non-preemptive.
 *   pick(): q_pop_min(&q, key) with key = proc_burst_remaining
 *   should_preempt(): always false
 * For your report: ossim knows the exact length of the next burst, a real
 * OS does not. How would you add exponential averaging (Sec. 5.3.2)? */
static queue_t sjf_q;

static void   sjf_init(void)               { q_init(&sjf_q); }
static void   sjf_enqueue(pcb_t *p)        { q_push(&sjf_q, p); }
static bool   sjf_should_preempt(pcb_t *r) { (void)r; return false; }
static pcb_t  *sjf_pick(void)              { return q_pop_min(&sjf_q, proc_burst_remaining); }
static void   sjf_on_dispatch(pcb_t *p)    { (void)p; }
static void   sjf_on_tick(pcb_t *r)        { (void)r; }
static int    sjf_ready_count(void)        { return q_size(&sjf_q); }

static scheduler_t sched_sjf = {
    "sjf", sjf_init, sjf_enqueue, sjf_should_preempt, sjf_pick,
    sjf_on_dispatch, sjf_on_tick, sjf_ready_count
};

/* SRTF
 * TODO: SJF with preemption. should_preempt(r) is true when a READY process
 * has less left of its burst than r, that is q_min_key(&q, key) < key(r). */
static queue_t srtf_q;

static void   srtf_init(void)               { q_init(&srtf_q); }
static void   srtf_enqueue(pcb_t *p)        { q_push(&srtf_q, p); }
static pcb_t *srtf_pick(void)               { return q_pop_min(&srtf_q, proc_burst_remaining); }
static void   srtf_on_dispatch(pcb_t *p)    { (void)p; }
static void   srtf_on_tick(pcb_t *r)        { (void)r; }
static int    srtf_ready_count(void)        { return q_size(&srtf_q); }

static bool srtf_should_preempt(pcb_t *r)
{
    if (proc_burst_remaining(r) > q_min_key(&srtf_q, proc_burst_remaining)) return true;
    return false;
}

static scheduler_t sched_srtf = {
    "srtf", srtf_init, srtf_enqueue, srtf_should_preempt, srtf_pick,
    srtf_on_dispatch, srtf_on_tick, srtf_ready_count
};

/* PRIO
 * TODO: priority with preemption (smaller number = higher priority), and RR
 * between processes of the same priority. Low priorities can starve; aging
 * is a stretch goal. */
static queue_t prio_q;
static int get_priority(const pcb_t *p) {return p->priority;}

static void   prio_init(void)            { q_init(&prio_q); }
static void   prio_enqueue(pcb_t *p)     { q_push(&prio_q, p); }
static pcb_t *prio_pick(void)            { return q_pop_min(&prio_q, get_priority); }
static void   prio_on_dispatch(pcb_t *p) { (void)p; }
static void   prio_on_tick(pcb_t *r)     { (void)r; }
static int    prio_ready_count(void)     { return q_size(&prio_q); }

static bool prio_should_preempt(pcb_t *r)
{
    if (r->priority > q_min_key(&prio_q, get_priority)) return true;
    return false;
}

static scheduler_t sched_prio = {
    "prio", prio_init, prio_enqueue, prio_should_preempt, prio_pick,
    prio_on_dispatch, prio_on_tick, prio_ready_count
};

/* MLFQ
 * TODO: multilevel feedback queue with 3 levels, RR with quantum q, 2q and
 * 4q. A process that uses its whole quantum goes down one level. A process
 * that blocks before the quantum ends stays where it is. Every 20 ticks,
 * move everyone back to level 0 so nobody starves. Use p->level. */
static queue_t mlfq_q;
static const int MAX_LEVEL = 3;
static int mlfq_tick = 0;
static int get_level(const pcb_t *p) { return p->level; }

static void   mlfq_init(void)            { q_init(&mlfq_q); mlfq_tick = 0; }
static void   mlfq_enqueue(pcb_t *p)     { q_push(&mlfq_q, p); }
static pcb_t *mlfq_pick(void)            { return q_pop_min(&mlfq_q, get_level); }
static void   mlfq_on_dispatch(pcb_t *p) { p->quantum_used = 0; }

static void mlfq_on_tick(pcb_t *r) {
    r->quantum_used++;
    mlfq_tick++;
    if (mlfq_tick >= 20) {
        for (int i = 0; i < q_size(&mlfq_q); i++) {
            pcb_t *temp = q_pop(&mlfq_q);
            temp->level = 0;
            q_push(&mlfq_q, temp);
        }
        if (r) r->level = 0;
        mlfq_tick = 0;
    }
}

static int mlfq_ready_count(void) { return q_size(&mlfq_q); }

static bool mlfq_should_preempt(pcb_t *r)
{
    int q_limit = cfg.quantum * (1 << r->level);
    if (r->quantum_used >= q_limit) {
        r->quantum_used = 0;
        if (r->level < MAX_LEVEL - 1) r->level++;
        return !q_empty(&mlfq_q);
    }

    return q_min_key(&mlfq_q, get_level) < r->level;
}

static scheduler_t sched_mlfq = {
    "mlfq", mlfq_init, mlfq_enqueue, mlfq_should_preempt, mlfq_pick,
    mlfq_on_dispatch, mlfq_on_tick, mlfq_ready_count
};

static scheduler_t *all[] = {
    &sched_fcfs, &sched_rr, &sched_sjf, &sched_srtf, &sched_prio, &sched_mlfq
};

void sched_select(const char *name)
{
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        if (strcmp(all[i]->name, name) == 0) {
            sched = all[i];
            sched->init();
            return;
        }
    }
    die("unknown scheduler '%s' (fcfs, rr, sjf, srtf, prio, mlfq)", name);
}
