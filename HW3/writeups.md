# Recitation notes

## Checkoff Item 1:  Look at the assembly code above (example1.S). The compiler has translated the code to set the start index at −216 and adds to it for each memory access. Why doesn’t it set the start index to 0 and use small positive offsets?

Take a look at the following code (I had to remove the `-g` to get it a little clearer)

```
test:                                   # @test
	endbr64
	leaq	65536(%rdi), %rax
	leaq	65536(%rsi), %rcx
	cmpq	%rcx, %rdi
	setb	%cl
	cmpq	%rax, %rsi
	setb	%al
	testb	%al, %cl
	je	.LBB0_1
	xorl	%eax, %eax
	.p2align	4
.LBB0_1:
	xorl	%eax, %eax
	.p2align	4
.LBB0_2:                                # =>This Inner Loop Header: Depth=1
	movdqu	(%rsi,%rax), %xmm0
	movdqu	16(%rsi,%rax), %xmm1
	movdqu	(%rdi,%rax), %xmm2
	paddb	%xmm0, %xmm2
	movdqu	16(%rdi,%rax), %xmm0
	paddb	%xmm1, %xmm0
	movdqu	32(%rdi,%rax), %xmm1
	movdqu	48(%rdi,%rax), %xmm3
	movdqu	%xmm2, (%rdi,%rax)
	movdqu	%xmm0, 16(%rdi,%rax)
	movdqu	32(%rsi,%rax), %xmm0
	paddb	%xmm1, %xmm0
	movdqu	48(%rsi,%rax), %xmm1
	paddb	%xmm3, %xmm1
	movdqu	%xmm0, 32(%rdi,%rax)
	movdqu	%xmm1, 48(%rdi,%rax)
	addq	$64, %rax
	cmpq	$65536, %rax                    # imm = 0x10000
	jne	.LBB0_2
.LBB0_5:
	retq
```

Now you can see here that the comparison instruction (`cmpq $65536, %rax`) checks whether `%rax` is larger than 2power16, which if is the case, it exits the loop. Also, prior to that, the instruction (`addq $64, %rax`) increments `%rax`. Strangely, my resulted version goes from 2power16 to zero requiring the `cmpq` instruction every time. In the cases where the compiler goes from -2power16 to 0, you don't need that compare. It can depend on the zero flag being set when the index goes to zero. Thus saving 2power16 `cmpq` instructions. However, this might not be good because `cmp` and `jne` get fused into a micro-op on x86, so performance difference might not change.

After
1. Adding the `restrict` keyword to assure the compiler that the two memory locations pointed to by our two pointers are not overlap
2. Adding the `AVX2` flag.

We get the following cleaner assembly

```
test:                                   # @test
	endbr64
	xorl	%eax, %eax
	.p2align	4
.LBB0_1:                                # =>This Inner Loop Header: Depth=1
	vmovdqu	(%rdi,%rax), %ymm0
	vmovdqu	32(%rdi,%rax), %ymm1
	vmovdqu	64(%rdi,%rax), %ymm2
	vmovdqu	96(%rdi,%rax), %ymm3
	vpaddb	(%rsi,%rax), %ymm0, %ymm0
	vpaddb	32(%rsi,%rax), %ymm1, %ymm1
	vpaddb	64(%rsi,%rax), %ymm2, %ymm2
	vpaddb	96(%rsi,%rax), %ymm3, %ymm3
	vmovdqu	%ymm0, (%rdi,%rax)
	vmovdqu	%ymm1, 32(%rdi,%rax)
	vmovdqu	%ymm2, 64(%rdi,%rax)
	vmovdqu	%ymm3, 96(%rdi,%rax)
	subq	$-128, %rax
	cmpq	$65536, %rax                    # imm = 0x10000
	jne	.LBB0_1
# %bb.2:
	vzeroupper
	retq
.Lfunc_end0:
	.size	test, .Lfunc_end0-test
	.cfi_endproc
                                        # -- End function
	.ident	"clang version 22.1.8"
	.section	".note.GNU-stack","",@progbits
```

## Write-up 2: This code is still not aligned when using AVX2 registers. Fix the code to make sure it uses aligned moves for the best performance.

You can see above that the instructions don't assume alignment (`vmovdqu`). To fix this, we need to ensure the compiler that the data is aligned by using a compiler built_in. Now since the AVX2 lanes are 32-byte wide, we align to that

```
a = __builtin_assume_aligned(a, 32);
b = __builtin_assume_aligned(b, 32);
```

### Now into example 2.

```
void test(uint8_t * restrict a, uint8_t * restrict b) {
	uint64_t i;

	uint8_t * x = __builtin_assume_aligned(a, 16);
	uint8_t * y = __builtin_assume_aligned(b, 16);

	for (i = 0; i < SIZE; i++) {
	/* max() */
		if (y[i] > x[i]) x[i] = y[i];
	}
}
```

Compiling this results in the following assembly

