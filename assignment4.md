# 2024q1 Homework4 (quiz3+4)
contributed by < [Kuanch](https://github.com/Kuanch) >

## Week 3 Quiz
[Week3 Quiz](https://hackmd.io/@sysprog/linux2024-quiz3)
### Quiz 1 - Bitwise Square

首先應為 $a_i = b_i2^i$，故任何整數 $N$ 可寫作
$$
N = b_02^{32}+b_12^{31}+...+b_{30}2+b_{31} = (a_{32}+a_{31}+a_{30}+...+{a_0})
$$

當然原本將 $b_0$ 為寫作有值的最高位，此處為了簡化直接假設該 32 位元整數之最高位位於最左方之第 32 位元處；為了再次簡化，我們假設 N = 15，故
$$
15 = 0 + 0 + 0 + ... + 1\cdot2^3 + 1\cdot2^2+ 1\cdot2^1+ 1\cdot2^0
$$
帶入展開後了 $N^2$ 得到
$$
15^2 = (2^{3})^2+[2\cdot2^3+2^2]\cdot2^2+[2(2^3+2^2)+2^1]\cdot2^1+[2(2^3+2^2+2^1)+2^0]\cdot2^0
$$
又
$$
\begin{split} \\
15^2 
=& (8+4+2+1)^2 \\
=& (2^3)^2+[2\cdot2^3+2^2]\cdot2^2+[2(2^3+2^2)+2^1]\cdot2^1+[2(2^3+2^2+2^1)+2^0]\cdot2^0 \\
\end{split}
$$

如作業說明 $P_m=8+4+3+2+1$ 即為所求，但還沒完，上述公式我們可以這樣觀察

$$
\begin{split} \\
(8+4+2+1)^2 =& (8+4+2+1)(8+4+2+1) \\
=& (8\cdot8)+(8\cdot4+8\cdot4+4\cdot4)+((8+4)\cdot2+(8+4)\cdot2+2\cdot2+2\cdot2) ...
\end{split}
$$

事實上**我們只是以 $a_n$ 作為項次去列出公式而已**，會發現其係數即是 $[2\left(\sum\limits_{i = 0}^{n-1}{a_i}\right)+a_n]$。

而程式碼的迴圈所做就是每一次都找出 $a_n$，即 `m` ，然後加回 $P_m$，即 `z`，且即 $P_m = P_{m+1} + a_m$，而當 `b` 還為大於等於 `x` 時，表示仍有項次還未歷遍，即
$$
P_m = P_{m+1} + 2^m, \text{if $P_m^2 \leq N^2$}
$$

#### fls 改進
首先，`1UL << ((31 - __builtin_clz(x))` 即為 `ilog2(x)` ，而 `~1UL` 則是當 $1<x<4$ 時避免位移，否則會直接使 `z = 2` 而超出 $\sqrt{x}$，m；而事實上我們能在 `include/linux/log2.h` 中發現使用 `fls` 的對數方法如下
```c
static __always_inline int fls(unsigned int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

static __always_inline __attribute__((const))
int __ilog2_u32(uint32_t n)
{
	return fls(n) - 1;
}

int i_sqrt_ver2(int x)
{
    if (x <= 1) /* Assume x is always positive */
        return x;

    int z = 0;
    for (int m = 1UL << __ilog2_u32(x) + 1; m; m >>= 2) {
        int b = z + m;
        z >>= 1;
        if (x >= b)
            x -= b, z += m;
    }
    return z;
}
```
透過 `fls()` 的 bitwise 操作，即可不依賴 GNU extension 實現開根。

### Quiz 2 - Bitwise Division and Modular
考慮以下程式碼
```c
void divmod_10(uint32_t in, uint32_t *div, uint32_t *mod)
{
    uint32_t x = (in | 1) - (in >> 2); /* div = in/10 ==> div = 0.75*in/8 */
    uint32_t q = (x >> 4) + x;
    x = q;
    q = (q >> 8) + x;
    q = (q >> 8) + x;
    q = (q >> 8) + x;
    q = (q >> 8) + x;

    *div = (q >> 3);
    *mod = in - ((q & ~0x7) + (*div << 1));
}
```
為方便理解，我們先假設 $in \leq \cfrac{128 \times64}{67}\approx122$，稍等我們會看到為什麼假設這個值，再來推廣到 $in \ge 122$ 的情境中。

首先 `uint32_t x = (in | 1) - (in >> 2);` 等價於
$$
x=
\begin{cases}
\cfrac{3(in + 1)}{4} , & \text{if x is even} \\
\cfrac{3in}{4} , & \text{if x is odd}
\end{cases}
$$
經過 `uint32_t q = (x >> 4) + x;` 後，等價於
$$
q=
\begin{cases}
\cfrac{67(in + 1)}{64} , & \text{if x is even} \\
\cfrac{67in}{64} , & \text{if x is odd}
\end{cases}
$$
因為我們假設 $in \leq 122$，所有的 `q = (q >> 8) + x;` 都可以忽略；這是因為右移 8 ，也就是除以 128 後會為零，僅剩 `+ x` 有值，故可以直接略過該四行，令 `q` 等於上式。

為了簡化說明，假設 `q` 為偶數；那麼右移 3 位，也就是除以8後
$$
\cfrac{q}{8}=(\cfrac{3}{4}in+\cfrac{19}{64}in)\cdot\cfrac{1}{8}=\cfrac{3}{32}in+\cfrac{19}{512}in \approx div
$$
由於 $in < 122$，後項可忽略，得前項$\cfrac{3}{32}\approx\cfrac{1}{10}$，即是 `div`；而由於 $in=10div+mod$ ，可得
$$
mod = in- 10div\approx in- \cfrac{30}{32}in=in-(\text{(q & !0x7)}+\cfrac{6}{32}in)=in-(\cfrac{24}{32}in+\cfrac{6}{32}in)
$$

`(q & ~0x7)`可以視作右移 3 位後再左移 3 位，**效果如同 $in-\cfrac{8}{32}in=\cfrac{24}{32}in$。(有問題，再想想)**

發現將 `(q & ~0x7)` 改為 `(*div << 3)` 運作正常，因為 `(*div << 3) + (*div << 1)` 即 $10\cdot div$；但為何兩者等價？係因像是 1/7 一樣二進制為循環數？

當 $128+122>in\geq122$ 時，`q = (q >> 8) + x;` 會被觸發，使得其值域再次回到 $in < 122$，故上述依然成立。

#### 評估 CPU 週期數量

考量[程式碼 (gist)](https://gist.github.com/Kuanch/658a79614f12b0ec6aa1df9f04c8d3b6#file-divmod-c)，並以 `gcc -O0 -o divmod divmod.c` 編譯和以 `objdump` 得到其分別的 [asm code (gist)](https://gist.github.com/Kuanch/658a79614f12b0ec6aa1df9f04c8d3b6#file-divmod-objdump)，並有以下 perf 效能分析：


```
 Performance counter stats for './divmod' (1000 runs):

              0.25 msec task-clock                #    0.729 CPUs utilized            ( +-  0.58% )
                 0      context-switches          #    0.000 /sec                   
                 0      cpu-migrations            #    0.000 /sec                   
                46      page-faults               #  221.960 K/sec                    ( +-  0.06% )
           94,7860      cycles                    #    4.574 GHz                      ( +-  0.51% )
          141,3930      instructions              #    1.84  insn per cycle           ( +-  0.03% )
           15,5680      branches                  #  751.189 M/sec                    ( +-  0.05% )
              1568      branch-misses             #    1.02% of all branches          ( +-  0.61% )
          367,1170      slots                     #   17.714 G/sec                    ( +-  0.24% )
          151,1658      topdown-retiring          #     44.2% retiring                ( +-  0.03% )
           17,2760      topdown-bad-spec          #      5.6% bad speculation         ( +-  0.44% )
          112,2946      topdown-fe-bound          #     30.0% frontend bound          ( +-  0.35% )
           86,3804      topdown-be-bound          #     20.2% backend bound           ( +-  0.58% )

        0.00034663 +- 0.00000229 seconds time elapsed  ( +-  0.66% )


 Performance counter stats for './divmod2' (1000 runs):

              0.17 msec task-clock                #    0.532 CPUs utilized            ( +-  0.83% )
                 0      context-switches          #    0.000 /sec                   
                 0      cpu-migrations            #    0.000 /sec                   
                46      page-faults               #  250.008 K/sec                    ( +-  0.06% )
           65,4373      cycles                    #    3.556 GHz                      ( +-  0.34% )
          109,9633      instructions              #    1.67  insn per cycle           ( +-  0.04% )
           15,7838      branches                  #  857.842 M/sec                    ( +-  0.05% )
              1665      branch-misses             #    1.08% of all branches          ( +-  0.58% )
          282,3770      slots                     #   15.347 G/sec                    ( +-  0.13% )
          119,5949      topdown-retiring          #     40.7% retiring                ( +-  0.04% )
           17,7177      topdown-bad-spec          #      6.4% bad speculation         ( +-  0.44% )
          104,0919      topdown-fe-bound          #     35.8% frontend bound          ( +-  0.30% )
           40,9723      topdown-be-bound          #     17.0% backend bound           ( +-  0.47% )

        0.00032812 +- 0.00000277 seconds time elapsed  ( +-  0.84% )
```

和 vax-r 同學結論不同，以 `perf stat -r 1000 ./divmod` 得到的 cycle 數量，直接使用除法和餘數運算子的數量僅有 bitwise 版本的 $\cfrac{2}{3}$ 倍；我使用之處理器為 Intel Core i5-1135G7，是 Tiger Lake 架構，且產生的 asm code 與 vax-r 同學的相同；單是兩個函式的數量，不使用 `/,%` 有 66 條指令，對比使用 `/,%` 33 條指令就有明顯差距，且後者使用 `imul` 乘上 magic number 也僅需 3 cpu cycles 而非 `div`。
 
#### 撰寫不依賴任何除法指令的 % 9 和 % 5
若要實作不同於 Hacker's Delight 程式碼，我認為可以是以下方法

1. 化為二進制
如 $\cfrac{1}{9} \approx \cfrac{4}{32} \approx \cfrac{7}{64} \approx \cfrac{14}{128}$，而後者可再拆分為二進制，如 $\cfrac{14}{128}=\cfrac{8}{128}+\cfrac{4}{128}+\cfrac{2}{128}$，故 $\cfrac{x}{9} \approx \cfrac{8}{128}x+\cfrac{4}{128}x+\cfrac{2}{128}x$。
2. 以 $x$ 值之大小判斷分母，譬如若 $x < 8$，使用 128 是不合理的，因 $14 \cdot 8 < 128$。
3. 以 bitwise shift 表示上述二進制
4. 得到商後，以 $mod = x - 9\cdot div$

以 $x=28$ 為例，欲求 $x \space mod\space9$ ，令 $\cfrac{1}{9} \approx \cfrac{4}{32}$，即左移 2 位後再右移 5 位，28 之二進制為 `11100.0` 操作後得 `11.1` ，商即為 3， $mod = x - 9\cdot 3 = 1$。

但這要求迴圈，效能會較 magic number 或 lookup table 的版本更差。

### Quiz 3 - Bitwise log2
採用二進制表示數時，任何 32 位元整數都可被寫作 
$$
b_{32}\cdot2^{32}+b_{31}\cdot2^{31}+ \ldots +b_{2}\cdot2^{2}+b_{1}\cdot2+b_{0}
$$

而三個版本皆係取最高位元作為 $log2$ 之近似值；為何是近似值？因每一次右移(即除以2)皆會忽略 $b_{0}$，導致誤差較大。版本二透過判斷值的大小一次右移更多位元，在數值較大時，能用更少的迴圈計算結果；而版本三直接呼叫編譯器內建之最佳化的函式，會有最佳的效能。

#### Linux Kernel 案例
在 `include/linux/log2.h` 中發現

```c
/*
 * non-constant log of base 2 calculators
 * - the arch may override these in asm/bitops.h if they can be implemented
 *   more efficiently than using fls() and fls64()
 * - the arch is not required to handle n==0 if implementing the fallback
 */
#ifndef CONFIG_ARCH_HAS_ILOG2_U32
static __always_inline __attribute__((const))
int __ilog2_u32(u32 n)
{
	return fls(n) - 1;
}
#endif

/**
 * ilog2 - log base 2 of 32-bit or a 64-bit unsigned value
 * @n: parameter
 *
 * constant-capable log of base 2 calculation
 * - this can be used to initialise global variables from constant data, hence
 * the massive ternary operator construction
 *
 * selects the appropriately-sized optimised version depending on sizeof(n)
 */
#define ilog2(n) \
( \
	__builtin_constant_p(n) ?	\
	((n) < 2 ? 0 :			\
	 63 - __builtin_clzll(n)) :	\
	(sizeof(n) <= 4) ?		\
	__ilog2_u32(n) :		\
	__ilog2_u64(n)			\
 )
```
`fls()` 則定義在 `include/asm-generic/bitops/fls.h`：

```c
/**
 * fls - find last (most-significant) bit set
 * @x: the word to search
 *
 * This is defined the same way as ffs.
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */

static __always_inline int fls(unsigned int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
```
用於找到最高位元的位置，可以看到與版本二的**形式**相似，差別在於版本二從低位開始且使用算術運算子，但本處皆為位元運算；可以看到此類形式是在考量攜帶性、相容性等因素後較好的實作方法。


### Quiz 4 - Exponentially Weighted Moving Average
我們嘗試將公式與程式碼比較，考慮
$$
S_t = \left\{
           \begin{array}{l}
              Y_0&   t = 0 \\
              \alpha Y_t + (1 - \alpha)\cdot S_{t-1}& t > 0 \\
           \end{array} 
       \right.
$$
與
```c
struct ewma *ewma_add(struct ewma *avg, unsigned long val)
{
    avg->internal = avg->internal
                        ? (((avg->internal << avg->weight) - avg->internal) +
                           (val << avg->factor)) >> avg->weight
                        : (val << avg->factor);
    return avg;
}
```

由於 `avg->factor` 僅用於定點數計算，略去後將兩者對照，假設

`val` : $Y_t$
`avg->internal` : $S_{t-1}$

依據上述，我們將變數帶入公式得到

$$
S_t = \cfrac{(S_{t-1}\cdot2^{weight}-S_{t-1}) + Y_t}{2^{weight}}=\cfrac{1}{2^{weight}}Y_t + (1 - \cfrac{1}{2^{weight}})S_{t-1}
$$

`avg->weight`   : $-log_{2} \space \alpha$ 或 $\alpha=\cfrac{1}{2^{weight}}$

#### Linux Kernal 案例
在 `drivers/net/wireless/mediatek/mt76` 以下使用了 ewma 相關函式；[MT76](https://www.mediatek.tw/products/home-networking/mt7628k-n-a)應是指聯發科技一系列搭載在路由器上的單晶片產品；在其中使用了許多 ewma 函式。

在 `mediatek/mt76/mt792x.h` 使用了 `DECLARE_EWMA(rssi, 10, 8);` 定義了 `ewma_rssi` 並有 `_init` `_read` `_add` 等操作；`DECLARE_EWMA` 巨集定義在 `include/linux/average.h`，宣告了 EWMA 物件以及操作。

在 `mediatek/mt76/mt7921/main.c` 下雖有 `_init` `_read` 相關操作，但主要的 `_add` 僅有出現在 `mediatek/mt76/mt792x_mac.c`，相關程式碼如下

```c
static void
mt792x_mac_rssi_iter(void *priv, u8 *mac, struct ieee80211_vif *vif)
{
	struct sk_buff *skb = priv;
	struct mt76_rx_status *status = (struct mt76_rx_status *)skb->cb;
	struct mt792x_vif *mvif = (struct mt792x_vif *)vif->drv_priv;
	struct ieee80211_hdr *hdr = mt76_skb_get_hdr(skb);

	if (status->signal > 0)
		return;

	if (!ether_addr_equal(vif->addr, hdr->addr1))
		return;

	ewma_rssi_add(&mvif->rssi, -status->signal);
}
```

`rssi` 應是指 Received Signal Strength Indicator、其中 `skb` 應是指 socket buffer、`mvif` 應是指 Virtual Interface 等；由上述上下文大致可推斷用於檢視目標裝置 (`vif`) 的訊號強度，如果訊號大於 0 (訊號強度通常定義小於 0，見 [Why is almost everything negative in Wireless? ](https://community.cisco.com/t5/small-business-support-knowledge-base/why-is-almost-everything-negative-in-wireless/ta-p/3159743)) 或 mac 不匹配則不符合，若符合則更新 EWMA 參數。

透過這種方法掌握具有時序資訊的訊號強度，比起算術平均帶有更多資訊。

### Quiz 5 - Bitwise Ceiling log
直接從傳回值觀察，可以發現 `r` `shift` `x > 1` 三項是負責不同值域的；由於 `x > 1` 只回傳 1 或 0，可以理解其為 $b_0$，而當 `x <= 0xF`，`shift` 也只回傳 10 or 00，故理解其負責 $b_1$；當 `x > 0xFFFF` `r` 的最大值為 `10000 | 1000 | 100 = 11100`；當三數使用與和 `|` 所得最大數為 `11111` 即 31，這是 32 位元數可能有的最大 `log2`。

由於 `x = 0` 時進行 `x--` 將導致下溢位，我們移除了 `x--` 後做相應的調整，將所有條件上調 1 維持相同的條件判斷，而在開頭時保留 x 是否大於零並於最後加上；修正後如下。
```c
int ceil_ilog2_fixzero(uint32_t x)
{
    uint32_t r, shift;
    uint32_t not_zero = (x > 0);

    r = (x > (0x1 << 16)) << 4;
    x >>= r;
    shift = (x > (0x1 << 8)) << 3;
    x >>= shift;
    r |= shift;
    shift = (x > (0x1 << 4)) << 2;
    x >>= shift;
    r |= shift;
    shift = (x > (0x1 << 2)) << 1;
    x >>= shift;

    uint32_t result = (r | shift | x > 2) + not_zero;
    return result;
}
```

為什麼要使用 branchless？branchless 可以讓編譯器嘗試最佳化，減少分支，達到減少 Control hazard 的可能。

我們不是比較了數值嗎，以上程式碼仍是 branchless？
Ans：不對。本質上 `(x > (0x1 << 16)) << 4` 是 bitwise 操作而不是 Control Flow Branch，故仍是 branchless

#### branchless code really is branchless？

我們以 `gcc -O2 -S -o branchless.s branchless.c` 取得其 asm 程式碼如下：

```c
ceil_ilog2:
.LFB23:
	.cfi_startproc
	endbr64
	subl	$1, %edi
	xorl	%edx, %edx
	cmpl	$65535, %edi
	jbe	.L2
	shrl	$16, %edi
	movl	$16, %edx
.L2:
	cmpl	$255, %edi
	jbe	.L3
	shrl	$8, %edi
	orl	$8, %edx
.L3:
	cmpl	$15, %edi
	jbe	.L4
	shrl	$4, %edi
	orl	$4, %edx
.L4:
	cmpl	$3, %edi
	jbe	.L5
	orl	$2, %edx
	shrl	$2, %edi
.L5:
	xorl	%eax, %eax
	cmpl	$1, %edi
	seta	%al
	orl	%edx, %eax
	addl	$1, %eax
	ret
	.cfi_endproc
.LFE23:
	.size	ceil_ilog2, .-ceil_ilog2
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"%s"
	.section	.text.startup,"ax",@progbits
	.p2align 4
	.globl	main
	.type	main, @function
```

可以見到仍然有 `jbe` 等分支存在，為什麼？
這可能與編譯器策略以及 CPU 架構有關，可能這些最佳化對其微不足道，或有對分支預測進行最佳化，便不需要刻意避免 Jump 指令。(**所以假設我們使用 STM32，依照其 toolchain 編譯以上程式碼就不會出現 Jump 指令？**)

:::info
:::spoiler Why the Jumps？（ChatGPT）
Why the Jumps?
Optimized but not entirely branchless: While the original intention might be to have an efficient computation, it seems the compiler generated code that still relies on branches. The code is likely optimized for the scenario and instruction set being targeted but didn't fully eliminate branching.

Compiler's Role: The way source code translates to assembly largely depends on the compiler's optimization strategies. Even if the high-level code avoids logical branches (like if statements), the compiler may introduce conditional jumps based on its interpretation and optimization process.

Misinterpretation of "branchless": Sometimes, "branchless" in a high-level code context aims to reduce or avoid costly branch mispredictions. However, low-level implementation (assembly code) might still have jumps that are handled efficiently by the CPU or are part of short-circuit operations.
:::

#### Linux Kernal 案例

## Week 4 Quiz
[Week4 Quiz](https://hackmd.io/@sysprog/linux2024-quiz4)

### Quiz 1 - Bitwise Hamming Distance

考慮到 "**兩個整數間的 Hamming distance 為其二進位的每個位元的差**"，換句話說則為 "**兩數間位元值不同的數量**"；透過 `nums[i] ^ nums[j]`，若在兩數在 n-th 位元不同值，n-th 被設為 1，反之則為 0，再透過 `__builtin_popcount` 計算和。
```c
int totalHammingDistance(int* nums, int numsSize)
{
    int total = 0;
    for (int i = 0;i < numsSize;i++)
        for (int j = 0; j < numsSize;j++)
            total += __builtin_popcount(nums[i] ^ nums[j]); 
    return total >> 1;
}
```

問題在為何最終要右移一位，即除以二？因為在過程中我們事實上歷遍　`int *nums` 兩次、重複計算了，事實上我們至少可以改為
```c
int totalHammingDistance2(int* nums, int numsSize)
{
    int total = 0;
    for (int i = 0;i < numsSize;i++)
        for (int j = i + 1; j < numsSize;j++)
            total += __builtin_popcount(nums[i] ^ nums[j]); 
    return total;
}
```
但這仍是 $O(n^2)$，是否有辦法僅歷遍一次？由於我們僅需要知道 n-th 位元中**不同的位元對有幾個**，而位元僅有 `1` or `0`，所以 `num_1s(numsSize - num_1s)` 的個數就是不同位元對的數量！

舉例來說，考慮

```
a = 1
b = 0
c = 1
d = 0
e = 0
```

有五項數字，其中有 2 個 `1`，所以其 Hamming distance 就是 2(5 - 2)；為什麼要相乘，因為我們要找的是**對**！就是版本一中，我們將數字倆倆比較的行為；寫作程式碼後如下
```c
int totalHammingDistance_improved(int* nums, int numsSize)
{
    int total = 0;
    for (int i = 0;i < 32;i++) {
        int count = 0;
        for (int j = 0;j < numsSize;j++)
            count += (nums[j] >> i) & 1;
        total += count * (numsSize - count);
    }
    return total;
}
```
使其有 $O(n)$ 的時間複雜度以及 $O(1)$ 的空間複雜度。


### Quiz 2 - Bitwise Modular Arithmetic
#### What is 0x24924925？
在 [Hacker's Delight](https://doc.lagout.org/security/Hackers%20Delight.pdf) 中，關於 7 的除法以及餘數運算多次出現了 `0x24924925` 這一魔法數字 (magic number)，即 $\lceil\cfrac{2^{32}}{7}\rceil=613566757$，如果我們撰寫以下 C 程式碼
```c
uint32_t mod7(uint32_t x)
{
  return x % 7;
}
```
其組合程式碼為
```c
mod7:
.LFB0:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movl	%edi, -4(%rbp)
	movl	-4(%rbp), %ecx
	movl	%ecx, %eax
	imulq	$613566757, %rax, %rax
	shrq	$32, %rax
	movl	%ecx, %edx
	subl	%eax, %edx
	shrl	%edx
	addl	%edx, %eax
	shrl	$2, %eax
	movl	%eax, %edx
	sall	$3, %edx
	subl	%eax, %edx
	movl	%ecx, %eax
	subl	%edx, %eax
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
```
可以見到，編譯器自行計算了 613566757 此一數字；Hacker's Delight 在第九章討論了整數除法、第十章詳盡講解了 3 5 7 等數如何避免使用除法指令計算除法以及餘數；故我們亦有
```c
int remu5(unsigned n)
{
    n = (0x33333333 * n + (n >> 3)) >> 29;
    return (0x04432210 >> (n << 2)) & 7;
}

int remu9(unsigned n)
{
    static char table[16] = {0, 1, 1, 2, 2, 3, 3, 4,
    5, 5, 6, 6, 7, 7, 8, 8};
    n = (0x1C71C71C*n + (n >> 1)) >> 28;
    return table[n];
}
```
那是如何計算的呢？我們考慮 $\cfrac{1}{7} = 0.1428571...$，若寫作定點數為 `0.001001001001...`，每三個位元重複 `1001`，當我們左移35位後取 100100100100100100100100100100.~~1000~~，即 `0x24924924`。但由於其為小數，最終需要右移 32 位。

~~注意到提供的程式碼為 29 位，是因為上述方法在 32 位元中顯然會溢位，故是左移 32 位後再右移 29 位。~~ 注意到其二進制是循環數，每 3 位元一次循環，故上溢位並不重要，右移後仍可得到正確的數值，係因 $8^k \equiv 1 (mod \ \ 7)$。


### Quiz 3 - Red Black Tree

程式 `treeint` 實現 XTree 的插入、搜尋以及刪除，以下探討其各項操作

#### 插入
首先透過 `__xt_find()` 尋找樹中是否有已存在相同鍵值的節點，若有則回傳 -1，若無則透過 `treeint_xt_node_create()` 創造節點，此處使用 `calloc` 做初始化值為零的記憶體分配 (`malloc` 不初始化)；並將初始化的節點依照 `__xt_find()` 傳回的 `xt_dit d` 放入左樹或右樹，並更新親代節點。

`xt_update` 即是平衡樹的重點；前面的平衡和旋轉在插入操作可以忽視，因其是新節點不具有子節點，而是對親代節點更新 hint，考慮以下新增：

![tree](https://hackmd.io/_uploads/Bk3ZCEFJA.png) 或 ![tree](https://hackmd.io/_uploads/SyCz04t1C.png)


顯然不需要平衡，因其 `xt_balance(a)` 等於 1 或 -1；但

![tree](https://hackmd.io/_uploads/BJ_xANKJA.png)


當新增 c 節點時，執行 `xt_update(root, b)` 後，其親代節點 b 會帶有 hint = 1，子節點 a 也有 hint = 1；執行 `xt_update(root, root)` 因節點 b 之親代節點為根結點，會發現 `xt_balance(root);` 將傳回 2，故需要旋轉平衡，`*root = xt_left(root);` 將節點 b 轉為根結點。

`xt_rotate_left(root)` 可以理解成

1. 將節點 b 的右子樹轉為 `root` 的左子樹，因 `xt_left(n) = xt_right(l);`
2. 將 `root` 轉為節點 b 的右節點，因 `xt_parent(n) = l;` 及 `xt_right(l) = n;`
3. 分別將親代節點左右子樹更正

`xt_rotate_right` 為鏡像操作。

#### 搜尋
搜尋時使用 `__xt_find2`，因其不需要知道親代節點以及該節點落於左子樹或右子樹，只需要 `treeint_xt_cmp` 比較值的大小，尋找的值比該節點更大則往左子樹繼續搜尋，更小往右子樹繼續搜尋。

#### 刪除

在 `__xt_remove()` 內，若刪除的節點有右子樹，則走訪到其最左側的葉節點，並進行 `xt_replace_right()`，考慮以下樹 **(因作圖效果，省略右子樹)**

假設欲刪除節點 b，最左側的葉節點為 f，`xt_replace_right(b, f)`，會使得兩者交換為

![tree](https://hackmd.io/_uploads/rJ5KW9ty0.png)   轉換為 ![tree](https://hackmd.io/_uploads/r1xMz5KJA.png)

並對節點 b 原始位置的右子樹做 `xt_update(root, xt_right(b))`，因其子樹變更，並一路向上更新 hint，由 `xt_update` 內的
```c
if (n->hint == 0 || n->hint != prev_hint)
        xt_update(root, p);
```
迭代向上更新。


考慮以下樹 **(因作圖效果，省略 11 之子樹)**，假設刪除節點 5，其右子樹的最左葉節點為 6，`xt_replace_right` 會將 6 換至 5 的位置，並移除 5，變為 

![tree](https://hackmd.io/_uploads/SyA_l9YkR.png) 轉換為 ![tree](https://hackmd.io/_uploads/SJ5Sb9tyC.png)


#### Linux Kernel 的紅黑樹

