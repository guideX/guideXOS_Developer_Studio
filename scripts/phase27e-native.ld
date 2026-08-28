/* NativeElf Phase 27E image layout.  Each load segment starts on a page
 * boundary because the kernel validator deliberately does not accept the
 * usual ELF segment bias/offset form emitted by a hosted linker. */
PHDRS
{
  ro PT_LOAD FLAGS(4);
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = 0x10000000;
  .rodata : { *(.rodata .rodata.*) } :ro
  . = ALIGN(0x1000);
  .text : { *(.text .text.*) } :text
  . = ALIGN(0x1000);
  .data : { *(.data .data.*) } :data
  .bss : { *(.bss .bss.*) *(COMMON) } :data
}
