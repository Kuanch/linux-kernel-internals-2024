# 2024q1 Homework2 (quiz1+2)
contributed by < [Kuanch](https://github.com/Kuanch) >

## Week 1 Quiz - Linked List
[Week1 Quiz](https://hackmd.io/@sysprog/linux2024-quiz1)
### Quiz 1 - Head Linked List `hlist_node`
#### 解釋程式碼
快速排序法為以指定值 pivot 為界，將小於和大於 pivot 的數值分開至左右側，再對左右側進行排序；因其明顯的分治特性 (divide and conquer)，通常以遞迴方式實作；若改以迭代方式實作，其外圈迭代應取代每一次遞迴，亦即分割出左右側後，再分別分割左側、右側，故如何儲存左右側會是其重點。

此處我們使用 `begin` 及 `end` 不僅儲存左右側，同時也儲存 pivot，且依照 `left` `pivot` `right` 的小中大順序儲存，每一次分割後先對 `right` 再分割，再對 `left` 再分割，每一次分割為一次迴圈。

以下列串列為例
![](https://hackmd.io/_uploads/B136cnLp6.png)  

每一次迴圈開始都設左側開頭為 L ，右側開頭為 R，若多個數字在串列內，即 `L!=R` ，設 pivot = L
![image](https://hackmd.io/_uploads/BkgC3CI6T.png)

接下來我認為是最關鍵的程式碼
```c=64
            while (p) {
                node_t *n = p;
                p = p->next;
                list_add(n->value > value ? &right : &left, n);
                print_list(n, "p");
            }
```
67 行的 `list_add` 用於將數字分類至左右側，小值分為左側，大值分為右側；
此行 `list_add` 做到兩件事情
1. 使用 **list ，故可以 `*list = node_t;`，使得 `*right` `*left` 更新
2. `n->value > value ? &right : &left` 可以比較值並直接迭代使用新的 `*right` `*left`

在該 `while` 迴圈結束後，更新為

![圖片1](https://hackmd.io/_uploads/SJkl6yDpT.png)


即下方程式碼，而 i=0

```c
            begin[i] = left;
            end[i] = list_tail(&left);
            begin[i + 1] = pivot;
            end[i + 1] = pivot;
            begin[i + 2] = right;
            end[i + 2] = list_tail(&right);
```

1. begin 儲存 left , right 之**開頭**以及 pivot
2. end 儲存 left , right 之**結尾**以及 pivot

接下來因 `begin[2]` 及 `end[2]` 僅有 9，不滿足 `L != R`，將 9 加入 `result`，此時 `i = 2; i--`。

`begin[1]` 及 `end[1]` 儲存 `pivot` 8，僅有一值，加入 `results`，此時 `i = 1; i--`。

至此，我們回到 `i = 0`，重複上述程序，排序上圖中的 `left -> 6 -> 3 -> 1 -> 7 -> 5 -> 2 -> 0 -> 4`，即可得到最終結果。

#### 改進程式碼
1. `begin[]` 和 `end[]` 的大小

在上述例子中，我們僅需要 `max_level = 5` 就能夠成功排序，是資料總數的一半，顯然我們僅有在特定的例子中才需要將其設置的非常大，是什麼例子呢？

考量 `10, 9, 8, 7, 6, 5, 4, 3, 2, 1`，我們需要至少 `max_level = 9` 也就是 `n - 1`；如果偷看一下答案，我們會知道快速排序在 **每次都恰好選到最大(最小)的 pivot** 時，會有最糟糕的表現；那在此處，我們至少可以將 `max_level = 2 * n` 改為`max_level = n`。

進一步探究為什麼每次都恰好選到最大(最小)的 pivot 會有最糟糕的表現？觀察前述解釋會發現，當 pivot 最大時，`right` 是不會有任何資料，而 `left` 則儲存 n-1 個資料；

![image](https://hackmd.io/_uploads/Sk6Tugwpp.png)

下一次進入排序，則是以 1 為 pivot、`left` 沒有任何資料、`right` 儲存 n-2 個資料；以此類推，由於我們**每一次只排序一個數字(pivot)，最終會需要使用到 i = 9**

:::info
一個想法是，`max_level` 是不是可以用作評判一個數據的排序程度的數值？
:::

2. 改為雙向串列，取代 `list_tail` 操作

為什麼要取代 `list_tail` ？其程式碼存在 `while`，在不考慮編譯器的最佳化下，每次尋找最尾端元素竟然要歷遍整個串列、浪費 CPU cycles，無疑會大大影響效能。
```c
static inline node_t *list_tail(node_t **left)
{
    while ((*left) && (*left)->next)
        left = &((*left)->next);
    return *left;
}
```
改寫內容由下方一起說明。

#### 以 Linux Kernel 風格改寫並分析快速排序實作

```diff
typedef struct __node {
-   struct __node *left, *right;
-   struct __node *next;
+   struct list_head list;
    long value;
} node_t;
```

以 `elemet_t` 的風格取代 `node_t`，使用 `list_head` 串連各個元素；以此我們可以
1. 使用 `list.h`
2. 快速找到尾部串列

將 `quick_sort` 改寫如下
```c
void quick_sort(struct list_head *head)
{
    int n = list_length(head);
    int value;
    int i = 0;
    int max_level = n;
    struct list_head *begin[max_level], *end[max_level];
    LIST_HEAD(result);
    LIST_HEAD(left);
    LIST_HEAD(right);

    begin[0] = head->next;
    end[0] = head->prev;
    list_del(head);
    INIT_LIST_HEAD(head);

    while (i >= 0) {
        struct list_head *L = begin[i], *R = end[i];
        if (L != R) {
            struct list_head *pivot = L;
            value = list_entry(pivot, node_t, list)->value;
            struct list_head *p = pivot->next;

            for (struct list_head *p = pivot->next, *next; p != pivot; p = next) {
                struct list_head *n = p;
                next = p->next;
                list_del(p);
                list_add(n, list_entry(n, node_t, list)->value > value ? &right : &left);
            }
            list_del(pivot);

            // TODO@Guan: refactor
            begin[i] = (list_empty(&left) ? NULL : left.next);
            end[i] = (list_empty(&left) ? NULL : list_tail(&left));
            begin[i + 1] = pivot;
            end[i + 1] = pivot;
            begin[i + 2] = (list_empty(&right) ? NULL : right.next);
            end[i + 2] = (list_empty(&right) ? NULL : list_tail(&right));

            list_del(&left);
            list_del(&right);
            INIT_LIST_HEAD(&left);
            INIT_LIST_HEAD(&right);
            i += 2;
        } else {
            if (L) 
                list_add(L, &result);
            i--;
        }
    }
    list_splice(&result, head);
}
```

我們可以分析以前後版本的效能差異，採用 lab0-c 的 `cpucycles.h`，在`count = 100000` 的條件下:


| Optimize/Cycles | Original | Double Linked List |
| -------- | -------- | -------- |
| -O0     | 122,059,902     | 105,774,075     |
| -O1     | 111,838,722     | 73,010,733     |
| -O2     | 97,242,785     | 46,045,798     |


### Quiz 2 - Timsort, merge sort
#### 解釋程式碼
依照[題幹說明](https://hackmd.io/@sysprog/linux2024-quiz1)，我們可以將程式碼一一對應
1. `find_run`

在 `find_run` 中，依照單調遞增及單調遞減形成 `if-else`，計算當前最長的單調數列的長度，並將長度紀錄於串列開頭的第一個元素的 `prev` 中，`head->next->prev = (struct list_head *) len;`；此處將原先為整數型態的指標轉為 `list_head *` 可讀性不佳，這麼做的理由是值得討論的；另外亦透過 `queue->next = prev;` 將遞減數列反轉為遞增數列。
:::info
:::spoiler ChatGPT  Why `head->next->prev = (struct list_head *) len;` ?
> ...
>
> 3. Memory Alignment: In some low-level programming scenarios, particularly in embedded systems or OS kernels, it might be known that certain memory regions start at well-defined boundaries. In these cases, a specific integer might be used to signify a memory address.
>
> In modern C code, especially code that aims to be **portable and maintainable**, ...
> **If the intention is to store metadata, it's better to use a dedicated field for that purpose, or use a union to explicitly represent the possibility of different types being stored in that memory location.**
:::  
  
2. 合併序列 `merge_collapse` 和 `merge_at`


```c=111
        if ((n >= 3 && run_size(tp->prev->prev) <= run_size(tp->prev) + run_size(tp)) ||
            (n >= 4 && run_size(tp->prev->prev->prev) <= run_size(tp->prev->prev) + run_size(tp->prev)))
        {
            if (run_size(tp->prev->prev) < run_size(tp))   
                tp->prev = merge_at(tp->prev);  // Merge B,c
            else {
                tp = merge_at(tp);              // Merge A,B
            }
        } else if (run_size(tp->prev) <= run_size(tp)) {　 
            tp = merge_at(tp);                  // Merge A,B
        } else {
            break;
        }
```
傳入的 `tp` 為遞增函數的首數，上述程式碼用於合併 run；由於 `result.head->prev = tp;` ，當未合併的 run 大於三，我們就必須往前找尋節點的 `prev` 嘗試合併。

為什麼設下限制使得後面的 run 長度必須要大於前面的 run 才能合併 (line 111,112,119)？是為了保持不同 run 的平衡： 

>過程中，Timsort 運用 merge_collapse 函式來確保堆疊上的 run 長度保持平衡。該函式主要檢查堆疊頂端的 3 個 run 是否滿足以下原則：
>
>A 的長度要大於 B 和 C 的長度總和。
>B 的長度要大於 C 的長度。

3. `merge_force_collapse`

當序列被歷遍，序列中會存在許多已經被 `merge_collapse` 合併排序的子序列，仿造其方法，當有三個子序列以上將 BC 合併，否則合併 AB；此處如果能夠得知剩下子序列的長度，是否能夠更平衡的取出子序列排序呢？



#### 實作 Galloping mode (WIP)




#### 將 Timsort 整合進 [sysprog21/lab0-c](https://github.com/sysprog21/lab0-c)
依照 [引入 lib/list_sort.c](https://hackmd.io/@Kuanch/linux2024-homework1#%E5%88%86%E6%9E%90%E8%88%87%E5%BC%95%E5%85%A5-liblist_sortc)，在 `qtest.c` 中加入對應 `do_timsort` 即可，見 [commit](https://github.com/Kuanch/lab0-c/commit/9acba7138357c20d8dd095a9cb07cb6efaeba5f7)。


## Week 2 Quiz - Hash Table and Bitwise
[Week2 Quiz](https://hackmd.io/@sysprog/linux2024-quiz2)
### Quiz 1
#### 解釋程式碼
深度優先搜尋(Depth First Search)，係指會優先歷遍樹的深分支，當被查探完，再回到最後一個未被探索完的根節點繼續探索。首先，`in_heads` 是以中序遍歷(inorder traversal) 順序建立；而在程式碼中，可以見到是以前序遍歷 (preorder) 順序進行，故是以`中->左->右`順序回傳根節點。
```c=69
    tn->val = preorder[pre_low];
    int idx = find(preorder[pre_low], size, in_heads);
    tn->left = dfs(preorder, pre_low + 1, pre_low + (idx - in_low), inorder,
                   in_low, idx - 1, in_heads, size);
    tn->right = dfs(preorder, pre_high - (in_high - idx - 1), pre_high, inorder,
                    idx + 1, in_high, in_heads, size);
```
故可以理解為，70 行找到的是根節點、71 行找到是左子樹根節點、73 行找到的是右子樹根節點。

#### 改進程式碼並撰寫測試
增加查找、移除功能，並提供 bfs 版本
:::danger
@Kuanch: Took too much time, refactor this later
:::
```c
struct TreeNode *bfs_search(int val, int size, struct TreeNode *root)
{
    struct node_queue *queue = malloc(sizeof(*queue));
    queue->node = root;
    queue->next = NULL;
    struct node_queue *next = queue;
    struct node_queue *tail = queue;
    while(next) {
        struct TreeNode *node = next->node;
        if (node->val == val)
            return node;
        if (node->left) {
            struct node_queue *nq = malloc(sizeof(*nq));
            nq->node = node->left;
            nq->next = NULL;
            if (!next->next) {
                tail = nq;
                next->next = tail;
            }
            else {
                tail->next = nq;
                tail = tail->next;
            }
        }
        if (node->right) {
            struct node_queue *nq = malloc(sizeof(*nq));
            nq->node = node->right;
            nq->next = NULL;
            if (!next->next) {
                tail = nq;
                next->next = tail;
            }
            else {
                tail->next = nq;
                tail = tail->next;
            }
        }
        next = next->next;
    }
    return NULL;
}

struct TreeNode *node_lookup(int val, int size, struct TreeNode *root)
{
    if (!root)
        return NULL;
    return bfs_search(val, size, root);
}

/* TODO@Kuanch
struct TreeNode *node_remove(int val, int size, struct TreeNode *root)
{
}
 */
```


#### 在 Linux 核心找出 pre-order walk 程式碼並探討

### Quiz 2
#### 解釋、改進程式碼並撰寫測試
#### 在 Linux 核心找出 LRU 相關程式碼並探討
在檔案系統下可以找到許多使用類似於此處的 LRU 結構以及操作的程式碼，如 **Btrfs** [linux/fs/btrfs/lru_cache.h](https://github.com/torvalds/linux/blob/master/fs/btrfs/lru_cache.h#L41)

```c=
struct btrfs_lru_cache_entry {
	struct list_head lru_list;
	u64 key;
	u64 gen;
	struct list_head list;
};

struct btrfs_lru_cache {
	struct list_head lru_list;
	struct maple_tree entries;
	unsigned int size;
	unsigned int max_size;
};

struct btrfs_lru_cache_entry *btrfs_lru_cache_lookup(struct btrfs_lru_cache *cache,
						     u64 key, u64 gen)
{
	struct list_head *head;
	struct btrfs_lru_cache_entry *entry;

	head = mtree_load(&cache->entries, key);
	if (!head)
		return NULL;

	entry = match_entry(head, key, gen);
	if (entry)
		list_move_tail(&entry->lru_list, &cache->lru_list);

	return entry;
}

void btrfs_lru_cache_remove(struct btrfs_lru_cache *cache,
			    struct btrfs_lru_cache_entry *entry)
{
	struct list_head *prev = entry->list.prev;

	ASSERT(cache->size > 0);
	ASSERT(!mtree_empty(&cache->entries));

	list_del(&entry->list);
	list_del(&entry->lru_list);

	if (list_empty(prev)) {
		struct list_head *head;

		head = mtree_erase(&cache->entries, entry->key);
		ASSERT(head == prev);
		kfree(head);
	}

	kfree(entry);
	cache->size--;
}
```

其中 `list_move_tail(&entry->lru_list, &cache->lru_list);` 以及 `head = mtree_erase(&cache->entries, entry->key);` 即是 lru 機制的展現；將已經被查詢過的放到隊列尾部，移除時則首先從頭部開始。

#### Maple Tree 與 RCU
除了我們熟悉的 `list_head` 之外，此處使用 [Maple Tree](https://hackmd.io/@sysprog/linux-rbtree#Maple-tree) 而非常見的紅黑樹，Maple Tree 的介面如下，我們可以見到在查詢快取時，會透過 [RCU 同步機制](https://hackmd.io/@sysprog/linux-rcu#Linux-%E6%A0%B8%E5%BF%83%E8%A8%AD%E8%A8%88-RCU-%E5%90%8C%E6%AD%A5%E6%A9%9F%E5%88%B6)；引述該文

> 即使存取舊的資料，不會影響最終行為的正確，這樣的情境就適合 RCU，對其他網路操作也有相似的考量。

你可以在 `rcu_read_lock()` [文檔](https://github.com/torvalds/linux/blob/master/include/linux/rcupdate.h#L699)中看到相呼應的描述

> So where is rcu_write_lock()?  It does not exist, as there is no way for writers to lock out RCU readers.  This is a feature, not a bug -- this property is what provides RCU's performance benefits.

也就是說，RCU 只能作為讀鎖，寫鎖是不合理的；此外，文檔也對 `synchronize_rcu()` 及 `call_rcu()` 有詳細描述：

* `synchronize_rcu()`： 確保所有讀者都已離開，為阻塞
* `call_rcu()`： 當所有讀者都離開，呼叫此函式，為非阻塞

以下程式碼展示如何使用 `call_rcu()`
```c
/*
 * mas_mat_destroy() - Free all nodes and subtrees in a dead list.
 * @mas - the maple state
 * @mat - the ma_topiary linked list of dead nodes to free.
 *
 * Destroy walk a dead list.
 */
static void mas_mat_destroy(struct ma_state *mas, struct ma_topiary *mat)
{
	struct maple_enode *next;
	struct maple_node *node;
	bool in_rcu = mt_in_rcu(mas->tree);

	while (mat->head) {
		next = mte_to_mat(mat->head)->next;
		node = mte_to_node(mat->head);
		mt_destroy_walk(mat->head, mas->tree, !in_rcu);
		if (in_rcu)
			call_rcu(&node->rcu, mt_free_walk);
		mat->head = next;
	}
}
```

簡而言之
1. Btrfs Cache 使用 LRU 以及 Maple Tree
2. Maple Tree 使用了 RCU 機制
3. RCU 是一種讀鎖，有非阻塞機制，故相當有效率

![image](https://hackmd.io/_uploads/BJdbGt1zA.png)

### Quiz 3
#### 解釋程式碼



#### 在 Linux 核心找出 find_nth_bit 的應用案例 (涵蓋 CPU affinity)
* 什麼是 CPU Affinity (親和力)?
其主要的目的是限制某一些 process 排程在特定的 CPU 上、不受作業系統調動，這可能可以讓 cache 的使用更好、減少 context switching 以及增加排程的效率。

在 [linux/cpumask.h](https://github.com/torvalds/linux/blob/e8f897f4afef0031fe618a8e94127a0934896aba/include/linux/cpumask.h#L397) 中使用了 `find_nth_bit`：

```c
/**
 * cpumask_test_cpu - test for a cpu in a cpumask
 * @cpu: cpu number (< nr_cpu_ids)
 * @cpumask: the cpumask pointer
 *
 * Return: true if @cpu is set in @cpumask, else returns false
 */
static __always_inline bool cpumask_test_cpu(int cpu, const struct cpumask *cpumask)
{
	return test_bit(cpumask_check(cpu), cpumask_bits((cpumask)));
}

/**
 * cpumask_nth - get the Nth cpu in a cpumask
 * @srcp: the cpumask pointer
 * @cpu: the Nth cpu to find, starting from 0
 *
 * Return: >= nr_cpu_ids if such cpu doesn't exist.
 */
static inline unsigned int cpumask_nth(unsigned int cpu, const struct cpumask *srcp)
{
	return find_nth_bit(cpumask_bits(srcp), small_cpumask_bits, cpumask_check(cpu));
}

static inline int
do_filter_cpumask_scalar(int op, const struct cpumask *mask, unsigned int cpu)
{
	switch (op) {
	case OP_EQ:
		return cpumask_test_cpu(cpu, mask) &&
			cpumask_nth(1, mask) >= nr_cpu_ids;
	case OP_NE:
		return !cpumask_test_cpu(cpu, mask) ||
			cpumask_nth(1, mask) < nr_cpu_ids;
	case OP_BAND:
		return cpumask_test_cpu(cpu, mask);
	default:
		return 0;
	}
}
```

下方兩個函式被定義在 [linux/kernel/trace/trace_events_filter.c](https://github.com/torvalds/linux/blob/master/kernel/trace/trace_events_filter.c#L693)；`do_filter_cpumask_scalar()` 中的 `OP_XX` 是對 `cpumask` 的不同"操作"：

* OP_EQ：是否僅有 `cpu` 唯一存在 `mask` 中
* OP_NE：確認 `cpu` 不在 `mask` 中，或有其他 `cpu` 存在 `mask` 中
* OP_BAND：確認 `cpu` 是否在 `mask` 當中

`cpumask_nth(1, mask)` 是為了尋找第二位的位置，若 `>= nr_cpu_ids` 表示不存在。

---
 
除此之外，雖然沒有其他直接使用到 `cpumask_nth()` 或 `find_nth_bit()` 的程式碼，但我們可以在 [`linux/kernel/sched/core.c`](https://github.com/torvalds/linux/blob/855684c7d938c2442f07eabc154e7532b4c1fbf9/kernel/sched/core.c#L8346) 中找到許多與 CPU Affinity 相關，且使用 `cpumask` 或其他 bitmap 操作的程式碼，如 `__sched_setaffinity()` 使用了大量 `cpumask_and()` `cpumask_subset()` `cpumask_copy()` 都使用了 bitmap 相關程式碼。

```c=8345
static int
__sched_setaffinity(struct task_struct *p, struct affinity_context *ctx)
{
	int retval;
	cpumask_var_t cpus_allowed, new_mask;

	if (!alloc_cpumask_var(&cpus_allowed, GFP_KERNEL))
		return -ENOMEM;

	if (!alloc_cpumask_var(&new_mask, GFP_KERNEL)) {
		retval = -ENOMEM;
		goto out_free_cpus_allowed;
	}

	cpuset_cpus_allowed(p, cpus_allowed);
	cpumask_and(new_mask, ctx->new_mask, cpus_allowed);

	ctx->new_mask = new_mask;
	ctx->flags |= SCA_CHECK;

	retval = dl_task_check_affinity(p, new_mask);
	if (retval)
		goto out_free_new_mask;

	retval = __set_cpus_allowed_ptr(p, ctx);
	if (retval)
		goto out_free_new_mask;

	cpuset_cpus_allowed(p, cpus_allowed);
	if (!cpumask_subset(new_mask, cpus_allowed)) {
		/*
		 * We must have raced with a concurrent cpuset update.
		 * Just reset the cpumask to the cpuset's cpus_allowed.
		 */
		cpumask_copy(new_mask, cpus_allowed);

		/*
		 * If SCA_USER is set, a 2nd call to __set_cpus_allowed_ptr()
		 * will restore the previous user_cpus_ptr value.
		 *
		 * In the unlikely event a previous user_cpus_ptr exists,
		 * we need to further restrict the mask to what is allowed
		 * by that old user_cpus_ptr.
		 */
		if (unlikely((ctx->flags & SCA_USER) && ctx->user_mask)) {
			bool empty = !cpumask_and(new_mask, new_mask,
						  ctx->user_mask);

			if (WARN_ON_ONCE(empty))
				cpumask_copy(new_mask, cpus_allowed);
		}
		__set_cpus_allowed_ptr(p, ctx);
		retval = -EINVAL;
	}

out_free_new_mask:
	free_cpumask_var(new_mask);
out_free_cpus_allowed:
	free_cpumask_var(cpus_allowed);
	return retval;
}
```
上述程式碼為某一項任務 `p` 設置 CPU Affinity，主要重點為

1. `cpuset_cpus_allowed(p, cpus_allowed)` 依照任務的 `cpuset` 取得目前可用的 CPU 至 `cpus_allowed`
2. `cpumask_and(new_mask, ctx->new_mask, cpus_allowed)` 前述取得的 CPU 與 [User CPU mask(desired affinity?)](https://github.com/torvalds/linux/blob/855684c7d938c2442f07eabc154e7532b4c1fbf9/kernel/sched/core.c#L8481) 的交集至 `new_mask`
3. 更新 (dl_task_check_affinity是什麼？)
4. Race with cpuset Updates　(line 8374, and why?)

:::info
:::spoiler **predicate** 是什麼意思
維基百科：
>In computer architecture, predication is a feature that provides an alternative to conditional transfer of control, as implemented by conditional branch machine instructions.

>In mathematics, a predicate is either a relation or the boolean-valued function that amounts to the characteristic function or the indicator function of such a relation.
>A function P: X→ {true, false} is called a predicate on X. When P is a predicate on X, we sometimes say P is a property of X.

或者，更喜歡這個 [stackoverflow](https://stackoverflow.com/questions/3230944/what-does-predicate-mean-in-the-context-of-computer-science) 的例子
```
Person mike;

if (!mike.isEating())
    feedPerson(mike);
```
>The isEating() member of mike (an instance of Person) is a predicate.
:::



---

#### `cpuset` 與 `cpumask`

找尋資料的過程中，[多處(註一)](#參考來源與註解)將 CPU Affinity 與 `cpuset` 共同討論而非 `cpumask`，而兩者都又與 `sched_setaffinity` 及 `sched_getaffinity` 或 NUMA(Non-Uniform Memory Access) 一起討論，故在留下篇幅探討 `cpuset` 。


```c=886
// defined at linux/cpumask.h
typedef struct cpumask *cpumask_var_t;
```

```c=94
// defined at kernel/cgroup/cpuset.c
struct cpuset {
	struct cgroup_subsys_state css;

	unsigned long flags;
	cpumask_var_t cpus_allowed;
	nodemask_t mems_allowed;

	/* effective CPUs and Memory Nodes allow to tasks */
	cpumask_var_t effective_cpus;
	nodemask_t effective_mems;

	/*
	 * ... skip
	 */
	cpumask_var_t effective_xcpus;

	/*
	 * Exclusive CPUs as requested by the user (default hierarchy only)
	 */
	cpumask_var_t exclusive_cpus;
    // ... skip
};
```

可以見到 `cpumask` 是 `cpuset` 的結構成員，後者更加複雜；大致來說，`cpumask` 僅是 bitmap，代表 CPU 的二元狀態，如可使用、已使用等，而 `cpuset` 更複雜，也提供更多介面操作

:::info
:::spoiler 原來他也知道這一切很難
在 `typedef struct cpumask *cpumask_var_t;` 的註解中，是這麼開頭的
```c
/*
 * cpumask_var_t: struct cpumask for stack usage.
 *
 * Oh, the wicked games we play!  In order to make kernel coding a
 * little more difficult, we typedef cpumask_var_t to an array or a
 * pointer: doing &mask on an array is a noop, so it still works.
 * 
 */
```
:::

#### 有趣的 `cpumask.h`

記錄幾項剛好看到的有趣程式碼
* `to_cpumask(bitmap)`
```c
/**
 * to_cpumask - convert an NR_CPUS bitmap to a struct cpumask *
 * @bitmap: the bitmap
 *
 * There are a few places where cpumask_var_t isn't appropriate and
 * static cpumasks must be used (eg. very early boot), yet we don't
 * expose the definition of 'struct cpumask'.
 *
 * This does the conversion, and can be used as a constant initializer.
 */
#define to_cpumask(bitmap)						\
	((struct cpumask *)(1 ? (bitmap)				\
			    : (void *)sizeof(__check_is_bitmap(bitmap))))
```
該三元判斷式始終為真，為什麼要有 `(void *)sizeof(__check_is_bitmap(bitmap))`？
此項會在編譯期確認傳入的 `bimap` 是 `unsigned long *`

#### cpumaks 與 IRQ Affinity (WIP)

## 參考來源與註解
* 註一：Subhra Mazumdar 在 [Scheduler Soft Affinity](https://lwn.net/Articles/792196/) 討論了 NUMA, sched_setaffinity 和 cpuset；台大計網在[多核心計算環境—NUMA與CPUSET簡介](https://www.cc.ntu.edu.tw/chinese/epaper/0015/20101220_1508.htm) 討論了 NUMA 與 cpuset；
* [Linux 核心的紅黑樹](https://hackmd.io/@sysprog/linux-rbtree#Linux-%E6%A0%B8%E5%BF%83%E7%9A%84%E7%B4%85%E9%BB%91%E6%A8%B9%E5%AF%A6%E4%BD%9C)
* [Linux 核心設計: RCU 同步機制](https://hackmd.io/@sysprog/linux-rcu#Linux-%E6%A0%B8%E5%BF%83%E8%A8%AD%E8%A8%88-RCU-%E5%90%8C%E6%AD%A5%E6%A9%9F%E5%88%B6)
* [Scheduler Soft Affinity - Subhra Mazumdar](https://lwn.net/Articles/792196/)
* [在 Linux 中以特定的 CPU 核心執行程式 (taskset)](https://blog.gtwang.org/linux/run-program-process-specific-cpu-cores-linux/)
* [What is CPU Affinity - Nvidia](https://enterprise-support.nvidia.com/s/article/what-is-cpu-affinity-x)
* [Linux 具重大安全性弱點，建議請管理者儘速評估更新，以降低受駭風險！2018-06 (kernel/trace/trace_events_filter.c)](https://cert.tanet.edu.tw/prog/showrpt.php?id=3480)