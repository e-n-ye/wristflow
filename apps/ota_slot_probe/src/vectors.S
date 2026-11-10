.syntax unified
.thumb
.section .wf_ota_vectors, "a", %progbits
.align 10
.global __Vectors
__Vectors:
    .word __StackTop
    .word Reset_Handler
    .rept 254
    .word wf_ota_slot_fault
    .endr

.section .text.Reset_Handler, "ax", %progbits
.thumb_func
.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    cpsid i
    ldr r0, =__StackLimit
    msr msplim, r0
    b wf_ota_slot_main
.size Reset_Handler, .-Reset_Handler

.section .text.wf_ota_slot_fault, "ax", %progbits
.thumb_func
.global wf_ota_slot_fault
.type wf_ota_slot_fault, %function
wf_ota_slot_fault:
    cpsid i
    movw r0, #0x3000
    movt r0, #0x2000
    movw r1, #0xdead
    movt r1, #0x5746
    str r1, [r0]
    dsb 0xf
1:  b 1b
.size wf_ota_slot_fault, .-wf_ota_slot_fault
