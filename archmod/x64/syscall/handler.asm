extern syscall_prepare
extern syscall_done

handler:
    mov r15, rsp
    mov rsp, [gs:0x20]

    push rcx
    push r11

    push rbp
    push 0
    mov rbp, rsp

    and r12, 0xFF

    mov rax, r12
    mov cx, 8
    mul cx

    mov r10, [gs:0x28]
    add r10, rax

    mov rdx, r14
    mov rcx, r8
    mov r8 , r9

    call syscall_prepare
    call [r10]

    push rax
    push rdx

    call syscall_done

    pop rdx
    pop rax

    pop rbp
    pop rbp

    pop r11
    pop rcx

    mov rsp, r15

    o64 sysret