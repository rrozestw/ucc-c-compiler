.section __TEXT,__text
Lsection_begin_text:
.globl _one
_one: # post_prologue
	movq $1, %rax
	cvtsi2sdq %rax, %xmm0
Lblk.0: # epilogue
	retq
Lfuncend__one:
.globl _zero
_zero: # _zero
	pushq %rbp
	movq %rsp, %rbp
Lblk.4: # prologue
	/* stack_sz{cur=0,max=8} stack_n_alloc=0 (total=0) call_spc=0 max_align=16 */
	subq $16, %rsp
Lblk.5: # post_prologue
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -8(%rbp)
	movb $0, %al
	callq _one
	movsd %xmm0, %xmm1
	movsd -8(%rbp), %xmm0
	movb $2, %al
	callq _sub
Lblk.3: # epilogue
	leaveq
	retq
Lfuncend__zero:
.globl _nan
_nan: # _nan
	pushq %rbp
	movq %rsp, %rbp
Lblk.7: # prologue
	/* stack_sz{cur=0,max=8} stack_n_alloc=0 (total=0) call_spc=0 max_align=16 */
	subq $16, %rsp
Lblk.8: # post_prologue
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -8(%rbp)
	movb $0, %al
	callq _zero
	movsd -8(%rbp), %xmm1
	divsd %xmm0, %xmm1
Lblk.6: # epilogue
	leaveq
	retq
Lfuncend__nan:
.globl _add
_add: # _add
	pushq %rbp
	movq %rsp, %rbp
	movsd %xmm0, -8(%rbp)
	movq %rdi, -184(%rbp)
	movq %rsi, -176(%rbp)
	movq %rdx, -168(%rbp)
	movq %rcx, -160(%rbp)
	movq %r8, -152(%rbp)
	movq %r9, -144(%rbp)
	testb %al, %al
	je Lblk.10
Lblk.11: # va_save
	movsd %xmm0, -136(%rbp)
	movsd %xmm1, -120(%rbp)
	movsd %xmm2, -104(%rbp)
	movsd %xmm3, -88(%rbp)
	movsd %xmm4, -72(%rbp)
	movsd %xmm5, -56(%rbp)
	movsd %xmm6, -40(%rbp)
	movsd %xmm7, -24(%rbp)
Lblk.10: # va_shortc
Lblk.12: # prologue
	/* stack_sz{cur=0,max=224} stack_n_alloc=0 (total=0) call_spc=0 max_align=16 */
	subq $224, %rsp
Lblk.13: # post_prologue
	/* va_start() begin */
	movl $0, -208(%rbp)
	movl -208(%rbp), %eax
	movl $56, -204(%rbp)
	movl -204(%rbp), %eax
	/* stack local offset: */
	movq %rbp, %rax
	subq $184, %rax
	movq %rax, -192(%rbp)
	movq -192(%rbp), %rax
	movq %rbp, %rax
	addq $16, %rax
	movq %rax, -200(%rbp)
	movq -200(%rbp), %rax
	/* va_start() end */
	jmp Lblk.15
Lblk.14: # while_body
	movsd -216(%rbp), %xmm0
	movsd -8(%rbp), %xmm1
	addsd %xmm0, %xmm1
	movsd %xmm1, -8(%rbp)
Lblk.15: # while_cont
	movl -204(%rbp), %eax
	cmpl $304, %eax
	jb Lblk.17
Lblk.18: # va_stack
	movq -200(%rbp), %rax
	movq -200(%rbp), %rdx
	addq $8, %rdx
	movq %rdx, -200(%rbp)
	movq %rax, %rcx
	jmp Lblk.19
Lblk.17: # va_reg
	movl %eax, %ecx
	addl $16, %ecx
	movl %ecx, -204(%rbp)
	movq -192(%rbp), %rcx
	addq %rax, %rcx
Lblk.19: # va_fin
	movsd (%rcx), %xmm0
	movsd %xmm0, -216(%rbp)
	movsd -216(%rbp), %xmm0
	/* saving register 0 */
	movsd %xmm0, -224(%rbp)
	movb $0, %al
	callq _nan
	movsd -224(%rbp), %xmm1
	
ucomisd %xmm1, %xmm0
movl $0, %eax
sete %al
setnp %cl
andb %cl, %al
andb $1, %al
testb %al, %al

	jne Lblk.14
Lblk.16: # while_break
	movsd -8(%rbp), %xmm0
Lblk.9: # epilogue
	leaveq
	retq
Lfuncend__add:
.globl _sub
_sub: # _sub
	pushq %rbp
	movq %rsp, %rbp
	movsd %xmm0, -16(%rbp)
	movsd %xmm1, -8(%rbp)
