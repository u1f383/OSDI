SECTIONS
{ 
  . = 0;
  .bss :
  {
    __bss_start = .;
    *(.bss)
    *(.bss.*)
    . = ALIGN(0x10);
    __bss_end = .;
  }

  . = 0x80000;
  .text :
  {
    KEEP(*(.text.boot))
    *(.text)
    *(.text.*)
  }

  . = ALIGN(0x10);
  .data :
  {
    *(.data)
  }
  
  . = ALIGN(0x10);
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