OUTPUT_ARCH( "riscv" )
ENTRY( entry_assembly )

MEMORY
{
  RAM(xrw) : ORIGIN = 0x20000000, LENGTH = 0x200000
}

SECTIONS
{
  .text :
  {
    . = ALIGN(8);
    PROVIDE( entry_assembly = .);
    KEEP( *(.text.entry) )
    *(.text)
      *(.text*)
      *(.rodata)
      *(.rodata*)
      . = ALIGN(8);
    _etext = .;
    _sidata = _etext;
  } >RAM

  .data :
  {
    . = ALIGN(8);
    _sdata = .;
    *(.data)
      *(.data*)
      . = ALIGN(8);
    _edata = .;
  } >RAM

  .bss :
  {
    . = ALIGN(8);
    _sbss = .;
    *(.bss)
      *(.bss*)
      *(COMMON)
      . = ALIGN(8);
    _ebss = .;
  } >RAM

  .heap :
  {
    . = ALIGN(8);
    _heap_start = .;
  } >RAM
  
  .stack :
  {
    . = ALIGN(0x1000);
    *(.stack)
      *(.stack*)
  } >RAM
}
