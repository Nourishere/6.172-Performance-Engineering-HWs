# Recitation notes

## Checkoff Item 1: Make note of one bottleneck

1. Run `make isort DEBUG=1`
2. Run `perf record ./isort 10000 10` then run `perf report`

```
        │    while (index >= left && *index > val) {                                                                      ▒
        │3b:   mov     -0x28(%rbp),%rcx                                                                                   ▒
        │      xor     %eax,%eax                                                                                          ▒
        │      cmp     -0x8(%rbp),%rcx                                                                                    ▒
  14.18 │      mov     %al,-0x29(%rbp)                                                                                    ▒
        │    ↓ jb      59                                                                                                 ▒
        │      mov     -0x28(%rbp),%rax                                                                                   ◆
   0.18 │      mov     (%rax),%eax                                                                                        ▒
  24.25 │      cmp     -0x1c(%rbp),%eax                                                                                   ▒
   0.44 │      seta    %al                                                                                                ▒
   1.06 │      mov     %al,-0x29(%rbp)                                                                                    ▒
  22.48 │59:   mov     -0x29(%rbp),%al                                                                                    ▒
        │      test    $0x1,%al                                                                                           ▒
   8.23 │    ↓ jne     62                                                                                                 ▒
   0.09 │    ↓ jmp     7d                                                                                                 ▒

```

From here, you can see that the bottleneck is in the loop checking. Numbers on the left are percentages of code execution.

```
        │    while (index >= left && *index > val) {                                                                      ▒
        │3b:   mov     -0x28(%rbp),%rcx                                                                                   ▒
        │      xor     %eax,%eax                                                                                          ▒
        │      cmp     -0x8(%rbp),%rcx                                                                                    ▒
    165 │      mov     %al,-0x29(%rbp)                                                                                    ▒
        │    ↓ jb      59                                                                                                 ▒
        │      mov     -0x28(%rbp),%rax                                                                                   ▒
      2 │      mov     (%rax),%eax                                                                                        ▒
    286 │      cmp     -0x1c(%rbp),%eax                                                                                   ◆
      5 │      seta    %al                                                                                                ▒
     12 │      mov     %al,-0x29(%rbp)                                                                                    ▒
    266 │59:   mov     -0x29(%rbp),%al                                                                                    ▒
        │      test    $0x1,%al                                                                                           ▒
     97 │    ↓ jne     62                                                                                                 ▒
      1 │    ↓ jmp     7d
```

Here the numbers show samples (meaning count of execution). You can see that the loop runs for about **266** times. The branch `jne 62` is taken for `97` times. This means that it misses **69** times.

Next I ran `valgrind --tool=cachegrind --brach-sim=yes ./isort 10000 10` and after about 10 seconds, I got this output

```
==367186== Cachegrind, a high-precision tracing profiler
==367186== Copyright (C) 2002-2025, and GNU GPL'd, by Nicholas Nethercote et al.
==367186== Using Valgrind-3.27.1 and LibVEX; rerun with -h for copyright info
==367186== Command: ./isort 10000 10
==367186==
Sorting 10000 values...
Done!
==367186==
==367186== I refs:        5,270,251,878
==367186==
==367186== Branches:        501,830,201  (501,729,851 cond + 100,350 ind)
==367186== Mispredicts:         103,182  (    103,015 cond +     167 ind)
==367186== Mispred rate:            0.0% (        0.0%     +     0.2%   )
```

which means either 1. our program doesn't miss much or 2. Our branch predictor is really good (On Alderlake, I don't know about that).

## Checkoff Item 2: Run sum under cachegrind to identify cache performance. It may take a little while. In the output, look at the D1 and LLd misses. D1 represents the lowest-level cache (L1), and LL represents the last (highest) level data cache (on most machines, L3). Do these numbers correspond with what you would expect? Try playing around with the values N and U in sum.c. How can you bring down the number of cache misses?

First, I ran the following command to list cache specs on my machine

```
lscpu -C

NAME ONE-SIZE ALL-SIZE WAYS TYPE        LEVEL  SETS PHY-LINE COHERENCY-SIZE
L1d       48K     320K   12 Data            1    64        1             64
L1i       32K     384K    8 Instruction     1    64        1             64
L2       1.3M       7M   10 Unified         2  2048        1             64
L3        12M      12M    8 Unified         3 24576        1             64
```

