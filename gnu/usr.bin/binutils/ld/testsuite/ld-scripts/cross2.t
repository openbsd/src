NOCROSSREFS ( .text .data )
SECTIONS
{
  .text : { *(.text) *(.pr) }
  .data : { *(.data) *(.sdata) *(.rw) *(.tc0) *(.tc) }
}