Lblk.21: # prologue
	/* stack_sz{cur=0,max=16} stack_n_alloc=0 (total=0) call_spc=0 max_align=0 */
	subq $16, %rsp
Lblk.22: # post_prologue
	movsd -16(%rbp), %xmm0
	movsd -8(%rbp), %xmm1
	subsd %xmm1, %xmm0
Lblk.20: # epilogue
	leaveq
	retq
Lfuncend__sub:
.globl _mul
_mul: # _mul
	pushq %rbp
	movq %rsp, %rbp
	movsd %xmm0, -16(%rbp)
	movsd %xmm1, -8(%rbp)
Lblk.24: # prologue
	/* stack_sz{cur=0,max=16} stack_n_alloc=0 (total=0) call_spc=0 max_align=0 */
	subq $16, %rsp
Lblk.25: # post_prologue
	movsd -16(%rbp), %xmm0
	movsd -8(%rbp), %xmm1
	mulsd %xmm1, %xmm0
Lblk.23: # epilogue
	leaveq
	retq
Lfuncend__mul:
.globl _div
_div: # _div
	pushq %rbp
	movq %rsp, %rbp
	movsd %xmm0, -16(%rbp)
	movsd %xmm1, -8(%rbp)
Lblk.27: # prologue
	/* stack_sz{cur=0,max=16} stack_n_alloc=0 (total=0) call_spc=0 max_align=0 */
	subq $16, %rsp
Lblk.28: # post_prologue
	movsd -16(%rbp), %xmm0
	movsd -8(%rbp), %xmm1
	divsd %xmm1, %xmm0
Lblk.26: # epilogue
	leaveq
	retq
Lfuncend__div:
.globl _equal
_equal: # _equal
	pushq %rbp
	movq %rsp, %rbp
	movsd %xmm0, -16(%rbp)
	movsd %xmm1, -8(%rbp)
Lblk.30: # prologue
	/* stack_sz{cur=0,max=16} stack_n_alloc=0 (total=0) call_spc=0 max_align=0 */
	subq $16, %rsp
Lblk.31: # post_prologue
	movsd -16(%rbp), %xmm0
	movsd -8(%rbp), %xmm1
	ucomisd %xmm1, %xmm0
	/* truncate cast from int to _Bool, size 4 -> 1 */
	movl $0, %eax
	sete %al
	setnp %cl
	andb %cl, %al
	andb $1, %al
Lblk.29: # epilogue
	leaveq
	retq
Lfuncend__equal:
.globl _phi
_phi: # _phi
	pushq %rbp
	movq %rsp, %rbp
Lblk.33: # prologue
	/* stack_sz{cur=0,max=48} stack_n_alloc=0 (total=0) call_spc=0 max_align=16 */
	subq $48, %rsp
Lblk.34: # post_prologue
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -8(%rbp)
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -16(%rbp)
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -24(%rbp)
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -32(%rbp)
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -40(%rbp)
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -48(%rbp)
	movb $0, %al
	callq _nan
	movsd %xmm0, %xmm1
	movsd -16(%rbp), %xmm0
	movsd %xmm1, %xmm2
	movsd -24(%rbp), %xmm1
	movsd %xmm2, %xmm3
	movsd -32(%rbp), %xmm2
	movsd %xmm3, %xmm4
	movsd -40(%rbp), %xmm3
	movsd %xmm4, %xmm5
	movsd -48(%rbp), %xmm4
	movb $6, %al
	callq _add
	callq _sqrt
	/* saving register 0 */
	movsd %xmm0, -16(%rbp)
	movb $0, %al
	callq _nan
	movsd %xmm0, %xmm1
	movsd -8(%rbp), %xmm0
	movsd %xmm1, %xmm2
	movsd -16(%rbp), %xmm1
	movb $3, %al
	callq _add
	/* saving register 0 */
	movsd %xmm0, -8(%rbp)
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -16(%rbp)
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -24(%rbp)
	movb $0, %al
	callq _nan
	movsd %xmm0, %xmm1
	movsd -16(%rbp), %xmm0
	movsd %xmm1, %xmm2
	movsd -24(%rbp), %xmm1
	movb $3, %al
	callq _add
	movsd %xmm0, %xmm1
	movsd -8(%rbp), %xmm0
	callq _div
Lblk.32: # epilogue
	leaveq
	retq
Lfuncend__phi:
.globl _psi
_psi: # _psi
	pushq %rbp
	movq %rsp, %rbp
Lblk.36: # prologue
	/* stack_sz{cur=0,max=8} stack_n_alloc=0 (total=0) call_spc=0 max_align=16 */
	subq $16, %rsp
Lblk.37: # post_prologue
	movb $0, %al
	callq _one
	/* saving register 0 */
	movsd %xmm0, -8(%rbp)
	movb $0, %al
	callq _phi
	movsd %xmm0, %xmm1
	movsd -8(%rbp), %xmm0
	callq _sub
