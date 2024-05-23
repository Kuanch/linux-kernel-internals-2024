# 2024q1 Term Project (prerequisite)
contributed by < [Kuanch](https://github.com/Kuanch) >

###### tags: `Linux Kenrel` `Buildroot` `QEMU` `GDB`

:::spoiler lscpu
```powershell!
$ lscpu
lscpu
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
CPU MHz:                            2400.000
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
Flags:                              fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl x
                                    topology nonstop_tsc cpuid aperfmperf tsc_known_freq pni pclmulqdq dtes64 monitor ds_cpl vmx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx 
                                    f16c rdrand lahf_lm abm 3dnowprefetch cpuid_fault epb cat_l2 invpcid_single cdp_l2 ssbd ibrs ibpb stibp ibrs_enhanced tpr_shadow vnmi flexpriority ept vpid ept_ad fsgsbase tsc_adjust bmi1 avx2 smep bmi2 
                                    erms invpcid rdt_a avx512f avx512dq rdseed adx smap avx512ifma clflushopt clwb intel_pt avx512cd sha_ni avx512bw avx512vl xsaveopt xsavec xgetbv1 xsaves split_lock_detect dtherm ida arat pln pts hwp hwp_
                                    notify hwp_act_window hwp_epp hwp_pkg_req avx512vbmi umip pku ospke avx512_vbmi2 gfni vaes vpclmulqdq avx512_vnni avx512_bitalg avx512_vpopcntdq rdpid movdiri movdir64b fsrm avx512_vp2intersect md_clear 
                                    flush_l1d arch_capabilities
```
:::

## Term Project Proposal

1. [以 QEMU + remote GDB + Buildroot 理解 Linux Kernel Scheduler 調用](#%E4%BB%A5-QEMU--remote-GDB--Buildroot-%E7%90%86%E8%A7%A3-Linux-Kernel-Scheduler-%E8%AA%BF%E7%94%A8)
2. 撰寫 Kernel Module 評估現有排程策略之效能 (EEVDF, EAS)
3. 嘗試以 sched_ext 撰寫簡單的排程器，並驗證其效能差異


## [Linux 核心專題: CPU 排程器研究](https://hackmd.io/@sysprog/rkJd7TFX0)
本專題專注於排程器研究，包含相關材料閱讀以及排程器測試分析工具：
* 研讀《Demystifying the Linux CPU Scheduler》
* 研究 EEVDF 及 BORE 並使用 schbench 等工具量化其表現
* 彙整排程器素材
* 《Demystifying the Linux CPU Scheduler》第七章的測試程式研究

## Kernel Analysis with QEMU + Buildroot + GDB
### QEMU
下載原始碼並編譯：
```powershell
$ wget https://download.qemu.org/qemu-8.2.3.tar.xz
$ ./configure --enable-debug
$ make -j8
$ make install
# $ sudo cp qemu-8.2.3/build/qemu-bundle/usr/local/bin/qemu-system* /usr/local/bin
```
建議將 debug 設定打開，此外我因在使用 linux gdb debug 指令 `lx-` 等遭遇問題，我亦重新將 gcc g++ gdb 等都升級版本：
```powershell
$ gcc -v
Using built-in specs.
COLLECT_GCC=gcc-11
COLLECT_LTO_WRAPPER=/usr/lib/gcc/x86_64-linux-gnu/11/lto-wrapper
OFFLOAD_TARGET_NAMES=nvptx-none:amdgcn-amdhsa
OFFLOAD_TARGET_DEFAULT=1
Target: x86_64-linux-gnu
Configured with: ../src/configure -v --with-pkgversion='Ubuntu 11.4.0-2ubuntu1~20.04' --with-bugurl=file:///usr/share/doc/gcc-11/README.Bugs --enable-languages=c,ada,c++,go,brig,d,fortran,objc,obj-c++,m2 --prefix=/usr --with-gcc-major-version-only --program-suffix=-11 --program-prefix=x86_64-linux-gnu- --enable-shared --enable-linker-build-id --libexecdir=/usr/lib --without-included-gettext --enable-threads=posix --libdir=/usr/lib --enable-nls --enable-bootstrap --enable-clocale=gnu --enable-libstdcxx-debug --enable-libstdcxx-time=yes --with-default-libstdcxx-abi=new --enable-gnu-unique-object --disable-vtable-verify --enable-plugin --enable-default-pie --with-system-zlib --enable-libphobos-checking=release --with-target-system-zlib=auto --enable-objc-gc=auto --enable-multiarch --disable-werror --enable-cet --with-arch-32=i686 --with-abi=m64 --with-multilib-list=m32,m64,mx32 --enable-multilib --with-tune=generic --enable-offload-targets=nvptx-none=/build/gcc-11-PfdVzN/gcc-11-11.4.0/debian/tmp-nvptx/usr,amdgcn-amdhsa=/build/gcc-11-PfdVzN/gcc-11-11.4.0/debian/tmp-gcn/usr --without-cuda-driver --enable-checking=release --build=x86_64-linux-gnu --host=x86_64-linux-gnu --target=x86_64-linux-gnu --with-build-config=bootstrap-lto-lean --enable-link-serialization=2
Thread model: posix
Supported LTO compression algorithms: zlib zstd
gcc version 11.4.0 (Ubuntu 11.4.0-2ubuntu1~20.04)

$ g++ -v
Using built-in specs.
COLLECT_GCC=g++-11
COLLECT_LTO_WRAPPER=/usr/lib/gcc/x86_64-linux-gnu/11/lto-wrapper
OFFLOAD_TARGET_NAMES=nvptx-none:amdgcn-amdhsa
OFFLOAD_TARGET_DEFAULT=1
Target: x86_64-linux-gnu
Configured with: ../src/configure -v --with-pkgversion='Ubuntu 11.4.0-2ubuntu1~20.04' --with-bugurl=file:///usr/share/doc/gcc-11/README.Bugs --enable-languages=c,ada,c++,go,brig,d,fortran,objc,obj-c++,m2 --prefix=/usr --with-gcc-major-version-only --program-suffix=-11 --program-prefix=x86_64-linux-gnu- --enable-shared --enable-linker-build-id --libexecdir=/usr/lib --without-included-gettext --enable-threads=posix --libdir=/usr/lib --enable-nls --enable-bootstrap --enable-clocale=gnu --enable-libstdcxx-debug --enable-libstdcxx-time=yes --with-default-libstdcxx-abi=new --enable-gnu-unique-object --disable-vtable-verify --enable-plugin --enable-default-pie --with-system-zlib --enable-libphobos-checking=release --with-target-system-zlib=auto --enable-objc-gc=auto --enable-multiarch --disable-werror --enable-cet --with-arch-32=i686 --with-abi=m64 --with-multilib-list=m32,m64,mx32 --enable-multilib --with-tune=generic --enable-offload-targets=nvptx-none=/build/gcc-11-PfdVzN/gcc-11-11.4.0/debian/tmp-nvptx/usr,amdgcn-amdhsa=/build/gcc-11-PfdVzN/gcc-11-11.4.0/debian/tmp-gcn/usr --without-cuda-driver --enable-checking=release --build=x86_64-linux-gnu --host=x86_64-linux-gnu --target=x86_64-linux-gnu --with-build-config=bootstrap-lto-lean --enable-link-serialization=2
Thread model: posix
Supported LTO compression algorithms: zlib zstd
gcc version 11.4.0 (Ubuntu 11.4.0-2ubuntu1~20.04)

$ gdb -v
GNU gdb (Ubuntu 10.2-0ubuntu1~20.04~1) 10.2
Copyright (C) 2021 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.

$ gdbserver --version
GNU gdbserver (Ubuntu 10.2-0ubuntu1~20.04~1) 10.2
Copyright (C) 2021 Free Software Foundation, Inc.
gdbserver is free software, covered by the GNU General Public License.
This gdbserver was configured as "x86_64-linux-gnu"

$ qemu-system-x86_64 --version
QEMU emulator version 8.2.3
Copyright (c) 2003-2023 Fabrice Bellard and the QEMU Project developers
```

### Buildroot
在編譯階段，由於我們同時需要設定開啟 debug option 供 GDB 使用，也要提供 kernel module 介面，供載入 VM 時使用，故編譯選項的設定較預設複雜。

#### Buildroot configuration
首先根據 [How to add a Linux kernel driver module as a Buildroot package?](https://stackoverflow.com/questions/40307328/how-to-add-a-linux-kernel-driver-module-as-a-buildroot-package) 設定 `.config`，並根據 [Linux Kernel Debugging](https://github.com/Rhydon1337/linux-kernel-debugging) 手動編輯 `.config` 加入 `BR2_ENABLE_DEBUG`，至此我們完成 buildroot configuration。

```powershell
$ make BR2_EXTERNAL="$(pwd)/../kernel_module" qemu_x86_64_defconfig
$ echo 'BR2_PACKAGE_KERNEL_MODULE=y
BR2_TARGET_ROOTFS_EXT2_EXTRA_BLOCKS=1024
BR2_ROOTFS_OVERLAY="overlay"
BR2_ENABLE_DEBUG=y
' >> .config
```

或使用 `make menuconfig` 介面編輯。

#### Buildroot Linux Kernel configuration

由於 Buildroot 會一併將 Linux 核心以及相關 toolchain 都編譯出來，我們亦需要提供 linux kernel compiling configuration；輸入 `make linux-menuconfig` 後，由於我們使用了 `BR2_ENABLE_DEBUG`，其會確認一連串 debug 相關設定：
![image](https://hackmd.io/_uploads/rk_bG70WR.png)

注意此處，若選項未在 `.config` 未被設定，則會被要求選擇；我原先忽略了設定 `BR2_OPTIMIZE_2` 而非 `BR2_OPTIMIZE_G`，導致使用 GDB 時變數會被最佳化；一樣可透過編輯 `.config` 修改。

之後出現 Linux Kernel configuration menu，更改 
**1. Kernel Hacking/Debug information/Generate DWARF Version 5 debuginfo**
**. Kernel Hacking/Kernel debugging**
**2. Kernel Hacking/Generic Kernel Debugging Instruments/KGDB: kernel debugger**
**3. Kernel Hacking/Compile-time checks and compiler options/Provide GDB scripts for kernel debugging**
4. Kernel Hacking/Scheduler Debugging/Collect scheduler statistics
5. Kernel Hacking/Debug preemptible kernel
6. Kernel Hacking/Remote debugging over FireWire early on boot
7. Kernel Hacking/Debug shared IRQ handlers
8. Kernel Hacking/Latency measuring infrastructure
9. Kernel Hacking/Debug kernel data structures/Debug linked list manipulation
10. General setup/BPF subsystem/Enable bpf() system call
11. General setup/BPF subsystem/Enable BPF Just In Time compiler

**前四個必定要打開供後續 GDB debugging 之用**，除了使用 menuconfig，知道 symbol 的話也可以直接編輯 `board/qemu/x86_64/linux.config`。

接著將編譯 Buildroot 的各項工具 `make -j8`，編譯完成後執行：
```powershell
$ qemu-system-x86_64 \
    -M pc \
    -kernel ./output/images/bzImage \
    -drive file=./output/images/rootfs.ext2,if=virtio,format=raw \
    -append "root=/dev/vda console=ttyS0 nokaslr" \
    -net user,hostfwd=tcp:127.0.0.1:3333-:22 \
    -net nic,model=virtio \
    -nographic \
    -S -s
```
即可到下一步開啟 remote gdb 連接 VM。

### apply remote GDB on QEMU and Buildroot
:::warning
注意，當鍵入 `apropos lx-` 並未顯示任何訊息表示 GDB scripts 未被成功設置，首先確認 Linux Kernel configuration 中的第三點已經被打開；並檢查 `~/.gdbinit` 以及 `/root/.gdbinit` 有寫入腳本位置，參考
* [Loading .gdbinit from current directory fails with "auto-loading has been declined by your `auto-load safe-path'"](https://stackoverflow.com/questions/16595417/loading-gdbinit-from-current-directory-fails-with-auto-loading-has-been-declin)
* [Debugging kernel and modules via gdb](https://www.kernel.org/doc/html/v4.10/dev-tools/gdb-kernel-debugging.html)
:::


由於 `qemu-system-x86_64 ... -s` 已經將 gdbserver:1234 打開，故我們可以進入 gdb 後接入 server，並設定中斷點
```powershell
$ gdb output/build/linux-{$}/vmlinux
(gdb) target remote :1234
(gdb) b start_kernel
```

或使用以下指令連接 gdbserver
```powershell
$ gdb \
    -ex "add-auto-load-safe-path $(pwd)" \
    -ex "file output/build/linux-$BR_LINUX_KERNEL_VERSION/vmlinux" \
    -ex 'set arch i386:x86-64:intel' \
    -ex 'target remote localhost:1234' \
    -ex 'break start_kernel' \
    -ex 'continue' \
    -ex 'disconnect' \
    -ex 'set arch i386:x86-64' \
    -ex 'target remote localhost:1234'
```

我們可以使用 GDB script 搭配 gdb 中斷點監測核心的運行狀況，如
```powershell
(gdb) target remote :1234
Remote debugging using :1234
0x000000000000fff0 in exception_stacks ()
(gdb) b schedule
Breakpoint 1 at 0xffffffff8177e9e0: file kernel/sched/core.c, line 6766.
(gdb) c
Continuing.

Breakpoint 1, schedule () at kernel/sched/core.c:6766
6766    {
(gdb) lx-ps
      TASK          PID    COMM
0xffffffff81e0a480   0   swapper/0
0xffff888002480000   1   swapper/0
0xffff888002480d80   2   swapper/0
(gdb) (gdb) c
Continuing.

Breakpoint 1, schedule () at kernel/sched/core.c:6766
6766    {
(gdb) lx-ps
      TASK          PID    COMM
0xffffffff81e0a480   0   swapper/0
0xffff888002480000   1   swapper/0
0xffff888002480d80   2   swapper/0
(gdb) c
Continuing.

Breakpoint 1, schedule () at kernel/sched/core.c:6766
6766    {
(gdb) lx-ps
      TASK          PID    COMM
0xffffffff81e0a480   0   swapper/0
0xffff888002480000   1   swapper/0
0xffff888002480d80   2   swapper/0
(gdb) c
Continuing.

Breakpoint 1, schedule () at kernel/sched/core.c:6766
6766    {
(gdb) lx-ps
      TASK          PID    COMM
0xffffffff81e0a480   0   swapper/0
0xffff888002480000   1   swapper/0
0xffff888002480d80   2   kthreadd
0xffff888002481b00   3   pool_workqueue_
// ...
(gdb) info registers 
rax            0x0                 0
rbx            0xffffffff81e0a480  -2115984256
rcx            0x0                 0
rdx            0x4000000000000000  4611686018427387904
rsi            0xffffffff81bbf453  -2118388653
rdi            0x4bf4              19444
rbp            0xffffffff81e03e38  0xffffffff81e03e38
rsp            0xffffffff81e03e30  0xffffffff81e03e30
...
(gdb) p *(struct file *)$rsi
$1 = {{f_llist = {next = 0x646165722f736600}, f_rcuhead = {next = 0x646165722f736600, func = 0x632e65746972775f}, f_iocb_flags = 796091904}, f_lock = {{rlock = {raw_lock = {{val = {
              counter = 1601398272}, {locked = 0 '\000', pending = 102 'f'}, {locked_pending = 26112, tail = 24435}}}}}}, f_mode = 1952543859, f_count = {counter = 8317150582334387039}, f_pos_lock = {
    owner = {counter = 8097303072271115776}, wait_lock = {raw_lock = {{val = {counter = 1818194799}, {locked = 111 'o', pending = 115 's'}, {locked_pending = 29551, tail = 27743}}}}, osq = {tail = {
        counter = 7037807}}, wait_list = {next = 0x6c696600706c6966, prev = 0x6c696600726e2d65}}, f_pos = 8245528484792577381, f_flags = 1701867359, f_owner = {lock = {raw_lock = {{cnts = {
            counter = 1886745391}, {wlocked = 47 '/', __lstate = "sup"}}, wait_lock = {{val = {counter = 1663988325}, {locked = 101 'e', pending = 114 'r'}, {locked_pending = 29285, 
              tail = 25390}}}}}, pid = 0x252d7338322e2500, pid_type = 620782700, uid = {val = 1126185587}, euid = {val = 1948741217}, signum = 1701867296}, f_cred = 0x646b636f6c62206e, f_ra = {
    start = 2738302304796046949, size = 1933454707, async_size = 1869444447, ra_pages = 7630453, mmap_miss = 2002739827, prev_pos = 8286750249527306610}, f_path = {mnt = 0x6166656761705f62, 
    dentry = 0x5f62730073746c75}, f_inode = 0x6c616e7265746e69, f_op = 0x735f733e2d732600, f_version = 7738151096000147065, private_data = 0x765f733e2d732600, f_ep = 0x6d616e65725f7366, 
  f_mapping = 0x786574756d5f65, f_wb_err = 1043165990, f_sb_err = 1902403443}
```


## What is eBPF (extended Berkeley Packet Filter)
動態追蹤 (Dynamic Tracing) 相對於靜態追蹤，它致力於在系統運行時讀取系統的各項狀態，就像是一個隨時變化內容的資料庫；由於有許多系統是不允許隨時下線維護或者其靜態分析問題是困難或無意義的，動態追蹤的技術對於系統分析故十分重要。

![image](https://hackmd.io/_uploads/B1otonjlA.png)

eBPF 的前身 BPF (cBPF) 用於分析網路封包，並運行於 Linux 核心之中，避免頻繁地在 Kernel mode 與 user model 切換，BPF 顯然是一種動態追蹤技術，但其仍不時需要將資料從核心層級複製到使用者層級；而 eBPF 在其基礎之上已不滿足單純的分析系統狀態，其本質上是運行於 Linux 核心中的虛擬機器，透過 [BPF Compiler Collection (BCC)](https://github.com/iovisor/bcc)，使用 LLVM toolschain 編譯 C code 為 eBPF bytecode 運行於 Linux 核心中。

![image](https://hackmd.io/_uploads/ryDe6njg0.png)

(?)上圖是 eBPF 在不同 Linux 核心版本所支援可追蹤的事件，注意到在 Linux 4.7 之後，就整合了追蹤 scheduler 事件的功能；由於我們的目標是要令 Linux 核心運行我們所寫的 CPU scheduler，eBPF 無疑成為最好的選擇之一。

### How eBPF "inject" into Kernel
:::info
一個主要的疑問是，究竟是 Kernel 直接執行 eBPF bytecode，也就是植入，還是存在中間層，Kernel 實際僅是提供環境運行 eBPF 程式？根據 [scx README.md](https://github.com/sched-ext/scx/blob/main/README.md)：

>sched_ext is a Linux kernel feature which **enables implementing kernel thread** schedulers in BPF and dynamically loading them.

以及
>Above, we **switch the whole system to use scx_simple** by running the binary, suspend it with ctrl-z to confirm that it's loaded, and **then switch back to the kernel default scheduler** by terminating the process with ctrl-c.

這暗示了使用 eBPF 技術的 sched_ext 是使 Kernel 直接運行 customized scheduler 的。
:::

我們可以使用 Linux Kernel 提供的 GDB scripts 更方便的知道核心的各項資訊，譬如 `lx-`

注意，若鍵入 `apropos lx-` 後並沒有顯示可用的 `lx-` 等 GDB scripts，請檢查是否開啟 Linux Kernel configuration menu 的第三項，並按 gdb 提示編輯 `~/.gdbinit` 及 `/root/.gdbinit`，其他相關疑難雜症請參考
[Debugging kernel and modules via gdb](https://www.kernel.org/doc/html/v4.10/dev-tools/gdb-kernel-debugging.html)
[]()
[Makefile : no rule to make target '/constants.py.in' needed by '/constants.py'. Stop](https://stackoverflow.com/questions/67969592/makefile-no-rule-to-make-target-constants-py-in-needed-by-constants-py)
[Using +gdb+ in Buildroot](https://buildroot.org/downloads/manual/using-buildroot-debugger.adoc)


## What is sched_ext
CPU scheduler 的調用十分複雜且難以調適以符合第三方應用，直接修改核心程式碼難度非常高，且第三方的自定義擴充幾乎不可能被以注重泛用性、可攜帶性的 Linux 核心採用，使得 CPU scheduler 難以發展；於是 sched_ext 應運而生，透過 eBPF 我們能夠較便利的「植入」自定義排程器，並對排程器進行分析、調適、實驗。

隨後我們會詳細分析 sched_ext 實際如何運作客製化排程器。

### sched_ext setup
安裝流程可以參考 [Linux 核心設計: 開發與測試環境](https://hackmd.io/@RinHizakura/SJ8GXUPJ6#virtme-ng)

#### Trouble-shooting
1. 確定安裝 `clang` `lld` `lldb` `ld.lld`且連結 bin file
    ```powershell
    $ clang --version
    Ubuntu clang version 17.0.6 (++20231208085846+6009708b4367-1~exp1~20231208085949.74)
    Target: x86_64-pc-linux-gnu
    Thread model: posix
    InstalledDir: /usr/bin
    $ lld --version
    lld is a generic driver.
    Invoke ld.lld (Unix), ld64.lld (macOS), lld-link (Windows), wasm-ld (WebAssembly) instead
    $ lldb --version
    lldb version 17.0.6
    $ ld.lld --version
    Ubuntu LLD 17.0.6 (compatible with GNU linkers)
    ```
3. upgrade clang and pahole
    * 若直接透過 `apt-get install dwarves` 安裝在編譯 `tools/sched_ext` 可能會遭遇以下問題，因其安裝版本小於 `1.25`：
    ```
    error: static assertion failed due to requirement 'SCX_DSQ_FLAG_BUILTIN': bpftool generated vmlinux.h is missing high bits for 64bit enums, upgrade clang and pahole
            _Static_assert(SCX_DSQ_FLAG_BUILTIN,
                           ^~~~~~~~~~~~~~~~~~~~
    1 error generated.
    ```
    升級 `dwarves` 版本解決問題：
    ```powershell
    $ wget https://fedorapeople.org/~acme/dwarves/dwarves-1.25.tar.xz
    $ tar -xvf dwarves-1.25.tar.xz && cd dwarves-1.25libdw-dev
    $ mkdir build && cd build
    $ cmake -D__LIB=lib -DBUILD_SHARED_LIBS=OFF .   // might need to install libdw-dev
    $ make install
    ```
    注意此時需要重新編譯 sched_ext 之 Linux 核心。
2. 在準備編譯 dwarves cmake 時遭遇 `missing: LIBDWARF_LIBRARIES LIBDWARF_INCLUDE_DIRS`
    * `sudo apt-get install libdw-dev` [參考](https://github.com/SimonKagstrom/kcov/issues/22)
    
2. 使用 pip3 安裝 virtme-ng 時遭遇 `error: Error: setup script specifies an absolute path`
    * 執行 `BUILD_VIRTME_NG_INIT=1 pip3 install --verbose -r requirements.txt .` 遭遇問題
    * 透過[修改](https://stackoverflow.com/a/45813710) `setup.py` 解決該問題

### How `scx_simple.c` works
`scx_simple.c` 是 sched_ext 提供的簡單範例，其 `main()` 實作如下：
```c
int main(int argc, char **argv)
{
	struct scx_simple *skel;
	struct bpf_link *link;
	__u32 opt;

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

	skel = scx_simple__open();
	SCX_BUG_ON(!skel, "Failed to open skel");

	while ((opt = getopt(argc, argv, "fh")) != -1) {
		switch (opt) {
		case 'f':
			skel->rodata->fifo_sched = true;
			break;
		default:
			fprintf(stderr, help_fmt, basename(argv[0]));
			return opt != 'h';
		}
	}

	SCX_OPS_LOAD(skel, simple_ops, scx_simple, uei);
	link = SCX_OPS_ATTACH(skel, simple_ops);

	while (!exit_req && !UEI_EXITED(skel, uei)) {
		__u64 stats[2];

		read_stats(skel, stats);
		printf("local=%llu global=%llu\n", stats[0], stats[1]);
		fflush(stdout);
		sleep(1);
	}

	bpf_link__destroy(link);
	UEI_REPORT(skel, uei);
	scx_simple__destroy(skel);
	return 0;
}
```




事實上，我們完全可以不理解 BPF 機制，僅需實作幾項關鍵函式，如 `ops.select_cpu()` `ops.enqueue()` `ops.dispatch()` 等，參考 [Scheduling Cycle](https://github.com/sched-ext/sched_ext/blob/sched_ext/Documentation/scheduler/sched-ext.rst#scheduling-cycle)，`scx_simple.bpf.c` 提供以下範例

```c
s32 BPF_STRUCT_OPS(simple_select_cpu, struct task_struct *p, s32 prev_cpu, u64 wake_flags)
{
	bool is_idle = false;
	s32 cpu;

	cpu = scx_bpf_select_cpu_dfl(p, prev_cpu, wake_flags, &is_idle);
	if (is_idle) {
		stat_inc(0);	/* count local queueing */
		scx_bpf_dispatch(p, SCX_DSQ_LOCAL, SCX_SLICE_DFL, 0);
	}

	return cpu;
}

void BPF_STRUCT_OPS(simple_enqueue, struct task_struct *p, u64 enq_flags)
{
	stat_inc(1);	/* count global queueing */

	if (fifo_sched) {
		scx_bpf_dispatch(p, SHARED_DSQ, SCX_SLICE_DFL, enq_flags);
	} else {
		u64 vtime = p->scx.dsq_vtime;

		/*
		 * Limit the amount of budget that an idling task can accumulate
		 * to one slice.
		 */
		if (vtime_before(vtime, vtime_now - SCX_SLICE_DFL))
			vtime = vtime_now - SCX_SLICE_DFL;

		scx_bpf_dispatch_vtime(p, SHARED_DSQ, SCX_SLICE_DFL, vtime,
				       enq_flags);
	}
}
```

## 以 QEMU + remote GDB + Buildroot 理解 Linux Kernel Scheduler 調用

我們使用以下命令列模擬 Linux 核心
```power
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
```power
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

### PID 0 與 `start_kernel()` 的 GDB 初探
`start_kernel` 是 Linux 核心的進入點，在這裡會執行許多初始化操作，參考[從 start_kernel 到第一個任務: Linux Scheduler 閱讀筆記 (2)](https://hackmd.io/@Kuanch/linux-kernel-scheduler-notes4)。

除此之外，當我們在`start_kernel` 設置中斷點導致中斷，此時鍵入 `lx-ps` 已經有 PID 0 ，並事實上 `start_kernel` 是由其執行，並創造第一個 user process 為 PID 1，用於初始化各項子系統。

而 PID 0 在初始化系統後，將成為 CPUIdle 模組的一部分，用於確保排程器總是有任務可排程，可用於減少 CPU 陷入睡眠的時間、減少功耗；亦可參考 [Linux 核心設計: CPUIdle(1): 子系統架構](https://hackmd.io/@RinHizakura/SyPwWWZkC)。

:::info
:::spoiler PID = 0 到底是什麼？(jserv)
>PID=1 通常是 init 或 systemd，即第一個 user-space process，所有的 Linux 行程均衍生 (fork) 於此，但我們不免好奇：
「PID = 0 在哪？」
>
>要解釋這議題，需要追溯到 UNIX 的歷史，首先 Solaris 是 Sun Microsystems (現為 Oracle) 發展的作業系統，血統純正，繼承絕大多數來自 AT&T UNIX 的設計，Linux 充其量只能說是「泛 UNIX 家族」，並未直接繼承 UNIX，而是取鏡 UNIX 的經典設計。因此在面對「PID = 0 為何者？」的問題時，我們應當區隔 UNIX/Solaris 和 Linux。
>
> 在早期 UNIX 設計中，PID = 0 為 swapper process，一如字面上的意思，swapper 會置換整個 process 的內容，包含核心模式的資料結構，到儲存空間 (通常為磁帶或硬碟)，也會反過來將儲存空間的內容還原為 process，這在 PDP-7 和 PDP-11 主機上，是必要的機制，其 MMU 不具備今日完整的虛擬記憶體能力。
> 
>這種置換形式的記憶體管理，讓 UNIX 效率不彰，於是在 UNIX System V R2V5 和 4.BSD (1980 年代) 就將 swapper process 更換為 demand-paged 的虛擬記憶體管理機制，後者是 BSD 開發團隊向 Mach 微核心學習並改造。
>PID = 0 是系統第一個 process，而 PID = 1 則是第一個 user process，由於 UNIX/BSD 朝向 demand paging 演化，swapper process 就失去原本的作用，UNIX System V 則乾脆將 "swapper" 更名為 "sched"，表示實際是 scheduler() 函式。
>
>發展於 1990 年代的 Linux 核心就沒有上述「包袱」，但基於歷史因素，Linux 也有 PID =0 的 process，稱為 idle process，對應 cpu_idle() 函式，本身只是無窮迴圈，無其他作用，其存在只是確保排程器總是有任務可排程。
>
>我們在GNU/Linux 執行 ps -f 1 命令，可發現以下輸出:
>
>    UID PID PPID CMD
>    root 1 0 /sbin/init
>
>顯然 PID =1 的 init 或 systemd 行程，其 parent PID 為 0，即 idle process，也維持「從零開始」的優良傳統。
>
>在 AT&T, BSDi, 加州大學柏克萊分校的三方官司訴訟後，BSD 家族 (FreeBSD, NetBSD, OpenBSD, DragonflyBSD 等等) 浴火重生，趁著系統改寫 (以符合授權規範) 之際，改變實際 PID = 0 的動作，可能是系統初始化或者如 Linux 一般的 idle。

[來源](https://www.facebook.com/groups/system.software2023/posts/623107396308085/)
:::

此外，我們也可得知，這個當下被中斷的正是 PID 0 行程，正在執行 `/init` 的路上：
```power
(gdb) p &init_task
$1 = (struct task_struct *) 0xffffffff8200a880 <init_task>
(gdb) p init_task.comm
$2 = "swapper\000\000\000\000\000\000\000\000"
(gdb) x $esp
0xffffffff82003f20:     0x821b303c
(gdb) p init_task.stack
$3 = (void *) 0xffffffff82000000
```
我們可以發現，目前 kernel stack 被使用了 `3f20` 約是 16KB，似乎比想像中大一些，根據 [Kernel stacks on x86-64 bit](https://docs.kernel.org/6.2/x86/kernel-stacks.html)，每一個 thread (task_struct) 約占用 8KB：
>x86_64 page size (PAGE_SIZE) is 4K.
>
>Like all other architectures, x86_64 has a kernel stack for every active thread. These thread stacks are THREAD_SIZE (2*PAGE_SIZE) big.

而事實上該行程僅佔 6,784 bytes：
```bash
(gdb) p sizeof(init_task)
$36 = 6784
```

我們透過以下方法看看目前 function call 使用 stack 的狀況：
```power
(gdb) bt
#0  start_kernel () at init/main.c:875
#1  0xffffffff821b303c in x86_64_start_reservations (real_mode_data=real_mode_data@entry=0x13b30 <exception_stacks+31536> <error: Cannot access memory at address 0x13b30>)
    at arch/x86/kernel/head64.c:556
#2  0xffffffff821b318a in x86_64_start_kernel (real_mode_data=0x13b30 <exception_stacks+31536> <error: Cannot access memory at address 0x13b30>) at arch/x86/kernel/head64.c:537
#3  0xffffffff810001dd in secondary_startup_64 () at arch/x86/kernel/head_64.S:449
#4  0x0000000000000000 in ?? ()
```
以及
```power
(gdb) x/4096x 0xffffffff82000000
0xffffffff82000000:     0x00000000      0x00000000      0x00000000      0x00000000
0xffffffff82000010:     0x00000000      0x00000000      0x00000000      0x00000000
0xffffffff82000020:     0x00000000      0x00000000      0x00000000      0x00000000
0xffffffff82000030:     0x00000000      0x00000000      0x00000000      0x00000000
...
0xffffffff82003e40:     0x00013b30      0x00000000      0x82003e68      0xffffffff
0xffffffff82003e50:     0x821b2ff8      0xffffffff      0x00013b30      0x00000000
0xffffffff82003e60:     0x000000a0      0x00000000      0x82003f48      0xffffffff
0xffffffff82003e70:     0x821b2baf      0xffffffff      0x00013b30      0x00000000
0xffffffff82003e80:     0x00000000      0x00000000      0x00000000      0x00000000
0xffffffff82003e90:     0x000000a0      0x00000000      0x82003f48      0xffffffff
0xffffffff82003ea0:     0x00013b30      0x00000000      0xffffffff      0xffff0000
0xffffffff82003eb0:     0x0000ffff      0xffffffff      0xffffffff      0x0000ffff
0xffffffff82003ec0:     0x00000000      0x00000000      0x00000000      0x00000000
0xffffffff82003ed0:     0x82003f10      0xffffffff      0xffffffff      0x00000000
0xffffffff82003ee0:     0x000001f0      0x00000000      0x00013b30      0xffff8880
0xffffffff82003ef0:     0x00000000      0x00000000      0x00000000      0x00000000
0xffffffff82003f00:     0x821bfe24      0xffffffff      0x00013b30      0x00000000
0xffffffff82003f10:     0x000000a0      0x00000000      0x00000000      0x00000000
0xffffffff82003f20:     0x821b303c      0xffffffff      0x82003f48      0xffffffff
0xffffffff82003f30:     0x821b318a      0xffffffff      0x00000000      0x00000000
0xffffffff82003f40:     0x00000000      0x00000000      0x00000000      0x00000000
0xffffffff82003f50:     0x810001dd      0xffffffff      0x00000000      0x00000000
...
0xffffffff82003ff0:     0x00000000      0x00000000      0x00000000      0x00000000
```
透過觀察以上資料，我們可以如下解讀：
1. `0xffffffff82000000` - `0xffffffff82003f20` 是 `start_kernel()` 所使用的 stack frame
2. function call `x86_64_start_reservations` 使用了 `0xffffffff82003f20` + 16 bytes
3. function call `x86_64_start_kernel` 使用了 `0xffffffff82003f30` + 16 bytes
4. function call `secondary_startup_64` 使用了 `0xffffffff82003f50` + 16 bytes

在這之後，我們讓 GDB 繼續初始化流程，直到登入 Linux。

### 使用 GDB 在 Linux 核心中追蹤程式
登入 Linux 後，在 GDB 介面按下 ctrl + c，會顯示我們目前正身在 `default_idle()` 中：
```power
(gdb) c
Continuing.
^C
Program received signal SIGINT, Interrupt.
default_idle () at arch/x86/kernel/process.c:743
743             raw_local_irq_disable();
```
此時鍵入 `where`，其 function call hierachy 如下：
```p
(gdb) where
#0  default_idle () at arch/x86/kernel/process.c:743
#1  0xffffffff8103c017 in amd_e400_idle () at arch/x86/kernel/process.c:862
#2  0xffffffff81843c3d in arch_cpu_idle () at arch/x86/kernel/process.c:779
#3  0xffffffff81843ea6 in default_idle_call () at kernel/sched/idle.c:97
#4  0xffffffff810cb276 in cpuidle_idle_call () at kernel/sched/idle.c:170
#5  do_idle () at kernel/sched/idle.c:282
#6  0xffffffff810cb4c9 in cpu_startup_entry (state=state@entry=CPUHP_ONLINE) at kernel/sched/idle.c:380
#7  0xffffffff81844620 in rest_init () at init/main.c:730
#8  0xffffffff821aa9bd in arch_call_rest_init () at init/main.c:827
#9  0xffffffff821aaeeb in start_kernel () at init/main.c:1072
#10 0xffffffff821b303c in x86_64_start_reservations (real_mode_data=real_mode_data@entry=0x13b30 <exception_stacks+31536> <error: Cannot access memory at address 0x13b30>)
    at arch/x86/kernel/head64.c:556
#11 0xffffffff821b318a in x86_64_start_kernel (real_mode_data=0x13b30 <exception_stacks+31536> <error: Cannot access memory at address 0x13b30>) at arch/x86/kernel/head64.c:537
#12 0xffffffff810001dd in secondary_startup_64 () at arch/x86/kernel/head_64.S:449
#13 0x0000000000000000 in ?? ()
```
我們也看看 `default_idle()` 是什麼：
```c
// defined at arch/x86/kernel/process.c
/*
 * We use this if we don't have any better idle routine..
 */
void __cpuidle default_idle(void)
{
	raw_safe_halt();
	raw_local_irq_disable();
}
```
故我們了解到，目前可能並沒有任何任務需要進行，詳細的 CPUIdle 機制我們暫且略過不提。

接下來我們想知道當我們啟動一個程式，他是如何被排入行程且被執行；由於 buildroot 不支援 compiler on target，故我們需要自行編輯相關 toolchain 以在 buildroot 內部使用 GCC，詳情參考我的 [Buildroot compiler on target]()。

首先我們轉寫一隻簡單的 C 程式，為了方便，我們執行一個簡單的無限迴圈，並控制接收到 `SIGNINT` 時的行為，當按下 ctrl + c，我們應當會在 `/var/log/syslog` (或是 `/var/log/messages`) 見到相應訊息：
```c
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
    signal(SIGINT, sigint_handler);
    while(1){
        if (stop) {
            syslog(LOG_INFO, "Caught SIGINT, exiting now");
            break; // Add break statement to exit the while loop
        }
    }
}
```
執行 `./infiteloop` 後，在 gdb 中斷並輸入 `lx-ps` 可以發現其正在運作
```power
(gdb) lx-ps
...
0xffff8880033b2640  110  udhcpc
0xffff8880033b0cc0  112  sh
0xffff8880033b4c80  113  getty
0xffff8880033b1980  122  inifiteloop
```
由於此時並沒有任務可以執行，當我們在 `kernel/sched/core.c` 中的 `schedule()` 設中斷點，隨即可以攔截到該程式
```power
(gdb) b kernel/sched/core.c:6768
(gdb) c
Continuing.

Breakpoint 1, schedule () at kernel/sched/core.c:6726
6726            if (task_is_running(tsk))
(gdb) p tsk->pid
$2 = 122
```
我們接下來將中斷點設定到我們熟悉的 `pick_next_task_fair`
```power
(gdb) b pick_next_task_fair
Breakpoint 2 at 0xffffffff810c3ee0: file kernel/sched/fair.c, line 8258.
(gdb) c
Continuing.

Breakpoint 4, pick_next_task_fair (rq=rq@entry=0xffff888007a2b900, prev=prev@entry=0xffff8880033b1980, rf=rf@entry=0xffffc900001d7e70) at kernel/sched/fair.c:8258
8258    {
(gdb) step
8259            struct cfs_rq *cfs_rq = &rq->cfs;
(gdb) p prev->pid
$4 = 122
(gdb) where
#0  pick_next_task_fair (rq=rq@entry=0xffff888007a2b900, prev=prev@entry=0xffff8880033b1980, rf=rf@entry=0xffffc900001d7e70) at kernel/sched/fair.c:8346
#1  0xffffffff818b20d1 in __pick_next_task (rf=0xffffc900001d7e70, prev=0xffff8880033b1980, rq=0xffff888007a2b900) at kernel/sched/core.c:6006
#2  pick_next_task (rf=0xffffc900001d7e70, prev=0xffff8880033b1980, rq=0xffff888007a2b900) at kernel/sched/core.c:6516
#3  __schedule (sched_mode=sched_mode@entry=0) at kernel/sched/core.c:6662
#4  0xffffffff818b2700 in schedule () at kernel/sched/core.c:6772
#5  0xffffffff81109b40 in exit_to_user_mode_loop (ti_work=8, regs=<optimized out>) at kernel/entry/common.c:159
#6  exit_to_user_mode_prepare (regs=0xffffc900001d7f58) at kernel/entry/common.c:204
#7  0xffffffff818aeb6d in irqentry_exit_to_user_mode (regs=<optimized out>) at kernel/entry/common.c:309
#8  0xffffffff818aebdf in irqentry_exit (regs=regs@entry=0xffffc900001d7f58, state=..., state@entry=...) at kernel/entry/common.c:412
#9  0xffffffff818ae06a in sysvec_apic_timer_interrupt (regs=0xffffc900001d7f58) at arch/x86/kernel/apic/apic.c:1076
#10 0xffffffff81a0156f in asm_sysvec_apic_timer_interrupt () at ./arch/x86/include/asm/idtentry.h:649
#11 0x0000000000000000 in ?? ()
```


### 使用 GDB 在 Linux 核心中追蹤 Kernel Module
如同前一節我們追中 User Program 在核心中的行為，此次我們關注 Kernel Module 是如何在核心中被排程以及運行的，和 User Program 的差別為和？

相同地，我們撰寫一隻簡單的 Kernel Module 如下，注意此處仍需要使用先前提到過的 [simrupt buildroot external toolchain]() 將本 linux module 載入 buildroot：
```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>

struct task_info {
    u64 start_time;
    u64 total_runtime;
    u64 switch_count;
    struct hlist_node hnode;
    pid_t pid;
};

static struct task_info *prev_info;

static int sched_switch_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
    return 0;
}

static void sched_switch_post_handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
}

static struct kprobe kp = {
    .symbol_name = "finish_task_switch.isra.0",
    .pre_handler = sched_switch_pre_handler,
    .post_handler = sched_switch_post_handler,
};

static int __init sched_monitor_init(void)
{
    printk(KERN_DEBUG "Hello, world!\n");
    int ret = register_kprobe(&kp);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register kprobe: %d\n", ret);
        return ret;
    }
    printk(KERN_DEBUG "Registered kprobe for finish_task_switch\n");

    prev_info = kmalloc(sizeof(struct task_info), GFP_KERNEL);
    if (!prev_info) {
        printk(KERN_ERR "Failed to allocate memory for prev_info\n");
        unregister_kprobe(&kp);
        return -ENOMEM;
    }
    printk(KERN_DEBUG "Allocated memory for prev_info\n");

    prev_info->start_time = jiffies;
    prev_info->total_runtime = 0;
    prev_info->switch_count = 0;
    prev_info->pid = current->pid;

    printk(KERN_INFO "Scheduler monitor module loaded.\n");
    printk(KERN_INFO "Running on PID: %d, Comm: %s\n", current->pid, current->comm);
    return 0;
}

static void __exit sched_monitor_exit(void)
{
    printk(KERN_INFO "Total runtime of Process %d: %llu\n", prev_info->pid, prev_info->total_runtime);
    kfree(prev_info);
    printk(KERN_INFO "Unregistering kprobe at %p\n", kp.addr);
    unregister_kprobe(&kp);
    printk(KERN_INFO "Scheduler monitor module unloaded.\n");
}

module_init(sched_monitor_init);
module_exit(sched_monitor_exit);

MODULE_LICENSE("Dual MIT/GPL");
```
我們透過 `cat /dev/cpureport` 來取得類似於當前行程的簡單資訊，並觀察其調用如下
```power
(gdb) 
```


### Reference
[Linux 核心設計: 透過 eBPF 觀察作業系統行為](https://hackmd.io/@sysprog/linux-ebpf?type=view)
[Linux 核心設計: 開發與測試環境](https://hackmd.io/@RinHizakura/SJ8GXUPJ6#virtme-ng)
[eBPF Programming for Linux Kernel Tracing](https://medium.com/@zone24x7_inc/ebpf-programming-for-linux-kernel-tracing-30364dde3fb7)
[Guide to compiling sched_ext and schedulers](https://www.reddit.com/r/sched_ext/comments/13k3eon/guide_to_compiling_sched_ext_and_schedulers/)
[測試 Linux 核心的虛擬化環境](https://hackmd.io/@sysprog/linux-virtme)
[动态追踪技术漫谈](https://blog.openresty.com.cn/cn/dynamic-tracing/)
[深入浅出运维可观测工具（一）：聊聊 eBPF 的前世今生](https://cloudnative.to/blog/current-state-and-future-of-ebpf/)
[sched_ext机制研究](https://rqdmap.top/posts/scx/)
[Linux 核心專題: 以 eBPF 建構 TCP 伺服器](https://hackmd.io/@sysprog/ryBw0adH2#Extended-Berkeley-Packet-Filter-eBPF)
[Buildroot and compiler on target](https://luplab.cs.ucdavis.edu/2022/01/06/buildroot-and-compiler-on-target.html)