and in bytes:

```
lscpu -B -C

NAME ONE-SIZE ALL-SIZE WAYS TYPE        LEVEL  SETS PHY-LINE COHERENCY-SIZE
L1d     49152   327680   12 Data            1    64        1             64
L1i     32768   393216    8 Instruction     1    64        1             64
L2    1310720  7340032   10 Unified         2  2048        1             64
L3   12582912 12582912    8 Unified         3 24576        1             64
```

Then accordingly I passed the following arguments to `cachegrind`:

```
valgrind --tool=cachegrind --cache-sim=yes \
    --I1=32768,8,64 \
    --D1=49152,12,64 \
    --LL=8388608,8,64 \
    ./sum
```
(Note that the L3 (LL) is slightly lower than I have just to cope with cachegrind, which requires a power2 size.)

Looking at the output

```
Allocated array of size 10000000
Summing 100000000 random values...
Done. Value = 938895920
==385988==
==385988== I refs:        3,540,133,602
==385988== I1  misses:            1,322
==385988== LLi misses:            1,308
==385988== I1  miss rate:          0.00%
==385988== LLi miss rate:          0.00%
==385988==
==385988== D refs:          610,043,593  (400,031,781 rd   + 210,011,812 wr)
==385988== D1  misses:      100,505,250  ( 99,879,835 rd   +     625,415 wr)
==385988== LLd misses:       79,731,018  ( 79,105,621 rd   +     625,397 wr)
==385988== D1  miss rate:          16.5% (       25.0%     +         0.3%  )
==385988== LLd miss rate:          13.1% (       19.8%     +         0.3%  )
==385988==
==385988== LL refs:         100,506,572  ( 99,881,157 rd   +     625,415 wr)
==385988== LL misses:        79,732,326  ( 79,106,929 rd   +     625,397 wr)
==385988== LL miss rate:            1.9% (        2.0%     +         0.3%  )
```

I can see right away is that the instruction cache produces no misses. This is because we don't have much control flow logic in `sum.c`. Next, D1 misses are **16.5%** while L3D are **13.1%**.

The first part is filling up the data array after allocating it:

```
  // fill up the array with sequential (sorted) values.
  int i;
  for (i = 0; i < U; i++) {
    data[i] = i;
  }
```

This would result in store instructions (writing into the cache). Now if the cache line size is **64 bytes** and each data element is `uint32_t`(**4 bytes**), then the loop will miss one time in 16 iterations. Now the number of iterations is 10,000,000, meaning we'll get a local miss rate of **(10,000,000/16)/10,000,000 = 0.6%**. But if we consider all the writes throughout the program **(10,000,000/16)/210,000,000 = 0.297%** (close enough to what cachegrind reported).

Now into the summing part:

```
  data_t val = 0;
  data_t seed = 42;
  for (i = 0; i < N; i++) {
    int l = rand_r(&seed) % U;
    val = (val + data[l]);
  }
```

Here the accesses are to random indices. In terms of memory reads, we have **four**, including three from the stack (**U, val, and seed**). Now the data access will almost always miss while other stack accesses will always hit, giving an almost **25%** miss rate. This is an off-the-shelf observation.

The last level here is **8 MiB**. Now intuitively, That would miss about every **5** times, which is **20%**. I guess

Now into getting these numbers low, I guess if we reduce the size of the data array to the size of the D1 cache, we'll get overall almost zero read/write misses (except for cold misses). So the array of size 48 KiB, maybe 40 KiB to keep space for stack variables, would mean about **10,000 elements**.

I did the following changes:
```
typedef uint32_t data_t;
-- const int U = 10,000;   // size of the array. 10 million vals ~= 40MB
++ const int U = 10000000;   // size of the array. 10 million vals ~= 40MB
```

Now...