```
test:                                   # @test
	.cfi_startproc
# %bb.0:
	endbr64
	xorl	%eax, %eax
	jmp	.LBB0_1
	.p2align	4
.LBB0_33:                               #   in Loop: Header=BB0_1 Depth=1
	addq	$16, %rax
	cmpq	$65536, %rax                    # imm = 0x10000
	je	.LBB0_34
.LBB0_1:                                # =>This Inner Loop Header: Depth=1
	movdqa	(%rsi,%rax), %xmm0
	movdqa	(%rdi,%rax), %xmm1
	pminub	%xmm0, %xmm1
	pcmpeqb	%xmm0, %xmm1
	movd	%xmm1, %edx
	movl	%edx, %r8d
	notb	%r8b
	movd	%xmm0, %ecx
	testb	$1, %r8b
	jne	.LBB0_2
# %bb.3:                                #   in Loop: Header=BB0_1 Depth=1
	testl	$256, %edx                      # imm = 0x100
	sete	%r8b
	testb	%r8b, %r8b
	jne	.LBB0_4
.LBB0_5:                                #   in Loop: Header=BB0_1 Depth=1
	testl	$65536, %edx                    # imm = 0x10000
	sete	%r8b
	testb	%r8b, %r8b
	jne	.LBB0_6
.LBB0_7:                                #   in Loop: Header=BB0_1 Depth=1
	testl	$16777216, %edx                 # imm = 0x1000000
	sete	%dl
	testb	%dl, %dl
	je	.LBB0_9
.LBB0_8:                                #   in Loop: Header=BB0_1 Depth=1
	shrl	$24, %ecx
	movb	%cl, 3(%rdi,%rax)
.LBB0_9:                                #   in Loop: Header=BB0_1 Depth=1
	pextrw	$2, %xmm1, %edx
	movl	%edx, %r8d
	notb	%r8b
	pextrw	$2, %xmm0, %ecx
	testb	$1, %r8b
	je	.LBB0_11
# %bb.10:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%cl, 4(%rdi,%rax)
.LBB0_11:                               #   in Loop: Header=BB0_1 Depth=1
	testl	$256, %edx                      # imm = 0x100
	sete	%dl
	testb	%dl, %dl
	je	.LBB0_13
# %bb.12:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%ch, 5(%rdi,%rax)
.LBB0_13:                               #   in Loop: Header=BB0_1 Depth=1
	pextrw	$3, %xmm1, %edx
	movl	%edx, %r8d
	notb	%r8b
	pextrw	$3, %xmm0, %ecx
	testb	$1, %r8b
	je	.LBB0_15
# %bb.14:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%cl, 6(%rdi,%rax)
.LBB0_15:                               #   in Loop: Header=BB0_1 Depth=1
	testl	$256, %edx                      # imm = 0x100
	sete	%dl
	testb	%dl, %dl
	je	.LBB0_17
# %bb.16:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%ch, 7(%rdi,%rax)
.LBB0_17:                               #   in Loop: Header=BB0_1 Depth=1
	pextrw	$4, %xmm1, %edx
	movl	%edx, %r8d
	notb	%r8b
	pextrw	$4, %xmm0, %ecx
	testb	$1, %r8b
	je	.LBB0_19
# %bb.18:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%cl, 8(%rdi,%rax)
.LBB0_19:                               #   in Loop: Header=BB0_1 Depth=1
	testl	$256, %edx                      # imm = 0x100
	sete	%dl
	testb	%dl, %dl
	je	.LBB0_21
# %bb.20:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%ch, 9(%rdi,%rax)
.LBB0_21:                               #   in Loop: Header=BB0_1 Depth=1
	pextrw	$5, %xmm1, %edx
	movl	%edx, %r8d
	notb	%r8b
	pextrw	$5, %xmm0, %ecx
	testb	$1, %r8b
	je	.LBB0_23
# %bb.22:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%cl, 10(%rdi,%rax)
.LBB0_23:                               #   in Loop: Header=BB0_1 Depth=1
	testl	$256, %edx                      # imm = 0x100
	sete	%dl
	testb	%dl, %dl
	je	.LBB0_25
# %bb.24:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%ch, 11(%rdi,%rax)
.LBB0_25:                               #   in Loop: Header=BB0_1 Depth=1
	pextrw	$6, %xmm1, %edx
	movl	%edx, %r8d
	notb	%r8b
	pextrw	$6, %xmm0, %ecx
	testb	$1, %r8b
	je	.LBB0_27
# %bb.26:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%cl, 12(%rdi,%rax)
.LBB0_27:                               #   in Loop: Header=BB0_1 Depth=1
	testl	$256, %edx                      # imm = 0x100
	sete	%dl
	testb	%dl, %dl
	je	.LBB0_29
# %bb.28:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%ch, 13(%rdi,%rax)
.LBB0_29:                               #   in Loop: Header=BB0_1 Depth=1
	pextrw	$7, %xmm1, %edx
	movl	%edx, %r8d
	notb	%r8b
	pextrw	$7, %xmm0, %ecx
	testb	$1, %r8b
	jne	.LBB0_30
# %bb.31:                               #   in Loop: Header=BB0_1 Depth=1
	testl	$256, %edx                      # imm = 0x100
	sete	%dl
	testb	%dl, %dl
	je	.LBB0_33
	jmp	.LBB0_32
	.p2align	4
.LBB0_2:                                #   in Loop: Header=BB0_1 Depth=1
	movb	%cl, (%rdi,%rax)
	testl	$256, %edx                      # imm = 0x100
	sete	%r8b
	testb	%r8b, %r8b
	je	.LBB0_5
.LBB0_4:                                #   in Loop: Header=BB0_1 Depth=1
	movb	%ch, 1(%rdi,%rax)
	testl	$65536, %edx                    # imm = 0x10000
	sete	%r8b
	testb	%r8b, %r8b
	je	.LBB0_7
.LBB0_6:                                #   in Loop: Header=BB0_1 Depth=1
	movl	%ecx, %r8d
	shrl	$16, %r8d
	movb	%r8b, 2(%rdi,%rax)
	testl	$16777216, %edx                 # imm = 0x1000000
	sete	%dl
	testb	%dl, %dl
	jne	.LBB0_8
	jmp	.LBB0_9
	.p2align	4
.LBB0_30:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%cl, 14(%rdi,%rax)
	testl	$256, %edx                      # imm = 0x100
	sete	%dl
	testb	%dl, %dl
	je	.LBB0_33
.LBB0_32:                               #   in Loop: Header=BB0_1 Depth=1
	movb	%ch, 15(%rdi,%rax)
	jmp	.LBB0_33
.LBB0_34:
	retq
.Lfunc_end0:
	.size	test, .Lfunc_end0-test
	.cfi_endproc
                                        # -- End function
	.ident	"clang version 22.1.8"
	.section	".note.GNU-stack","",@progbits
```

