PHDRS
{
  header PT_PHDR PHDRS ;
  text PT_LOAD FILEHDR PHDRS ;
  data PT_LOAD ;
}

SECTIONS
{
  . = 0x80000 + SIZEOF_HEADERS;
  .text : { *(.text) } :text
  .data : { *(.data) } :data
  /DISCARD/ : { *(.*) }
}
