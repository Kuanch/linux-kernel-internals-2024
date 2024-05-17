# Review with code: Linux Scheduler 閱讀筆記 (1)

本文意在透過閱讀 Linux 核心程式碼理解 CPU Scheduler 機制，並配合 [Linux 核心設計: Scheduler 系列](https://hackmd.io/@RinHizakura/S1opp7-mP) 補充以及記錄我個人的疑問和心得。

以下程式碼為 Linux 核心 `v6.8-rc7` 版本，本篇專注於 EEVDF。

由於 EEVDF 中存在許多基於 CFS 的機制，如 nice level 以及 vruntime，本文在說明這些機制時將以 CFS 角度說明，若不了解 CFS 以及 EEVDF 機制，建議先行閱讀 [Demystifying the Linux CPU Scheduler 閱讀筆記 2024](https://hackmd.io/@Kuanch/linux2024-collection/https%3A%2F%2Fhackmd.io%2F%40Kuanch%2Flinux-kernel-scheduler-notes2)。

後續發現，由於 EEVDF 近日才被引進，許多改進仍在進行中，建議讀者在有一定理解後可以參考 [Yiwei Lin](https://hackmd.io/@RinHizakura/SyG4t5u1a#Linux-%E6%A0%B8%E5%BF%83%E8%A8%AD%E8%A8%88-Scheduler5-EEVDF-Scheduler) 以 Patch 追蹤的方式來跟進和驗證 EEVDF 之實作。

## `pick_next_entity()`

CFS 是為了做到比 $O(1)$ 更細緻的排程所改進的排程器，引入 nice 的概念，能夠盡可能「公平」的讓各行程獲得 CPU，即便是低優先權的任務也能夠分配到一定比例的 CPU 使用時間。

具體如何做？我們先看定義在 `kernel/sched/core.c` 中的 `schedule()`
```c=6807
asmlinkage __visible void __sched schedule(void)
{
	struct task_struct *tsk = current;

#ifdef CONFIG_RT_MUTEXES
	lockdep_assert(!tsk->sched_rt_mutex);
#endif

	if (!task_is_running(tsk))
		sched_submit_work(tsk);
	__schedule_loop(SM_NONE);
	sched_update_worker(tsk);
}
EXPORT_SYMBOL(schedule);
```
以此我們可以紀錄其 call hierachy 直至 EEVDF 取用下一任務的函式 `pick_next_entity()`：(pp1)
```ｃ
- schedule()                                               // in core.c at line 6807
  - __schedule_loop()                                      // in core.c at line 6798
    - __schedule()                                         // in core.c at line 6607
      - context_switch()                                   // in core.c at line 5344
      - pick_next_task()                                   // in core.c at line 6543
        - __pick_next_task()                               // in core.c at line 6007
          - pick_next_task_fair()                          // in fair.c at line 8380
            - update_curr()                                // in fair.c at line 8414
                - update_deadline()                        // in fair.c at line  978
                - update_min_vruntime()                    // in fair.c at line  762
            - pick_next_entity()                           // in fair.c at line 5453
```
以上函式均在 CFS 中扮演重要腳色，我們先由 `pick_next_entity()` 開始分析。

 `pick_next_entity()` 實作如下：
```c=5446
/*
 * Pick the next process, keeping these things in mind, in this order:
 * 1) keep things fair between processes/task groups
 * 2) pick the "next" process, since someone really wants that to run
 * 3) pick the "last" process, for cache locality
 * 4) do not run the "skip" process, if something else is available
 */
static struct sched_entity *
pick_next_entity(struct cfs_rq *cfs_rq)
{
	/*
	 * Enabling NEXT_BUDDY will affect latency but not fairness.
	 */
	if (sched_feat(NEXT_BUDDY) &&
	    cfs_rq->next && entity_eligible(cfs_rq, cfs_rq->next))
		return cfs_rq->next;

	return pick_eevdf(cfs_rq);
}
```
首先我們可以看到 `NEXT_BUDDY`，在 "*Demystifying the Linux CPU Scheduler*" 提到
> **NEXT BUDDY** is a feature that works after WAKEUP_PREEMPTION failed to make the waking task (wakee) preempt the currently running task. This feature gives the scheduler a preference to preempt the running task with the wakee.
>
> **NEXT_BUDDY** presumes the wakee is going to consume the data produce by the waker, this feature allows us to wakes the producer/consumer pair in a consecutive manner, making them to run before any other task, increasing cache locality. Without this feature, other task may interleave the producer/consumer pair, potentially result in cache thrashing.

`cfs_rq->next` 是被目前任務選定的下一個任務，它們可能是 producer/consumer 的關係，能夠提升 cache locality；註釋的第二點 " **2) pick the "next" process, since someone really wants that to run**" 亦驗證我們的想法。

:::danger
>[name=Kuanch]
>
>`cfs_rq->next` `cfs_rq->last` `cfs_rq->skip` 實作上怎麼被挑選待追蹤；注意 `last` 和 `skip` 在 `v6.8-rc7` 已不存在，在 `v5.19-rc7` 仍可見；原因待討論，在 [Main Patches](https://hackmd.io/@RinHizakura/SyG4t5u1a#Main-patches) 亦有討論。
:::
##### `vruntime_eligible`
這是一個在 EEVDF 中十分重要的函式，簡而言之，它判斷一個 `sched_entity` 是不是夠資格使用 CPU，換句話說，其 `vruntime` 需要足夠小才能夠使用 CPU (小於 average vruntime 以及 小於 deadline)，也是 EEVDF 的主要機制，若 $lag>0$ 則被認為是 eligible。

**注意，以下程式碼事實上即是 `avg_vruntime() > se->vruntime` ，但因為定點數計算將導致不精確的結果，故採用此法。**

```c
static int vruntime_eligible(struct cfs_rq *cfs_rq, u64 vruntime)
{
	struct sched_entity *curr = cfs_rq->curr;
	s64 avg = cfs_rq->avg_vruntime;
	long load = cfs_rq->avg_load;

	if (curr && curr->on_rq) {
		unsigned long weight = scale_load_down(curr->load.weight);

		avg += entity_key(cfs_rq, curr) * weight;
		load += weight;
	}

	return avg >= (s64)(vruntime - cfs_rq->min_vruntime) * load;
}
```

:::spoiler (待驗證內容摺疊)
想像一個情景，由於 RB tree 的平衡是最差狀況下是 $O(log N)$，當 leftmost node (with smallest `vruntime`) 不斷被取出，平衡可能都沒有完成，但 `avg_vruntime` 和 `avg_load` 的更新是 $O(1)$，我們可以透過 `vruntime_eligible()` 判斷該 `sched_entity` 是否有足夠小的 `vruntime`，或是說 $lag > 0$：
:::

$$
log_i = S - s_i = w_i \times (V - v_i)
$$

$log_i$ 為第 i 個任務的應執行時間與實際執行時間的差值
$S$ 為在該隊列下的平均執行時間 (第 i 個任務的應執行時間)
$s_i$ 為實際執行時間
$v$ 為 `cfs_rq->min_vruntime`

由於 $log_i>=0 \implies V >= v_i$ ，而 $V = \frac{\sum{(v_i - v) * w_i}}{\sum{w_i}} + v$，故可得

$$
task_i \text{ is eligible if } \sum{(v_i - v) * w_i} \ge (v_i - v) * \sum{w_i}
$$

即 `avg >= (s64)(vruntime - cfs_rq->min_vruntime) * load;`。

##### `cfs_rq->next`
而我們如何得到 `cfs_rq->next` 呢？透過 `check_preempt_wakeup_fair()` 呼叫 `set_next_buddy()`；

---
#### `vruntime`

什麼是 virtual runtime？是一根據任務權重計算出來的時間，越高權重之任務，其 `vruntime` 愈小，反之愈大。

為什麼要使用 `vruntime`？為了解決高權重任務容易占用大量 CPU 時間，導致低權重任務幾乎無法使用 CPU 的狀況，故設計 `vruntime`，當任務使用過 CPU 後，其 `vruntime` 將會被上調，越小的 `vruntime` 表示其越不需要 (不急迫、不具資格) 使用 CPU；理想的狀態下，我們希望所有任務的 `vruntime` 相等，**也就是所有任務對於 CPU 的需求都相同**。

> Practically, it is the actual execution time normalized by the number of runnable tasks. CFS uses it to determine the next task to put on the core. The target is to maintain the virtual runtime of every task close to each other (ideally, to the same value), therefore CFS always picks the task with the smallest runtime.

任務使用 CPU 後如何增加 `vruntime`？在 `update_curr()` 當中的 line 1168 即用於增加任務的 `vruntime`，`update_curr_se()` 也很值得關注，它回傳的 `delta_exec` 來自於 `rq->clock_task`，即該任務目前為止消耗的 CPU 時間。

:::info
:::spoiler What is `rq->clock_task`?

`vruntime` 本質是什麼？就是加權後的執行時間，那我們總歸要將它和真實執行時間連結起來；往上尋找 `rq->clock_task` 會發現它在 `core.c` 中被 `update_rq_clock()` 更新 (line 742)，其更新值來自 `sched_clock()` 定義在 `kernel/sched/clock.c`，根據 "*Demystifying the Linux CPU Scheduler*"：

>**sched_clock()** An important function is sched_clock(): it returns the system’s uptime in nanoseconds. An architecture may provide an implementation, but if it is not provided, the system will use the jiffies counter to calculate the system’s uptime.

:::

```c=1153
/*
 * Update the current task's runtime statistics.
 */
static void update_curr(struct cfs_rq *cfs_rq)
{
	struct sched_entity *curr = cfs_rq->curr;
	s64 delta_exec;

	if (unlikely(!curr))
		return;

	delta_exec = update_curr_se(rq_of(cfs_rq), curr);
	if (unlikely(delta_exec <= 0))
		return;

	curr->vruntime += calc_delta_fair(delta_exec, curr);
	update_deadline(cfs_rq, curr);
	update_min_vruntime(cfs_rq);

	if (entity_is_task(curr))
		update_curr_task(task_of(curr), delta_exec);

	account_cfs_rq_runtime(cfs_rq, delta_exec);
}
```

#### Weight Function (pp2)
由 `calc_delta_fair()` 往下追尋，我們會發現 `__calc_delta()` 進行了 `delta_exec` 的加權，考慮加權公式

$$
vruntime = delta\_exec \times \frac{weight\_nice\_0}{task\_weight}
$$

我們可以看到 `if (unlikely(se->load.weight != NICE_0_LOAD))`，因為若 NICE 值為 0 則不須加權 ($vruntime = delta\_exec$ )；此處有趣的是我們透過 `fact = mul_u32_u32(fact, lw->inv_weight);` (計算 $\frac{weight\_nice\_0}{task\_weight}$) 和 `ret = mul_u32_u32(al, mul) >> shift;`  (計算 $vruntime$) 避免使用除法，考慮 $\frac{x}{3}$，即是：

$$
\frac{x}{3} = \frac{x}{1024} \times \frac{1024}{3} \approx (341x) >> 10
$$

另外，在這之前，如 `sched_fork()` 時，任務就已經透過 `set_load_weight()` 根據其優先級設定了 `struct load_weight *load`：
```c=1345
// at set_load_weight()
	if (update_load && p->sched_class == &fair_sched_class) {
		reweight_task(p, prio);
	} else {
		load->weight = scale_load(sched_prio_to_weight[prio]);
		load->inv_weight = sched_prio_to_wmult[prio];
	}
```
可以注意到 `sched_prio_to_wmult` 即是 `sched_prio_to_weight` 之倒數，如 $\frac{2^{32}}{88761} = 48388$ 或 $\space \frac{2^{32}}{15} = 286331153$，所以需要透過 `mul_u64_u32_shr()` 右移。

:::info
:::spoiler Enabling NEXT_BUDDY will affect latency but not fairness (ChatGPT)
為什麼上一個任務決定下一個任務不會影響到 fairness 呢？

The fairness here is about the long-term proportion of CPU time that each task receives. Even if NEXT_BUDDY causes a task to be scheduled sooner than others in the short term, over the long term, the CFS will ensure that each task gets its fair share of CPU time.
:::

### `update_deadline()`
EEVDF 使用 `se->deadline` 排序，故如何更新亦十分重要，`update_deadline()` 存在於 `update_curr()` 中，展示如何更新 `se->deadline`。

```c
/*
 * XXX: strictly: vd_i += N*r_i/w_i such that: vd_i > ve_i
 * this is probably good enough.
 */
static void update_deadline(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if ((s64)(se->vruntime - se->deadline) < 0)
		return;

	/*
	 * For EEVDF the virtual time slope is determined by w_i (iow.
	 * nice) while the request time r_i is determined by
	 * sysctl_sched_base_slice.
	 */
	se->slice = sysctl_sched_base_slice;

	/*
	 * EEVDF: vd_i = ve_i + r_i / w_i
	 */
	se->deadline = se->vruntime + calc_delta_fair(se->slice, se);

	/*
	 * The task has consumed its request, reschedule.
	 */
	if (cfs_rq->nr_running > 1) {
		resched_curr(rq_of(cfs_rq));
		clear_buddies(cfs_rq, se);
	}
}
```
透過 `se->deadline = se->vruntime + calc_delta_fair(se->slice, se);` 理解 deadline 事實上是：
$$
    deadline_i = vruntime_i\ +\ slice\times \frac{w_i}{\sum_iw_i}
$$
其中 $slice$ 是 `sysctl_sched_min_granularity` 預設為 0.75ms。

向下追尋可以發現，事實上 `se->deadline` 使用的權重與 `se->vruntime` 相同，仍是來自於相同的 nice level，而非 latency nice，你可在 [[RFC][PATCH 12/15] sched: Introduce latency-nice as a per-task attribute](https://lore.kernel.org/lkml/20230531124604.410388887@infradead.org/) 看到 latency nice 的相關開發，但最後未被加入；而在 [[RFC][PATCH 00/10] sched/fair: Complete EEVDF](https://lore.kernel.org/lkml/20240405102754.435410987@infradead.org/T/#t) 中被接續改進。

### `sched_fork()`

另一個主要的疑惑是 `task_struct *p` 是何時被指令優先級、任務類別以及其他參數的，我們能夠在 `kernel/sched/core.c` 找到 `sched_fork()` 函式，其主要在 `kernel/fork.c` 中被 `copy_process()` 呼叫，其又被 `kernel_clone()` 呼叫，兩者都是創造新行程的重要函式。

```c=4745
/*
 * fork()/clone()-time setup:
 */
int sched_fork(unsigned long clone_flags, struct task_struct *p)
{
	__sched_fork(clone_flags, p);
	/*
	 * We mark the process as NEW here. This guarantees that
	 * nobody will actually run it, and a signal or other external
	 * event cannot wake it up and insert it on the runqueue either.
	 */
	p->__state = TASK_NEW;

	/*
	 * Make sure we do not leak PI boosting priority to the child.
	 */
	p->prio = current->normal_prio;

	uclamp_fork(p);

	/*
	 * Revert to default priority/policy on fork if requested.
	 */
	if (unlikely(p->sched_reset_on_fork)) {
		if (task_has_dl_policy(p) || task_has_rt_policy(p)) {
			p->policy = SCHED_NORMAL;
			p->static_prio = NICE_TO_PRIO(0);
			p->rt_priority = 0;
		} else if (PRIO_TO_NICE(p->static_prio) < 0)
			p->static_prio = NICE_TO_PRIO(0);

		p->prio = p->normal_prio = p->static_prio;
		set_load_weight(p, false);

		/*
		 * We don't need the reset flag anymore after the fork. It has
		 * fulfilled its duty:
		 */
		p->sched_reset_on_fork = 0;
	}

	if (dl_prio(p->prio))
		return -EAGAIN;
	else if (rt_prio(p->prio))
		p->sched_class = &rt_sched_class;
	else
		p->sched_class = &fair_sched_class;

	init_entity_runnable_average(&p->se);


#ifdef CONFIG_SCHED_INFO
	if (likely(sched_info_on()))
		memset(&p->sched_info, 0, sizeof(p->sched_info));
#endif
#if defined(CONFIG_SMP)
	p->on_cpu = 0;
#endif
	init_task_preempt_count(p);
#ifdef CONFIG_SMP
	plist_node_init(&p->pushable_tasks, MAX_PRIO);
	RB_CLEAR_NODE(&p->pushable_dl_tasks);
#endif
	return 0;
}
```

首先在 `__sched_fork()` 明顯是一個初始化操作；其後我們也可以學習到 `sched_class` 的不同，包含 `rt_sched_class` `fair_sched_class` `idle_sched_class` `dl_sched_class` 等。


### sched_class

> scheduler 的定義透過 DEFINE_SCHED_CLASS 這個 macro 完成，後者將 scheduler 根據 linker script 擺放到編譯出的 linux image 的對應位置。設計上，每一個執行單元在建立之後，都要選擇一種調度策略，而每一種調度策略會對應一個 sched_class。因此，除了執行單元在 scheduler 中有優先級，不同的 scheduler 之間也存在先後之分。

透過 `sched_class` ，我們可以提供一種類似的 OOP 實作方法，如 `p->sched_class->enqueue_task` `p->sched_class->dequeue_task` `p->sched_class->update_curr` 等等，使用同一介面使用不同 `sched_class` 的操作。



:::info
:::spoiler Why `sched_class` and Scheduling policies is seperate ?
首先，`sched_class` 是一種類似的 OOP 實作方法，重點在提供 OOP 介面；而 Scheduling policies 用途之一是用於設定 `weight` 的大小

```c
// defined at set_load_weight()
	/*
	 * SCHED_IDLE tasks get minimal weight:
	 */
	if (task_has_idle_policy(p)) {
		load->weight = scale_load(WEIGHT_IDLEPRIO);
		load->inv_weight = WMULT_IDLEPRIO;
		return;
	}
```
`SCHED_IDLE` 的優先級甚至可以小於最小的 nice value。

>   \- SCHED_IDLE: This is even weaker than nice 19, but its not a true
    idle timer scheduler in order to avoid to get into priority
    inversion problems which would deadlock the machine.



:::


### CFS operations

#### Per-Entity Load Tracking (PELT) (pp3)
由於多處理器多核心架構被現代運算廣泛使用，平行程式也開始普及，但一個重要的問題隨即浮現：是否會有些處理器負載特別高，而有些負載特別低的不平衡狀況呢？顯然是會的，故我們需要一種方法來衡量每個處理器的負載，並動態調整派發任務；Per-Entity Load Tracking (PELT) 的概念因此被廣泛應用，**如果我們有能力評估每個任務對核心的負載，就能夠評估個別處理器的負載了**。

考慮如果使用例如權重 (load.weight)，CPU-bound 和 I/O-bound 很可能有相同的權重，但後者大多數時間在等待；PELT 是持續監測任務使用 CPU 的時間，唯有這樣才能夠貼合我們的目的：負載平衡 (Load Balance)。

>To accurately measure load, it is necessary to monitor the amount of time a task utilizes the CPU. Thus, a task’s load becomes a combination of its weight and average CPU utilization, and the core’s load is the sum of its tasks’ loads.

如何“監測任務使用 CPU 的時間” ？首先找到儲存負載的變數，其存在於 `sched_entity` 中：
```c
struct sched_entity {
// ...
#ifdef CONFIG_SMP
	/*
	 * Per entity load average tracking.
	 *
	 * Put into separate cache line so it does not
	 * collide with read-mostly values above.
	 */
	struct sched_avg		avg;
#endif
};
```
進一步觀察 `sched_avg`
```c
struct sched_avg {
	u64				last_update_time;
	u64				load_sum;
	u64				runnable_sum;
	u32				util_sum;
	u32				period_contrib;
	unsigned long			load_avg;
	unsigned long			runnable_avg;
	unsigned long			util_avg;
	unsigned int			util_est;
} ____cacheline_aligned;
```

:::info
:::spoiler Meaning of Load, Runnable and Util


:::

`enqueue_task_fair()` 用於加入任務到等待執行的佇列中，並且重新評估該任務的各項數值，譬如調整頻率；首先出現的兩個函式 `util_est_enqueue` 和 `cpufreq_update_util()` 即是：
#### `util_est_enqueue()`
```c
static inline void util_est_enqueue(struct cfs_rq *cfs_rq,
				    struct task_struct *p)
{
	unsigned int enqueued;

	if (!sched_feat(UTIL_EST))
		return;

	/* Update root cfs_rq's estimated utilization */
	enqueued  = cfs_rq->avg.util_est;
	enqueued += _task_util_est(p);
	WRITE_ONCE(cfs_rq->avg.util_est, enqueued);

	trace_sched_util_est_cfs_tp(cfs_rq);
}
```
可以看到，當任務每次被加入到 `cfs_rq` 之前 (`h_nr_running++`)，必須先更新 `cfs_rq->avg.util_est`；此外，`p->se.avg.util_est` 則是在 `dequeue_task_fair()` 時被 `util_est_update()` 更新。

`p->se.avg.util_est` 有四種更新分支被定義在 `util_est_update()`：
1. 大於 ewma (使用率上升)
    `p->se.avg.util_est = p->se.avg.util_avg`
3. 小於 ewma 但在誤差內 (使用率不變)
    `p->se.avg.util_est = p->se.avg.util_est`

5. 任務得到超過 CPU capacity 的使用時間
    `p->se.avg.util_est = p->se.avg.util_est`
7. 任務並沒有得到理論上的使用時間
    `p->se.avg.util_est = p->se.avg.util_avg`
9. 小於 ewma
    `p->se.avg.util_est = 0.25 * (p->se.avg.util_est) + 0.75 * (p->se.avg.util_avg)`

此處的命名很容易混淆 `ewma = READ_ONCE(p->se.avg.util_est);` 和 `dequeued = task_util(p);`，前者應該是指 `p->se.avg.util_est` 是過去資料的 EWMA，而後者為 `p->se.avg.util_avg` 才是該任務當前的使用率。

:::info
:::spoiler How is `se->avg. util_avg` updated?
`se.avg.util_avg` 
```c=4264
static inline void
update_tg_cfs_util(struct cfs_rq *cfs_rq, struct sched_entity *se, struct cfs_rq *gcfs_rq)
{
	long delta_sum, delta_avg = gcfs_rq->avg.util_avg - se->avg.util_avg;
	u32 new_sum, divider;

	/* Nothing to update */
	if (!delta_avg)
		return;

	/*
	 * cfs_rq->avg.period_contrib can be used for both cfs_rq and se.
	 * See ___update_load_avg() for details.
	 */
	divider = get_pelt_divider(&cfs_rq->avg);


	/* Set new sched_entity's utilization */
	se->avg.util_avg = gcfs_rq->avg.util_avg;
	new_sum = se->avg.util_avg * divider;
	delta_sum = (long)new_sum - (long)se->avg.util_sum;
	se->avg.util_sum = new_sum;

	/* Update parent cfs_rq utilization */
	add_positive(&cfs_rq->avg.util_avg, delta_avg);
	add_positive(&cfs_rq->avg.util_sum, delta_sum);

	/* See update_cfs_rq_load_avg() */
	cfs_rq->avg.util_sum = max_t(u32, cfs_rq->avg.util_sum,
					  cfs_rq->avg.util_avg * PELT_MIN_DIVIDER);
}
```
:::

#### `cpufreq_update_util()`
```c
/**
 * cpufreq_update_util - Take a note about CPU utilization changes.
 * @rq: Runqueue to carry out the update for.
 * @flags: Update reason flags.
 *
 * This function is called by the scheduler on the CPU whose utilization is
 * being updated.
 *
 * It can only be called from RCU-sched read-side critical sections.
 *
 * The way cpufreq is currently arranged requires it to evaluate the CPU
 * performance state (frequency/voltage) on a regular basis to prevent it from
 * being stuck in a completely inadequate performance level for too long.
 * That is not guaranteed to happen if the updates are only triggered from CFS
 * and DL, though, because they may not be coming in if only RT tasks are
 * active all the time (or there are RT tasks only).
 *
 * As a workaround for that issue, this function is called periodically by the
 * RT sched class to trigger extra cpufreq updates to prevent it from stalling,
 * but that really is a band-aid.  Going forward it should be replaced with
 * solutions targeted more specifically at RT tasks.
 */
static inline void cpufreq_update_util(struct rq *rq, unsigned int flags)
{
	struct update_util_data *data;

	data = rcu_dereference_sched(*per_cpu_ptr(&cpufreq_update_util_data,
						  cpu_of(rq)));
	if (data)
		data->func(data, rq_clock(rq), flags);
}
#else
static inline void cpufreq_update_util(struct rq *rq, unsigned int flags) {}
#endif /* CONFIG_CPU_FREQ */
```


#### `cpufreq`


## Earliest Eligible Virtual Deadline First (EEVDF) (pp4)


EEVDF 的重要函式之一便是接續 `pick_next_entity()` 的 `pick_eevdf()`：

```c=878
static struct sched_entity *pick_eevdf(struct cfs_rq *cfs_rq)
{
	struct rb_node *node = cfs_rq->tasks_timeline.rb_root.rb_node;
	struct sched_entity *se = __pick_first_entity(cfs_rq);
	struct sched_entity *curr = cfs_rq->curr;
	struct sched_entity *best = NULL;

	/*
	 * We can safely skip eligibility check if there is only one entity
	 * in this cfs_rq, saving some cycles.
	 */
	if (cfs_rq->nr_running == 1)
		return curr && curr->on_rq ? curr : se;

	if (curr && (!curr->on_rq || !entity_eligible(cfs_rq, curr)))
		curr = NULL;

	/*
	 * Once selected, run a task until it either becomes non-eligible or
	 * until it gets a new slice. See the HACK in set_next_entity().
	 */
	if (sched_feat(RUN_TO_PARITY) && curr && curr->vlag == curr->deadline)
		return curr;

	/* Pick the leftmost entity if it's eligible */
	if (se && entity_eligible(cfs_rq, se)) {
		best = se;
		goto found;
	}

	/* Heap search for the EEVD entity */
	while (node) {
		struct rb_node *left = node->rb_left;

		/*
		 * Eligible entities in left subtree are always better
		 * choices, since they have earlier deadlines.
		 */
		if (left && vruntime_eligible(cfs_rq,
					__node_2_se(left)->min_vruntime)) {
			node = left;
			continue;
		}

		se = __node_2_se(node);

		/*
		 * The left subtree either is empty or has no eligible
		 * entity, so check the current node since it is the one
		 * with earliest deadline that might be eligible.
		 */
		if (entity_eligible(cfs_rq, se)) {
			best = se;
			break;
		}

		node = node->rb_right;
	}
found:
	if (!best || (curr && entity_before(curr, best)))
		best = curr;

	return best;
}
```
Line 899 - 906 便闡述了 EEVDF 的核心：
1. 讓 `curr` 使用 CPU 直到 non-eligible
2. 若有 leftmost entity 是 eligible 則選取它

