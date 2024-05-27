# Linux 核心專題: CPU 排程器研究
> 執行人: Kuanch

## 研讀《Demystifying the Linux CPU Scheduler》並紀錄相關疑惑和提出修正
> [閱讀筆記](https://hackmd.io/@linhoward0522/HyC28W9N3)

### 1 Basics of the Linux Kernel

### 1.3 Process management

#### 1.3.1 Process and threads
本節討論一經典問題：行程 (Process) 與執行緒 (Threads) 的分別是什麼？首先
> Linux inherits the Unix view of a process as a program in execution.

行程是一個執行中的程式 (Program)，包含存放其指令及資料的記憶體、擁有執行指令的處理器 (processor) 以及和外界互動的 I/O 裝置；每一個行程互相獨立，意思是他們僅能看見自己的資料，必須要透過分享資源的系統和其他行程溝通才能夠得到其他行程的資料。

而執行緒是因應更好的讓行程處理並行 (concurrent) 程式而生的，由於其能夠輕易與其他執行緒共享資料，在多工 (multitasking) 更有優勢。

> ..., using threads in a program instead of spawning new processes results in much better performance. 

此處亦提及了 `clone()` 與 `fork()` 的區別，前者能夠更細緻選擇要從親代進程「克隆」哪些資源，即透過 `CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND` ，在 [Linux 對於 fork 實作的手法](https://hackmd.io/@sysprog/unix-fork-exec#Linux-%E5%B0%8D%E6%96%BC-fork-%E5%AF%A6%E4%BD%9C%E7%9A%84%E6%89%8B%E6%B3%95) 亦有討論；注意此處不使用「複製」 (copy) 一詞，係因克隆強調副本與正本相同，但 "resources are shared" ，而複製強調擁有獨立副本。

在 Linux 核心內，每一個任務 (task) 以 [`task_struct`](https://elixir.bootlin.com/linux/latest/source/include/linux/sched.h#L748) 呈現，這是一個非常巨大的結構體，約有 3.5 KB，又稱 process descripter or PCB (process control block)。

為了理解 `task_struct` 的實作，應留意以下。 (todo4)

- [ ] `volatile long state`
>..., it is possible that the variable is changed outside of the current execution context, so loading the variable on every access is required.

`volatile` 要求編譯器每次存取該變數都需要從記憶體讀取，抑制編譯器最佳化利用暫存器存取，也就是說在使用前一定會有 `load` 指令發生，參考我的 [Linux 核心修飾字隨筆](https://hackmd.io/@Kuanch/linux_kernel_decorator)；至於什麼狀況下是 "outside of the current execution context"，考慮

- [ ] `list_head` 和 `hlist_node`

`list_head` 與 `container_of` 在 [Assignment 1(lab0)](https://hackmd.io/@Kuanch/linux2024-homework1) 有豐富的討論，是標準的 Linux 核心鏈結串列的標準設計；而 `hlist_node` 亦出現在 [Assignment 2 (quiz1+2)](https://hackmd.io/@Kuanch/linux2024-homework2)，並使用 `hlist_node` 實作 LRU hash table。
    
其 LRU hash table 可對照 Linux 核心中的 PID hash table，故給定一個 PID，我們能在 $O(1)$ 的狀況下搜尋到該任務，如 `kill [PID]`。
    
觀察 `include/linux/pid.h` 之 `pid_type`，事實上在 PID hash table 之前還有一個 hash table 用於分類儲存 `PID` `TGID` `PGID` `SID` 的 hash table：

```c
// kernel/pid.c
struct task_struct *pid_task(struct pid *pid, enum pid_type type)
{
    struct task_struct *result = NULL;
    if (pid) {
        struct hlist_node *first;
        first = rcu_dereference_check(hlist_first_rcu(&pid->tasks[type]),
                          lockdep_tasklist_lock_is_held());
        if (first)
            result = hlist_entry(first, struct task_struct, pid_links[(type)]);
    }
    return result;
}
```

以 `pid_task()` 為範例，可見其傳入 PID 的類型用於建構 hash table 中的 `hlist_node`。

#### 1.3.3 Scheduling

這一小節專注於討論 Linux 核心在排程上的「哲學」和常見的排程演算法說明。

首先，Linux 核心排程的核心是 "time sharing"，CPU core 被行程透過 "time multiplexing" 分割成多塊，注意重點是以「時間」分割。

>Linux achieves the effect of an apparent simultaneous execution of multiple processes by switching from one process to another in a very short time frame, based on the time sharing technique.

既然是以時間分割，那每個行程分配的順序以及時間就很重要，故有排程演算法如 FIFO, RR, EDF, SJF 等等有不同的策略；即便是 SCHED_DEADLINE 策略，所有的排程策略都是可搶佔的 (preemptive)，如此我們就可以隨時替換執行的任務，並有更大的彈性。
> 我的疑惑是，此處的描述和「並行」(concurrent) 非常相似，但不使用該詞，而如同[並行程式設計: 排程器原理](https://hackmd.io/@sysprog/concurrent-sched)所說，當古早以前僅有單一 CPU，就存在並行的需求，故認為 "time sharing" 的設計是與並行互相呼應的，可能是老師有意將兩者拆開討論。
> $\to$ 即使沒有作業系統，也有並行的需求，例如在中斷處理常式中被搶佔，以處理具備更高優先權的中斷 (nested interrupt)。time-sharing 則是多工作業系統的實作手法。

:::warning
區分用語:
* (operating system) kernel: 核心
* (processor) core: 核
:::

現代的多處理器互相分享匯流排、I/O 設備以及記憶體，也就是 SMP (symmetric multiprocessing) 架構，以提供平行處理的能力，甚至更現代的計算機中，不但是多處理器也是多核：每個處理器能夠有多個核 (core)，我們可以將每個核當作一個處理器；但帶來的問題是 load balance：有些核心可能特別忙碌，有些特別閒；之後我們會知道透過計算每一個任務對核心的負擔，讓排程器能夠減少過荷核心的負載，更多的使用閒置的核心，也就是 Per-entity load tracking (PELT)。

Context Switch 是「儲存被打斷的行程的資訊，並回復另一行程」的行為，即儲存前一行程、載入下個行程的 Register Files、Memory Management Unit (MMU) 和 Translation Look-aside Buffer (TLB)，由於快取 (cache) 都會被置換，故 Context Switch 是開銷龐大的行為。

相對於 preemptive multitasking；也就是每過一段時間就發生 timer interrupt，中斷目前的行程進行 Context Switch 並交由排程器決定下一行程，cooperative multitasking 是一種自發性交出核心的行為，由於是計畫中的，這可能會減少 context swith 頻率和其開銷；實作方面可以參考 [coroutine 實作體驗](https://hackmd.io/@sysprog/concurrent-sched#coroutine-%E5%AF%A6%E4%BD%9C%E9%AB%94%E9%A9%97)。

Context Switching 的細節是什麼呢？首先要談論到 6 個 *segmentation registers* (pp2)，分別是：
1. Code segment (.text)
2. Stack segment 
3. Data segment (.data)
4. Extra segment (ES)
5. FS
6. GS

後三者被稱作 "genreal-purpose" register，即 ["Simply to access data beyond the default data segment (DS). Exactly like ES."](https://stackoverflow.com/a/61763516)。

:::info
:::spoiler What does "F" and "G" stands for in FS/GS?
According to [What is the "FS"/"GS" register intended for?](https://stackoverflow.com/questions/10810203/what-is-the-fs-gs-register-intended-for#comment14067959_10810203)

> As far as I know, the F and G in these two do not stand for anything. It's just that there was room on the CPU (and in the instruction set) for six user-specifiable segment registers, and someone noticed that besides the "S"tack segment, the letters "C" and "D" (code and data) were in sequence, so "E" was the "extra" segment, and then "F" and "G" just sort of followed.
:::

這些 *segementation registers* 都含有 *segment selector*，它是一個 16 位元值，組成是：

* Index (13 bits)：索引值乘上 8 (bytes, size of a segment descriptor) + GDTR (LDTR, depends on TI, LDTR is the base address in the segment descriptor)
* Table Indicator (TI, 1 bit)： 當值為 1，取 GDT 中的 segment descriptor，反之取 LDT。
* Requested Privilege Level (RPL, 2 bits)：segment descriptor 的特權等級，參考 [x86 特權等級](https://www.csie.ntu.edu.tw/~wcchen/asm98/asm/proj/b85506061/chap3/privilege.html)

![image](https://hackmd.io/_uploads/Skm3k7Se0.png)

更詳細如 segment descriptor 格式請參考 [x86 分段架構](https://www.csie.ntu.edu.tw/~wcchen/asm98/asm/proj/b85506061/chap2/segment.html)。

以及 3 個 data registers，分別是：
1. Instruction pointer (cs:ip)
2. Stack pointer (ss:sp)
3. Base pointer (ss:bp)

回到 `schedule()`，上述流程我們可能會想像有一個 "kernel thread" 在運行 scheduler，但事實是當前的行程 (行程 A) 負責：
> The scheduler does not run as a separate thread, it always runs in the context of the current thread. 
> This means that any process in the system that goes from/to kernel mode can potentially execute the scheduler itself, using its own kernel stack. 

又此處還未討論誰發起這個事件 (我認為是 OS)，稍待補完。

:::info
:::spoiler Where is the heap? (pp3)
user stack pointer 被儲存在 `mm_struct` 中的 `start_stack`，而 kernel stack pointer 存在於 `task_struct` 中的 `void *stack`。

另一個值得關心的資料是 heap；當我們動態存取記憶體如使用 `malloc`，heap 就會被配置，如
```c
	movl	$10, %edi
	call	malloc@PLT
```
配置 10 bytes 記憶體。

而 context switch 的時候難道不儲存 heap 內的資訊嗎？當然需要儲存，否則剛配置好的記憶體不就消失了嗎，原來它被放置在 `mm_struct.start_brk` 和 `mm_struct.brk`，`mm_struct mm` 就是 `task_struct` 中的一個重要欄位。

參考 [Day 14 VMA 來襲](https://ithelp.ithome.com.tw/articles/10274922)。
:::

#### context switch (pp1)

context switch 的步驟是：

1. 當 system call, interrupt, 或 exception 發生，正在執行的行程 A 會開始 context switch
2. 行程 A 進入核心模式，將 user mode 的資料，如 `ss:sp`, `ss:bp`, `cs:ip` 等等儲存至 `task_struct`，如 `task_struct->thread.sp`，並儲存一部分 registers 到行程 A 自己的 kernel stack (使用一連串 `mov [register]`)
3. 注意並不是所有 registers 都會被儲存，被儲存的可稱 [non-volatile](https://www.techopedia.com/definition/8591/non-volatile-register)，反之稱 volatile registers；亦可參考 [x86 Assembly - Why is [e]bx preserved in calling conventions?](https://stackoverflow.com/questions/22214208/x86-assembly-why-is-ebx-preserved-in-calling-conventions)
4. 呼叫 `schedule()` 取得下一個行程 B
5. 行程 B 從自己的 kernel stack 載入 registers (使用一連串 `pop [register]`)、切換到 user mode，取出 `ss:sp`, `cs:ip` 和其他 data register

`context_switch()` 定義在 `kernel/sched/core.c` 展示了完整的流程，其中 kernel mode 和 user mode 的轉換涉及 lazy TLB strategy 和 `task_struct->mm` `task_struct->active_mm`，**這就是一個為什麼執行緒會比行程開銷更小的主要機制之一！**

:::danger
:::spoiler What is lazy TLB mode and why?
>[name=Kuanch]
>lazy TLB mode 是為了避免頻繁的 flush TLB 導致後進任務需要重新載入 TLB，如果前後任務使用的 TLB 有重疊，那我們應該選擇性地保留 TLB。
>
>首先可以見到註解就明確說明本函式應該稱為 `switch_to_kernel_thread`，但也有進行 "
>enter lazy mode"，我認為 `enter_lazy_tlb` 註解應有誤，或是這個函式並沒有將兩個狀況解耦
>1. 進入 lazy TLB mode
>2. 標示和 kernel thread (process) 交換
>
>考慮該程式碼：
```c`
/*
 * Please ignore the name of this function.  It should be called
 * switch_to_kernel_thread().
 *
 * enter_lazy_tlb() is a hint from the scheduler that we are entering a
 * kernel thread or other context without an mm.  Acceptable implementations
 * include doing nothing whatsoever, switching to init_mm, or various clever
 * lazy tricks to try to minimize TLB flushes.
 *
 * The scheduler reserves the right to call enter_lazy_tlb() several times
 * in a row.  It will notify us that we're going back to a real mm by
 * calling switch_mm_irqs_off().
 */
void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
	if (this_cpu_read(cpu_tlbstate.loaded_mm) == &init_mm)
		return;

	this_cpu_write(cpu_tlbstate_shared.is_lazy, true);
}
```
>以下嘗試解釋程式碼：
>如果當前的 `mm` 指向 `&init_mm`，表示該任務已經處在核心模式中，故不需 flush TLB (cause kernel space is shared)；反之則標示為 lazy，如以下狀況：
> 1. 下一個執行緒(行程)仍是 kernel thread
> 2. 下一個執行緒與當前執行緒同屬一個 process，使用同個 TLB
> 
> 等等。

:::

在 `context_switch()` 中，首先是透過 `next->mm` 判斷 `next` 是 kernel mode 或 user mode，若 `!next->mm` 成立則是 kernel model，反之是 user mode。

在下一個任務是 kernel mode 時，透過 `enter_lazy_tlb`，我們無論如何可以避免 flush TLB，而如果前個任務是來自於 user mode，我們則透過 `atomic_inc(&mm->mm_count);` 增加其 mm reference 數量。

>若下個任務是 user mode 呢？


接下來，我們亦可以在 Linux 核心中找到相應的函式 `__switch_to_asm()`，用於儲存和轉換 task registers：
```c
// defined in arch/x86/entry/entry_32.S
/*
 * %eax: prev task
 * %edx: next task
 */
.pushsection .text, "ax"
SYM_CODE_START(__switch_to_asm)
	/*
	 * Save callee-saved registers
	 * This must match the order in struct inactive_task_frame
	 */
	pushl	%ebp
	pushl	%ebx
	pushl	%edi
	pushl	%esi
	/*
	 * Flags are saved to prevent AC leakage. This could go
	 * away if objtool would have 32bit support to verify
	 * the STAC/CLAC correctness.
	 */
	pushfl

	/* switch stack */
	movl	%esp, TASK_threadsp(%eax)
	movl	TASK_threadsp(%edx), %esp

#ifdef CONFIG_STACKPROTECTOR
	movl	TASK_stack_canary(%edx), %ebx
	movl	%ebx, PER_CPU_VAR(__stack_chk_guard)
#endif

	/*
	 * When switching from a shallower to a deeper call stack
	 * the RSB may either underflow or use entries populated
	 * with userspace addresses. On CPUs where those concerns
	 * exist, overwrite the RSB with entries which capture
	 * speculative execution to prevent attack.
	 */
	FILL_RETURN_BUFFER %ebx, RSB_CLEAR_LOOPS, X86_FEATURE_RSB_CTXSW

	/* Restore flags or the incoming task to restore AC state. */
	popfl
	/* restore callee-saved registers */
	popl	%esi
	popl	%edi
	popl	%ebx
	popl	%ebp

	jmp	__switch_to
SYM_CODE_END(__switch_to_asm)
.popsection
```

注意 `pushl` 和 `popl`，他們將這些 register 放置到 kernel stack 以及取出，如果執行緒共享快取，這些 register 可以很快地從 cache 中取出；若否，則在 memory accessing 會有 cache miss，這也是一個為什麼執行緒會比行程開銷更小的主要機制！

另外，`pushl %ebp` 等價於
```c
subl $4, %esp
movl %ebp, (%esp)
```

參考 [C Function Call Convention: Why movl instead of pushl?](https://stackoverflow.com/questions/23309620/c-function-call-convention-why-movl-instead-of-pushl)

一個有趣的地方是 `TASK_threadsp`，我們可以找到
```c
// include/generated/asm-offsets.h
#define TASK_threadsp 3160 /* offsetof(struct task_struct, thread.sp) */

// arch/x86/kernel/asm-offsets.c
OFFSET(TASK_threadsp, task_struct, thread.sp);
```
透過這樣的方式操作 `thread.sp`。

### 2 The Linux CPU scheduler
本章主要著重介紹排程器，也是本書重點；而我們為什麼需要排程器呢？追根究底，每一個 CPU 同一時間下只能執行一個任務，而任務的總量遠超 CPU 數量，就算是今日最先進的多處理器機器也是如此，因此，排程器作為負責排程任務，決定何時該停止任務、加入任務以及選取任務的模組就至關重要。

### 2.1 Introduction

#### 2.1.1 Objectives of the scheduler
本節釐清各項現代作業系統或排程器中的關鍵名詞，他們可能存在 trade-off 關係或常是重點性質。

**Response time and throughput**
在考慮互動介面時，response time 與 throughput 是兩個對於使用者體驗有重大影響的因素，在不同的場景中注重的因素不同，譬如前者在使用文字編輯器的時候很重要，後者則是對於 CPU 有大量需求的任務，系統必須要提供 high throughput。

:::info
或者是否可理解為，前者是 I/O 密集任務重視 response time，而後者是 CPU 密集任務重視 throughput？
:::

而兩者可能衝突，譬如說當需要回應時，排程器需要暫停當下任務並處理需要回應的任務，越快回應 reponse time 越小，對於接收輸出入的對象(使用者)對於系統的效能評估會越好；但同時，這樣的行為可能會降低 throughput，因為任務被打斷了

**Fairness and Liveness**
Fairness 是現代排程器中十分被看重的性質，稍後我們會看到 $O(1)$ 排程器並不重視公平性，高優先度的任務長期占用 CPU 導致 **starvation**，引出 Fairness 與 Liveness 的關係，後者也就是 "freedom from starvation"，在排程器必須要讓任務在有限的時間內取得 CPU，不得使其無限等待。

我個人認為，老師在此處比較兩者係因，後者是現代排程器在最差狀況下也應該要擁有的性質，前者則是理想中要具有的性質；印象中 IC verification 中也有類似的用語，其更傾向是敘述某一種邏輯表述最終能夠被驗證成立與否，該敘述貼近 "Something good will eventually happen"。

**Real-Time vs. Non-RT**
Real-time 任務不但需要結果正確，還需要在 deadline 前回應，又分 hard real-time 和 soft real-time，前者只要超出 deadline 則算失敗，後者的評分則是隨著延遲增加而評分降低；我想可以想像前者如駕駛行為，後者如網路連線。而 Non-real-time 則是沒有 deadline 概念，可以根據系統資源調整。


#### 2.1.2 Handling various workload patterns

### 2.2 Prior to CFS
本節討論了在 CFS 前，由 Early Scheduler 到 $O(1)$ Scheduler 的細節與相對應的改進，可以參照[Linux 核心設計: 不只挑選任務的排程器 閱讀筆記](https://hackmd.io/@Kuanch/linux-kernel-scheduler-notes1)即可，我們將重點放在更現代的 $O(1)$ 排程器以及提出 CFS 雛形的 RSDL。

#### 2.2.3 O(1) Scheduler
$O(n)$ 排程器除了挑選下一任務為線性複雜度外，還有其他缺點，如
1. A single runqueue (global runqueue)
2. A single runqueue lock
3. An inability to preempt running processes

這樣的設計直觀上導致許多問題，比較少被提出的觀點是 processor affinity；當任務因為許多因素被迫交出 CPU，當它重新執行時，原先的 CPU 中很可能還儲存它原有的記憶體狀態等，包含暫存器以及 L1，這樣的性質被稱作 ***affinity***。(同一行程下的執行緒顯然也具有 affinity，故更加 cache-friendly，在 context switch 中使用相同的暫存器資料、有更多的 cache hit)

當只有一個 global runqueue，任務可能會被任意的 CPU 取出執行，便有較差的 affinity；此外，存取 runqueue 將受到鎖的影響，顯著影響效能。

$O(1)$ 的最顯著改進就是 per-CPU queues，不但解決上述問題，有 load balancing 的可能，也可以做到 work stealing；也就是當某一 CPU idle 時，從其他 runqueue 取出任務交給該 CPU (或應該說是使用 work stealing 解決 per-CPU queues 後新增的問題)。

注意此處 per-CPU queues 並非將所有任務都儲存於該 runqueue，而是按 priority 儲存 list head，每一個 list 才真正連結任務，也就是
```c
struct prio_array {
    int nr_active; /* number of tasks in this array */
    unsigned long bitmap[BITMAP_SIZE]; /* priority bitmap */
    struct list_head queue[MAX_PRIO]; /* priority queues */
}
```

此外，$O(1)$ 已有所謂 static priority 和 bonus priority，每一個任務都會有兩種 priority；前者是固定的，即 nice value，後者則會依照該任務先前 sleeping 的時間來決定，如果 sleeping 較久，就應該提升它的 bonus priority，反之則下降。

換個角度看，什麼樣的任務會有較頻繁的 sleeping 和 waking呢？I/O任務、與使用者相關的任務，如鍵盤或滑鼠，這就是 highly interactive tasks；透過提升其 dynamic(bonus) priority，能夠更好的和 I/O 溝通、提升使用者體驗。

再換個角度和，static priorty (nice value) 代表的是 CPU intense 的任務權重，權重越大對 throughput 的需求越大；而 dynamic(bonus) priority 則是 I/O 任務，對 responsiveness 更重要。

:::info
>[name=Kuanch]
這樣的行為似乎和後續的 CFS virtual runtime 想要達成的目的十分相似：透過追蹤任務實際使用 CPU 的時間，加權之後評估該任務是否需要被分配更多 CPU 的使用權。
:::

而問題是，nice level 是 `-20` 的任務無疑會有更多的 CPU 使用權而 `+19` 幾乎沒有，我們從何處判斷 `+19` 是否會有許多 sleeping 時間，如果它從未或很少使用到 CPU 呢？

>This approach is the biggest weakness of the O(1) scheduler: it generated unpredictable behavior and could cause some tasks to be marked as interactive even when they were not. Furthermore, the different kinds of complex heuristics made the code bigger and harder to understand and maintain.

最後，$O(1)$ 排程器使用 nice value 對評估該任務需要取得多少 CPU 時間的影響是非線性的，譬如當 nice level 0 -> 1 和 nice level 18 -> 19，前者的百分比變化極小約 5%，後者則是 100%，種種非預期的行為使得 nice API 加劇排程的不穩定性。

最後的最後，該 active array 及 expired array 設計導致多媒體應用任務使用 `SCHED_FIFO` 策略 (其非使用 `SCHED_NORMAL` )的問題：
> This approach caused another major problem because SCHED_FIFO, as we stated earlier, is not starvation proof. Tasks in O(1) scheduler had to wait until all of the other tasks, in all of active runqueues at all of the levels of higher priority, exhausted their timeslices. This introduced unacceptable jitter in applications that needed to be performant in realtime, such as VoIP.

#### 2.2.4 Rotating Staircase DeadLine Scheduler (RSDL)
RSDL 的前身是 $O(1)$ 排程器，由 Con Kolivas 提出，故又稱 Kolivas Scheduler；其提出的 Completely Fair 的概念被改良應用在後續的 CFS 中。

不同於 $O(1)$ 排程器的主要設計是為了在線性時間內決定下一個執行任務，RSDL 改良 $O(1) 排程器，避免高優先度任務占用大量 CPU 時間，也就是注重公平，首次關注 **fairness**。

首先，每一個 CPU 仍有 active and expired runqueue，依然依照 priority 排序任務，越高優先級的任務可以有越多的 timeslice (`RR_INTERVAL`, default 6 ms)，排程器由最高優先級任務開始執行。

```
RSDL Run Queue
--------------
| Priority 0 | -> Process 1 -> Process 2 -> Process 3
--------------
| Priority 1 | -> Process 4 -> Process 5
--------------
|     .      |
|     .      |
|     .      |
--------------
| Priority N |
--------------

     ||  Process 1 going down the stairs
     \/

RSDL Run Queue
--------------
| Priority 0 | -> Process 2 -> Process 3
--------------
| Priority 1 | -> Process 4 -> Process 5 -> Process 1
--------------
|     .      |
|     .      |
|     .      |
--------------
| Priority N |
--------------
```

每當任務消耗完它的 timeslice，則優先級下降，並加回 runqueue；再次被執行後下降優先級、家回 runqueue；此外，經常 sleeping 的任務將會被保留 timeslice 以及優先權，如此一來 interactive task 通常也會維持較高的優先級；注意，這需要睡眠超過一定時間，故任務傾向用少於 timeslice 的時間。這稱作 "minor rotation"。

>P is also pushed back up the staircase if it sleeps for a predefined period. Sleeping processes retain their timeslices and if they wake up in the same era, they can use them to the end. As a result, interactive tasks which tend to sleep more often should remain at the top of the staircase, while CPU-intensive processes should continuously expend more timeslices but at a lower frequency.

當任務被降到最低優先級並消耗完該優先級的 timeslice，將被依照任務原先的 static priority 加入 expired runqueue，而當 active runqueue 為空，兩者將調換，這稱作 "major rotation"。

這樣的方法帶來的一個好處是，任何任務都可以計算出一個最快取得 CPU 的時間，而非無盡等待，這樣的特性符合前述提到的 liveness。

### 2.3 Completely Faire Scheduler (CFS)
最終，RSDL 並沒有被 Linux 核心採用，但後續受到其啟發的 CFS 自 v2.6 以來就是 Linux 核心的主要演算法之一。

RSDL 對每一個 priority 都有預設 timeslice，並透過定期調整 priority；而 CFS 從另一個角度出發：**將 CPU 平均分割給任務**。

但因為硬體效能或其他因素，譬如 context switch overhead，我們很難真的平均地分配 CPU 給不同任務；故此，CFS 創造了 *virtual runtime*；我們的目標是使得所有任務的 virtual runtime 相同，一個任務實際的執行時間被加權後才是 virtual runtime，高優先權的任務將獲得比實際執行更小的 virtual runtime，反之獲得更多的 virtual runtime；每一次排程器將選擇 virtual runtime 最小的任務執行一個非常短的 timeslice ，以努力達到上述「使所有任務的 virtual runtime 相同」的目的。

該加權方式使得 $O(1)$ 排程器的 nice level inconsistent 問題被解決，增加 1 nice level，增加的是固定一個單位的 timeslice $t$。

再換個角度想，之前的排程器考量到各項因素，時常需要重新分配各個任務的 timeslice，但 CFS vruntime 考慮的是「**任務在 runqueue 中等待的時間**」；並每次都執行等待時間最長，或說是使用 CPU 時間不符合預期的任務，是兩種不同的概念。

另外，CFS 不再有所謂 "dynamic priority"，除了在處理 mutex 可能導致的 priority inversion 會暫時提升外 (priority inheritance)，每個任務僅能有一個唯一的 priority。

#### 2.3.1 Proportional-Share Scheduling
CFS 的重點是 "Proportional-Share"，也就是如何「(按比例)公平地」「分配」「運算資源」；並以「以非常短的 timeslice 分配」、「按 priority 比例分配」、「每次都選擇執行(加權後)等待時間最久的任務」等方式實作；首先必然被討論的是如何加權，被稱作 Generalized Processor-shareing (GPS) model：
$$
c_i = \frac{W_i}{\sum_WW}\times\sum_CC
$$

對於任務 $p_i$，給定一段時間 $C$，將依照其權重比例 $W_i \in W$ 分配其得到的 CPU 時間 $c_i\in C$；放在 CFS 排程器中的場景中，每個任務得到的 timeslice 與其權重佔所有 *runnable tasks* 權重總和成比例。($\sum_CC$ 即為 *target latency*。)

另外，在本節中我們可得知：
1. CFS 每一個任務得到的 timeslice 並非不變，而是按照 *target latency* 進行權重分配，"weighted timeslice of all runnable tasks in the runqueue" 加起來會等於 target latency；當 runnable tasks 超過一定數量，則需按照任務數量乘以 *minimum granularity*，來保證獲得每個任務的 timeslice，細節參考 2.3.3。

>**Notice that the task-switching rate is dependent on the overall system load, unlike with a fixed timeslice.** This means that as a system using CFS becomes more loaded, it will tend to sacrifice some throughput in order to retain a desired level of responsiveness. ...The value of **6 ms** used in the examples is the default for uniprocessor systems.
>
>However, if system load becomes extremely high, CFS does not continue sacrificing throughput to response time. **This is because there is a lower bound on how little time each task can receive.**

:::info
>[name=Kuanch]
任務應該會不斷被加入和消耗 runqueue，或是說 `nr_running` 是不斷變動的，會不會前一刻的分配與下一刻的任務數量不一致，導致任務取得 CPU 的時間嚴重不符預期？
>
>譬如基於此刻僅有一個 runnable task，該任務執行整個 *target latency* = 6ms，但下一個突然有 8 個任務被加入 runqueue，使得每個任務現在僅能執行 *minimum granularity* = 0.75ms，這符合我們對 CFS 行為的預期嗎？
:::

2. 為了避免得到極小的 `vruntime`，新創造的任務將被賦予與其親代任務相同的 `vruntime`，而陷入長時間睡眠的任務醒來時，會被指定略小於 `cfs_rq->min_vruntime`。

> The counts of total run time maintained by tasks grow more slowly for
heavier tasks. The task with the least run time counter is at the front of CFS’s runqueue. Newly created tasks are assigned a fictitious run time clock that, regardless of their weight, puts them at the end of the runqueue. This means that new, high priority tasks must wait for even the lowest priority task to run first. **If tasks are blocked** (doing I/O, sleeping, or handling a page fault) for more than one timeslice, **CFS additionally offers I/O compensation** by pushing them to the front of the runqueue.

3. (todo3)



#### 2.3.2 Weight function
在前一節我們談到了每個任務的 timeslice 是依據 *target latency* 權重所得，而權重是如何計算呢？
$$
w(n) = \frac{1024}{1.25^n}
$$
其中 n 是該任務的 nice level，數值愈小，優先級越大，CFS 中自 -20 到 19 共 40 個等級。


這個數值有兩個性質：
1. 假設僅有兩個任務相差 1 nice level，所獲得的時間差距約為 $\frac{1}{9} = 0.1111... \approx 10\%$，也就是
$$
\begin{equation}
    \begin{split}
    CPU\%_a = \frac{w(n_a)}{w(n_a) + w(n_b)} \\
    CPU\%_b = \frac{w(n_a)}{w(n_a) + w(n_b)} \\
    \\
    CPU\%_{diff} = CPU\%_a - CPU\%_b \approx \frac{1}{9}
    \end{split}
\end{equation}
$$
3. 由於核心使用定點數，我們可以預先計算好的權重 `sched_prio_to_weight` 和 `sched_prio_to_wmult`，而後者是倒數，用於計算除法；見[我另一份筆記](https://hackmd.io/@Kuanch/linux2024-collection/https%3A%2F%2Fhackmd.io%2F%40Kuanch%2Flinux-kernel-scheduler-notes3#Weight-Function-pp2)。

#### 2.3.3 Assigned time and virtual runtime
本節一部分已經在 2.3.1 被提及，我認為也是理解 CFS 排程的重點之一，首先做名詞解釋

* target latency: 又稱 scheduler period，是預估讓所有存在 runqueue 中的 runnable tasks 至少跑過一次的時間，或者亦可想像這是一個 runqueue round robin，即 `sysctl_sched_latency` (default to 6 ms)
* minimum granularity: 考量 task switching overhead，一旦任務數量過於龐大，target latency 切割給每個任務的 timeslice 會變得極小使得任務只足夠將時間用於 task switching 而沒有進展；故當任務數量超出預設值 `sched_nr_latency` (default to 8)，我們將延長 target latency 使得每個任務至少得到 `sysctl_sched_min_granularity` (default to 0.75 ms)。

    :::spoiler Source code
    ```c
    /*
     * The idea is to set a period in which each task runs once.
     *
     * When there are too many tasks (sched_nr_latency) we have to stretch
     * this period because otherwise the slices get too small.
     *
     * p = (nr <= nl) ? l : l*nr/nl
     */
    static u64 __sched_period(unsigned long nr_running)
    {
        if (unlikely(nr_running > sched_nr_latency))
            return nr_running * sysctl_sched_min_granularity;
        else
            return sysctl_sched_latency;
    }

    /*
     * Targeted preemption latency for CPU-bound tasks:
     *
     * NOTE: this latency value is not the same as the concept of
     * 'timeslice length' - timeslices in CFS are of variable length
     * and have no persistent notion like in traditional, time-slice
     * based scheduling concepts.
     *
     * (to see the precise effective timeslice length of your workload,
     *  run vmstat and monitor the context-switches (cs) field)
     *
     * (default: 6ms * (1 + ilog(ncpus)), units: nanoseconds)
     */

    unsigned int sysctl_sched_latency			= 6000000ULL;
    static unsigned int normalized_sysctl_sched_latency	= 6000000ULL;

    /*
     * Minimal preemption granularity for CPU-bound tasks:
     *
     * (default: 0.75 msec * (1 + ilog(ncpus)), units: nanoseconds)
     */
    unsigned int sysctl_sched_min_granularity			= 750000ULL;
    static unsigned int normalized_sysctl_sched_min_granularity	= 750000ULL;

    /*
     * This value is kept at sysctl_sched_latency/sysctl_sched_min_granularity
     */
    static unsigned int sched_nr_latency = 8;
    ```
    :::


* Virtual runtime
前述已有提及，我們的目標是使所有的任務獲得相同的 virtual runtime，以及排程器每次皆選取擁有最小 virtual runtime 的任務，virtual runtime 的計算如下：
$$
vruntime = delta\_exec \times \frac{weight\_of\_nice_0}{task\_weight}
$$
其中 $delta\_exec$ 即為實際執行的 wall time。Virtual runtime 和前述 Proportional-Share Scheduling 一節的內容互相呼應，想像兩個任務，權重分別為 1 和 $\frac{1}{3}$，前者從 6ms 中分配到 4.75ms 後者分配到 1.25ms，計算後兩者將有相同的 Virtual runtime。

更多細節亦可參考[我的另一篇筆記](https://hackmd.io/@Kuanch/linux2024-collection/https%3A%2F%2Fhackmd.io%2F%40Kuanch%2Flinux-kernel-scheduler-notes3#vruntime)

#### 2.3.4 Runqueue

從 Linux Kernel code，我們可以觀察到 `cfs_rq` 維護了 `min_vruntime`，這是為了避免不公平性，考量以下兩種狀況；當一個任務執行期間不斷 sleeping，當其醒來時可能擁有遠小於其他任務的 `vruntime`，這會使其不斷擁有 CPU 直到其 `vruntime` 追上其他任務；我們檢查其 `vruntime` 是否小於 `min_vruntime`，若是，則使 `vruntime = min_vruntime`。

或者，當一個新任務被加入到 runqueue，由於其才被創造，若將 `vruntime` 設為零也會導致上述狀況；為了避免，我們將其 `vruntime` 設為它親代任務的 `vruntime`。

>When a new task is created via fork() and inserted into the runqueue, it inherits the virtual runtime from the parent: this prevents the exploit where a task can take control of the CPU by continuously forking itself.

圖 2.18 描繪了每個 CPU 對應一個 runqueue、runqueue 中存在不同 policies 包含 stop class, deadline class, real time class, fair class and idle class。其中 real time class 的任務排序仍是維持按照 0 - 99 priority，不同 priority 存有 list head 並將任務串鏈起來；而 fair class 則是樹狀 (rbtree)。

TODO: 運用 [Scheduling Internals](https://tontinton.com/posts/scheduling-internals/) 圖解 CFS 運作原理

### 2.4 EEVDF Scheduler
由於本書編撰基於 v6.6 之前，EEVDF 之細節著墨較少，但在深入了解核心程式碼之前，閱讀本節仍可窺見 EEVDF 的設計哲學。

在 v6.6 之後，CFS 功能轉換為基於 EEVDF，它更有效率、更少啟發式 (heuristic) 的動態調整，亦更穩定；CFS 致力於公平分配 CPU 時間，但許多任務不僅僅需要考慮獲得 CPU 時間的多寡，還需要考慮獲得 CPU 的時刻，也就是說，有些可能需要立即獲得少量的 CPU 時間，另一些則是可以接受較晚使用 CPU，而一旦使用，基於公平性，相同優先級下需要使用相對長的時間；**EEVDF 因能夠評估這樣的行為，能夠有效減少任務切換的次數，增加 throughput，並且減少 response time。**

>  Unlike CFS, EEVDF is not just about fair CPU time distribution; it specifically tackles the latency challenges CFS misses. With EEVDF, a process’s nice value signals its priority, with lower values indicating urgency.

一個顯而易見的區別是，CFS 在 rbtree 是以 vruntime 排序，而 EEVDF 是以 virtual deadline 排序；後者會不斷被更新、不斷獲取新的 virtual deadline 以滿足其對 response time 的需求。

本節也解釋了 EEVDF 中重要的名詞，分別是 *Virtual runtime*, *Eligible time*, *Virtual Deadline*, *Lag*, *Latency nice*，我們一一介紹。

* Latency nice (per-task)
    有別於 CFS 僅使用 nice level，EEVDF 引入了 latency-nice 的概念，這個值代表了任務對於延後使用 CPU 的敏感程度，越小表示其 response time 要越少，排程器應該盡快將 CPU 交予它，我們透過分配許多更近的 virtual deadline 給它已達到目的。
    
    > A lower value (latency nice) means the task should be relatively prioritized over others response-time-wise. In the EEVDF algorithm it is done by allocating the task nearer deadlines (shorter, more frequently-updated time slices).

* Eligible time (per-runqueue)
    Eligible time 即是指任務佇列的平均 vruntime，而當任務的 vruntime 小於該值，就會被認為是 "eligible"，唯有當一個任務是 eligible 並且有最小 virtual deadline 才會被執行，否則會被認為是可以推遲執行的。
    > The purpose of the Eligible time is to ensure fairness among tasks by delaying the execution of later (non-eligible) entities.

* Lag (per-task)
    $lag = vruntime - eligible\ time$ ，即是一個任務被排程器忽視的程度，當是正值表示被 "under-served"，負值責被繼續忽視、延遲執行，直到 eligible 為止。
    > A positive value means the task is under-served, so that it should be given some advantage. A negative value means the task has been over-served, so it should be postponed until it will be eligible again.
    

* Virtual Deadline (per-task)
    可以想像每一次排程執行完畢會重新計算下一次 virtual deadline，Latency nice 越低的任務會有越早的 virtual deadline，故更早被執行。本文中並沒有說明如何被計算，可以參考[我的另一篇筆記](https://hackmd.io/@Kuanch/linux2024-collection/https%3A%2F%2Fhackmd.io%2F%40Kuanch%2Flinux-kernel-scheduler-notes3#Earliest-Eligible-Virtual-Deadline-First-EEVDF-pp4)。
 

具體的順序為何呢？(todo1)

1. Enqueue placing
    當一個任務被加入佇列，按照其 lag 值放入 rbtree

:::info
EEVDF rbtree 究竟是按照 virtual deadline 排序或是以 lag 排序呢？抑或都是？
:::

3. Picking next task
    搜尋是 eligible 的任務 (rbtree 的右子樹)，
5. Updating deadline
6. Dequeuing

### 2.5 Multiprocessing
由於現代的計算機主要透過增加核心頻率，以及擴增其核心數以提升運算能力，故對於多核心的處理在排程器中至關重要；談到多核心，最簡單的合作方式是 synchornization，也就是

1. 對於核心共享的資料，每一次仍是只有單個核心能夠存取
2. 透過 signal and wait 的方式使核心的前後順序符合預期

前者像是對於 runqueue 的存取，如果只有一個 global runqueue，每一次有 core 需要取出或放入任務，都需要暫時限制其他 core 的存取，但顯然這是昂貴且效率差勁的，這也是為什麼 CFS 採取 per core runqueues；然而這遠遠不夠，我們經常需要透過 IPC 的方式在多核心中溝通，如 cache coherence。

除了同步議題，另一個隨之出現的問題是 Load Balance；想像一個 core 忙得不可開交導致 throughput 下降，而另一個卻沒有任務可以執行，合理地分配任務到各個核心對系統效能也很重要。

#### 2.5.1 Load
考慮到 Load 的時候，有兩種觀點

1. top-down
    聚焦於整個系統的負載以及資源分配
2. bottom-up (centered around the task)
    以任務觀點來看，也比較聚焦於 CPU 資源，顯然此處主要討論本項
    
**Per-Entity Load Tracking**
在討論 PELT 之前，不免要討論我們要如何定義每一個任務的 Load？使用 weight 是不合理的，它僅反應使用 core 的優先權，很可能一個有極高優先權的任務幾乎不對系統構成負載。

故回到本質，一個任務對系統的負載是使用 core 的時間，而一個 core 的當前負載是其任務的負載之和；為了要評估任務的負載，Linux 核心引入了 PELT 的機制，用於紀錄一個代表負載的統計量，並且需要時刻更新。

給定一段時間，並將它切割成數段 *contributing units*，當某個任務在某一段使用了 core，則獲得 positive contributing units，並且考慮歷史資料構成該任務的負載
$$
Load_t = u_t + u_{t-1}\times y^1 +u_{t-2}\times y^2 + ... = \sum_{i=0}u_{t-i} \times y^i
$$

但我們不可能紀錄其過去每一段時間的 positive contributing units，也不可能準確紀錄 $y^i$，故其退化成
$$
Load_t = u_t \times (1 - y) + u_{t-1} \times y
$$
僅消耗常數時間以及空間就能計算 PELT。

PELT 不但能夠被排程器用於了解每一個 CPU 當前的負載，也可用於 CPU frequency scaling，也就是後續會提到的 DVFS，換句話說，PELT 事實上提供了一個指標幫助我們了解當前多核心系統的狀態並被多種排程、計算模組應用。

#### Load balancing
回到 Load balancing，雖然維護一個 global runqueue 會更容易進行 load balancing，但如同前述提到 per-core runqueue 對於 cache 會是更友好的作法，除了考慮到 lock 以外，tasks 在同一 CPU 底下被喚醒，其資料可能仍被保存，反之可能需要重新由 L2 cache 甚至主記憶體中重新取得資料。
>Remaining in the same list also benefits the tasks themselves, as it is likely that the data they are working on is still in the core’s private cache when they are rescheduled.

CFS 提供了三種主要的 load balancing 模式

1. **Regular**
    標準模式，週期性的做 load balancing
3. **Idle**
    當一個 CPU 即將變成 idle 時開始尋找額外任務
3. **Wakeup**
    為某一個特定的實體 (entity) 發動 load balancing，將其放到一個最少負載，且距離其原先 CPU 最近 shared level 的 CPU。(?)
    
    >balancing may be executed for a specific entity that is being awakened, placing it on the least occupied core **within the nearest shared cache level**.

然而，僅僅是能將任務放入輕量負載的 CPU 是不足的，load balancing 還需要具有遷移 (migrating) 能力。

**Migrating tasks**
排程器有能力將任務從某一 CPU 遷移到另一 CPU，這樣的行為是十分昂貴的，如上述多次提到的 cache coherence 問題，有時候甚至不遷移會有更好的效果。Mirgating tasks 包含
1. **pull migration**
    當 core 變成 idle，排程器選擇將某一些任務由其他 core 轉移。
    >
    
3. **push migration**
    有個 kernel thread 專門監控 imbalance，若發生則由其他忙碌的 core 主動將任務轉移。
    
    >..., at each system tick, a special, high priority task is started to balance the workload across the system. At the same time, whenever the idle task is executed, it attempts to steal tasks from another core.
    
另外需要被提及的是，我們可以出於各種原因將任務固定在某一個 CPU 不使它被遷移，相關詞如 CPU Isolation, cpu affinity, cpuset, cpumaks 等，譬如網路 I/O 相關，主要為該任務的 response time 考量。

在遷移時還需要考慮記憶體架構，如 Unified Memory Access (UMA) 或 Non-unified Memory Access (NUMA)，前者的特色是對於所有的 core 來說，它們存取記憶的時間和頻寬都一樣，通常會有 false sharing (cachelines being used at the same time) 的問題；由於記憶體共享，遷移任務的成本可能遠小於 NUMA；可參考[Linux 核心設計: 記憶體管理](https://hackmd.io/@sysprog/linux-memory)

![numa](https://hackmd.io/_uploads/BJg09ULyXR.jpg)

我們甚至可以考慮一種混合版本，UMA + NUMA，透過分群的方式讓部分 core 有共享記憶體，在群內的 core 遷移任務時更節約有效率。

**Scheduling Domains**
具體如何分群呢？此處就需要講到 scheduling domains 和 CPU groups，一群 CPU 組成了一個 scheduling domain，其中又可分為幾個 CPU groups，後者為前者的子集。一般來說，同樣 domain 的 CPU 之間的任務遷移較為有效率且較常發生，domains 之間的任務遷移也是有可能的，但開銷會顯著較大。

參考 [Linux 核心手冊](https://docs.kernel.org/scheduler/sched-domains.html)：
>Balancing within a sched domain occurs between groups. That is, each group is treated as one entity. The load of a group is defined as the sum of the load of each of its member CPUs, and only when the load of a group becomes out of balance are tasks moved between groups.
>
>In SMP, the parent of the base domain will span all physical CPUs in the node. Each group being a single physical CPU. Then with NUMA, the parent of the SMP domain will span the entire machine, with each group having the cpumask of a node. Or, you could do multi-level NUMA or Opteron, for example, might have just one domain covering its one NUMA level.

注意在 NUMA 中，scheduling domain 可能是多層的，也就是一個 domain 包含另一個 domain。

:::info
本段文字似乎暗示 scheduling domain 是一個可隨軟體變動的 CPU 集合，但原先認為這樣的設定必須要配合 CPU 與記憶體架構，或者只是釋例而已，需要再釐清。

>Each set is denoted by **a domain containing two CPU groups**, one for each processor. In this case, the groups contain only one CPU. Balancing within this domain can take place quite frequently as it involves no additional cost, and it is triggered by minor load differences between the domain’s groups.
>
>**To enable tasks to migrate between domains, a higher-level domain encompasses all the CPUs.** This domain is divided into two CPU groups, one for each pair. Although the two groups are balanced, the scheduler does not attempt to balance the load within a group. This domain seeks to balance the load less frequently and is more tolerant of load imbalances.

看過核心手冊後，顯然是可以透過軟體設定的：
> The implementor should read comments in include/linux/sched/sd_flags.h: SD_* to get an idea of the specifics and what to tune for the SD flags of a sched_domain.
>
>Architectures may override the generic domain builder and the default SD flags for a given topology level by creating a sched_domain_topology_level array and calling set_sched_topology() with this array as the parameter.

且似乎該模組本身就具備檢查 CPU topology 並進行設定的能力。
:::


#### 2.5.3 CPU Isolation
我認為本小節是在說明 CPU affinity 的實際應用；有些應用如 10 Gigabit Ethernet 需要不被打斷，故我們可以設定某一 CPU 專注於執行該任務，並不被排程器干擾。

#### 2.5.4 Unintened Side Effects
本節提到 2016 年發表的論文 "*The Linux Scheduler: a Decade of Wasted Cores*"，點出四件當時 Linux 針對多核處理器排程演算法的問題，包括
1. **The Goupr Imbalance Bug**
    使用高於或低於平均負載判斷是否需要 load balancing 可能是有誤的。
3. **Scheduling Group Construction**
5. **Overload on Wakeup**
    CPU affinity 可能會被濫用，希望重回該 CPU group 的要求可能會讓其負載更重。
7. **Missing Scheduling Domains**

相關細節待補。(todo2)


### 2.6 Energy-Aware Scheduling (EAS)
由於行動裝置的普及，能效考量無疑成為排程器中的一大重點，其主要透過兩個功能實作：
1. **big.LITTLE CPUIdle**

    CPUIdle 透過選擇 CPU 運行模式來實作最小功耗，譬如在沒有任務執行時將 CPU 轉入某些低功耗模式；但仍有許多問題要考量，譬如越低功耗的模式，在喚醒時通常需要花費更多時間，若是頻繁的切換反而對整機功耗造成問題，應該要在確定會閒置一段長時間才將 CPU 轉入低功耗模式，並在需要 CPU 時喚醒最淺層閒置的 CPU。

    >However, the more power savings a mode gives, the longer it will take to wake the CPU up. Additionally, many modes consume energy when shifting in and out of them, meaning the CPU should be idle for a sufficient period of time for entering the mode to be worth it. Most CPUs have multiple idle modes with different tradeoffs between power savings and latency.

    此外，哪些任務應該要執行在大核 (big)、哪些執行在小核 (LITTLE) 上？什麼時候任務遷移？往哪遷移？當新任務被指派，它應該要在何處先被執行，以便在遷移時消耗最少資源？這些都是值得被討論的問題。

2. **DVFS cpufreq**

    Dynamic Voltage and Frequency Scaling (DVFS) 關注 CPU 電壓與頻率的調整，當電壓越高，雖然越耗電，但能夠運行的頻率越高，效能表現就越好；這個功能透過 `cpufreq` 介面實作，並且相容於 Arm 架構，與 big.LITTLE 同時運行能夠有更好的功耗表現。
    
上述兩種方式都是現代行動裝置常見的功能，然而，CFS 或 EEVDF 是以 throughput 為考量，與良好的功耗表現常是無關，甚至有時是互相矛盾的，如何滿足前述考量，是排程器在現代行動裝置上的一大課題。

TODO: 閱讀並彙整以下:
* [從 big.LITTLE 到 EAS](https://hackmd.io/@sysprog/big-little)
* [Energy Aware scheduling Report](https://hackmd.io/@Daichou/ByzK-S60E)
* [Linux 核心設計: Scheduler(8): Energy Aware Scheduling](https://hackmd.io/@RinHizakura/Skelo1WY6)

---

## 研究 EEVDF 並量化其表現
> 參照 [CPU 排程器研究 (2023 年)](https://hackmd.io/@sysprog/BJh9FdlS2)

在 Linux v6.8 環境比對 EEVDF 和 [BORE](https://github.com/firelzrd/bore-scheduler) 之效能表現，予以分析。利用 [schbench](https://kernel.googlesource.com/pub/scm/linux/kernel/git/mason/schbench)

### 使用 QEMU + Buildroot + GDB 追蹤排程器行為

由於目前 Buildroot 部分功能暫時僅支援到 v6.6，以下我們先以該版本理解 EEVDF 排程器。

環境安裝請參考[Kernel Analysis with QEMU + Buildroot + GDB](https://hackmd.io/@Kuanch/linux2024-collection/https%3A%2F%2Fhackmd.io%2F%40Kuanch%2Flinux2024-termproj-prereq#Kernel-Analysis-with-QEMU--Buildroot--GDB)。

使用以下命令在模擬環境中執行 Linux 核心
```shell
qemu-system-x86_64 \
    -M pc \
    -kernel ./output/images/bzImage \
    -drive file=./output/images/rootfs.ext2,if=virtio,format=raw \
    -append "root=/dev/vda console=ttyS0 nokaslr" \
    -net user,hostfwd=tcp:127.0.0.1:3333-:22 \
    -net nic,model=virtio \
    -nographic \
    -S -s
```

並連結 gdbserver
```shell
$ gdb output/build/linux-6.6.18/vmlinux
(gdb) target remote :1234
(gdb) b start_kernel
(gdb) c
Continuing.

Breakpoint 1, start_kernel () at init/main.c:875
875     {
(gdb) lx-ps
      TASK          PID    COMM
0xffffffff8200a880   0   swapper
(gdb) where
#0  start_kernel () at init/main.c:875
#1  0xffffffff821b303c in x86_64_start_reservations (real_mode_data=real_mode_data@entry=0x13b30 <exception_stacks+31536> <error: Cannot access memory at address 0x13b30>)
    at arch/x86/kernel/head64.c:556
#2  0xffffffff821b318a in x86_64_start_kernel (real_mode_data=0x13b30 <exception_stacks+31536> <error: Cannot access memory at address 0x13b30>) at arch/x86/kernel/head64.c:537
#3  0xffffffff810001dd in secondary_startup_64 () at arch/x86/kernel/head_64.S:449
#4  0x0000000000000000 in ?? ()
```

並追蹤以下簡單程式在 CPU 排程器裡頭的行為：
```c
#include <stdio.h>
#include <syslog.h>
#include <signal.h>

static int stop = 0;

static void sigint_handler(int sig)
{
    (void) sig;
    stop = 1;
}

int main()
{
    int i = 0;
    signal(SIGINT, sigint_handler);
    while (1) {
        printf("%d\n", i);
        i++;
        if (stop) {
            syslog(LOG_INFO, "Caught SIGINT, exiting now");
            break; // Add break statement to exit the while loop
        }
    }
}
```

#### 創造任務行程
設定 breakpoint，觀察程式啟動時的呼叫順序
```shell
(gdb) break copy_process
Breakpoint 1 at 0xffffffff8107a650: file kernel/fork.c, line 2245.
(gdb) break wake_up_new_task
Breakpoint 2 at 0xffffffff810ba790: file kernel/sched/core.c, line 4853.
// run ./infiniteloop in qemu console
(gdb) c
Continuing.

Breakpoint 1, copy_process (pid=pid@entry=0x0 <fixed_percpu_data>, 
    trace=trace@entry=0, node=node@entry=-1, args=args@entry=0xffffc900001b7e98)
    at kernel/fork.c:2245
2245    {
(gdb) c
Continuing.

Breakpoint 2, wake_up_new_task (p=p@entry=0xffff8880033c1980)
    at kernel/sched/core.c:4853
4853    {
(gdb) where
#0  wake_up_new_task (p=p@entry=0xffff8880033c1980) at kernel/sched/core.c:4853
#1  0xffffffff8107c04e in kernel_clone (args=args@entry=0xffffc900001b7e98)
    at kernel/fork.c:2940
#2  0xffffffff8107c386 in __do_sys_clone (clone_flags=<optimized out>, 
    newsp=<optimized out>, parent_tidptr=<optimized out>, 
    child_tidptr=<optimized out>, tls=<optimized out>) at kernel/fork.c:3052
#3  0xffffffff8107c724 in __se_sys_clone (tls=<optimized out>, 
    child_tidptr=<optimized out>, parent_tidptr=<optimized out>, 
    newsp=<optimized out>, clone_flags=<optimized out>) at kernel/fork.c:3036
#4  __x64_sys_clone (regs=<optimized out>) at kernel/fork.c:3036
#5  0xffffffff818aa5e7 in do_syscall_x64 (nr=<optimized out>, regs=0xffffc900001b7f58)
    at arch/x86/entry/common.c:51
#6  do_syscall_64 (regs=0xffffc900001b7f58, nr=<optimized out>)
    at arch/x86/entry/common.c:81
#7  0xffffffff81a000ea in entry_SYSCALL_64 () at arch/x86/entry/entry_64.S:120
#8  0x0000000000000000 in ?? ()
```
單單是此處輸出我們就有十分多訊息可以探討，可以參考 [Linux 核心搶佔](https://hackmd.io/@sysprog/linux-preempt)；而由於此處著重於理解 EEVDF 運作，其餘補充內容寫至 [使用 GDB 在 Linux 核心中追蹤程式](https://hackmd.io/@Kuanch/linux2024-collection/https%3A%2F%2Fhackmd.io%2F%40Kuanch%2Flinux2024-termproj-prereq#%E4%BD%BF%E7%94%A8-GDB-%E5%9C%A8-Linux-%E6%A0%B8%E5%BF%83%E4%B8%AD%E8%BF%BD%E8%B9%A4%E7%A8%8B%E5%BC%8F)。

值得注意的是，當 `wake_up_new_task` 時才能夠看到該任務被賦予 pid，這發生在 `kernel_clone` 中的 `pid = get_task_pid(p, PIDTYPE_PID);`，其在 `copy_process` 呼叫之後；至此，我們完成行程的創建。

#### 加入排程
接下來我們關注如何將行程加入 `cfs_rq`，設定中斷點 `enqueue_task_fair`：
```shell
(gdb) b enqueue_task_fair
Breakpoint 3 at 0xffffffff810c06a0: file kernel/sched/fair.c, line 6600.
(gdb) c
Continuing.

Breakpoint 3, enqueue_task_fair (rq=0xffff888007a2b900, p=0xffff8880033c1980, flags=8)
    at kernel/sched/fair.c:6600
6600    {
(gdb) where
#0  enqueue_task_fair (rq=0xffff888007a2b900, p=0xffff8880033c1980, flags=8)
    at kernel/sched/fair.c:6600
#1  0xffffffff810ba561 in enqueue_task (flags=8, p=0xffff8880033c1980, 
    rq=0xffff888007a2b900) at kernel/sched/core.c:2102
#2  activate_task (rq=rq@entry=0xffff888007a2b900, p=p@entry=0xffff8880033c1980, 
    flags=flags@entry=8) at kernel/sched/core.c:2132
#3  0xffffffff810ba8f9 in wake_up_new_task (p=p@entry=0xffff8880033c1980)
    at kernel/sched/core.c:4876
// ...skip
(gdb) b place_entity
Breakpoint 4 at 0xffffffff810c0500: file kernel/sched/fair.c, line 5065.
(gdb) c
Continuing.

Breakpoint 4, place_entity (cfs_rq=cfs_rq@entry=0xffff888007a2b940, 
    se=se@entry=0xffff8880033c1a00, flags=flags@entry=8) at kernel/sched/fair.c:5065
5065            u64 vslice, vruntime = avg_vruntime(cfs_rq);
(gdb) where
#0  place_entity (cfs_rq=cfs_rq@entry=0xffff888007a2b940, 
    se=se@entry=0xffff8880033c1a00, flags=flags@entry=8) at kernel/sched/fair.c:5065
#1  0xffffffff810c078b in enqueue_entity (flags=8, se=0xffff8880033c1a00, 
    cfs_rq=0xffff888007a2b940) at kernel/sched/fair.c:5205
#2  enqueue_task_fair (rq=0xffff888007a2b900, p=0xffff8880033c1980, flags=8)
    at kernel/sched/fair.c:6626
#3  0xffffffff810ba561 in enqueue_task (flags=8, p=0xffff8880033c1980, 
    rq=0xffff888007a2b900) at kernel/sched/core.c:2102
#4  activate_task (rq=rq@entry=0xffff888007a2b900, p=p@entry=0xffff8880033c1980, 
    flags=flags@entry=8) at kernel/sched/core.c:2132
#5  0xffffffff810ba8f9 in wake_up_new_task (p=p@entry=0xffff8880033c1980)
    at kernel/sched/core.c:4876
// ...skip
```


再來我們可以關注到 `0xffff8880033c1980` 是被傳入的 `task_struct` 記憶體位置之值，我們可以透過 `lx-ps` 找到其 pid 並與其 `p->pid` 比對：
```shell
(gdb) p ((struct task_struct *)0xffff8880033c1980)->pid
$1 = 111
(gdb) lx-ps
...
0xffff8880033c0cc0  108  sh
0xffff8880033c1980  111  sh
```

故我們可以確定該位於 `0xffff8880033c1980` 的 `struct task_struct *p` 為我們所執行的 C 程式；如前述所提，排程器實際上操作的是 `sched_enttiy`，故我們專注於 `p->se`，其能夠觀察到 EEVDF 的各項數值：
```
(gdb) set print pretty on
(gdb) p *se
$2 = {
  load = {
    weight = 1048576,
    inv_weight = 4194304
  },
  run_node = {
    __rb_parent_color = 1,
    rb_right = 0x0 <fixed_percpu_data>,
    rb_left = 0x0 <fixed_percpu_data>
  },
  deadline = 7357816932,
  min_deadline = 7298321980,
  group_node = {
    next = 0xffff8880033c00b8,
    prev = 0xffff8880033c00b8
  },
  on_rq = 0,
  exec_start = 0,
  sum_exec_runtime = 0,
  prev_sum_exec_runtime = 0,
  vruntime = 7357441932,
  vlag = 0,
  slice = 750000,
  nr_migrations = 0,
  avg = {
    last_update_time = 19060785152,
    load_sum = 47518,
    runnable_sum = 5512088,
    util_sum = 5512088,
    period_contrib = 800,
    load_avg = 1024,
    runnable_avg = 116,
    util_avg = 116,
    util_est = {
      enqueued = 0,
      ewma = 0
    }
  }
}
```

另外注意，此時當我們檢查 `cfs_rq->tasks_timeline.rb_root.rb_node` rbtree 結構，會發現當前 `sched_entity` 仍不在其中，需要直到 `place_entity` 結束後的 `__enqueue_entity(cfs_rq, se);` 才被加入 rbtree 中：

```shell
(gdb) b __enqueue_entity
Breakpoint 4 at 0xffffffff810bdcf0: file kernel/sched/fair.c, line 833.
(gdb) c
Continuing.

Breakpoint 4, __enqueue_entity (cfs_rq=cfs_rq@entry=0xffff888007a2b940, se=se@entry=0xffff8880033c1a00)
    at kernel/sched/fair.c:833
(gdb) p cfs_rq->tasks_timeline.rb_root.rb_node
$5 = (struct rb_node *) 0x0 <fixed_percpu_data>
(gdb) finish
Run till exit from #0  __enqueue_entity (cfs_rq=cfs_rq@entry=0xffff888007a2b940, 
    se=se@entry=0xffff8880033c1a00) at kernel/sched/fair.c:833
enqueue_entity (flags=<optimized out>, se=0xffff8880033c1a00, cfs_rq=0xffff888007a2b940)
    at kernel/sched/fair.c:5217
5217            se->on_rq = 1;
(gdb) p cfs_rq->tasks_timeline
$8 = {rb_root = {rb_node = 0xffff8880033c1a10}, rb_leftmost = 0xffff8880033c1a10}
(gdb) p &se->run_node 
$11 = (struct rb_node *) 0xffff8880033c1a10
```

接著我們回到 `wake_up_new_task`，如同 [Linux 核心搶佔](https://hackmd.io/@sysprog/linux-preempt#1-%E7%9B%AE%E5%89%8D%E8%A1%8C%E7%A8%8B%E5%9F%B7%E8%A1%8C%E5%AE%8C%E8%A2%AB%E5%88%86%E9%85%8D%E7%9A%84%E6%99%82%E9%96%93%E5%BE%8C) 所說，由於前後任務的優先權相同，此時搶佔應該由計時器觸發，`check_preempt_curr` 無法觸發搶佔：
```
(gdb) b check_preempt_wakeup
Breakpoint 5 at 0xffffffff810c1170: file kernel/sched/fair.c, line 8144.
(gdb) c
Continuing.

Breakpoint 5, check_preempt_wakeup (rq=0xffff888007a2b900, p=0xffff8880033c1980, 
    wake_flags=4) at kernel/sched/fair.c:8144
8144    {
(gdb) step
(gdb) step
(gdb) p rq->curr
$3 = (struct task_struct *) 0xffff8880033c0cc0

(gdb) p se
$4 = (struct sched_entity *) 0xffff8880033c26c0
(gdb) p pse
$5 = (struct sched_entity *) 0xffff8880033c1a00

p &((struct task_struct *)0xffff8880033c0cc0)->se
$6 = (struct sched_entity *) 0xffff8880033c26c0

(gdb) p &((struct task_struct *)0xffff8880033c1980)->se
$6 = (struct sched_entity *) 0xffff8880033c1a00
```
可以看到此時 CPU 仍在執行前一個 PID 為 108 的 shell 任務，且此時 `se` 和 `pse` 分別屬於「當前執行任務」和「我們正在加入佇列任務」的 `sched_entity`。

接著我們透過以下方式確定確定 `check_preempt_wakeup` 的回傳點
```shell
(gdb) set logging file disassemble_output.txt
(gdb) set logging on
(gdb) disassemble /r check_preempt_wakeup
(gdb) set logging off
// find "ret" in disassemble_output.txt
(gdb) tbreak *0xffffffff810c1208
(gdb) tbreak *0xffffffff810c1244
(gdb) c
Continuing.

Temporary breakpoint 9, 0xffffffff810c1208 in check_preempt_wakeup (rq=<optimized out>, 
    p=<optimized out>, wake_flags=<optimized out>) at kernel/sched/fair.c:8221
8221    }
(gdb) where
#0  0xffffffff810c1208 in check_preempt_wakeup (rq=<optimized out>, p=<optimized out>, 
    wake_flags=<optimized out>) at kernel/sched/fair.c:8221
#1  0xffffffff810b72e0 in check_preempt_curr (rq=rq@entry=0xffff888007a2b900, 
    p=p@entry=0xffff8880033c1980, flags=flags@entry=4) at kernel/sched/core.c:2224
#2  0xffffffff810ba917 in wake_up_new_task (p=p@entry=0xffff8880033c1980) at kernel/sched/core.c:4878
#3  0xffffffff8107c04e in kernel_clone (args=args@entry=0xffffc900001b7e98) at kernel/fork.c:2940
// ...skip
```
可以發現直到函式返回在整個函式執行完後，並不滿足中間執行的任何返回條件；並且 `fair_sched_class` 並無設定 `task_woken`，故 `wake_up_new_task` 至此執行結束，我們已經將任務 `0xffff8880033c1980` 加入 `rq->cfs_rq` 之中。

過程中，另一件值得關注的是 `p->thread_info.flags`，後續排程器將透過確定當前任務 `curr->thread_info.flags` 判斷是否要觸發重新排程、選取下一個任務，各項 TIF 對於後續排程理解至關重要，但礙於篇幅我們暫且跳過。

#### 觸發搶佔

前述講到，由於前後執行任務皆屬於 `fair_sched_class`，搶佔應該由計時器觸發，於是我們設定 `entity_tick` 和 `resched_curr`，其將呼叫 `set_tsk_thread_flag` 將當前任務標註為**需要重新排程**，也就是當 `((struct task_struct *)...)->thread_info.flags` 為 8 時。，此時 `exit_to_user_mode_loop` 將觸發 `schedule()`：

```c
static unsigned long exit_to_user_mode_loop(struct pt_regs *regs,
					    unsigned long ti_work)
{
	/*
	 * Before returning to user space ensure that all pending work
	 * items have been completed.
	 */
	while (ti_work & EXIT_TO_USER_MODE_WORK) {
        ...
		if (ti_work & _TIF_NEED_RESCHED)
			schedule();
        ...
        ti_work = read_thread_flags();
    }

	/* Return the latest work state for arch_exit_to_user_mode() */
	return ti_work;
}
```

由於我們事實上不知道任務何時才會被挑選到，故設定當被挑選的下一個 `cfs_rq` 為 `0xffff888007a2b940` 時才觸發中斷，這是 `0xffff8880033c1980->se` 的 `cfs_rq` ：

```
(gdb) b __pick_eevdf if cfs_rq == 0xffff888007a2b940
(gdb) c
Continuing.

Breakpoint 6, pick_eevdf (cfs_rq=0xffff888007a2b940) at kernel/sched/fair.c:971
971             struct sched_entity *se = __pick_eevdf(cfs_rq)
(gdb) where
#0  pick_eevdf (cfs_rq=0xffff888007a2b940) at kernel/sched/fair.c:971
#1  pick_next_entity (curr=0x0 <fixed_percpu_data>, cfs_rq=0xffff888007a2b940) at kernel/sched/fair.c:5368
#2  pick_next_task_fair (rq=rq@entry=0xffff888007a2b900, prev=prev@entry=0xffff8880033b1980, 
    rf=rf@entry=0xffffc900001cfe70) at kernel/sched/fair.c:8350
#3  0xffffffff818b20d1 in __pick_next_task (rf=0xffffc900001cfe70, prev=0xffff8880033b1980, 
    rq=0xffff888007a2b900) at kernel/sched/core.c:6006
#4  pick_next_task (rf=0xffffc900001cfe70, prev=0xffff8880033b1980, rq=0xffff888007a2b900)
    at kernel/sched/core.c:6516
#5  __schedule (sched_mode=sched_mode@entry=0) at kernel/sched/core.c:6662
#6  0xffffffff818b2700 in schedule () at kernel/sched/core.c:6772
#7  0xffffffff81109b40 in exit_to_user_mode_loop (ti_work=8, regs=<optimized out>)
    at kernel/entry/common.c:159
// ... skip
```
在運行數次後，我們可以發現 `0xffff8880033c1a00` (`0xffff8880033c1980->se`) 已經存在 `cfs_rq` rbtree 中，為其 `rb_right`：
```shell
(gdb) n
882             if (curr && (!curr->on_rq || !entity_eligible(cfs_rq, curr)))
(gdb) p *cfs_rq->tasks_timeline->rb_root.rb_node
$13 = {__rb_parent_color = 1, rb_right = 0xffff8880033c1a10, rb_left = 0xffff888002924050}
(gdb) p &((struct task_struct *)0xffff8880033c1980)->se.run_node 
$15 = (struct rb_node *) 0xffff8880033c1a10
(gdb) p cfs_rq->tasks_timeline
$16 = {rb_root = {rb_node = 0xffff888002907350}, rb_leftmost = 0xffff888002924050}
(gdb) p ((struct sched_entity *) 0xffff888002907340)->run_node 
$17 = {__rb_parent_color = 1, rb_right = 0xffff8880033c1a10, rb_left = 0xffff888002924050}
```
顯然本次仍有一 `sched_entity` 具有更小的 `deadline`，且也是 `cfs_rq` 中具有最小 `deadline` 的；若印出左中右三個節點的 `deadline` 可以看到即是按照大中小排序。

本次在 `__pick_eevdf` 執行順序為
1. `if (left->min_deadline == se->min_deadline) break;`
2. `if (!best_left || (s64)(best_left->min_deadline - best->deadline) > 0) return best`
3. `if (se->deadline == se->min_deadline) return se;`

最終選擇左子節點 `0xffff888002924050` 作為下一個任務；我們可以執行幾次直到 `0xffff8880033c1a10` 成為左子節點後，觀察其 `se` 各項數值的變化：
```shell
(gdb) p cfs_rq->tasks_timeline
$50 = {rb_root = {rb_node = 0xffff888002907350}, rb_leftmost = 0xffff8880033c1a10}
(gdb) p ((struct task_struct *)0xffff8880033c1980)->se
$51 = {load = {weight = 1048576, inv_weight = 4194304}, run_node = {
    __rb_parent_color = 18446612682113053520, rb_right = 0x0 <fixed_percpu_data>, 
    rb_left = 0x0 <fixed_percpu_data>}, deadline = 10134845796, min_deadline = 10134845796, group_node = {
    next = 0xffff888007a2c268, prev = 0xffff888002924078}, on_rq = 1, exec_start = 38204580406, 
  sum_exec_runtime = 177587192, prev_sum_exec_runtime = 33042362, vruntime = 10134728280, 
  vlag = 9990933450, slice = 750000, nr_migrations = 0, avg = {last_update_time = 38204579840, 
    load_sum = 47457, runnable_sum = 48320082, util_sum = 47446461, period_contrib = 744, 
    load_avg = 1023, runnable_avg = 1018, util_avg = 999, util_est = {enqueued = 0, ewma = 0}}}
(gdb) c
Continuing.

Breakpoint 4, __pick_eevdf (cfs_rq=cfs_rq@entry=0xffff888007a2b940) at kernel/sched/fair.c:877
877             struct rb_node *node = cfs_rq->tasks_timeline.rb_root.rb_node;
(gdb) p ((struct task_struct *)0xffff8880033c1980)->se
$52 = {load = {weight = 1048576, inv_weight = 4194304}, run_node = {
    __rb_parent_color = 18446612682113171536, rb_right = 0x0 <fixed_percpu_data>, 
    rb_left = 0x0 <fixed_percpu_data>}, deadline = 10151034583, min_deadline = 10151034583, group_node = {
    next = 0xffff888002907378, prev = 0xffff888007a2c268}, on_rq = 1, exec_start = 38378171360, 
  sum_exec_runtime = 193753892, prev_sum_exec_runtime = 177587192, vruntime = 10150894980, 
  vlag = 10134845796, slice = 750000, nr_migrations = 0, avg = {last_update_time = 38378170368, 
    load_sum = 46999, runnable_sum = 48120830, util_sum = 14966698, period_contrib = 282, 
    load_avg = 1023, runnable_avg = 1023, util_avg = 318, util_est = {enqueued = 0, ewma = 0}}}
```

並依照 `vruntime` 公式計算
$$
vruntime = delta\_exec \times \frac{weight\_nice\_0}{task\_weight}=delta\_exec\times \frac{2^{10}}{task\_weight} 
$$
由於希望避免使用除法，故以權重的倒數再右移計算
$$
task\_weight = \frac{2^{32}}{inv\_task\_weight}= 1048576 = 1024 \cdot1024 = \frac{2^{32} \cdot 2^{10}}{4194304}
$$
即
$$
vruntime = delta\_exec \times \frac{inv\_task\_weight \cdot weight\_nice\_0}{2^{32}} = delta\_exec \times \frac{inv\_task\_weight \cdot 2^{10}}{2^{32}}
$$
我們將上述 `se` 各項數值代入驗算
$$
\frac{((193753892 - 177587192)*4194304)}{2^{22}} = 10150894980 - 10134728280=16166700
$$
左右式相等，證明 `se` 數值符合上述 $vruntime$ 公式；證明兩次 `__pick_eevdf` 之間會經歷一次 `update_curr`。

:::warning
另外特別的是，`se->deadline` - `se->vruntime` = 139603 < 750000，其他任務雖亦有差值恰好為 750000，但也有遠小於 750000 的狀況，是什麼機制造成？
:::

:::info
>[name=Kuanch]
>此處特別的是，我們是先取根結點，其若有左子樹，再取左子樹，因為有可能其左子樹為空，然而，為什麼不是直接取 `cfs_rq->tasks_timeline.rb_leftmost`？是因為其可能不是 eligible？那是否有可能保證 `rb_leftmost` 必為 eligible？

```c
static struct sched_entity *__pick_eevdf(struct cfs_rq *cfs_rq)
{
	struct rb_node *node = cfs_rq->tasks_timeline.rb_root.rb_node;
    ...
    while (node) {
		struct sched_entity *se = __node_2_se(node);

		/*
		 * If this entity is not eligible, try the left subtree.
		 */
		if (!entity_eligible(cfs_rq, se)) {
			node = node->rb_left;
			continue;
		}
        ...
        if (node->rb_left) {
            ...   
        }
    }
}
```
:::




### BORE (Burst-Oriented Response Enhancer)


### schbench

## TODO: 彙整排程器素材

整理以下素材:
* [淺談排程器演進的思考，從 CFS 到 EEVDF 有感](https://github.com/rsy56640/triviality/tree/master/content/sched-eevdf) / [討論](https://zhuanlan.zhihu.com/p/680182553)
* [CPU 排程器測試工具](https://hackmd.io/@RinHizakura/H1Eh3clIp)
* [Deep dive in the scheduler](https://static.linaro.org/connect/san19/presentations/san19-220.pdf), Vincent Guittot (2019)
* [System pressure and CPU capacity](https://lpc.events/event/17/contributions/1490/attachments/1182/2434/System%20pressure,%20compute%20capacity%20and%20scheduler.pdf), Vincent Guittot (2023)
* [Sched Ext: The pluggable Linux scheduler](https://www.socallinuxexpo.org/scale/21x/presentations/sched-ext-pluggable-linux-scheduler): 談及 CFS 的歷史背景和其限制
 
## TODO: 電子書和程式碼貢獻
> 《Demystifying the Linux CPU Scheduler》第七章的測試程式

