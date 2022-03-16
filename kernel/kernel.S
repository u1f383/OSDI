
.section ".text.kern"

.global _kernel

_kernel:
    ldr x0, =_stack
    mov sp, x0
    b kernel