Ugly, let's do the change

```
void test(uint8_t * restrict a, uint8_t * restrict b) {
	uint64_t i;

	a = __builtin_assume_aligned(a, 16);
	b = __builtin_assume_aligned(b, 16);

	for (i = 0; i < SIZE; i++) {
		/* max() */
		if (b[i] > a[i]) a[i] = b[i];
	}
}
```

Now we get

```
test:                                   # @test
	.cfi_startproc
# %bb.0:
	endbr64
	xorl	%eax, %eax
	.p2align	4
.LBB0_1:                                # =>This Inner Loop Header: Depth=1
	movdqa	(%rsi,%rax), %xmm0
	movdqa	16(%rsi,%rax), %xmm1
	pmaxub	(%rdi,%rax), %xmm0
	pmaxub	16(%rdi,%rax), %xmm1
	movdqa	%xmm0, (%rdi,%rax)
	movdqa	%xmm1, 16(%rdi,%rax)
	movdqa	32(%rsi,%rax), %xmm0
	movdqa	48(%rsi,%rax), %xmm1
	pmaxub	32(%rdi,%rax), %xmm0
	pmaxub	48(%rdi,%rax), %xmm1
	movdqa	%xmm0, 32(%rdi,%rax)
	movdqa	%xmm1, 48(%rdi,%rax)
	addq	$64, %rax
	cmpq	$65536, %rax                    # imm = 0x10000
	jne	.LBB0_1
# %bb.2:
	retq
.Lfunc_end0:
	.size	test, .Lfunc_end0-test
	.cfi_endproc
                                        # -- End function
	.ident	"clang version 22.1.8"
	.section	".note.GNU-stack","",@progbits
```

Much better.

## Write-up 3: Provide a theory for why the compiler is generating dramatically different assembly.

The first version only writes when the condition is true. The second version always writes, but what is written is conditional. Also the comparison `b[i] > a[i]` is simple a max-of-two operation, which maps to the `pmaxub` instruction.

### example 3

```
void test(uint8_t * restrict a, uint8_t * restrict b) {
	uint64_t i;

	for (i = 0; i < SIZE; i++) {
		a[i] = b[i + 1];
	}
}

```

The resulted assembly

```
test:                                   # @test
	.cfi_startproc
# %bb.0:
	endbr64
	incq	%rsi
	movl	$65536, %edx                    # imm = 0x10000
	jmp	memcpy@PLT                      # TAILCALL
.Lfunc_end0:
	.size	test, .Lfunc_end0-test
	.cfi_endproc
                                        # -- End function
	.ident	"clang version 22.1.8"
	.section	".note.GNU-stack","",@progbits
```

## Write-up 4: Inspect the assembly and determine why the assembly does not include instructions with vector registers. Do you think it would be faster if it did vectorize? Explain.

You can see here that the compiler optimizes our line to a `memcpy` stdlib call. Can I try to add the `-ffreestanding` flag here? It works! Now we look at the output code.

```
test:                                   # @test
# %bb.0:
	endbr64
	xorl	%eax, %eax
	.p2align	4
.LBB0_1:                                # =>This Inner Loop Header: Depth=1
	movups	1(%rsi,%rax), %xmm0
	movups	17(%rsi,%rax), %xmm1
	movups	%xmm0, (%rdi,%rax)
	movups	%xmm1, 16(%rdi,%rax)
	movups	33(%rsi,%rax), %xmm0
	movups	49(%rsi,%rax), %xmm1
	movups	%xmm0, 32(%rdi,%rax)
	movups	%xmm1, 48(%rdi,%rax)
	movups	65(%rsi,%rax), %xmm0
	movups	81(%rsi,%rax), %xmm1
	movups	%xmm0, 64(%rdi,%rax)
	movups	%xmm1, 80(%rdi,%rax)
	movups	97(%rsi,%rax), %xmm0
	movups	113(%rsi,%rax), %xmm1
	movups	%xmm0, 96(%rdi,%rax)
	movups	%xmm1, 112(%rdi,%rax)
	subq	$-128, %rax
	cmpq	$65536, %rax                    # imm = 0x10000
	jne	.LBB0_1
# %bb.2:
	retq
.Lfunc_end0:
	.size	test, .Lfunc_end0-test
                                        # -- End function
	.ident	"clang version 22.1.8"
	.section	".note.GNU-stack","",@progbits
```