```
Allocated array of size 10000
Summing 100000000 random values...
Done. Value = 1744280048
==390199==
==390199== I refs:        3,500,173,293
==390199== I1  misses:            1,316
==390199== LLi misses:            1,288
==390199== I1  miss rate:          0.00%
==390199== LLi miss rate:          0.00%
==390199==
==390199== D refs:          600,053,499  (400,031,710 rd   + 200,021,789 wr)
==390199== D1  misses:            2,255  (      1,245 rd   +       1,010 wr)
==390199== LLd misses:            2,020  (      1,044 rd   +         976 wr)
==390199== D1  miss rate:           0.0% (        0.0%     +         0.0%  )
==390199== LLd miss rate:           0.0% (        0.0%     +         0.0%  )
==390199==
==390199== LL refs:               3,571  (      2,561 rd   +       1,010 wr)
==390199== LL misses:             3,308  (      2,332 rd   +         976 wr)
==390199== LL miss rate:            0.0% (        0.0%     +         0.0%  )
```

## Write-up 1: Compare the Cachegrind output on the DEBUG=1 code versus DEBUG=0 compiler optimized code. Explain the advantages and disadvantages of using instruction count as a substitute for time when you compare the performance of different versions of this program.

First we run `make`, which compiles an `-O3 -DNDEBUG` version, and we launch `cachegrind`

```
Running test #0...
Generating random array of 1000000 elements
Arrays are sorted: yes
 --> test_correctness at line 217: PASS
sort_a          : Elapsed execution time: 1.340220 sec
sort_a repeated : Elapsed execution time: 1.330546 sec
Generating inverted array of 1000000 elements
Arrays are sorted: yes
 --> test_correctness at line 217: PASS
sort_a          : Elapsed execution time: 2.665162 sec
sort_a repeated : Elapsed execution time: 2.652518 sec

Done testing.
==390830==
==390830== I refs:        2,475,555,399
==390830== I1  misses:            1,664
==390830== LLi misses:            1,647
==390830== I1  miss rate:          0.00%
==390830== LLi miss rate:          0.00%
==390830==
==390830== D refs:          790,661,904  (479,275,887 rd   + 311,386,017 wr)
==390830== D1  misses:        9,187,900  (  4,751,996 rd   +   4,435,904 wr)
==390830== LLd misses:          676,067  (    269,687 rd   +     406,380 wr)
==390830== D1  miss rate:           1.2% (        1.0%     +         1.4%  )
==390830== LLd miss rate:           0.1% (        0.1%     +         0.1%  )
==390830==
==390830== LL refs:           9,189,564  (  4,753,660 rd   +   4,435,904 wr)
==390830== LL misses:           677,714  (    271,334 rd   +     406,380 wr)
==390830== LL miss rate:            0.0% (        0.0%     +         0.1%  )
```

Now we run `make DEBUG=1`

```
Running test #0...
Generating random array of 1000000 elements
Arrays are sorted: yes
 --> test_correctness at line 217: PASS
sort_a          : Elapsed execution time: 3.223247 sec
sort_a repeated : Elapsed execution time: 3.196422 sec
Generating inverted array of 1000000 elements
Arrays are sorted: yes
 --> test_correctness at line 217: PASS
sort_a          : Elapsed execution time: 6.350743 sec
sort_a repeated : Elapsed execution time: 6.328700 sec

Done testing.
==391204==
==391204== I refs:        4,677,814,665
==391204== I1  misses:            1,639
==391204== LLi misses:            1,622
==391204== I1  miss rate:          0.00%
==391204== LLi miss rate:          0.00%
==391204==
==391204== D refs:        3,149,831,488  (2,428,333,996 rd   + 721,497,492 wr)
==391204== D1  misses:        9,185,599  (    4,754,162 rd   +   4,431,437 wr)
==391204== LLd misses:          675,895  (      269,559 rd   +     406,336 wr)
==391204== D1  miss rate:           0.3% (          0.2%     +         0.6%  )
==391204== LLd miss rate:           0.0% (          0.0%     +         0.1%  )
==391204==
==391204== LL refs:           9,187,238  (    4,755,801 rd   +   4,431,437 wr)
==391204== LL misses:           677,517  (      271,181 rd   +     406,336 wr)
==391204== LL miss rate:            0.0% (          0.0%     +         0.1%  )
```

The no-debug version is ~**2.4x** faster in **sort_a** and **sort_a inverted**.

Using instruction counts might be better in case of testing across different machines **with the same architecture**. It might not fit in cross-architecture comparisons.

## Write-up 2: Explain which functions you chose to inline and report the performance differences you observed between the inlined and uninlined sorting routines.

Now into `sort_i.c`.

