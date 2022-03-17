SECTIONS
{ 
  _boot_base = 0x60000;

  . = 0x80000;
  .text :
  {
    KEEP(*(.text.boot))
    *(.text)
    *(.text.*)
  }

  . = 0x90000;
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