Is this better? Now instead of that one call to `memcpy` we get all these vector instructions. `memcpy` can be faster here. I guess profiling this might show the difference.

### example 4

```
double test(double * restrict a) {
	size_t i;

	double *x = __builtin_assume_aligned(a, 16);

	double y = 0;

	for (i = 0; i < SIZE; i++) {
		y += x[i];
	}
	return y;
}
```
Compiling this code doesn't vectorize:

```
test:                                   # @test
# %bb.0:
	endbr64
	xorpd	%xmm0, %xmm0
	xorl	%eax, %eax
	.p2align	4
.LBB0_1:                                # =>This Inner Loop Header: Depth=1
	addsd	(%rdi,%rax,8), %xmm0
	addsd	8(%rdi,%rax,8), %xmm0
	addsd	16(%rdi,%rax,8), %xmm0
	addsd	24(%rdi,%rax,8), %xmm0
	addsd	32(%rdi,%rax,8), %xmm0
	addsd	40(%rdi,%rax,8), %xmm0
	addsd	48(%rdi,%rax,8), %xmm0
	addsd	56(%rdi,%rax,8), %xmm0
	addq	$8, %rax
	cmpq	$65536, %rax                    # imm = 0x10000
	jne	.LBB0_1
# %bb.2:
	retq

```

However, after adding `-ffast-math`, the following is the output

```
test:                                   # @test
# %bb.0:
	endbr64
	xorpd	%xmm1, %xmm1
	xorl	%eax, %eax
	xorpd	%xmm0, %xmm0
	.p2align	4
.LBB0_1:                                # =>This Inner Loop Header: Depth=1
	addpd	(%rdi,%rax,8), %xmm0
	addpd	16(%rdi,%rax,8), %xmm1
	addpd	32(%rdi,%rax,8), %xmm0
	addpd	48(%rdi,%rax,8), %xmm1
	addpd	64(%rdi,%rax,8), %xmm0
	addpd	80(%rdi,%rax,8), %xmm1
	addpd	96(%rdi,%rax,8), %xmm0
	addpd	112(%rdi,%rax,8), %xmm1
	addq	$16, %rax
	cmpq	$65536, %rax                    # imm = 0x10000
	jne	.LBB0_1
# %bb.2:
	addpd	%xmm0, %xmm1
	movapd	%xmm1, %xmm0
	unpckhpd	%xmm1, %xmm0                    # xmm0 = xmm0[1],xmm1[1]
	addsd	%xmm1, %xmm0
	retq
```

## Write-up 5: Check the assembly and verify that it does in fact vectorize properly. Also what do you notice when you run the command $ clang -O3 example4.c -o example4; ./example4 with and without the -ffast-math ﬂag? Speciﬁcally, why do you a see a difference in the output.

From the clang manual

> -ffast-math
Enable fast-math mode. This option lets the compiler make aggressive, potentially-lossy assumptions about floating-point math. These include:
    Floating-point math obeys regular algebraic rules for real numbers (e.g. + and * are associative, x/y == x * (1/y), and (a + b) * c == a * c + b * c),
    No NaN or infinite values will be operands or results of floating-point operations,
    0 and -0 may be treated as interchangeable.
-ffast-math also defines the __FAST_MATH__ preprocessor macro. Some math libraries recognize this macro and change their behavior. With the exception of -ffp-contract=fast, using any of the options below to disable any of the individual optimizations in -ffast-math will cause __FAST_MATH__ to no longer be set. -ffast-math enables -fcx-limited-range.

What makes the compiler think that this optimization might be lossy or aggressive?

Running the commands

```
git:(main)(0)-> clang -O3 example4.c -o example4; ./example4
The decimal floating point sum result is: 11.667578
The raw floating point sum result is: 0x1.755cccec10aa5p+3
git:(main)(0)-> clang -O3 -ffast-math example4.c -o example4; ./example4
The decimal floating point sum result is: 11.667578
The raw floating point sum result is: 0x1.755cccec10aa3p+3
```

If you observe closely, you can see a difference in the raw floating point sum result. This is why `clang` doesn't do this on its own. As per the documentation above, fp operations are not associative.

### homework/loop

## Write-up 6: What speedup does the vectorized code achieve over the unvectorized code? What additional speedup does using -mavx2 give? You may wish to run this experiment several times and take median elapsed times; you can report answers to the nearest 100% (e.g., 2×, 3×, etc). What can you infer about the bit width of the default vector registers on the awsrun machines? What about the bit width of the AVX2 vector registers? Hint: aside from speedup and the vectorization report, the most relevant information is that the data type for each array is uint32_t. Let's measure the performance difference of loop after running without vectorization, with vectorization, and with AVX2 vectorization.


