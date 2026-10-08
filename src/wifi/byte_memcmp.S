.text
.global fzn1_byte_memcmp
.type fzn1_byte_memcmp,%function
fzn1_byte_memcmp:
    cbz x2, 2f
1:
    ldrb w3, [x0], #1
    ldrb w4, [x1], #1
    subs w3, w3, w4
    b.ne 3f
    subs x2, x2, #1
    b.ne 1b
2:
    mov w0, #0
    ret
3:
    mov w0, w3
    ret
.size fzn1_byte_memcmp, .-fzn1_byte_memcmp
.section .note.GNU-stack,"",%progbits
