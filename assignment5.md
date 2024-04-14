# 2024q1 Homework5 (assessment)
contributed by < [Kuanch](https://github.com/Kuanch) >

## 期末專題提案
希望能夠進行
1. [排程器研究](https://hackmd.io/@sysprog/linux2023-projects#Linux-%E6%8E%92%E7%A8%8B%E5%99%A8%E7%A0%94%E7%A9%B6)，尤其是 Energy Aware Scheduling (EAS) 或 EEVDF
    * 針對 EAS，發現有同學留下相關資料 [Linux 核心設計: Scheduler(8): Energy Aware Scheduling](https://hackmd.io/@RinHizakura/Skelo1WY6?utm_source=preview-mode&utm_medium=rec)，此系列源自於 [Linux 核心設計: 不只挑選任務的排程器](https://hackmd.io/@sysprog/linux-scheduler?type=view#Linux-%E6%A0%B8%E5%BF%83%E8%A8%AD%E8%A8%88-%E4%B8%8D%E5%8F%AA%E6%8C%91%E9%81%B8%E4%BB%BB%E5%8B%99%E7%9A%84%E6%8E%92%E7%A8%8B%E5%99%A8)，並延伸探討 Completely Fair Scheduling (CFS) 至 EEVDF 及 EAS。
    * 一個可能的專案進行方式是繼續更新、補完該系列文章，因不可避免的要閱讀、理解其所有參考資料；缺點是目前不清楚該系列文章分析的深度與廣度，具體要如何更深入探討仍待研究。
2. 實作高效記憶體配置器
    * 傾向與 [WangHanChi](https://hackmd.io/@sysprog/HkNR4RVrn) 相似的內容，加入更多實作而較少研讀與分析既有程式碼與材料；如基於其 [rbtmalloc](https://hackmd.io/@wanghanchi/linux2023-rbtmalloc) 的改進版本，或具有相似特性的分配器。
2. 理解如何修改、驗證、部屬 Linux 核心(驅動)程式
    * 若沒有能力執行、部屬 Linux 核心程式，便很難驗證新的想法和實作是否有價值；這一項可以單獨提出，選定某一易於分析、使用的模塊，專注修改、驗證、部屬的流程，而非模塊功能本身；或者與上述 EAS 專案作結合，嘗試修改、驗證、部屬 EAS；參考
        * [建構 User-Mode Linux 的實驗環境](https://hackmd.io/@sysprog/user-mode-linux-env)
        * [Linux 核心模組運作原理](https://hackmd.io/@sysprog/linux-kernel-module)
        * [測試 Linux 核心的虛擬化環境](https://hackmd.io/@sysprog/linux-virtme)
        * [Linux 核心設計: Scheduler(7): sched_ext](https://hackmd.io/@RinHizakura/Hka7Kzeap/%2Fr1uSVAWwp)
        * [sched_ext: a BPF-extensible scheduler class (Part 1)](https://blogs.igalia.com/changwoo/sched-ext-a-bpf-extensible-scheduler-class-part-1/)
        * [Linux 核心設計: 透過 eBPF 觀察作業系統行為](https://hackmd.io/@sysprog/linux-ebpf?type=view)
        * [Linux 核心設計: eBPF](https://hackmd.io/@RinHizakura/HynIEOD7n)
        * [BPF 的可移植性: Good Bye BCC! Hi CO-RE!](https://hackmd.io/@RinHizakura/HynIEOD7n)

### Reference
[arm EAS Mainline](https://developer.arm.com/Tools%20and%20Software/EAS%20Mainline%20and%20Scheduling)
[Demystifying the Linux CPU Scheduler 閱讀筆記](https://hackmd.io/@linhoward0522/HyC28W9N3?utm_source=preview-mode&utm_medium=rec)
[Energy Aware Scheduling](https://docs.kernel.org/scheduler/sched-energy.html)
[Energy Aware scheduling Report](https://hackmd.io/@Daichou/ByzK-S60E)

## 研讀第 1 到第 6 週「課程教材」和 CS:APP 3/e
[Linux 核心實作排程器教材 閱讀筆記](https://hackmd.io/@Kuanch/linux-scheduler-books) 為整理綜合閱讀
1. [Linux 核心設計: 不只挑選任務的排程器](https://hackmd.io/@sysprog/linux-scheduler)
2. Demystifying the Linux CPU Scheduler
3. [Linux 核心設計: Scheduler 系列 (Yiwei Lin)](https://hackmd.io/@RinHizakura/S1opp7-mP)

之筆記，記載閱讀過程中的疑問與心得，因篇幅較長故以書本模式分為三篇發表。




## 前期作業改進
1. [Homework4 紅黑樹補完](https://hackmd.io/@Kuanch/linux2024-homework4#Quiz-3---Red-Black-Tree)
2. [Homework2 cgroup 和 Hash Table 補完](https://hackmd.io/@Kuanch/linux2024-homework2#Quiz-11)
3. [Homework3 coroutine](https://hackmd.io/@Kuanch/linux2024-homework1#%E5%BC%95%E5%85%A5-coroutine-%E9%80%B2%E8%A1%8C%E9%9B%BB%E8%85%A6-vs-%E9%9B%BB%E8%85%A6%E5%B0%8D%E5%BC%88)


## 〈因為自動飲料機而延畢的那一年〉的啟發與課程心得
「完成比完美更重要」，除了強調大多數人都會被虛妄且毫無價值的 "完美主義" 干擾，最終放棄外，我認為這句話更重要的就是字面上的意思，「完成一件事情的過程和最終完成它」的價值遠高於人們的想像；Jserv 在過程中的提點是這句話的具體展現，走捷徑沒關係、不完美沒關係，重點是去嘗試克服重重難關，克服不了的就暫且繞過它，重點是"完成"路上的過程。

我大概在好幾年前就讀過這篇文章了，它也在我的人生中再出現過幾次，在寫作業的時候也時不時提醒我：作業的份量很多，但我會盡力完成它，誠實面對自己，還沒做的、做不出來的、不知道怎麼做的，誠實記錄並在之後的時間逐步修正和補完；故這句話也在寫作業的過程中具體實現了。

另外，先是完成，就會完美；一旦完成了，再看它時就會越來越不順眼，自己再讀過一次後覺得不通順、邏輯不通或不完備，我認為這也是這門課作業形式的價值所在：允許自由書寫，並能夠不斷自我修正，最終渴求知道「為什麼」；「完成比完美更重要」+「誠實面對自己」+「持之以恆」是這門課教我的，我想這三點必然存在人生的終極哲學之中，感謝老師。
