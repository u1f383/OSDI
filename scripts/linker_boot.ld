SECTIONS
{ 
  . = 0x60000;
  _boot_base = .;
  .text :
  {
    KEEP(*(.text.boot))
    *(.text)
    *(.text.*)
  }

  . = ALIGN(0x1000);
  .bss :
  {
    __bss_start = .;
    *(.bss)
    *(.bss.*)
    . = ALIGN(0x1000);
    __bss_end = .;
  }

  . = ALIGN(0x1000);
  .data :
  {
    *(.data)
  }
  
  . = ALIGN(0x1000);
  .rodata :
  {
    *(.rodata)
  }

  /DISCARD/ :
  {
    *(.comment)
    *(.eh_frame)
  }

  _end = .;
}