First, I only added `inline` to the `memfree` function in `util.c`.

```
Running test #0...
Generating random array of 1000000 elements
Arrays are sorted: yes
 --> test_correctness at line 217: PASS
sort_a          : Elapsed execution time: 1.361266 sec
sort_a repeated : Elapsed execution time: 1.352034 sec
sort_i          : Elapsed execution time: 1.330678 sec
```

which is only ~**1.02x** faster.

Adding inline to `copy_i`

```
Running test #0...
Generating random array of 1000000 elements
Arrays are sorted: yes
 --> test_correctness at line 217: PASS
sort_a          : Elapsed execution time: 1.361516 sec
sort_a repeated : Elapsed execution time: 1.350867 sec
sort_i          : Elapsed execution time: 1.333425 sec
Generating inverted array of 1000000 elements
```

which doesn't change much. Let's try adding inline to the recursive `sort_i` function.

```
Running test #0...
Generating random array of 1000000 elements
Arrays are sorted: yes
 --> test_correctness at line 217: PASS
sort_a          : Elapsed execution time: 1.356269 sec
sort_a repeated : Elapsed execution time: 1.346136 sec
sort_i          : Elapsed execution time: 1.329344 sec
Generating inverted array of 1000000 elements
```

which also doesn't change much.

Thing is I don't see inlining any of these routines as making the big differences. As for recursion, it can be worse in the sense that it produces more instructions leading to potentially more cache misses.

## Write-up 3: Explain the possible performance downsides of inlining recursive functions. How could profiling data gathered using cachegrind help you measure these negative performance effects?

Inlining recursive functions can result in code duplication as the compiler inlines each and every function call. This can also in turn destroy cache locality.

## Write-up 4: Give a reason why using pointers may improve performance. Report on any performance differences you observed in your implementation.

```
sort_i          : Elapsed execution time: 0.294271 sec
sort_p          : Elapsed execution time: 0.290066 sec
```

There is some improvement. Probably modern compilers emit the same instructions for array and pointer versions. The difference in performance here is not that big.

## Write-up 4 (Using a different algorithm): Explain what sorting algorithm you used and how you chose the number of elements to be sorted in the base case. Report on the performance differences you observed.

So I used the already-implemented insertion sort and coarsened the recursion a little bit and I got a 2.25x performance gain

```
Running test #0...
Generating random array of 131072 elements
Arrays are sorted: yes
 --> test_correctness at line 217: PASS
sort_a          : Elapsed execution time: 0.149440 sec
sort_a repeated : Elapsed execution time: 0.144748 sec
sort_i          : Elapsed execution time: 0.146304 sec
sort_p          : Elapsed execution time: 0.143626 sec
sort_c          : Elapsed execution time: 0.063780 sec
```

The idea is that most of the recursive calls end up doing nothing (hitting the `assert`). Insertion sort helps minimize this and doesn't require allocating extra memory. Also, it doesn't hurt on a small range of numbers. A larger number of elements will probably put this implementation at a disadvantage.

## Write-up 5 (Reducing memory usage): Explain any difference in performance in your `sort_m.c`. Can a compiler automatically make this optimization for you and save you all the effort? Why and why not?

Building `sort_m` test resulted in the following memory report

```
==29829==
==29829== HEAP SUMMARY:
==29829==     in use at exit: 0 bytes in 0 blocks
==29829==   total heap usage: 33 allocs, 33 frees, 25,176 bytes allocated
==29829==
==29829== All heap blocks were freed -- no leaks are possible
==29829==
==29829== For lists of detected and suppressed errors, rerun with: -s
==29829== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```
Now the final `sort_f` resulted in the following
```
==30776==
==30776== HEAP SUMMARY:
==30776==     in use at exit: 0 bytes in 0 blocks
==30776==   total heap usage: 7 allocs, 7 frees, 13,032 bytes allocated
==30776==
==30776== All heap blocks were freed -- no leaks are possible
==30776==
==30776== For lists of detected and suppressed errors, rerun with: -s
==30776== ERROR SUMMARY: 1002 errors from 6 contexts (suppressed: 0 from 0)
```

`make memcheck` uses an array with 1000 elements

I think the compiler won't help us much. This is a high level design detail. The compiler only helps in lower-level optimization.
