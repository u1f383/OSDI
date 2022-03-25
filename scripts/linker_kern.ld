SECTIONS
{ 
  . = 0x80000;
  .text :
  {
    KEEP(*(.text.kern))
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
  
  . = 0x100000;
  _stack = .;
  _end = .;
}