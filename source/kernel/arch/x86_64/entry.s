.section .text
.global _start
.type _start, @function

_start:
    cli
    # Ensure 16-byte stack alignment
    andq $-16, %rsp
    
    # Call 64-bit kernel main
    call kernel_main

    # Halt if kernel returns
.halt_loop:
    cli
    hlt
    jmp .halt_loop

.size _start, . - _start

.section .note.GNU-stack,"",@progbits
