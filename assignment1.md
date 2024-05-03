# 2024q1 Homework1 (lab0)
contributed by < [Kuanch](https://github.com/Kuanch) >

### Reviewed by `Lccgth`
佇列實作部分應只貼出關鍵程式碼，例如 q_merge 中可省略變數宣告的程式碼，以文字敘述。
```c
for (struct list_head *p = head->next->next; p != head; p = p->next) {
    queue_contex_t *node = list_entry(p, queue_contex_t, chain);
    list_splice_init(node->q, merged_list->q);
}
q_sort(merged_list->q, descend);
```
在佇列實作部分可列出相應的 git commit 紀錄，方便日後優化時進行比較。

在 q_sort 與 list_sort 效能比較中，將 list_sort 的 likely 和 unlikely 巨集進行更動，會影響編譯時的最佳化，可以參考 [gcc 13.5 第 6.59 章](https://gcc.gnu.org/onlinedocs/gcc-13.2.0/gcc/Other-Builtins.html)中的 __builtin_expect。

>[name=Kuanch]
>1. 減少張貼的程式碼
>2. 將 likely 和 unlikely 補回進行實驗；在 qsort 與 listsort 在 `l1d` 以及 `mem_load_retired` 之[實驗](#L1-cache-分析)時已經補回。此外亦針對 likely 和 unlikely 產生的 asm 有[分析](#Impacts-of-likely-and-unlikely)。

### Reviewed by `lintin528`
github 上佇列實作分為多次 commit 在維護的時候會較方便。

## 開發環境
:::spoiler
```shell
$ uname -a
Linux 5.15.0-100-generic #110~20.04.1-Ubuntu SMP

$ lscpu
Architecture:                       x86_64
CPU op-mode(s):                     32-bit, 64-bit
Byte Order:                         Little Endian
Address sizes:                      39 bits physical, 48 bits virtual
CPU(s):                             8
On-line CPU(s) list:                0-7
Thread(s) per core:                 2
Core(s) per socket:                 4
Socket(s):                          1
NUMA node(s):                       1
Vendor ID:                          GenuineIntel
CPU family:                         6
Model:                              140
Model name:                         11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz
Stepping:                           1
CPU MHz:                            776.504
CPU max MHz:                        4200.0000
CPU min MHz:                        400.0000
BogoMIPS:                           4838.40
Virtualization:                     VT-x
L1d cache:                          192 KiB
L1i cache:                          128 KiB
L2 cache:                           5 MiB
L3 cache:                           8 MiB
NUMA node0 CPU(s):                  0-7
Vulnerability Gather data sampling: Mitigation; Microcode
Vulnerability Itlb multihit:        Not affected
Vulnerability L1tf:                 Not affected
Vulnerability Mds:                  Not affected
Vulnerability Meltdown:             Not affected
Vulnerability Mmio stale data:      Not affected
Vulnerability Retbleed:             Not affected
Vulnerability Spec rstack overflow: Not affected
Vulnerability Spec store bypass:    Mitigation; Speculative Store Bypass disabled via prctl and seccomp
Vulnerability Spectre v1:           Mitigation; usercopy/swapgs barriers and __user pointer sanitization
Vulnerability Spectre v2:           Mitigation; Enhanced IBRS, IBPB conditional, RSB filling, PBRSB-eIBRS SW sequence
Vulnerability Srbds:                Not affected
Vulnerability Tsx async abort:      Not affected

$ gcc --version
gcc (Ubuntu 9.4.0-1ubuntu1~20.04.2) 9.4.0
Copyright (C) 2019 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

$ valgrind --version
valgrind-3.15.0

$ cppcheck --version
Cppcheck 1.90
```
:::

## 實作指定的佇列操作

<s>
以下暫時僅展示有修正改進、有探討議題的操作，完整的程式碼請見 [Github](https://github.com/Kuanch/lab0-c/blob/queue/queue.c)。
大部分程式碼雖能通過測試，但可讀性不足，仍待重構，並將重構完畢的程式碼放上。
</s>

:::danger
你的洞見呢？
:::

### 佇列實作


#### `q_sort`

:::danger
程式碼列出是為了討論和檢討，倘若你沒有進行討論，就不要列出。讀者要看你的洞見。

>[name=Kuanch]
>依照老師和同學建議減少張貼的程式碼
:::

`q_sort` [要求 In-place 操作](##補充)，故使用 `LIST_HEAD(new_head);` 為實作重點；對比於 `malloc` 的動態分配(Dynamic Allocation)，此處為靜態分配(Static Allocation)，後者有幾項特點
1. 編譯期決定，編譯器管理生命週期且無需佔用作業系統資源
3. 由於由編譯器管理，不存在記憶體破碎 (memory fragmentation)
4. 分配在Stack上，其容量有限，大的物件可能會導致溢出 (stack overflow)

:::danger
無論標題和內文中，中文和英文字元之間要有空白字元 (對排版和文字搜尋有利)。

留意各式細節，唯有重視小處並步步為營，方可挑戰原始程式碼規模超越三千七百萬行的 Linux 核心。
:::

#### `q_merge`

本例中將前述常用的結構 `element_t` 改為 `queue_context_t`，每一個包含 `list_head` 的 `queue_context_t` 帶有一個佇列開頭 `node->q`，透過 `list_splice_init` 合併到第一個 `queue_context_t` 當中的佇列，之後便可進行排序。

我認為 `queue_context_t` 特別之處在於每一個元素都能夠再帶有一個佇列，所以也許可以考慮以下這種應用
```c
#include <stdio.h>
#include <stdlib.h>

struct list_head {
    struct list_head *next, *prev;
};

typedef struct {
    struct list_head list;
    int id;
    int runtime;
    int priority;
    double energy_cost;
    // other properties of tasks
    // ...
} task_t;

typedef struct {
    struct list_head q;     // Head of the queue of tasks
    struct list_head *other_task_queue; 
    int size;               // Number of tasks in the queue
    int id;                 // Identifier for the queue
} queue_context_t;

void link_task_queue(queue_context_t queue1, queue_context_t queue2) {
    queue1.other_task_queue = &queue2.q;
    queue2.other_task_queue = &queue1.q;
}

int main() {
    queue_context_t queue_task_type1;
    queue_context_t queue_task_type2;

    // initialize task queue, enqueue, schedule etc.
    // ...
        
    // link task queue
    link_task_queue(queue_task_type1, queue_task_type2);

    return 0;
}
```
假設 `queue_task_type1` 和 `queue_task_type２` 為性質相似的任務佇列，在內可以儲存各自的任務，亦可走訪有相似性質的其他任務佇列。



### 相關資料結構及操作
#### `container_of`
:::info
<s>後來發現 Jserv 有更詳細的專文討論，請見[Linux 核心原始程式碼巨集: container_of](https://hackmd.io/@sysprog/linux-macro-containerof)</s>
> 本來就是指定教材，為何你要捨近求遠呢？ :notes: jserv

引用
> 請不要小看這巨集，畢竟大量在 Linux 核心原始程式碼採用的巨集，應有其獨到之處。在 container_of 巨集出現前，程式設計的思維往往是:
>
> 1. 給定結構體起始地址
> 2. 求出結構體特定成員的記憶體內容
> 3. 傳回結構體成員的地址，作日後存取使用
> ...

理解為 `->` 有了 `container_of` 後，可以依結構體成員的地址反向查詢結構體起始地址，無須儲存結構體起始地址、無須存取結構體成員的地址

另外是否也有功能是，可以作為統一的鏈結串列的介面，對於不同的應用，只需要設計不同的 `element_t` 並加入 `struct list_head *head`，便可使用 Linux Kernel 的固有函式走訪成員？
:::

`container_of` 是 linux kernel 實作佇列結構的重要函式，我認為有必要分析並了解其如何運作
```c
#ifndef container_of
#ifdef __LIST_HAVE_TYPEOF
#define container_of(ptr, type, member)                            \
    __extension__({                                                \
        const __typeof__(((type *) 0)->member) *__pmember = (ptr); \
        (type *) ((char *) __pmember - offsetof(type, member));    \
    })
#else
#define container_of(ptr, type, member) \
    ((type *) ((char *) (ptr) - offsetof(type, member)))
#endif
#endif
```
讓我們觀察下方未定義 `__LIST_HAVE_TYPEOF` 的版本開始；當我們已知 `list_head` 的記憶體位置 `ptr` 且已知結構大小，便可反向推算 `element_t` 之記憶體位置；考慮以下程式碼

```c
#include <stddef.h> // For offsetof
#include <stdio.h>

struct list_head {
  struct list_head *next;
  struct list_head *prev;
};

struct element_t {
  int data;
  struct list_head list;
};

int main() {
  struct element_t element;
  struct list_head *list_ptr = &element.list;
  printf("size of element_t: %lu\n", sizeof(struct element_t)); // 24

  printf("Original list_head address: %p\n",
         (void *)list_ptr);                        // 0x7ffe406db2a8
  printf("the offset of list in element_t: %lu\n", // 8
         offsetof(struct element_t, list));

  // Calculate the start of element_t from list_ptr
  struct element_t *element_ptr =
      (struct element_t *)((char *)list_ptr - offsetof(struct element_t, list));

  printf("Original element_t address: %p\n",
         (void *)&element); // 0x7ffe406db2a0
  printf(
      "Calculated element_t address using container_of: %p\n", // 0x7ffe406db2a0
      (void *)element_ptr);

  return 0;
}
```
有幾處需要說明
1. `offsetof(struct element_t, list)` 應該為 4，因為 `element_t` 的位址應該是 `list` 的位址 - `sizeof(data)`，但此處卻為 8？
Ans: 因為 data alignment，參考[你所不知道的 C 語言：記憶體管理、對齊及硬體特性](https://hackmd.io/@sysprog/c-memory#data-alignment)
如何驗證？使用 `__attribute__` 修改 `element_t`
```c
struct __attribute__((packed)) element_t {
  int data;
  struct list_head list;
};

// After introducing __attribute__
// offsetof(struct element_t, list) == 4
// sizeof(struct element_t) == 20 
```

2. 為什麼轉型至 `(char *)` 且以 `(void *)` 取得？
Ans: 是兩件事情。轉型至 `(char *)` 係因其為 1 byte，能夠在 byte 之上操作。 `(void *)` 稱做通用指標，此處僅為與 `%p` 互動輸出正確結果。

:::danger
查閱 C 語言規格書，看是否存在「通用」指標這說法。

>[name=Kuanch] 沒有！
>查閱 ISO/IEC 9899:2018 不存在 "generic pointer" (我自作主張翻譯作通用指標) 一詞
>引用 ISO/IEC 9899:2018
>> A pointer to void may be converted to or from a pointer to any object type. A pointer to any object type may be converted to a pointer to void and back again; the result shall compare equal to the original pointer.
>
>更多的是稱作 "a pointer to void"，文中共提到 10 次。
>搜尋引擎的結果，包含它本身都錯把 generic pointer 與 `(void *)` 畫上等號。
:::

---

## 分析與引入 `lib/list_sort.c`
### 修改 `lib/list_sort.c`
原先想直接與 linux/lib/list_sort.c 編譯，但屢試無果，故參考 komark06 的方式移植到 `queue.c` [2023q1 Homework1 (lab0)](https://hackmd.io/@kart81604/HkGHY1cTs#%E5%B0%87-liblist_sort-%E5%BC%95%E5%85%A5%E8%87%B3-lab0-c-%E5%B0%88%E6%A1%88)。

<s>
此處不列出所有修改，只列出需要討論的部分，完整修改請見 [queue.c](https://github.com/Kuanch/lab0-c/blob/list_sort/queue.c#L53)。
</s>

:::danger
「本該如此」的事，就不要寫。
:::

#### `merge`
```diff
-__attribute__((nonnull(2,3,4)))
-static struct list_head *merge(void *priv, list_cmp_func_t cmp,
-				struct list_head *a, struct list_head *b)
+__attribute__((nonnull)) struct list_head *merge(struct list_head *a,
+                                                 struct list_head *b)
```


#### `merge_final`


```diff
-        if (unlikely(!++count))
-            cmp(priv, b, b);
+        // if (!++count)
+        //    strcmp(list_entry(b, element_t, list)->value,
+        //           list_entry(b, element_t, list)->value);
```

#### `list_sort`
```diff
-        if (likely(bits)) {
-            struct list_head *a = *tail, *b = a->prev;
+        if (bits) {
+            struct list_head *a = *tail, *b = a->prev;
```

### q_sort 與 list_sort 效能比較
使用 `perf` 進行分析，測試程式如下

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "queue.h"

int main() {
    struct list_head *q = q_new();
    int list_size = 100000;
    for (int i = 0; i < list_size; i++) {
        char s = (char) (rand() % 26 + 'a');
        q_insert_tail(q, &s);
    }
    // q_sort(q, true);
    list_sort(q);
    q_free(q);
}
```

以下使用 <s>`perf state -r 5`</s>`perf stat -r 5`，可以參考 [Linux 效能分析工具: Perf](https://wiki.csie.ncku.edu.tw/embedded/perf-tutorial)

```
 Performance counter stats for './q_sort' (5 runs):

         7,2825.90 msec task-clock                #    1.000 CPUs utilized            ( +-  0.28% )
               362      context-switches          #    4.968 /sec                     ( +-  6.35% )
                 8      cpu-migrations            #    0.115 /sec                     ( +- 22.14% )
              3582      page-faults               #   49.186 /sec                     ( +-  0.01% )
    2957,7153,0020      cycles                    #    4.061 GHz                      ( +-  0.22% )
     625,4618,0370      instructions              #    0.21  insn per cycle           ( +-  0.00% )
     208,1336,9257      branches                  #  285.796 M/sec                    ( +-  0.00% )
          124,9445      branch-misses             #    0.01% of all branches          ( +-  0.59% )
  1,4784,0455,5899      slots                     #   20.301 G/sec                    ( +-  0.22% )
     285,2832,2723      topdown-retiring          #      1.9% retiring                ( +-  1.54% )
     846,9593,7809      topdown-bad-spec          #      5.7% bad speculation         ( +-  8.49% )
      69,6204,8854      topdown-fe-bound          #      0.5% frontend bound          ( +-  2.68% )
  1,3755,0196,3845      topdown-be-bound          #     92.0% backend bound           ( +-  0.25% )

        72.831 +- 0.206 seconds time elapsed  ( +-  0.28% )


         Performance counter stats for './list_sort' (5 runs):

     7,3189.10 msec task-clock                #    1.000 CPUs utilized            ( +-  0.17% )
           384      context-switches          #    5.252 /sec                     ( +-  5.30% )
            12      cpu-migrations            #    0.158 /sec                     ( +- 34.55% )
          3582      page-faults               #   48.942 /sec                     ( +-  0.01% )
2972,8367,5031      cycles                    #    4.062 GHz                      ( +-  0.18% )
 626,1171,8129      instructions              #    0.21  insn per cycle           ( +-  0.00% )
 208,3703,4674      branches                  #  284.701 M/sec                    ( +-  0.00% )
      126,7828      branch-misses             #    0.01% of all branches          ( +-  1.30% )
1,4852,8263,7616      slots                     #   20.294 G/sec                    ( +-  0.17% )
 282,5978,1184      topdown-retiring          #      1.9% retiring                ( +-  1.87% )
 908,7784,9886      topdown-bad-spec          #      6.0% bad speculation         ( +-  6.01% )
  63,9197,5271      topdown-fe-bound          #      0.4% frontend bound          ( +-  4.39% )
1,3783,7450,0815      topdown-be-bound          #     91.7% backend bound           ( +-  0.33% )

        73.193 +- 0.123 seconds time elapsed  ( +-  0.17% )

```
整理重要項為表格 :

|  | my sort | list sort |
| -------- | -------- | -------- |
| task clock     | 72825.9 msec   | 73189.1 msec     |
| context switches     | 362     | 384     |
| page faults     | 3582     | 3582     |
| branches misses     | 124,9445     | 126,7828     |

在作業說明中提到
>用 bottom up 實作 merge sort 對 cache 較友善，因為過程中就是一直合併，cache 被參照到的機會更大。
而 top down 是會先做 partition 再來 merge，但 partition 本身對 cache 不友善，在 cache 移進移出（內容不斷更新，導致 cache thrashing。

以及 [改進 lib/list_sort.c](https://hackmd.io/@sysprog/Hy5hmaKBh#TODO-%E8%A7%A3%E9%87%8B-Linux-%E6%A0%B8%E5%BF%83%E7%9A%84-liblist_sortc) 中提到
> lib/list_sort.c 中實作的 merge sort 演算法採用 bottom-up 策略。它首先將給定的串列轉換為一個以空結尾的單向鏈結串列。該演算法維護一個稱為 "pending" 的數據結構，表示等待進一步合併的已排序子串列。

:::warning
雖然可以見到幾乎無差距，但原先設想是，我的實作方法是遞迴，每次都會切出一半的資料進行兩次遞迴(即我理解的 top-down 而非 bottom-up)，理論上應該會導致 cache thrashing、有大量的 page-faults？

此處效能測量可能有誤，待釐清。

3 / 7 更新
可能被編譯器最佳化了？除了調整最佳化等級，可以用 perf + 觀察 asm 看是否有顯著差距
:::

### 設計新實驗

以下實驗針對幾點做改進
1. 執行時間樣本的分析，如分布 (distribution)
2. 設計具有某些特性的輸入，而非隨機輸入
3. 增加 `perf` 試驗數

#### 時間複雜度

我們依然分別隨機產生資料，但本次係從 10 個資料點，每次增加 10 個資料並進行排序，直到 20000 個資料點，紀錄 CPU cycles 後繪製成下圖

![sort_perf](https://hackmd.io/_uploads/rk5dwpM0p.png)

可以見到我所實作的 `q_sort` 隨著資料增加，CPU cycles **有指數增加的趨勢**，相對於 list sort 接近於線性的增加，在大量資料的狀況下已有明顯差距，以下為測試程式碼

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <x86intrin.h>

#include "queue.h"
#include "dudect/cpucycles.h"

void static inline q_sort_wrapper(struct list_head *head) {
    bool predefined_descend = true;
    q_sort(head, predefined_descend);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <sort_name>\n", argv[0]);
        return 1;
    }
    char* sort_name = argv[1];
    char* save_file;
    void (*sort_func)(struct list_head *head);
    if (strcmp(sort_name, "qsort") == 0) {
        sort_func = &q_sort_wrapper;
        save_file = "qsort_perf.txt";
    }
    else if (strcmp(sort_name, "listsort") == 0) {
        sort_func = list_sort;
        save_file = "listsort_perf.txt";
    }
    else {
        printf("Invalid sort name: %s\n", sort_name);
        return 1;
    }
    struct list_head *q = q_new();
    FILE *fileout = fopen(save_file, "w");
    for (int num_data = 10; num_data <= 20000; num_data += 10) {
        q = q_new();
        for (int i = 0; i < num_data; i++) {
            char s = (char) (rand() % 26 + 'a');
            q_insert_tail(q, &s);
        }
        _mm_mfence();
        int64_t start = cpucycles();
        sort_func(q);
        int64_t end = cpucycles();
        // save the result to text for analysis later
        fprintf(fileout, "%d %ld\n", num_data, end - start);
    }
}
```

#### 分析樣本分布

:::info
多少樣本數才足以取得具有代表性的統計量？

問題的答案因母體分配而異，若假設母體為常態分佈，則根據 《Educational research competencies for analysis and application》 (Gay, L. R., 1992)，至少需有 30 個樣本數，小於 30 則影響大多數檢定之檢定力，如 z-test；又根據 〈[A Practical Guide for Using Statistical Tests to Assess Randomized Algorithms in Software Engineering](https://web-backend.simula.no/sites/default/files/publications/Simula.approve.64.pdf)〉(Andrea Arcuri, 2011)，至少 30 次試驗是一個普遍適用的標準，但對於部分檢定，如 U test，100 次仍不足，而 1000 次則是推薦的次數。
:::

以隨機產生的資料，每次有 20000 筆資料，進行 1000 次排序，刪除了大於第 99 分位數 ($P_{99}$) 的資料後，得到的 CPU Cycle 如下圖 

![sort_dist_random](https://hackmd.io/_uploads/Syi341X06.png)

作圖後可以顯著地看到兩者分布的有顯著差異，我們可以判斷兩者母體服從於不同分布；而就視覺上看，其母體可能非常態分布，若需要執行統計檢定需要注意，可以透過 K-S 檢定判斷是否服從常態分布。


#### 增加 `perf` 試驗數和不同樣本特性

本次除了增加 `perf` 的試驗數外，每一次試驗都將進行一千次的排序，即

```c
    int num_test = 1000;
    for (int test = 0; test < num_test; test++) {
        q = q_new();
        data_prod(q);
        // print_list(q);
        _mm_mfence();
        int64_t start = cpucycles();
        sort_func(q);
        int64_t end = cpucycles();
        // printf("Cycles: %ld\n", end - start);
        // print_list(q);
        fprintf(fileout, "%ld ", end - start);
    }
```

猜測 `list_sort` 可能在特定場景能發揮更好的效能，故我們準備 2 種資料，依據這 2 種資料，使用 `perf stat -r 1000` 得到以下四種結果

1. 大致排序完成而尾部少數不等

我們以以下程式碼產生資料
```c
void static inline almost_sorted_data(struct list_head *head) {
    for (int i = 0; i < 10000; i++) {
        char s = (char) (rand() % 26 + 'a');
        q_insert_tail(head, &s);
    }
    q_sort(head, false);
    for (int i = 0; i < 100; i++) {
        char s = (char) (rand() % 26 + 'a');
        q_insert_tail(head, &s);
    }
}

```

```shell
 Performance counter stats for './sort_dist qsort almost_sort' (1000 runs):

           3137.17 msec task-clock                #    0.998 CPUs utilized            ( +-  0.01% )
                 8      context-switches          #    2.544 /sec                     ( +-  1.39% )
                 0      cpu-migrations            #    0.000 /sec                   
           35,5163      page-faults               #  112.949 K/sec                    ( +-  0.00% )
     131,3528,1701      cycles                    #    4.177 GHz                      ( +-  0.01% )
     206,3582,6760      instructions              #    1.57  insn per cycle           ( +-  0.00% )
      45,1489,8824      branches                  #    1.436 G/sec                    ( +-  0.00% )
       1,0356,1613      branch-misses             #    2.29% of all branches          ( +-  0.01% )
     656,3148,1975      slots                     #   20.872 G/sec                    ( +-  0.01% )
     203,3289,0494      topdown-retiring          #     30.9% retiring                ( +-  0.02% )
     154,4270,1641      topdown-bad-spec          #     23.2% bad speculation         ( +-  0.06% )
     118,3940,4591      topdown-fe-bound          #     18.1% frontend bound          ( +-  0.03% )
     180,5324,4299      topdown-be-bound          #     27.8% backend bound           ( +-  0.03% )

          3.144975 +- 0.000249 seconds time elapsed  ( +-  0.01% )


 Performance counter stats for './sort_dist listsort almost_sort' (1000 runs):

           2889.82 msec task-clock                #    0.999 CPUs utilized            ( +-  0.01% )
                 7      context-switches          #    2.421 /sec                     ( +-  1.51% )
                 2      cpu-migrations            #    0.692 /sec                     ( +-  1.12% )
           35,5164      page-faults               #  122.833 K/sec                    ( +-  0.00% )
     120,8325,0954      cycles                    #    4.179 GHz                      ( +-  0.01% )
     189,2326,3768      instructions              #    1.57  insn per cycle           ( +-  0.00% )
      41,4924,8982      branches                  #    1.435 G/sec                    ( +-  0.00% )
       1,0522,7313      branch-misses             #    2.54% of all branches          ( +-  0.01% )
     603,1795,1240      slots                     #   20.861 G/sec                    ( +-  0.01% )
     189,2327,8820      topdown-retiring          #     31.3% retiring                ( +-  0.01% )
     151,3862,3056      topdown-bad-spec          #     25.4% bad speculation         ( +-  0.05% )
     120,6359,0248      topdown-fe-bound          #     20.0% frontend bound          ( +-  0.02% )
     141,9245,9115      topdown-be-bound          #     23.3% backend bound           ( +-  0.04% )

          2.892066 +- 0.000265 seconds time elapsed  ( +-  0.01% )
```
整理重要項為表格 :

|  | my sort | list sort |
| -------- | -------- | -------- |
| task clock     | 3137.17 msec   | 2889.82 msec     |
| context switches     | 8     | 7     |
| page faults     | 35,5163     | 35,5164     |
| instructions    | 206,3582,6760 | 189,2326,3768 |
| branches misses     | 1,0356,1613     | 1,0522,7313     |


2. 反向排序完成

以以下程式碼產生測試資料
```c
void static inline reverse_sorted_data(struct list_head *head) {
    for (int i = 0; i < 10000; i++) {
        char s = (char) (rand() % 26 + 'a');
        q_insert_head(head, &s);
    }
    q_sort(head, true);
}
```

```shell
Performance counter stats for './sort_dist qsort reverse_sort' (1000 runs):

           3198.79 msec task-clock                #    1.002 CPUs utilized            ( +-  0.01% )
                11      context-switches          #    3.447 /sec                     ( +-  1.05% )
                 1      cpu-migrations            #    0.313 /sec                     ( +-  2.77% )
           35,1647      page-faults               #  110.201 K/sec                    ( +-  0.00% )
     133,7968,5032      cycles                    #    4.193 GHz                      ( +-  0.01% )
     206,4549,2457      instructions              #    1.55  insn per cycle           ( +-  0.00% )
      44,8644,2627      branches                  #    1.406 G/sec                    ( +-  0.00% )
       1,0598,1816      branch-misses             #    2.36% of all branches          ( +-  0.01% )
     666,4662,4310      slots                     #   20.886 G/sec                    ( +-  0.01% )
     201,2466,6948      topdown-retiring          #     30.4% retiring                ( +-  0.02% )
     167,2699,5905      topdown-bad-spec          #     24.8% bad speculation         ( +-  0.06% )
     111,9575,3395      topdown-fe-bound          #     16.8% frontend bound          ( +-  0.05% )
     188,1787,0393      topdown-be-bound          #     28.0% backend bound           ( +-  0.03% )

          3.191458 +- 0.000349 seconds time elapsed  ( +-  0.01% )


 Performance counter stats for './sort_dist listsort reverse_sort' (1000 runs):

           2869.04 msec task-clock                #    0.997 CPUs utilized            ( +-  0.01% )
                13      context-switches          #    4.517 /sec                     ( +-  0.78% )
                 0      cpu-migrations            #    0.000 /sec                   
           35,1649      page-faults               #  122.183 K/sec                    ( +-  0.00% )
     120,0394,6287      cycles                    #    4.171 GHz                      ( +-  0.01% )
     191,8827,7875      instructions              #    1.59  insn per cycle           ( +-  0.00% )
      40,7025,6305      branches                  #    1.414 G/sec                    ( +-  0.00% )
       1,0781,1929      branch-misses             #    2.65% of all branches          ( +-  0.01% )
     600,1660,7175      slots                     #   20.853 G/sec                    ( +-  0.01% )
     190,6409,8749      topdown-retiring          #     31.8% retiring                ( +-  0.02% )
     169,4586,5555      topdown-bad-spec          #     27.8% bad speculation         ( +-  0.05% )
     105,9116,5972      topdown-fe-bound          #     18.0% frontend bound          ( +-  0.03% )
     134,1547,6897      topdown-be-bound          #     22.4% backend bound           ( +-  0.04% )

          2.878567 +- 0.000240 seconds time elapsed  ( +-  0.01% )
```
整理重要項為表格 :
|  | my sort | list sort |
| -------- | -------- | -------- |
| task clock     | 3198.79 msec   | 2869.04 msec     |
| context switches     | 11     | 13     |
| page faults     | 35,1647     | 35,1649     |
| instructions    | 206,4549,2457 | 191,8827,7875 |
| branches misses     | 1,0598,1816     | 1,0781,1929     |


新實驗後我們可以暫時整理三點：
1. list sort 確實顯著快於我所寫的 qsort
2. 引述相關文獻，就實驗次數檢視本實驗是否具證據力
3. 為什麼比較不好仍待驗證：
    * page-fault 沒有特別大，為什麼？因 cache 足夠大？
    * instructions 較多，多了什麼？

#### L1 cache 分析
和老師討論後發現原先 `perf` 並沒有顯示出 Cache 項次，在補上了 `likely` 及 `unlikely` 後，數據如下

```
 Performance counter stats for './perf_sort qsort' (1000 runs):

       1,1367,2224      L1-dcache-loads                                               ( +-  0.08% )  (36.89%)
       1,0174,7077      L1-dcache-loads-misses    #   91.49% of all L1-dcache accesses  ( +-  0.01% )  (37.03%)
           40,2304      LLC-loads                                                     ( +-  0.83% )  (37.03%)
              3121      LLC-loads-misses          #    0.91% of all LL-cache accesses  ( +-  3.67% )  (37.03%)
       1,0315,2129      l1d.replacement                                               ( +-  0.02% )  (36.12%)
            2,2600      l1d_pend_miss.l2_stall                                        ( +-  9.23% )  (35.98%)
          113,4894      mem_load_retired.l1_hit                                       ( +-  0.55% )  (35.98%)
       1,0398,5941      mem_load_retired.l1_miss                                      ( +-  0.01% )  (35.98%)
       8,4183,5218      cycle_activity.cycles_l1d_miss                                     ( +-  0.02% )  (35.98%)
       8,4118,9368      cycle_activity.stalls_l1d_miss                                     ( +-  0.02% )  (35.98%)
       1,0465,9441      mem_inst_retired.all_loads                                     ( +-  0.01% )  (35.98%)

         0.3565312 +- 0.0000646 seconds time elapsed  ( +-  0.02% )


 Performance counter stats for './perf_sort listsort' (1000 runs):

       1,1180,5254      L1-dcache-loads                                               ( +-  0.06% )  (35.94%)
       1,0282,2597      L1-dcache-loads-misses    #   92.29% of all L1-dcache accesses  ( +-  0.01% )  (37.05%)
           55,1223      LLC-loads                                                     ( +-  0.79% )  (37.88%)
              5174      LLC-loads-misses          #    1.06% of all LL-cache accesses  ( +-  3.52% )  (37.88%)
       1,0396,6378      l1d.replacement                                               ( +-  0.01% )  (37.43%)
            2,7826      l1d_pend_miss.l2_stall                                        ( +-  5.49% )  (36.33%)
          102,7050      mem_load_retired.l1_hit                                       ( +-  2.02% )  (35.50%)
       1,0478,8193      mem_load_retired.l1_miss                                      ( +-  0.01% )  (35.50%)
       8,5502,9344      cycle_activity.cycles_l1d_miss                                     ( +-  0.02% )  (35.50%)
       8,5046,0227      cycle_activity.stalls_l1d_miss                                     ( +-  0.02% )  (35.50%)
       1,0576,1424      mem_inst_retired.all_loads                                     ( +-  0.01% )  (35.50%)

         0.3605272 +- 0.0000668 seconds time elapsed  ( +-  0.02% )
```

整理重要項為表格 :
|  | my sort | list sort |
| -------- | -------- | -------- |
| task clock     | 3198.79 msec   | 2869.04 msec     |
| context switches     | 11     | 13     |
| page faults     | 35,1647     | 35,1649     |
| instructions    | 206,4549,2457 | 191,8827,7875 |
| branches misses     | 1,0598,1816     | 1,0781,1929     |

#### Impacts of likely and unlikely


### 引入到 lab0-c 專案
在 `queue.c` 加入 
```diff
+bool do_lsort(int argc, char *argv[]) {...}
...
+    ADD_COMMAND(lsort, "Sort the queue with list sort in Linux Kernel", "");
```
即可使用 `lsort` 呼叫 `list_sort` 進行排序。
我們也可以加入 `option time_sort`，用以快速測量排序的速度
```diff
+    struct timespec start, end;
+    if (current && exception_setup(true)) {
+        clock_gettime(CLOCK_MONOTONIC, &start);
+        list_sort(current->q);
+        clock_gettime(CLOCK_MONOTONIC, &end);
+        if (time_sort) {
+            uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
+            report(4, "Time to sort: %ld microsecond", delta_us);
+        }
+    }
...
+    add_param("time_sort", &time_sort, "If report the time of sort", NULL);
```
效果如下
```
option fail 0
option malloc 0
option time_sort 1 
new
ih RAND 100000
lsort
free

// l = []
// l = [rajxbcvef qxakpf adhydugvm ... ]
// Time to sort: 30542 microsecond
// l = [aaacprq aaahp aaalbkd ... ]
```
## 提供新的命令 shuffle
實作請見 [Github commit](https://github.com/Kuanch/lab0-c/commit/0409218acfd39434e993531e1bb30a089313cf42)
使用作業說明中提供的[測試程式](https://hackmd.io/@sysprog/linux2024-lab0/%2F%40sysprog%2Flinux2024-lab0-d#%E6%B8%AC%E8%A9%A6%E7%A8%8B%E5%BC%8F)，計算卡方值以及繪圖

由於我們希望檢驗演算法輸出的資料是否符合均勻分布 (uniform distribution)，故我們假設期望值

$E=100000/24 \thickapprox 4166$

即隨機 shffle 100000次，平均分配到 24 種狀況，每種狀況平均應該出現 4166 次。
依照測試程式計算，樣本之卡方值為 30.16，依照自由度 5 查表，由於 20.52 < 30.16，得到 p-value = 0.001，故我們可以推論，在設定 $\alpha=0.05$ 之下，不拒絕虛無假設，**依照抽樣結果，本演算法得到的資料分布不是均勻分布的可能性小於 0.001**。

能夠見到畫出的圖片亦近似於均勻分布
![test_shuffle](https://hackmd.io/_uploads/BkPeKU7aa.png)


## 研讀論文〈Dude, is my code constant time?〉
完整開發請見 [Github branch fix_dudect](https://github.com/Kuanch/lab0-c/tree/fix_dudect)

<s>
TL;DR
</s>

:::danger
不要濫用 TL;DR，誠實面對自己，闡述你的洞見，並接受他人批評指教，從而及早發現自身認知不足。
:::

本節做到的是
1. 理解並記錄本論文如何透過統計檢定，推論演算法是否為線性複雜度
2. 理解並記錄計算 CPU Cycle 的做法以及問題
3. 如何解決樣本不滿足檢定前提 (解決後 CI 即通過)


[Dude, is my code constant time?](https://eprint.iacr.org/2016/1123.pdf) 使用統計方法判斷固定輸入資料與隨機輸入資料的執行時間是否有不同，若有則表示演算法可能會因為輸入資料而有不同的效能，導致了非線性複雜度的可能。(結合 Time attack，應該有更好的表達方式，再修改)

> We first measure the execution time for two different input data classes, and then check whether the two timing distributions are statistically different.

文中亦闡明了演算法的過程，即
1. Measure execution time
2. Apply post-processing
3. Apply statistical test

我們可以依照上述過程找到專案中的相應程式碼，亦可參照該論文釋出的程式碼 [dudect.h](https://github.com/oreparaz/dudect/blob/master/src/dudect.h)。

### Measure execution time
首步為計算 CPU cycles，定義於 `dudect/cpucycles.h`
```c
static inline int64_t cpucycles(void)
{
#if defined(__i386__) || defined(__x86_64__)
    unsigned int hi, lo;
    __asm__ volatile("rdtsc\n\t" : "=a"(lo), "=d"(hi));
    return ((int64_t) lo) | (((int64_t) hi) << 32);
#elif defined(__aarch64__)
    // ....
#endif
}
```
透過指令 `rdtsc` ，即 Read Time-Stamp Counter，該指令會傳回自 CPU 啟動以來的循環 cycle 數，其中 `lo` 為低位的 32 位元，hi 為高位的 32 位元，`((int64_t) lo) | (((int64_t) hi) << 32)` 即可組成 CPU cycles。

這樣的計算具有幾個特徵，
1. 不考慮 out-of-order，在計算途中可能會混入其他指令；也是論文中提到要進行後處理的主因。
2. 假定 CPU 頻率為定值，而事實上可能有高有低

特別的是，[dudect.h](https://github.com/oreparaz/dudect/blob/master/src/dudect.h) 中的 `cpucycles()` 為
```c
static inline int64_t cpucycles(void) {
  _mm_mfence();
  return (int64_t)__rdtsc();
}
```

`_mm_mfence()` 應是指令 `MFENCE` ，其作用為確保在指令 `rdtsc` 之前，先前的所有記憶體讀寫都應該被完成，故在計算 CPU cycles 時便不會引入先前其他指令。

:::spoiler 為何 `MFENCE` 能夠使 `rdtsc` 更準確計算 CPU cycles (ChatGPT)
> 1. Serialization of Memory Operations: MFENCE ensures that all load (read) and store (write) operations that appear before the MFENCE in the program order are completed before any load or store operations that come after the MFENCE. It effectively draws a line in the sand, saying "no memory operation that comes after this point can be completed until all memory operations before this point are done."
> 2. Impact on rdtsc: The rdtsc instruction reads the time-stamp counter, which counts the number of cycles since the processor was reset. It's a non-serializing instruction, which means it doesn't by itself ensure that previous instructions have completed. By placing an MFENCE before rdtsc, you can ensure that all prior memory operations have been fully carried out before the cycle count is read.
> 3. Preventing Speculative Execution: Speculative execution is another form of out-of-order execution where the processor guesses the path of execution and begins executing ahead. If the guess is incorrect, the results are discarded. MFENCE prevents the results of any speculative execution that involve memory operations from being committed until all previous memory operations are confirmed to be completed.
::: 

### Apply post-processing
Student's t-test 用於估計小樣本的未知標準差的常態分布之期望值，若樣本數大則可使用中央極限定理求其母體參數；該論文使用 Welch’s t-test 作為檢定方法，其基於 Student's t-test。

比較 `dudect/fixture.c` 可得知並未實作 `prepare_percentiles()` ，這可能導致不符合 Welch’s t-test 假設；由於其假設樣本來自於常態分布，而執行時間則是左偏態(執行時間短的任務佔大多數)，故我們需要捨棄左側一部份樣本來達到常態分布假設。

### Apply statistical test

在 `dudect/fixture.c` 中，可以找到 `update_statistics()`，比對 `dudect.h`，缺乏移除左側長尾的部分
```diff
-static void update_statistics(const int64_t *exec_times, uint8_t *classes)
+static void update_statistics(const int64_t *exec_times,
+                              uint8_t *classes,
+                              const int64_t *percentiles)
 {
     for (size_t i = 0; i < N_MEASURES; i++) {
         int64_t difference = exec_times[i];
@@ -74,6 +109,14 @@ static void update_statistics(const int64_t *exec_times, uint8_t *classes)
 
         /* do a t-test on the execution time */
         t_push(t, difference, classes[i]);
+
+        // t-test on cropped execution times, for several cropping thresholds.
+        for (size_t crop_index = 0; crop_index < DUDECT_NUMBER_PERCENTILES;
+             crop_index++) {
+            if (difference < percentiles[crop_index]) {
+                t_push(t, difference, classes[i]);
+            }
+        }
     }
 }
```

在加入這部分後，線上 CI 通過，達到 100/100，見 [fix_dudect](https://github.com/Kuanch/lab0-c/tree/fix_dudect)。

:::danger
指出論文和程式碼實作的出入之處。
:::

## 補充
* In-place 操作: 引用 ChatGPT
> Here are the key reasons why in-place operations hold significant importance:
> 1. Limited Memory Resources
> 2. Performance Optimization
> 3. Real-time Constraints
> 4. Atomicity and Synchronization
> 5. Reducing Fragmentation
* Why don't we replace all memory allocations with static? (stupid question but useful) 引用 ChatGPT
 1. Data might not be known at compile time
 2. Precise life management might be necessary
 3. Large data might not fit into stack


## 疑難雜症
### enable Git workflow
```shell
$ make
Fatal: GitHub Actions MUST be activated.
Check this article: https://docs.github.com/en/actions/managing-workflow-runs/disabling-and-enabling-a-workflow
Then, execute 'make' again.
make: *** [Makefile:34: .git/hooks/applied] Error 1
```
參考 [作業說明 - GitHub Actions 設定](https://hackmd.io/@sysprog/linux2024-lab0/%2F%40sysprog%2Flinux2024-lab0-a#GitHub-Actions-%E8%A8%AD%E5%AE%9A)
:::danger
留意作業說明！
:::


## tic-tac-toe

### 蒙地卡羅搜尋樹 (Monte Carlo Tree Search) 如何運作
下棋的時候，我們是先在腦中知道可以走的走法、試誤，譬如炮二平五、俥五進六，發現某些棋對局勢更好 (更有獲勝的機會、想像不到對手有好的走法)，我們就選擇這樣的走法；對手根據我們的走法，也同樣選擇，若他的選擇在我們的估計內，我們會發現勝率越來越高，最終獲勝。

![維基百科-蒙地卡羅搜尋樹](https://hackmd.io/_uploads/r1DXW7PRT.png)


參考上圖，各項流程的意義是
**1. Simulation**

基於目前的局勢猜測勝率，此處實作是雙方隨機落子，即蒙地卡羅模擬法，僅有 `1.0` `0.0` `0.5` 三種離散結果。

**2. Backpropagation**

依照隨機模擬的結果，更新某一個落子的勝率以及被選到的次數；前一輪被選中的落子，會在下一輪才被 Bcakpropagation 更新勝率。

**3. Selection**

根據 UCT(upper confidence bound applied to trees) 「真正」選擇下一步，在 Backpropagation 當中預估的勝率越大，前項越大 (exoloitation)；在先前被選中的次數越少，後項越大，鼓勵選取未曾探索過的落子 (exploration)，兩者互相拮抗；實作中設定為：
$$
UCT = \cfrac{score_{ch}}{num\_visits_{ch}} \times \sqrt{\cfrac{2 \cdot ln(num\_total\_visits_{pr})}{num\_visits_{ch}}}
$$

* $score_{ch}$ 代表使用該子節點的勝率
* $num\_visits_{ch}$ 代表該子節點曾被使用於模擬的次數
* $num\_total\_visits_{pr}$ 代表該節點的親代節點曾被使用於模擬的次數

依照上式，我們可以推論
a. 當使用該節點在模擬中獲得更好的分數，越可能再次選擇它向下模擬
b. 當該節點的親代節點越常被使用，我們越可能再次選擇它向下模擬
c. 當該節點被選擇的次數太多，我們盡量避免使用它向下模擬


**4. Expansion**

如果某一個落子之後的結果還沒有被預估（葉節點），則將目前合法的落子初始化成他的子節點

( 如果這樣落子，會有多少勝率 (Simulation and Backpropagation) -> 思考這樣落子後，後續的發展 (Expansion) ) $\times N$ -> 在 $N$ 次落子中選擇一個勝率最高的走法 (Selection)


### 定點數取代浮點數

定點數就是將某一正數放大後的表示方法，縮放係數 (scalar factor) 係指將正數轉換成定點數需要的乘數；如 12.34 在縮放係數為 $2^8$ 時，即是 $12.34 \times 2^8=3159.04$；換句話說，當我們使用定點數來表示數字時，3159.04 代表 12.34。

我們可以將十進位轉為二進位來觀察
* 定點數
12.34:　`0b1100.01010111000010100011110101110000101000111101011100001`
3159.04:`0b110001010111.000010100011110101110000101000111101011100001`


可以觀察到，3159.04之定點數是將 12.34 向左位移 8 個位元，即
$$
12.34 << 8 \implies 12.34 \times 2^8 = 3159.04
$$

* IEEE 754
12.34:　 `0b0.10000000010.1000101011100001010001111010111000010100011110101110`
3159.04: `0b0.10000001010.1000101011100001010001111010111000010100011110101110`


而 IEEE 754 中，sign 與 mantissa 相同，而雙精度下指數項之 bias 為 $2^{10} - 1$；而當我們乘以 $2^8$，即是
$$
(2^{10} - 1) + 3 + 8 \implies 0b10000000010 + 0b00000001000 = 0b10000001010
$$ 


:::info
:::spoiler What is the purpose of "bias" in IEEE 754 ?
* 怎麼把十進位轉換為 IEEE 754 浮點數？
1. 寫作定點數 $8.125 \implies 1000.001$
2. 轉換為科學記號 $1000.001 \implies 1.000001 \times 2^3$
3. 指數項為 $2^{bias + n}$  $127 + 3 \implies 0b10000010$
4. 尾數項為科學記號小數後 $0b000001$
5. $8.125 \implies 0.10000010.00000100000000000000000$

如果只是 0.5 呢？因為是 $2^{-1}$，所以指數項是 127 - 1
0.025？127 - 2　即　0b0.01111101.00000000000000000000000
0.0125？127 - 3 即 0b0.01111100.00000000000000000000000
0.00625？127 - 4 即 0b0.01111011.00000000000000000000000

會發現，當僅有小數時，bias 是用於被扣除的！僅有小數時，可以儲存更小精度的位數，故 IEEE 754 的值域才能夠達 

還有**簡化比較**，若沒有加上 bias，以上述狀況，指數項是負的，我們必須要在指數項引入二補數，直接比較會導致負數大於正數，譬如 $11111100 > 01111101$ 但前者是負數，需要特別處理；引入後儲存的皆是正數，就可以直接比較。
:::

---

在決定縮放係數前，我們首先觀察 `score` 之值域，以求至少在儲存浮點數以及其四則運算中誤差最小；去除掉第一次拜訪所得 `DBL_MAX` 之值，最大值為 3.552684，最小值為 0。

**當縮放係數為 $2^8$**：
* 可表示為 11.10001101
* 轉換回浮點數得到 3.550781，損失為 0.001903

**當縮放係數為 = $2^{16}$**：
* 可表示為 11.1000110101111100
* 轉換回浮點數得到 3.552673，損失為 0.000011

**當縮放係數為 = $2^{20}$**：
* 可表示為 11.10001101011111001011
* 轉換回浮點數得到 3.552684，損失為 0

**雖然採用越大的縮放係數能夠逼近更小的位數，使得損失降低，但並非越大越好；接下來我們展示定點數的乘除，就能發現縮放係數的大小事實上是一種取捨。**

#### Q Format

#### 定點數的乘法
定點數乘法的問題在於容易上溢位 (overflow)，假設縮放係數為 $2^{16}$，並考慮以下程式碼

```c=
double a = 2.25;
double b = 5;
printf("%f\n", a * b);

// fixed point number
unsigned int c = a * (1 << FIXED_POINT_SCALE);
unsigned int d = b * (1 << FIXED_POINT_SCALE);
printf("%f\n", (double) (c * d) / pow(2, FIXED_POINT_SCALE * 2));
```

當 `FIXED_POINT_SCALE = 14` 時，兩者相等；當 `FIXED_POINT_SCALE = 15` 時，定點數乘法計算為 `3.25`，是因為左移過程中有兩個位元溢出，故與原先值差為 `8`；`FIXED_POINT_SCALE = 16` 時，計算結果為 `0.25`，亦是溢出兩個位元。

故設計不符需求的縮放係數可能會導致上溢位；而我們可以檢測當兩乘數之最高位元相加，當大於系統位元數，就必須處理上溢位。

#### 定點數的除法
定點數除法則會導致下溢位 (underflow)，除了上溢位的反例，即過小的縮放係數會導致小數位的損失外，當兩數的最高位數相近，或除數大於被除數，也會導致下溢位；我們可以先放大被除數來避免，如

```c
static unsigned int fixed_point_divide(unsigned int a, unsigned int b)
{
    unsigned int result;
    int a_ls = __fls(a);
    int b_ls = __fls(b);
    if ((b > a || b_ls - a_ls >= FIXED_POINT_SCALE) && a_ls + FIXED_POINT_SCALE < 32)
        result = (a << FIXED_POINT_SCALE) / b;
    else
        result = (a / b) << FIXED_POINT_SCALE;
    return result;
}

int main()
{
    double a = 123;
    double b = 45;
    printf("%f\n", a / b);                                   // 2.733333

    unsigned int c = a * (1 << FIXED_POINT_SCALE);
    unsigned int d = b * (1 << FIXED_POINT_SCALE);
    double r1 = c / d;
    double r2 = fixed_point_divide(c, d);
    printf("%f\n", (double) r1);                             // 2.000000
    printf("%f\n", (double) r2 / (1 << FIXED_POINT_SCALE));  // 2.730469
}
```

透過 `__fls()` 得到最高位數進行檢查避免上溢位，以上程式碼可以處理 $a > 2^{16} - 1$ 的狀況；經實驗，在 `#define ITERATIONS 100000` 的情況下，`log(n_total) / n_visits` 約在 1.9 附近，遠小於 $2^{16}$；且後續應用中，被除數 a >= 除數 b 必然成立，故不採用 `__fls()` 檢查除法中的上溢位，直接將 `(a << FIXED_POINT_SCALE) / b` 作為除法結果。


#### 根據牛頓勘根法 (Newton's Method) 求近似平方根
牛頓勘根法的一般式可寫作
$$
x_{n+1} = x_{n} - \cfrac{f(x_{n})}{f'(x_{n})}
$$
逐項迭代後，$x_{n+1}$ 最後將逼近 $f(x)=0$ 之解。

故我們將 $f(x,c) = x^2 - c$ 帶入，其中 c 為任意欲求平方根之常數；如令 c = 2，依據牛頓勘根法可球的 $\sqrt{2}$ 之近似值；以以下程式碼實作：

```c
#define FIXED_POINT_SCALE 8
#define TOLERANCE (1 << (FIXED_POINT_SCALE / 2))

static inline unsigned int fixed_point_divide(unsigned int a, unsigned int b)
{
    return (a << FIXED_POINT_SCALE) / b;
}

static inline unsigned int fixed_point_sqrt(unsigned int x)
{
    unsigned int r = x > 1 ? (x >> 1) : 1;
    unsigned int r_new;
    while (1) {
        if (r == 0)
            return r;
        unsigned int div = fixed_point_divide(x, r);
        r_new = (r + div) >> 1;
        unsigned int diff = r_new - r;
        if (diff < TOLERANCE) {
            return r_new;
        }
        r = r_new;
    }
}

int main()
{
    double max_diff = 0;
    double sum_diff = 0;
    double std_diff = 0;
    double *diff_arr = malloc(65535 * sizeof(double));
    for (int a = 1; a < 65536; a++) {
        float sqrt_2 = sqrt(a);
        unsigned int approx_sqrt_2 = fixed_point_sqrt(a * (1 << FIXED_POINT_SCALE));
        double approx_sqrt_2_double = fixed_point_to_double(approx_sqrt_2, FIXED_POINT_SCALE);
        double diff = fabs(sqrt_2 - approx_sqrt_2_double);
        diff_arr[a - 1] = diff;
        sum_diff += diff;
        if (diff > max_diff) {
            max_diff = diff;
        }
    }
    double mean_diff = sum_diff / 65535;
    for (int i = 0; i < 65535; i++) {
        std_diff += pow(diff_arr[i] - mean_diff, 2);
    }
    std_diff = sqrt(std_diff / 65535);
    printf("%f, %f, %f\n", max_diff, mean_diff, std_diff);
}
```

經以上實驗，牛頓勘根法的近似值與浮點數 `sqrt` 之最大差值為 0.003906，其平均值為 0.001957，標準差為 0.001132。


礙於顯示效果，我們每 100 個數才作圖：

![sqrt_approx](https://hackmd.io/_uploads/HJ3Yd3TR6.png)

由於差異極小，很難辨別誤差，但確實展示出極佳的逼近效果。


#### 根據牛頓勘根法 (Newton's Method) 求近似對數

依據上述方法，令 $f(x, c)=2^x - c$ ，求得對數之迭代公式為
$$
x_{n+1} = x_{n} - \cfrac{2^{x_n} - c}{2^{x_n} ln2}
$$

```c=
static inline unsigned int fixed_point_log(unsigned int x)
{
    if (x == 1)
        return 0;
    unsigned int l = x > 1 ? (x >> 1) : 1;
    unsigned int l_new;
    while (1) {
        if (l == 0)
            return l;
        unsigned int fx0 = pow(2, (double) l / (1 << FIXED_POINT_SCALE)) * (1 << FIXED_POINT_SCALE);
        unsigned int div = fixed_point_divide(x - fx0, fixed_point_multiply(fx0, LN2));
        l_new = l + div;
        unsigned int diff = l_new - l;
        if (diff < 1) {
            return l_new;
        }
        l = l_new;
    }
}
```

在實作時發現重大問題是，當 $x_n$ 帶有小數，我們無法使用定點數計算出 $2^{x_n}$；可以看到在第 10 行，仍是採用 `pow()`；參考 [marvin0102](https://hackmd.io/@marvin0102/linux2024-homework1#%E5%AE%9A%E9%BB%9E%E6%95%B8%E7%9A%84log%E8%A8%88%E7%AE%97%EF%BC%9A)，應引入尤拉公式。

---

#### 使用定點數實作 mcts 之誤差

主要將 `uct_score` 改為定點數運算：
```diff
static inline unsigned int uct_score(unsigned int n_total, unsigned int n_visits, unsigned int score)
{
    if (n_visits == 0)
-       return DBL_MAX;
+       return UINT_MAX;
-   return score / n_visits +
-          EXPLORATION_FACTOR * sqrt(log(n_total) / n_visits);
+   unsigned int exploitation = fixed_point_divide(score, n_visits);
+   unsigned int exploration = fixed_point_sqrt(fixed_point_divide(fixed_point_log(n_total), n_visits));
+   return exploitation + EXPLORATION_FACTOR * exploration;
}
```

與原先 `uct_score` 之值相減後計算出誤差，作圖如下


見 [commit 08cf048](https://github.com/Kuanch/lab0-c/commit/08cf0489bbc4f22a4f2a20d4c6ec34472bc9d117)。

### 允許使用者切換對弈模式
如 lab0，新增 `ai_vs_ai` 參數，並使用 `do_ttt` 包裝 `ttt` 函數，將控制參數傳入即可，見 [commit 7d11da4](https://github.com/Kuanch/lab0-c/commit/7d11da44ac1022b1d818f7653f816117f6d9da4e)。

惟為了使用 function pointer 使程式更簡潔，將 `negamax_predict` 傳回類型改為整數類，因目前實作中暫不需要 `move_t`；且三者皆改為傳入 `(char *, char *)`，隨後再轉型回各自之型態。

在實作 coroutine 前，係直接取代 `get_input`，而直接導向原先電腦演算法，此時已經可以是 "電腦 v.s. 電腦" 對弈。

```diff
-               move = get_input(turn);
-               if (table[move] == ' ') {
+               if (ai_vs_ai)
+#ifdef USE_RL
+                   move = ai_algorithm(table, &agent);
+#else
+                   move = ai_algorithm(table, &turn);
+#endif
+               else
+                   move = get_input(turn);
+               if (table[move] == ' ')
                    break;
-               }
```

#### 顯示時間


### 引入 coroutine 進行電腦 v.s. 電腦對弈
考慮 `concurrent-programs/coro/coro.c`





## 參考資料
* [IEEE-754 Floating Point Converter](https://www.h-schmidt.net/FloatConverter/IEEE754.html)
* [[轉錄][教學] 計概 IEEE754 表示式](https://www.ptt.cc/bbs/CYCU-IM/M.1233932341.A.13C.html)
* [Chapter 3 - Arithmetic for Computers](https://hackmd.io/@csie808-notes/rJf4k9pW2)
* [二進位表達負數：1’s Complement 與 2’s Complement](https://medium.com/tiffany-blog/negative-binary-numbers-9839d3760bc1)