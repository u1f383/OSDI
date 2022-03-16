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

  . = ALIGN(0x1000);
  _stack = .;

  /DISCARD/ :
  {
    *(.comment)
    *(.eh_frame)
  }
  
  _end = .;
}