NOCROSSREFS ( .text .data )
SECTIONS
{
  .text : { tmpdir/cross1.o }
  .data : { tmpdir/cross2.o }
}
