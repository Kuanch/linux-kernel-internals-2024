# 2024q1 Homework6 (integration)
contributed by < [Kuanch](https://github.com/Kuanch) >

## Linux 核心模組原理
### What is a Kernel Module?
參閱 "*The Linux Kernel Module Programming Guide*"，一個核心模組 (kernel module) 最重要的特色是能夠被動態載入、在不重新開機的狀況下擴展核心功能：
>A Linux kernel module is precisely defined as a code segment capable of dynamic loading and unloading within the kernel as needed. These modules enhance kernel capabilities without necessitating a system reboot.

更完備的理解是，在不影響當前核心運作的狀況下掛載新的模組、動態擴充核心；另外，如果每一次功能的新增都需要被加入到 kernel image，不可避免的會導致擁腫的核心、增加重新編譯核心以及重新佈署系統的成本。

另外，模組是運行在 kernel space 中的，故它具有調動 kernel mode 下各項功能的權力。

### Hello World Module
在我們分析 ksort 及 simrupt 之前，我認為有必要透過編寫一個最簡單的模組了解撰寫模組的必要部分，考慮以下程式碼：

```c
#include <linux/module.h> /* Needed by all modules */
#include <linux/printk.h> /* Needed for pr_info() */

int init_module(void)
{
    pr_info("Hello world 1.\n");

    /* A non 0 return means init_module failed; module can't be loaded. */
    return 0;
}

void cleanup_module(void)
{
    pr_info("Goodbye world 1.\n");
}

MODULE_LICENSE("GPL");
```
並考慮以下 `Makefile`
```c
obj-m += hello_mod.o

PWD := $(CURDIR)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```
經過編譯後得到 `hello_mod.ko`，可以透過 `modinfo hello_mod.ko` 得知其模組資訊，並透過 `sudo insmod hello-1.ko` 載入模組，在此之後 `dmesg` 可以看到訊息
```powershell
[765002.198127] Hello world 1.
```
若以 `sudo rmmod hello_mod.ko` 移除該模組後則會得到
```powershell
[765848.444403] Goodbye world 1.
```

### [ksort](https://github.com/sysprog21/ksort/tree/master) module
ksort 是一個 **character device**，可理解是個能夠循序存取檔案，或說是可存取的  "stream of bytes"，像是檔案一樣，故我們至少需要定義它的 `open`, `close`, `read`, `write`, `ioctl`, `mmap` 等 system calls，這些呼叫會透過 OS 轉送到裝置驅動：
>Text Console (/dev/console) and the Serial Ports (/dev/ttyS0)，都是 Streaming 結構。

與之相對的是 **block device** 和 **network device**，此處我們僅先討論前者；與 stream 的概念不同，block device 是 chunks(blocks)，更適合用在存取大型資料的時候：
>Usage: Character devices are often used for devices that **need to communicate small amounts of data with low latency**, while block devices are **used for storing files where the capacity and speed of reading/writing large blocks of data** are paramount.

`/dev/sda` for the first SCSI disk 和 `/dev/nvme0n1` for NVMe drives 是常見的 block device；此外通常並沒有 `read()`, `write()`，而是透過 block I/O layer 管理存取請求。

由 Hello World Module 一節可知，我們至少需要定義 `init_module()` 和 `cleanup_module()`，或者如 ksort 使用 `module_init()`，其為一定義於 `linux/module.h` 之巨集，這種能夠客制函式名的方式似乎也是較為常見的；此外，由於 ksort 為 character device，一些存取的 system calls 也必不可少，我們從最基礎的 `sort_read()` 開始理解。

我們知道透過需要提供 Linux Virtual File System 介面給後續使用，當呼叫 `read()` 的時候就會對應到 `sort_read()`，我們嘗試簡單修改 `user.c`：

```diff
-    size_t n_elements = 1000;
-    size_t size = n_elements * sizeof(int);
-    int *inbuf = malloc(size);
-    if (!inbuf)
-        goto error;
+    size_t n_elements;
+    size_t size;
+    int *inbuf;
+    if (argc > 1) {
+        n_elements = argc - 1;
+        size = n_elements * sizeof(int);
+        inbuf = malloc(size);
+        if (!inbuf)
+            goto error;
+        
+        for (size_t i = 0; i < n_elements; i++)
+            inbuf[i] = atoi(argv[i + 1]);
+
+    } else {
+        n_elements = 100;
+        size = n_elements * sizeof(int);
+        inbuf = malloc(size);
+        if (!inbuf)
+            goto error;
 
-    for (size_t i = 0; i < n_elements; i++)
-        inbuf[i] = rand() % n_elements;
+        for (size_t i = 0; i < n_elements; i++)
+            inbuf[i] = rand() % n_elements;
+    }

+    if (pass)
+        for (size_t i = 0; i < n_elements; i++) printf("%d ", inbuf[i]);
+    printf("\n");
```
現在我們可以自行輸入排序，且也確定，該 `ssize_t r_sz = read(fd, inbuf, size);` 即呼叫 `sort_read()`；注意，**此為系統呼叫而非單純的使用者層級的函式呼叫**，所以在某個時候，這個行程事實上轉換為 kernel mode，且要求 OS 協助存取某一個檔案或 I/O 裝置。

![image](https://hackmd.io/_uploads/BkaZIVMbA.png)
上圖幾乎詮釋了 `read()` 之後的流程，由於 `sort_read()` 明顯是一 blocking operation，因為排序的 `sort_main()` 需要時間，故呼叫的 user thread 很可能會 sleep，由另一個 thread 執行。

:::info
>[name=Kuanch]
>
>使用 `printk(KERN_INFO "sort_read() called by process %d\n", current->pid);` 判斷執行的 PID，雖然 PID 前後皆一致，但是否 sleep 卻無法判斷；嘗試透過 
```c
static void does_process_sleep(void)
{
    printk(KERN_INFO "Process %d is running\n", current->pid);
    char proc_path[1024];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/stat", current->pid);
    struct file *proc_stat = filp_open(proc_path, 0, O_RDONLY);

    size_t buf_size = 1024;
    char *buf = kmalloc(buf_size, GFP_KERNEL);
    ssize_t len = kernel_read(proc_stat, 0, buf, buf_size);
    if (len > 0 && buf != NULL)
        printk(KERN_INFO "Process %d: %s\n", current->pid, buf);
    else
        printk(KERN_INFO "Failed to read process %d\n", current->pid);

    filp_close(proc_stat, NULL);
}
```
>讀取 process state，但測試失敗，問題應該在如何正確讀取 `/proc/.../stat`。

後來思考這個做法較困難，若是要觀察該行程是否曾經休眠，應該有其他方法，如計時。
:::

### Time kernel module
在 lab 介紹了 ktime 的使用方法，透過在 `sort_read()` 頭尾插入 `kt = ktime_get();` 及 `kt = ktime_sub(ktime_get(), kt);`，再修改 `sort_write()`，我們即可透過在 `user.c` 呼叫 `long long time = write(fd, inbuf, size);` 得到核心模組的執行時間。

如同 lab 所說，此 Timer 機制用於 1. 安排「在某個時間點做某件事情」 或 2. 用來作為逾時的通知，但我們此處僅止於計算 time elapsed；那我們就使用 ktime 試試看是否能夠得到足以推論行程是否有休眠的證據。

| sort_read | sort_main | copy_from_user | copy_to_user |
| -------- | -------- | -------- | -------- |
| 16174 (ns)  | 15916 (ns)    | 126 (ns)     | 73 (ns)

在去除掉 `void *sort_buffer = kmalloc(size, GFP_KERNEL);` 及 `kfree(sort_buffer);` 等操作後發現，其餘 overhead 十分的少，僅有大約 60 ns，考慮到 ktime 本身和呼叫 `sort_write()` 等開銷，我們幾乎可以篤定該行程並未休眠。 (但仍覺得不是好方法，若能直接觀測 task_struct 本身狀態是很有幫助的，需要再研究)

## CMWQ
首先先釐清 simrupt 的呼叫邏輯，大致同 [simrupt 流程圖](https://hackmd.io/@sysprog/linux2024-integration/%2F%40sysprog%2Flinux2024-integration-c#simrupt-%E6%B5%81%E7%A8%8B%E5%9C%96)，但 `timer_setup(&timer, timer_handler, 0);` 後仍需要透過 `mod_timer()` 或 `add_timer()` 設定 `timer` 過期時間，才能夠觸發 `timer_hanlder()`；事實上，`simrupt_open()` 先被呼叫，觸發了 `mod_timer(&timer, jiffies + msecs_to_jiffies(delay));`，才有後續的 `timer_handler()` 運作。

當我們執行 `cat /dev/simrupt` 時，便觸發 `.read` 也就是 `simrupt_read()`，而當我們 ctrl + C 退出 `cat` 時，`simrupt_release()` 被呼叫。

雖然我們已知清晰的呼叫邏輯，但仍未理解為什麼如此實作；我們需要將各個部件分開說明。

### `process_data()` 與 `simrupt_tasklet_func()`
實際上的工作單位 `work` 是 `simrupt_work_func()`，也就是實際被執行的部分，`simrupt_tasklet` 即 `simrupt_tasklet_func()` 作為管理並運作 `work` 的函式，也是實際被 `tasklet_schedule()` 排程的單位；可以注意到 `simrupt_tasklet_func()` 的主體事實上是：
```c
queue_work(simrupt_workqueue, &work);
```
這是 Linux 核心中 interrupt handle 的實作，又可明顯地分出 **top half** 及 **bottom half** 或稱 First-Level Interrupt Handler (FLIH) 及 the Second-Level Interrupt Handlers (SLIH)，稱作 Divided handler。

#### top half and bottom half
interrupt 通常與**硬體**相關，相較軟體，硬體的速度極快 (by cycles)，故與硬體互動時，效率十分重要；將 interrupt handle 分成兩部分的作用在於降低回應延遲、避免 nested interrpt 以及享受排程機制帶來的效率與便利性，根據  "*Linux Device Drivers, Chapter 10. Interrupt Handling*"：
>One of the main problems with interrupt handling is how to perform lengthy tasks within a handler. Often a substantial amount of workmust be done in response to a device interrupt, but interrupt handlers need to finish up quickly and not keep interrupts blocked for long. These two needs (work and speed) conflict with each other, leaving the driver writer in a bit of a bind.
>
>Linux (along with many other systems) resolves this problem by splitting the interrupt handler into two halves. The so-called top half is the routine that actually responds to the interrupt—the one you register with request_irq. The bottom half is a routine that is scheduled by the top half to be executed later, at a safer time.

:::info
:::spoiler What will happen during the top half of interruption handling?
一般而言應包含
1. (minimal) context saving
2. ask interrupt descriptor table table using interrupt vector
3. Jumps to the address of the ISR designated for that vector.
4. acknowledging the interrupt
5. disable (other) interruptions (to prevent nested irq)
6. schedule bottom half task

特別注意的事情是，此處的 context saving 並非我們在兩個行程交換時所進行的 context switch，因為要快速回應中斷；我猜測由於中斷無法被預期，且通常是被不同的行程執行，如果進行完整的 context switch 將會大幅影響 top half 的效率，故僅儲存必要的 register 等資訊。

>[name=Kuanch] **(minimal) context saving** 並不是一個正式的詞彙，但稱 "context switch" 可能會和 scheduling 中的混淆，暫時找不到相關資料稱呼該過程。
:::

**top half (process_data)**
需要快速的回應發起中斷的硬體，而且必然伴隨 disable interrupt 避免 nested interrupt；依據這一條件，`process_data()` 顯然是 top half：
```c
    local_irq_disable();

    tv_start = ktime_get();
    process_data();
    tv_end = ktime_get();

    nsecs = (s64) ktime_to_ns(ktime_sub(tv_end, tv_start));

    pr_info("simrupt: [CPU#%d] %s in_irq: %llu usec\n", smp_processor_id(),
            __func__, (unsigned long long) nsecs >> 10);
    mod_timer(&timer, jiffies + msecs_to_jiffies(delay));

    local_irq_enable();
```
可以見到前後透過 `local_irq_disable()` 和 `local_irq_enable()` 避免 nested interrupt，且 `process_data()` 內亦透過 `tasklet_schedule(&simrupt_tasklet);` 排程實際任務；且 `fast_buf_put(update_simrupt_data());` 亦模擬了從發起中斷的硬體取得資料的動作，如從網卡取得隨著時間不斷增加的封包編號（Packet Identifier）。

**bottom half (simrupt_tasklet_func)**
被排程的 `simrupt_tasklet` 就可認為是 bottom half 了，故我們也可觀察到，`simrupt_work_func` 是發生在其他 CPU 上的，即是因為 bottom half 是在排程在 `simrupt_workqueue` 之後被 kernel thread 取出執行的。
```powershell
[1195122.950825] simrupt: simrupt_open simrupt_open
[1195122.950829] openm current cnt: 1
[1195122.950833] simrupt: simrupt_read(0000000026af7165, 131072, 0)
[1195123.052356] simrupt: [CPU#3] enter timer_handler
[1195123.052378] simrupt: [CPU#3] produce data
[1195123.052383] simrupt: [CPU#3] scheduling tasklet
[1195123.052387] simrupt: [CPU#3] timer_handler in_irq: 7 usec
[1195123.052408] simrupt: [CPU#3] simrupt_tasklet_func in_softirq: 3 usec
[1195123.052484] simrupt: [CPU#2] simrupt_work_func
[1195123.052496] simrupt: produce_data: in 1/1 bytes
[1195123.052623] simrupt: simrupt_read: out 1/0 bytes
[1195123.052656] simrupt: simrupt_read(0000000026af7165, 131072, 0)
...
```
### `workqueue_struct`, `work_struct`, `struct worker` and `task_struct`
我們在其他排程器的教材中，理解每一個行程(線程)事實上就是一個 `task_struct`，而排程器透過排程其成員 `sched_entity` 管理排程行為；而無論從 `workqueue_struct` 或 `work_struct`，我們並沒有看到與排程直接相關的成員或屬性，那麼它們是怎麼被排程的呢？

首先我們可以在 `kernel/workqueue_internal.h` 找到 `struct worker`，其中帶有我們熟悉的 `task_struct`：
```c
struct worker {
	/* on idle list while idle, on busy hash table while busy */
	union {
		struct list_head	entry;	/* L: while idle */
		struct hlist_node	hentry;	/* L: while busy */
	};

	struct work_struct	*current_work;	/* K: work being processed and its */
	work_func_t		current_func;	/* K: function */
	struct pool_workqueue	*current_pwq;	/* K: pwq */
	u64			current_at;	/* K: runtime at start or last wakeup */
	unsigned int		current_color;	/* K: color */

	int			sleeping;	/* S: is worker sleeping? */

	/* used by the scheduler to determine a worker's last known identity */
	work_func_t		last_func;	/* K: last work's fn */

	struct list_head	scheduled;	/* L: scheduled works */

	struct task_struct	*task;		/* I: worker task */
	struct worker_pool	*pool;		/* A: the associated pool */
						/* L: for rescuers */
	struct list_head	node;		/* A: anchored at pool->workers */
						/* A: runs through worker->node */

	unsigned long		last_active;	/* K: last active timestamp */
	unsigned int		flags;		/* L: flags */
	int			id;		/* I: worker id */

	/*
	 * Opaque string set with work_set_desc().  Printed out with task
	 * dump for debugging - WARN, BUG, panic or sysrq.
	 */
	char			desc[WORKER_DESC_LEN];

	/* used only by rescuers to point to the target workqueue */
	struct workqueue_struct	*rescue_wq;	/* I: the workqueue to rescue */
};
```
此處有趣的地方是註解紀載了每個成員的 locking annotation，也就是與鎖的關係。
:::spoiler Locking Annotations
```c=113
// defined at kernel/workqueue.c
/*
 * Structure fields follow one of the following exclusion rules.
 *
 * I: Modifiable by initialization/destruction paths and read-only for
 *    everyone else.
 *
 * P: Preemption protected.  Disabling preemption is enough and should
 *    only be modified and accessed from the local cpu.
 *
 * L: pool->lock protected.  Access with pool->lock held.
 *
 * K: Only modified by worker while holding pool->lock. Can be safely read by
 *    self, while holding pool->lock or from IRQ context if %current is the
 *    kworker.
 *
 * S: Only modified by worker self.
 *
 * A: wq_pool_attach_mutex protected.
 *
 * PL: wq_pool_mutex protected.
 *
 * PR: wq_pool_mutex protected for writes.  RCU protected for reads.
 *
 * PW: wq_pool_mutex and wq->mutex protected for writes.  Either for reads.
 *
 * PWR: wq_pool_mutex and wq->mutex protected for writes.  Either or
 *      RCU for reads.
 *
 * WQ: wq->mutex protected.
 *
 * WR: wq->mutex protected for writes.  RCU protected for reads.
 *
 * MD: wq_mayday_lock protected.
 *
 * WD: Used internally by the watchdog.
 */
```
:::
\
我們藉由觀察定義在 `kernel/workqueue.c` 的 `create_worker()` 了解如何創建一個 woker thread：
```c
/**
 * create_worker - create a new workqueue worker
 * @pool: pool the new worker will belong to
 *
 * Create and start a new worker which is attached to @pool.
 *
 * CONTEXT:
 * Might sleep.  Does GFP_KERNEL allocations.
 *
 * Return:
 * Pointer to the newly created worker.
 */
static struct worker *create_worker(struct worker_pool *pool)
{
	struct worker *worker;
	int id;
	char id_buf[23];

	/* ID is needed to determine kthread name */
	id = ida_alloc(&pool->worker_ida, GFP_KERNEL);
	if (id < 0) {
		pr_err_once("workqueue: Failed to allocate a worker ID: %pe\n",
			    ERR_PTR(id));
		return NULL;
	}

	worker = alloc_worker(pool->node);
	if (!worker) {
		pr_err_once("workqueue: Failed to allocate a worker\n");
		goto fail;
	}

	worker->id = id;

	if (pool->cpu >= 0)
		snprintf(id_buf, sizeof(id_buf), "%d:%d%s", pool->cpu, id,
			 pool->attrs->nice < 0  ? "H" : "");
	else
		snprintf(id_buf, sizeof(id_buf), "u%d:%d", pool->id, id);

	worker->task = kthread_create_on_node(worker_thread, worker, pool->node,
					      "kworker/%s", id_buf);
	if (IS_ERR(worker->task)) {
		if (PTR_ERR(worker->task) == -EINTR) {
			pr_err("workqueue: Interrupted when creating a worker thread \"kworker/%s\"\n",
			       id_buf);
		} else {
			pr_err_once("workqueue: Failed to create a worker thread: %pe",
				    worker->task);
		}
		goto fail;
	}

	set_user_nice(worker->task, pool->attrs->nice);
	kthread_bind_mask(worker->task, pool_allowed_cpus(pool));

	/* successful, attach the worker to the pool */
	worker_attach_to_pool(worker, pool);

	/* start the newly created worker */
	raw_spin_lock_irq(&pool->lock);

	worker->pool->nr_workers++;
	worker_enter_idle(worker);
	kick_pool(pool);

	/*
	 * @worker is waiting on a completion in kthread() and will trigger hung
	 * check if not woken up soon. As kick_pool() might not have waken it
	 * up, wake it up explicitly once more.
	 */
	wake_up_process(worker->task);

	raw_spin_unlock_irq(&pool->lock);

	return worker;

fail:
	ida_free(&pool->worker_ida, id);
	kfree(worker);
	return NULL;
}
```
上述函式可以看到
1. 分配 worker 記憶體 `worker = alloc_worker(pool->node);`
3. 初始化 worker thread `worker->task = kthread_create_on_node(worker_thread, worker, pool->node, "kworker/%s", id_buf);`
4. 指定 nice level `set_user_nice(worker->task, pool->attrs->nice);`
5. 指定執行之 CPU `kthread_bind_mask(worker->task, pool_allowed_cpus(pool));`
6. 喚醒負責執行的線程 (note: idle and sleep is different) `kick_pool(pool);` 和 `wake_up_process(worker->task);`
7. 解鎖 thread pool `raw_spin_unlock_irq(&pool->lock);`

另外，在作業說明中提到：
>實作上，只要該 CPU 上有一個或多個 runnable worker thread，worker-pools 就暫時不會執行新的 work。一直到最後一個正在運行的 worker thread 進入睡眠狀態時，才立即排程一個新的 worker thread，這樣 CPU 就不會在仍有尚未處理的 work item 時無所事事，但也不至於過度的建立大量 worker thread。

我們可觀察到檢查是否需要喚醒 worker thread 的一系列操作：
```c
/*
 * Need to wake up a worker?  Called from anything but currently
 * running workers.
 *
 * Note that, because unbound workers never contribute to nr_running, this
 * function will always return %true for unbound pools as long as the
 * worklist isn't empty.
 */
static bool need_more_worker(struct worker_pool *pool)
{
	return !list_empty(&pool->worklist) && !pool->nr_running;
}

/* Can I start working?  Called from busy but !running workers. */
static bool may_start_working(struct worker_pool *pool)
{
	return pool->nr_idle;
}

/* Do we need a new worker?  Called from manager. */
static bool need_to_create_worker(struct worker_pool *pool)
{
	return need_more_worker(pool) && !may_start_working(pool);
}
```
並可以觀察到 `need_to_create_worker()` 常在創建新的 worker thread 前被檢查，如 `maybe_create_worker()`，而 `pool_mayday_timeout()` 用於當存在 `work`，但卻沒有 idle worker 的狀況，透過 `send_mayday()` 緊急喚醒 worker thread。

追蹤 `worker_enter_idle()` 亦可發現符合作業描述的程式碼，此處暫不展開討論。


### `rx_fifo` 與 `rx_wait`
另一個部件是 `rx_fifo` 與 `rx_wait`，事實上它涉及了數個保護和同步機制：
1. `read_lock` 確保 `simrupt_read()` 是 thread-safe 
2. `consumer_lock` `producer_lock` 負責確保由裝置讀取、存入 `rx_fifo` 這兩個動作是 thread-safe

除此之外，`wait_event_interruptible()` 和 `wake_up_interruptible()` 是重點操作：
當 `kfifo_len(&rx_fifo)` == 0 時，任務被不斷加入到 `rx_wait`，並進入等待；追蹤程式碼後會見到定義在 `kernel/sched/wait.c` 的 `prepare_to_wait_event()`，看起來是一 spin waiting 機制。

而當 `kfifo_len(&rx_fifo)` > 0，除了該行程直接傳回值使 `ret == 0` 成立外，其餘所有在 `rx_wait` 的行程因為 `wake_up_interruptible(&rx_wait);` 被呼叫而重新成為 runnable。

:::warning
>[name=Kuanch]
>此處十分不直觀，嘗試是否能夠在 [Use QEMU + remote gdb](#Use-QEMU--remote-gdb-to-analyze-dynamically) 觀察到狀態的改變
:::

以下程式碼可接續追尋到 `kernel/sched/wait.c` 的 `prepare_to_wait_event()`
```c=479
// defined at include/linux/wait.h
/**
 * wait_event_interruptible - sleep until a condition gets true
 * @wq_head: the waitqueue to wait on
 * @condition: a C expression for the event to wait for
 *
 * The process is put to sleep (TASK_INTERRUPTIBLE) until the
 * @condition evaluates to true or a signal is received.
 * The @condition is checked each time the waitqueue @wq_head is woken up.
 *
 * wake_up() has to be called after changing any variable that could
 * change the result of the wait condition.
 *
 * The function will return -ERESTARTSYS if it was interrupted by a
 * signal and 0 if @condition evaluated to true.
 */
#define wait_event_interruptible(wq_head, condition)				\
({										\
	int __ret = 0;								\
	might_sleep();								\
	if (!(condition))							\
		__ret = __wait_event_interruptible(wq_head, condition);		\
	__ret;									\
})
```

### Use `strace` to trace the callback and signal
`strace` 是一種診斷工具，它可以追蹤程式運行中的系統呼叫，有助於我們了解一部分 Linux 模組在核心中的運作；透過 `strace -f cat /dev/simrupt`

```powershell
execve("/usr/bin/cat", ["cat", "/dev/simrupt"], 0x7ffe5faf5928 /* 24 vars */) = 0
brk(NULL)                               = 0x5631458c3000
arch_prctl(0x3001 /* ARCH_??? */, 0x7ffebbfb7910) = -1 EINVAL (Invalid argument)
access("/etc/ld.so.preload", R_OK)      = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
fstat(3, {st_mode=S_IFREG|0644, st_size=82854, ...}) = 0
mmap(NULL, 82854, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7fb162844000
close(3)                                = 0
openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3
read(3, "\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\300A\2\0\0\0\0\0"..., 832) = 832
pread64(3, "\6\0\0\0\4\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0"..., 784, 64) = 784
pread64(3, "\4\0\0\0\20\0\0\0\5\0\0\0GNU\0\2\0\0\300\4\0\0\0\3\0\0\0\0\0\0\0", 32, 848) = 32
pread64(3, "\4\0\0\0\24\0\0\0\3\0\0\0GNU\0\207\2631\3004\246E\214d\316\t\30099\351G"..., 68, 880) = 68
fstat(3, {st_mode=S_IFREG|0755, st_size=2029592, ...}) = 0
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fb162842000
pread64(3, "\6\0\0\0\4\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0@\0\0\0\0\0\0\0"..., 784, 64) = 784
pread64(3, "\4\0\0\0\20\0\0\0\5\0\0\0GNU\0\2\0\0\300\4\0\0\0\3\0\0\0\0\0\0\0", 32, 848) = 32
pread64(3, "\4\0\0\0\24\0\0\0\3\0\0\0GNU\0\207\2631\3004\246E\214d\316\t\30099\351G"..., 68, 880) = 68
mmap(NULL, 2037344, PROT_READ, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x7fb162650000
mmap(0x7fb162672000, 1540096, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x22000) = 0x7fb162672000
mmap(0x7fb1627ea000, 319488, PROT_READ, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x19a000) = 0x7fb1627ea000
mmap(0x7fb162838000, 24576, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1e7000) = 0x7fb162838000
mmap(0x7fb16283e000, 13920, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x7fb16283e000
close(3)                                = 0
arch_prctl(ARCH_SET_FS, 0x7fb162843580) = 0
mprotect(0x7fb162838000, 16384, PROT_READ) = 0
mprotect(0x563144cd9000, 4096, PROT_READ) = 0
mprotect(0x7fb162886000, 4096, PROT_READ) = 0
munmap(0x7fb162844000, 82854)           = 0
brk(NULL)                               = 0x5631458c3000
brk(0x5631458e4000)                     = 0x5631458e4000
openat(AT_FDCWD, "/usr/lib/locale/locale-archive", O_RDONLY|O_CLOEXEC) = 3
fstat(3, {st_mode=S_IFREG|0644, st_size=6354512, ...}) = 0
mmap(NULL, 6354512, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7fb162040000
close(3)                                = 0
fstat(1, {st_mode=S_IFCHR|0620, st_rdev=makedev(0x88, 0x1), ...}) = 0
openat(AT_FDCWD, "/dev/simrupt", O_RDONLY) = 3
fstat(3, {st_mode=S_IFCHR|0600, st_rdev=makedev(0x1fd, 0), ...}) = 0
fadvise64(3, 0, 0, POSIX_FADV_SEQUENTIAL) = 0
mmap(NULL, 139264, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fb16201e000
read(3, " ", 131072)                    = 1
write(1, " ", 1 )                        = 1
read(3, "!", 131072)                    = 1
write(1, "!", 1!)                        = 1
....
```
透過對照 [LINUX SYSTEM CALL TABLE FOR X86 64](https://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/)，可以很好的理解每個呼叫以及其參數的意義，輕易地可以將上述訊息與 `simrupt.c` 的行為對應：

* `execve`
    執行 `cat /dev/simrupt` 之呼叫
* `brk(NULL)`
    動態分配記憶體。記得我們曾在 [Demystifying the Linux CPU Scheduler 閱讀筆記 2024](https://hackmd.io/@Kuanch/linux2024-collection/https%3A%2F%2Fhackmd.io%2F%40Kuanch%2Flinux-kernel-scheduler-notes2) 討論過 "Where is heap?" 的 `mm_struct.brk`，`brk(NULL)` 用於查詢目前 heap 的位置。
* `mmap(NULL, 82854, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7fb162844000`
    上述指令用於讀取檔案的一部分進入記憶體，`82854` 表示需要配置的記憶體空間 `PROT_READ` 表示僅讀不寫，回傳值為此新配置的空間的起點。
    

### Use QEMU + remote gdb + Buildroot to analyze dynamically
#### 安裝設定
請參考 [Kernel Analysis with QEMU + remote GDB + Buildroot](https://hackmd.io/@Kuanch/linux2024-termproj-prereq#Kernel-Analysis-with-QEMU--remote-GDB--Buildroot) 設定；但我們需要針對 `kernel_module` 的部分設定，將其改為 `simrupt`：

#### 與 `strace` 比較
`strace` 屬於靜態分析，也就是


### How mutex lock works

### How workers are assigned to specific CPUs

## 整合井字遊戲對弈


## Reference
[Driver (驅動)](https://hackmd.io/@combo-tw/ryRp--nQS)
[[Linux Kernel慢慢學]Linux modules載入及載入順序](https://meetonfriday.com/posts/c4426b79/)
[Booting a Custom Linux Kernel in QEMU and Debugging It With GDB](https://nickdesaulniers.github.io/blog/2018/10/24/booting-a-custom-linux-kernel-in-qemu-and-debugging-it-with-gdb/)
[用 gdb debug 在 QEMU 上跑的 Linux Kernel](https://blog.austint.in/2022/01/16/run-and-debug-linux-kernel-in-qemu-vm.html)
[LINUX SYSTEM CALL TABLE FOR X86 64](https://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/)