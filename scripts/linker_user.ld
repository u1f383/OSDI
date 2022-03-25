SECTIONS
{ 
  . = 0x0;
  .text :
  {
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