Lblk.35: # epilogue
	leaveq
	retq
Lfuncend__psi:
.globl _exp
_exp: # _exp
	pushq %rbp
	movq %rsp, %rbp
	movsd %xmm0, -16(%rbp)
	movsd %xmm1, -8(%rbp)
Lblk.39: # prologue
	/* stack_sz{cur=0,max=40} stack_n_alloc=0 (total=0) call_spc=0 max_align=16 */
	subq $48, %rsp
Lblk.40: # post_prologue
	movsd -8(%rbp), %xmm0
	/* saving register 0 */
	movsd %xmm0, -24(%rbp)
	movb $0, %al
	callq _zero
	movsd %xmm0, %xmm1
	movsd -24(%rbp), %xmm0
	callq _equal
	test %al, %al
	jz Lblk.42
Lblk.41: # if_true
	movb $0, %al
	callq _one
	jmp Lblk.38
Lblk.42: # if_false
	movsd -16(%rbp), %xmm0
	movsd -16(%rbp), %xmm1
	movsd -8(%rbp), %xmm2
	/* saving register 0 */
	movsd %xmm0, -24(%rbp)
	/* saving register 1 */
	movsd %xmm1, -32(%rbp)
	/* saving register 2 */
	movsd %xmm2, -40(%rbp)
	movb $0, %al
	callq _one
	movsd %xmm0, %xmm1
	movsd -40(%rbp), %xmm0
	callq _sub
	movsd %xmm0, %xmm1
	movsd -32(%rbp), %xmm0
	callq _exp
	movsd %xmm0, %xmm1
	movsd -24(%rbp), %xmm0
	callq _mul
Lblk.38: # epilogue
	leaveq
	retq
Lfuncend__exp:
.globl _fib
_fib: # _fib
	pushq %rbp
	movq %rsp, %rbp
	pushq %rdi
Lblk.45: # prologue
	/* stack_sz{cur=0,max=24} stack_n_alloc=8 (total=8) call_spc=0 max_align=16 */
	subq $24, %rsp
Lblk.46: # post_prologue
	movb $0, %al
	callq _phi
	movl -8(%rbp), %eax
	cvtsi2sdl %eax, %xmm1
	callq _exp
	/* saving register 0 */
	movsd %xmm0, -16(%rbp)
	movb $0, %al
	callq _psi
	movl -8(%rbp), %eax
	cvtsi2sdl %eax, %xmm1
	callq _exp
	movsd %xmm0, %xmm1
	movsd -16(%rbp), %xmm0
	callq _sub
	/* saving register 0 */
	movsd %xmm0, -16(%rbp)
	movb $0, %al
	callq _phi
	/* saving register 0 */
	movsd %xmm0, -24(%rbp)
	movb $0, %al
	callq _psi
	movsd %xmm0, %xmm1
	movsd -24(%rbp), %xmm0
	callq _sub
	movsd %xmm0, %xmm1
	movsd -16(%rbp), %xmm0
	callq _div
	cvttsd2sil %xmm0, %eax
Lblk.44: # epilogue
	leaveq
	retq
Lfuncend__fib:
.globl _main
_main: # _main
	pushq %rbp
	movq %rsp, %rbp
Lblk.48: # prologue
	/* stack_sz{cur=16,max=16} stack_n_alloc=0 (total=16) call_spc=0 max_align=16 */
	subq $16, %rsp
Lblk.54: # call_save
	movq %rbx, -16(%rbp)
Lblk.49: # post_prologue
	movl $0, -4(%rbp)
	movl -4(%rbp), %eax
	jmp Lblk.50
Lblk.51: # for_body
	movl -4(%rbp), %eax
	movl -4(%rbp), %ecx
	/* saving register 0 */
	movl %eax, %ebx
	movl %ecx, %edi
	callq _fib
	leaq str.1(%rip), %rdi
	movl %ebx, %esi
	movl %eax, %edx
	movb $0, %al
	callq _printf
Lblk.53: # for_inc
	movl -4(%rbp), %eax
	movl -4(%rbp), %ecx
	incl %ecx
	movl %ecx, -4(%rbp)
Lblk.50: # for_test
	movl -4(%rbp), %eax
	cmpl $10, %eax
	jl Lblk.51
Lblk.52: # for_end
	movl $0, %eax
Lblk.47: # epilogue
	movq -16(%rbp), %rbx
	leaveq
	retq
Lfuncend__main:
Lsection_end_text:
.section __TEXT,const
Lsection_begin_rodata:
.align 1
str.1:
.ascii "fib(%d) = %d\012\000"
Lsection_end_rodata:
