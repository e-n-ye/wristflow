ENTRY(Reset_Handler)
MEMORY {
    ROM (rx) : ORIGIN = 0x12020000, LENGTH = 0x1000
    STACK (rw) : ORIGIN = 0x20001000, LENGTH = 0x1000
    RESULT (rw) : ORIGIN = 0x20003000, LENGTH = 0x100
}
SECTIONS {
    .vectors : { KEEP(*(.wf_ota_vectors)) } > ROM
    .text : { *(.text*) *(.rodata*) } > ROM
    .ARM.extab : { *(.ARM.extab*) } > ROM
    .ARM.exidx : { *(.ARM.exidx*) } > ROM
    wf_ota_slot_image_end = .;
    .stack (NOLOAD) : { . = ALIGN(8); __StackLimit = .; . += 0x1000; __StackTop = .; } > STACK
    .result (NOLOAD) : { KEEP(*(.wf_ota_result)) } > RESULT
    .data : { *(.data*) } > RESULT AT > ROM
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RESULT
    ASSERT(SIZEOF(.data) == 0 && SIZEOF(.bss) == 0, "Probe must not need C runtime initialization")
    ASSERT(SIZEOF(.vectors) == 0x400, "Full bounded vector table required")
    ASSERT(SIZEOF(.result) == 0x100, "Fixed result record required")
    /DISCARD/ : { *(.comment*) *(.note*) *(.eh_frame*) *(.init) *(.fini) }
}
