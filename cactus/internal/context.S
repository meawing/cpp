/**
 * x86_64 Calling Convention:
 * Arguments are passed via:
 *   - GP: %rdi, %rsi, %rdx, %rcx, %r8, %r9
 *   - FP: %xmm0, %xmm1, ..., %xmm7
 * Return value is passed via:
 *   - GP: %rax
 *   - FP: %xmm0, %xmm1
 * Callee preserves:
 *   - GP: %rbx, %rbp, %rsp, %r12, %r13, %r14, %r15
 * Others:
 *   - %r10 - Static Chain Pointer (?)
 *   - %r11 - Scratch Register
 *   - %xmm8, %xmm9, ..., %xmm15 - Scratch Registers
*/

#define CONCAT_AUX(x, y) x ## y
#define CONCAT(x, y) CONCAT_AUX(x, y)

#if defined(__APPLE__)

#define NAME(name) CONCAT(_, name)
#define TYPE_FUNCTION(name)

#else

#define NAME(name) name
#define TYPE_FUNCTION(name) .type name, @function

#endif

    .global NAME(CactusContextEntry)
    .global NAME(CactusContextTrampoline)
    .global NAME(CactusContextActivate)

    .text

NAME(CactusContextTrampoline):
    TYPE_FUNCTION(CactusContextTrampoline)
    movq %rax, %rdi      /* CactusContextActivate returns suspended context in rax */
    jmp NAME(CactusContextEntry)
    .size NAME(CactusContextTrampoline), . - NAME(CactusContextTrampoline)

NAME(CactusContextActivate):
    TYPE_FUNCTION(CactusContextActivate)
    .cfi_startproc       /* CFI stands for call frame information */
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset rbp,-16
    movq %rsp, %rbp
    .cfi_def_cfa_register   rbp

    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    call next            /* Save instruction pointer for debugging purproses (see .gdbinit) */
next:
    pushq %rsi           /* Save data */
    movq %rsp, %rax      /* Save old SP */
    movq %rdi, %rsp      /* Load new SP. */
    leaq 16(%rsp), %rsp  /* Discard saved instruction pointer and data */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx

    popq %rbp
    .cfi_def_cfa rsp,8
    ret
    .cfi_endproc
    .size NAME(CactusContextActivate), . - NAME(CactusContextActivate)

    .section        .note.GNU-stack,"",@progbits
