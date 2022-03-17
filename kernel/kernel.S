
.section ".text.kern"

.global _kernel

_kernel:
    ldr x1, =dtb_base
    str x0, [x1]
    
    ldr x0, =_stack
    mov sp, x0
    b kernel

.section ".data"

.global dtb_base
dtb_base:
    .long 0