## Write-up 7: Compare the contents of loop.s when the VECTORIZE ﬂag is set/not set. Which instruction (copy its text here) is responsible for the vector add operation? Which instruction (copy its text here) is responsible for the vector add operation when you additionally pass AVX2=1? You can ﬁnd an x86 instruction manual on LMOD. Look for MMX and SSE2 instructions, which are vector operations. To make the assembly code more readable it may be a good idea to remove debug symbols from release builds by moving the -g and -gdwarf-3 CFLAGS in your Makeﬁle. It might also be a good idea to turn off loop unrolling with the -fno-unroll-loops ﬂag while you study the assembly code.

For the SSE2 vector extension, the instruction that does the addition is the one annotated:

```
	movl	$12, %eax
	.p2align	4
.LBB0_2:                                #   Parent Loop BB0_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movdqa	6144(%rsp,%rax,4), %xmm0
	movdqa	6160(%rsp,%rax,4), %xmm1
	movdqa	6176(%rsp,%rax,4), %xmm2
	movdqa	6192(%rsp,%rax,4), %xmm3
	paddd	12288(%rsp,%rax,4), %xmm0 <- add a + b
	paddd	12304(%rsp,%rax,4), %xmm1 <- add a + b
	movdqa	%xmm0, (%rsp,%rax,4) <- store in c
	movdqa	%xmm1, 16(%rsp,%rax,4) <- store in c
	paddd	12320(%rsp,%rax,4), %xmm2 <- add a + b
	paddd	12336(%rsp,%rax,4), %xmm3 <- add a + b
	movdqa	%xmm2, 32(%rsp,%rax,4) <- store in c
	movdqa	%xmm3, 48(%rsp,%rax,4) <- store in c
	addq	$16, %rax
	cmpq	$1548, %rax                     # imm = 0x60C
	jne	.LBB0_2
# %bb.3:                                #   in Loop: Header=BB0_1 Depth=1
	incl	%ebx
	cmpl	$100000, %ebx                   # imm = 0x186A0
	jne	.LBB0_1
```

Notice that here the compiler is using all of the four `%xmm` registers per iteration. Each register can hold 4 * uint32_t (128 byte). Then addition happens in four steps. So per iteration, we add 4 * 128 = 512 bytes or 16 elements.
The instruction that does the addition is `paddd 12288(%rsp,%rax,4), %xmm0` and the three others.

Now when we compile with AVX2, we get this (the instructions for adding are preceded by (->):

```
.LBB0_1:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_2 Depth 2
	movl	$24, %eax
	.p2align	4
.LBB0_2:                                #   Parent Loop BB0_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	vmovdqu	6080(%rsp,%rax,4), %ymm0
	vmovdqu	6112(%rsp,%rax,4), %ymm1
	vmovdqu	6144(%rsp,%rax,4), %ymm2
	vmovdqu	6176(%rsp,%rax,4), %ymm3
->	vpaddd	12224(%rsp,%rax,4), %ymm0, %ymm0
->	vpaddd	12256(%rsp,%rax,4), %ymm1, %ymm1
->	vpaddd	12288(%rsp,%rax,4), %ymm2, %ymm2
->	vpaddd	12320(%rsp,%rax,4), %ymm3, %ymm3
	vmovdqu	%ymm0, -64(%rsp,%rax,4)
	vmovdqu	%ymm1, -32(%rsp,%rax,4)
	vmovdqu	%ymm2, (%rsp,%rax,4)
	vmovdqu	%ymm3, 32(%rsp,%rax,4)
	addq	$32, %rax
	cmpq	$1560, %rax                     # imm = 0x618
	jne	.LBB0_2
# %bb.3:                                #   in Loop: Header=BB0_1 Depth=1
	incl	%ebx
	cmpl	$100000, %ebx                   # imm = 0x186A0
	jne	.LBB0_1
```

## Write-up 8: Use the __OP__ macro to experiment with different operators in the data parallel loop. For some operations, you will get division by zero errors because we initialize array B to be full of zeros—fix this problem in any way you like. Do any versions of the loop not vectorize with VECTORIZE=1 AVX2=1? Study the assembly code for << with just VECTORIZE=1 and explain how it differs from the AVX2 version.

First, subtraction. The only difference is the `psubd` instead of `paddd` and `vpsubd` instead of `vpaddd`.
Second, multiplication. It compiles and produces `vpmulld` instructions normally.
Division, now I have to initialize B differently if I want to compile correctly. After doing that, I get a non-vectorized code:

```
	movl	$1, %esi
	.p2align	4
.LBB0_2:                                #   Parent Loop BB0_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movl	12316(%rsp,%rsi,4), %eax
	movl	12320(%rsp,%rsi,4), %ecx
	xorl	%edx, %edx
	divl	6172(%rsp,%rsi,4)
	movl	%eax, 28(%rsp,%rsi,4)
	movl	%ecx, %eax
	xorl	%edx, %edx
	divl	6176(%rsp,%rsi,4)
	movl	%eax, 32(%rsp,%rsi,4)
	addq	$2, %rsi
	cmpq	$1537, %rsi                     # imm = 0x601
	jne	.LBB0_2
# %bb.3:                                #   in Loop: Header=BB0_1 Depth=1
	incl	%ebx
	cmpl	$100000, %ebx                   # imm = 0x186A0
	jne	.LBB0_1
```

As for the shift-left operator, I get the following assembly for SSE2:

```
.LBB0_3:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_4 Depth 2
	xorl	%ecx, %ecx
	.p2align	4
.LBB0_4:                                #   Parent Loop BB0_3 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movdqa	12336(%rsp,%rcx,4), %xmm1
	movdqa	6192(%rsp,%rcx,4), %xmm2
	pslld	$23, %xmm2
	paddd	%xmm0, %xmm2
	cvttps2dq	%xmm2, %xmm2
	pshufd	$245, %xmm1, %xmm3              # xmm3 = xmm1[1,1,3,3]
	pmuludq	%xmm2, %xmm1
	pshufd	$232, %xmm1, %xmm1              # xmm1 = xmm1[0,2,2,3]
	pshufd	$245, %xmm2, %xmm2              # xmm2 = xmm2[1,1,3,3]
	pmuludq	%xmm3, %xmm2
	pshufd	$232, %xmm2, %xmm2              # xmm2 = xmm2[0,2,2,3]
	punpckldq	%xmm2, %xmm1            # xmm1 = xmm1[0],xmm2[0],xmm1[1],xmm2[1]
	movdqa	%xmm1, 48(%rsp,%rcx,4)
	addq	$4, %rcx
	cmpq	$1536, %rcx                     # imm = 0x600
	jne	.LBB0_4
# %bb.5:                                #   in Loop: Header=BB0_3 Depth=1
	incl	%eax
	cmpl	$100000, %eax                   # imm = 0x186A0
	jne	.LBB0_3
```

Now looking at the AVX2 version:

```
.LBB0_1:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_2 Depth 2
	movl	$24, %eax
	.p2align	4
.LBB0_2:                                #   Parent Loop BB0_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	vmovdqu	12224(%rsp,%rax,4), %ymm0
	vmovdqu	12256(%rsp,%rax,4), %ymm1
	vmovdqu	12288(%rsp,%rax,4), %ymm2
	vmovdqu	12320(%rsp,%rax,4), %ymm3
	vpsllvd	6080(%rsp,%rax,4), %ymm0, %ymm0
	vpsllvd	6112(%rsp,%rax,4), %ymm1, %ymm1
	vpsllvd	6144(%rsp,%rax,4), %ymm2, %ymm2
	vpsllvd	6176(%rsp,%rax,4), %ymm3, %ymm3
	vmovdqu	%ymm0, -64(%rsp,%rax,4)
	vmovdqu	%ymm1, -32(%rsp,%rax,4)
	vmovdqu	%ymm2, (%rsp,%rax,4)
	vmovdqu	%ymm3, 32(%rsp,%rax,4)
	addq	$32, %rax
	cmpq	$1560, %rax                     # imm = 0x618
	jne	.LBB0_2
# %bb.3:                                #   in Loop: Header=BB0_1 Depth=1
	incl	%ebx
	cmpl	$100000, %ebx                   # imm = 0x186A0
	jne	.LBB0_1
```

The loop is much shorter. The trick here is the `vpsllvd` which is the variable bit shift left logical instruction. The older SSE2 doesn't support this, so it emulates it.

### Packing Smaller Words into Vectors

## Write-up 9: What is the new speedup for the vectorized code, over the unvectorized code, and for the AVX2 vectorized code, over the unvectorized code, when you change __TYPE__ to uint64_t, uint32_t, uint16_t and uint8_t? For each experiment, set __OP__ to + and do not change N.

For the unvectorized loop, the following are the results
```
@ make __TYPE__="uint64_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.037636 sec; N: 1536, I: 100000, __OP__: +, __TYPE__: uint32_t
@ make __TYPE__="uint32_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.032854 sec; N: 1536, I: 100000, __OP__: +, __TYPE__: uint32_t
@ make __TYPE__="uint16_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.037821 sec; N: 1536, I: 100000, __OP__: +, __TYPE__: uint32_t
@ make __TYPE__="uint8_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.038438 sec; N: 1536, I: 100000, __OP__: +, __TYPE__: uint32_t
```

Performance difference is negligible.

As for SSE2

```
@ make VECTORIZE=1 __TYPE__="uint64_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.025492 sec; N: 1536, I: 100000, __OP__: -, __TYPE__: uint64_t
@ make VECTORIZE=1 __TYPE__="uint32_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.011215 sec; N: 1536, I: 100000, __OP__: -, __TYPE__: uint32_t
@ make VECTORIZE=1 __TYPE__="uint16_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.007935 sec; N: 1536, I: 100000, __OP__: -, __TYPE__: uint16_t
@ make VECTORIZE=1 __TYPE__="uint8_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.003498 sec; N: 1536, I: 100000, __OP__: -, __TYPE__: uint8_t
```

You can see the speed difference here. uint32_t is 2.27x faster than uint64_t. uint16_t is 3.21x faster. uint8_t is 7.29x faster.

Finally for AVX2

```
@ make VECTORIZE=1 AVX2=1 __TYPE__="uint64_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.012624 sec; N: 1536, I: 100000, __OP__: -, __TYPE__: uint64_t
@ make VECTORIZE=1 AVX2=1 __TYPE__="uint32_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.011635 sec; N: 1536, I: 100000, __OP__: -, __TYPE__: uint32_t
@ make VECTORIZE=1 AVX2=1 __TYPE__="uint16_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.007386 sec; N: 1536, I: 100000, __OP__: -, __TYPE__: uint16_t
@ make VECTORIZE=1 AVX2=1 __TYPE__="uint8_t" &> /dev/null
@ ./loop
Elapsed execution time: 0.000027 sec; N: 1536, I: 100000, __OP__: -, __TYPE__: uint8_t
```

uint32_t is 1.08x faster. uint16_t is 1.7x. uint8_t is 467x faster!

## Write-up 10: You already determined that uint64_t yields the least performance improvement for vectorized codes (Section 3.1.4). Test a vector multiplication (i.e., __OP__ is *) using uint64_t arrays. What happens to the AVX2 vectorized code’s speedup relative to the unvectorized code (also using uint64_t and *)? What about when you set the data type width to be smaller — say uint8_t?

Let's compile and run

```
@ make __TYPE__="uint64_t" __OP__="*" &> /dev/null
@ ./loop
Elapsed execution time: 0.033221 sec; N: 1536, I: 100000, __OP__: *, __TYPE__: uint32_t
@ make VECTORIZE=1 AVX2=1 __TYPE__="uint64_t" __OP__="*" &> /dev/null
@ ./loop
Elapsed execution time: 0.035272 sec; N: 1536, I: 100000, __OP__: *, __TYPE__: uint64_t
```

You can see here that the performance is almost the same.

Using uint8_t

```
@ make __TYPE__="uint8_t" __OP__="*" &> /dev/null
@ ./loop
Elapsed execution time: 0.033127 sec; N: 1536, I: 100000, __OP__: +, __TYPE__: uint32_t
@ make VECTORIZE=1 AVX2=1 __TYPE__="uint8_t" __OP__="*" &> /dev/null
@ ./loop
Elapsed execution time: 0.002694 sec; N: 1536, I: 100000, __OP__: *, __TYPE__: uint8_t
```

Now the AVX2 vectorized version is 12.3x faster.

## Write-up 11: Open up the aws-perf-report tool for the AVX2 vectorized multiply code using uint64_t (as you did in Recitation 2). Remember to ﬁrst use the awsrun perf record tool to collect a performance report. Does the vector multiply take the most time? If not, where is time going instead? Now change __OP__ back to +, rerun the experiment and inspect aws-perf-report again. How does the percentage of time taken by the AVX2 vector add instruction compare to the time spent on the AVX2 vector multiply instruction?

The perf annotate results are
```
   6.85 │       vpmuludq   %ymm5,%ymm8,%ymm8
        │       vpaddq     %ymm4,%ymm8,%ymm4
        │       vpsllq     $0x20,%ymm4,%ymm4
        │       vpmuludq   %ymm2,%ymm5,%ymm2
   7.46 │       vpaddq     %ymm4,%ymm2,%ymm2
        │       vpsrlq     $0x20,%ymm6,%ymm4
        │       vpmuludq   %ymm3,%ymm4,%ymm4
        │       vpsrlq     $0x20,%ymm3,%ymm5
   7.49 │       vpmuludq   %ymm5,%ymm6,%ymm5
        │       vpaddq     %ymm4,%ymm5,%ymm4
        │       vpsllq     $0x20,%ymm4,%ymm4
        │       vpmuludq   %ymm3,%ymm6,%ymm3
   7.57 │       vpaddq     %ymm4,%ymm3,%ymm3
        │       vpsrlq     $0x20,%ymm7,%ymm4
        │       vpmuludq   %ymm0,%ymm4,%ymm4
        │       vpsrlq     $0x20,%ymm0,%ymm5
   9.56 │       vpmuludq   %ymm5,%ymm7,%ymm5
        │       vpaddq     %ymm4,%ymm5,%ymm4
   2.71 │       vpsllq     $0x20,%ymm4,%ymm4
        │       vpmuludq   %ymm0,%ymm7,%ymm0
   7.82 │       vpaddq     %ymm4,%ymm0,%ymm0
        │       vmovdqu    %ymm1,-0x40(%rsp,%rax,8)
   1.64 │       vmovdqu    %ymm2,-0x20(%rsp,%rax,8)
   7.16 │       vmovdqu    %ymm3,(%rsp,%rax,8)
   6.42 │       vmovdqu    %ymm0,0x20(%rsp,%rax,8)
```

Indeed the multiplication instructions are taking the longest. After them come the move instructions and the loop branch.

Now testing the plus operation

```
        │ b0:   vmovdqu    0x2fc0(%rsp,%rax,8),%ymm0
   1.05 │       vmovdqu    0x2fe0(%rsp,%rax,8),%ymm1
        │       vmovdqu    0x3000(%rsp,%rax,8),%ymm2
  10.90 │       vmovdqu    0x3020(%rsp,%rax,8),%ymm3
   0.52 │       vpaddq     0x5fc0(%rsp,%rax,8),%ymm0,%ymm0
  11.17 │       vpaddq     0x5fe0(%rsp,%rax,8),%ymm1,%ymm1
        │       vpaddq     0x6000(%rsp,%rax,8),%ymm2,%ymm2
   6.18 │       vpaddq     0x6020(%rsp,%rax,8),%ymm3,%ymm3
  50.46 │       vmovdqu    %ymm0,-0x40(%rsp,%rax,8)
        │       vmovdqu    %ymm1,-0x20(%rsp,%rax,8)
        │       vmovdqu    %ymm2,(%rsp,%rax,8)
   8.79 │       vmovdqu    %ymm3,0x20(%rsp,%rax,8)
        │       add        $0x10,%rax
        │       cmp        $0x60c,%rax
  10.92 │     ↑ jne        b0
        │       inc        %ebx
        │       cmp        $0x186a0,%ebx
        │     ↑ jne        a0
```

Now the add instructions are the hottest by far more (50%).

## Write-up 12: Get rid of the #define N 1024 macro and redeﬁne N as: int N = atoi(argv[1]); (at the beginning of main()). (Setting N through the command line ensures that the compiler will make no assumptions about it.) Rerun (with various choices of N) and compare the AVX2 vectorized, non-AVX2 vectorized, and unvectorized codes. Does the speedup change dramatically relative to the N = 1024 case? Why?


## Write-up 13: Set __TYPE__ to uint32_t and __OP__ to +, and change your inner loop to be strided. Does clang vectorize the code? Why might it choose not to vectorize the code?

So actually clang doesn't vectorize when I change the increment from `j++` to `j+=2`. This might be because the vector registers will carry useless data as they use contiguous data. If j increments by 2, then we need a[0], a[2], a[4], ...
Now a[1], a[3], ... are useless and the vector register carries them anyway. Also, it does little calculation now. This might make vectorization useless.

## Write-up 14: Use the #vectorize pragma described in the clang language extensions webpage above to make clang vectorize the strided loop. What is the speedup over non-vectorized code for non-AVX2 and AVX2 vectorization? What happens if you change the vectorize_width to 2? Play around with the clang loop pragmas and report the best you found (that vectorizes the loop). Did you get a speedup over the non-vectorized code?

There is almost no speedup from the vectorized code. Also, it looks more complex (probably handling the non-symmetric additions)

```
	vmovdqu	(%r12,%rdx,8), %ymm0
	vpaddd	(%r15,%rdx,8), %ymm0, %ymm0
	vmovd	%xmm0, (%r14,%rdx,8)
	vpextrd	$2, %xmm0, 8(%r14,%rdx,8)
	vextracti128	$1, %ymm0, %xmm0
	vmovd	%xmm0, 16(%r14,%rdx,8)
	vpextrd	$2, %xmm0, 24(%r14,%rdx,8)
	vmovdqu	32(%r12,%rdx,8), %ymm0
	vpaddd	32(%r15,%rdx,8), %ymm0, %ymm0
	vmovd	%xmm0, 32(%r14,%rdx,8)
	vpextrd	$2, %xmm0, 40(%r14,%rdx,8)
	vextracti128	$1, %ymm0, %xmm0
	vmovd	%xmm0, 48(%r14,%rdx,8)
	vpextrd	$2, %xmm0, 56(%r14,%rdx,8)
	addq	$8, %rdx
	cmpq	%rdx, %rax
	jne	.LBB0_15
```

### Strip Mining

The idea is to vectorize this loop (combining all the elements on an array into a single value)

```
	for (j = 0; j < N; j++) {
		total += A[j];
	}
```

The assembly

```
	vpaddd	(%r15,%rcx,4), %ymm0, %ymm0
	vpaddd	32(%r15,%rcx,4), %ymm1, %ymm1
	vpaddd	64(%r15,%rcx,4), %ymm2, %ymm2
	vpaddd	96(%r15,%rcx,4), %ymm3, %ymm3
	addq	$32, %rcx
	cmpq	%rcx, %rax
	jne	.LBB0_15
# %bb.16:
	vpaddd	%ymm0, %ymm1, %ymm0
	vpaddd	%ymm2, %ymm3, %ymm1
	vpaddd	%ymm0, %ymm1, %ymm0
	vextracti128	$1, %ymm0, %xmm1
	vpaddd	%xmm1, %xmm0, %xmm0
	vpshufd	$238, %xmm0, %xmm1              # xmm1 = xmm0[2,3,2,3]
	vpaddd	%xmm1, %xmm0, %xmm0
	vpshufd	$85, %xmm0, %xmm1               # xmm1 = xmm0[1,1,1,1]
	vpaddd	%xmm1, %xmm0, %xmm0
	vmovd	%xmm0, %r12d
	cmpl	%eax, %ebx
	je	.LBB0_22
```

This accumulates the sum in the four `%ymm` registers, and the idea of using four accumulators here is to keep the hardware busy, probably.
The part after `%bb.16` adds the accumulated registers.
