# [Linux Kernel Internals 2024 Spring Collections](https://hackmd.io/@Kuanch/linux2024-collection) Overview

Instructor: Jim Huang (黃敬群) <jserv.tw@gmail.com>  
Syllabus/Schedule: https://wiki.csie.ncku.edu.tw/linux/schedule

This is a collection of the course materials and assignments for the course Linux Kernel Internals 2024 Spring at National Cheng Kung University. 

---
>**_NOTE:_**  
>This course is currently in progress and scheduled to conclude on July 2.  
The content on this page will be updated regularly throughout the duration of the course.
>
> You might be also interested on my degree essay about Point Cloud Segmentation [2DDATA](https://arxiv.org/abs/2309.11755) and [Image and Point Cloud Misamatch (SNPD 2023)](https://arxiv.org/abs/2309.14932).
> 
>課程將持續至 7 / 2，仍將持續更新內容
>若您從履歷至此，亦可參考我的碩士論文 [2DDATA](https://arxiv.org/abs/2309.11755) 及 [Image and Point Cloud Misamatch (SNPD 2023)](https://arxiv.org/abs/2309.14932)
---

## Assigment 1 (Due 3/4)
Keyword: Circular Linked List in Linux Kernel, `container_of`, [Dude, is my code constant time?](https://eprint.iacr.org/2016/1123.pdf)

[Assignment Requirements 作業說明](https://hackmd.io/@sysprog/linux2024-lab0/%2F%40sysprog%2Flinux2024-lab0-a)  
[Github](https://github.com/Kuanch/lab0-c)

#### 重點節錄
* `container_of` 及 `struct list_head` 初探
* 實作 `struct list_head` 之佇列操作
* 論文〈Dude, is my code constant time?〉

---
## Assigment 2 (Due 3/11)
Keyword: Timsort, Hash Table, LRU, Maple Tree, CPU affinity, `cpumaks`, `cpuset`

[Assignment Requirements 作業說明](https://hackmd.io/@sysprog/BkmmEQCn6)

#### 重點節錄
* 以 linked list 實作 Linux 核心風格之 merge sort
* 理解 Linux Kernel 中的 merge sort
* **Maple Tree** 與 **RCU** 初探
* **CPU affinity** 初探
---
## Assigment 3 (Due 3/25)
Keyword: Double replacing with Fixed Point, Fixed Point Arithmetic, Square and log, coroutine

[Assignment Requirements 作業說明](https://hackmd.io/@sysprog/linux2024-ttt)  
[Github](https://github.com/Kuanch/lab0-c/tree/ttt)


#### 重點節錄
* IEEE 754 浮點數與定點數轉換
* 使用牛頓勘根法求定點數之近似平方根
* 使用尤拉公式求定點數之近似對數

---

## Assigment 4 (Due 4/2)
Keyword: 
Red Black Tree, Bitwise Square, Bitwise Disivion and Modular, Bitwise Log2, Bitwise Hamming Distance, EWMA

[Assignment Requirements 作業說明](https://hackmd.io/@sysprog/HkatSCZCT)


#### 重點節錄
* 使用 bitwise operations 計算數學函式
* Square, division, mod and log2
* 使用 magic number (`0x24924925` etc.) 計算**定點數除法**
* 紅黑數初探

---
## Assigment 5 (Due 4/15)

Keyword : CPU Scheduling, Completely Fair Scheduling (CFS), Energy Aware Scheduling (EAS), EEVDF

Midterm Self Assessment and Term Project Proposal  
[Assignment Requirements 作業說明](https://hackmd.io/@sysprog/BySwQtt06)

---
## Assigment 6 (Due 4/29)
Keyword: Linux Kernel Module, Concurrency Managed Workqueue (cmwq)

[Assignment Requirements 作業說明](https://hackmd.io/@sysprog/linux2024-integration/%2F%40sysprog%2Flinux2024-integration-a)

#### 重點節錄
* 理解 character device 、其涉及的系統呼叫機制以及 ABI
* 透過核心模組 `simrupt` 理解 **CMWQ** 以及 **Divided Handler** 模型
* 嘗試釐清 `work_struct` 與 `task_struct` 關係
* 嘗試以 `strace` 靜態分析 `simrupt`
* 嘗試以 `QEMU + remote gdb + Buildroot` 動態分析 `simrupt`

---


## [Term Project](https://hackmd.io/@sysprog/rkJd7TFX0) (in progress)
Keyword: EEVDF, EAS, QEMU and Buildroot, sched_ext

#### 重點節錄
* 以 QEMU + remote GDB + Buildroot 整理 Linux Kernel Scheduler 調用
* 以不同方式分析及測量 EEVDF, BORE, EAS 等排程器效能 (schbench)
* 撰寫 Kernel Module 評估現有排程策略之效能 (EEVDF, EAS)
* 嘗試以 sched_ext 撰寫簡單的排程器，並驗證其效能差異
* 編撰 《Demystifying the Linux CPU Scheduler》之 EEVDF 等章節
