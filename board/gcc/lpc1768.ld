MEMORY { FLASH (rx) : ORIGIN = 0, LENGTH = 512K  RAM (rwx) : ORIGIN = 0x10000000, LENGTH = 32K }
_estack = ORIGIN(RAM) + LENGTH(RAM) - 32;
SECTIONS {
  .text : { KEEP(*(.isr_vector)) . = ALIGN(4); *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH
  _sidata = LOADADDR(.data);
  .data : { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  .bss  : { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
}
