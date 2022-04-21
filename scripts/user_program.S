.section ".text"

.global _start
_start:
    mov x0, 0xff0000
    mov x1, 0x10
    svc 2
test:
    b test