#!./perl

BEGIN {
    chdir 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 3;

eval { for (\2) { $_ = <FH> } };
like($@, 'Modification of a read-only value attempted', '[perl #19566]');

{
  open A,"+>a"; $a = 3;
  is($a .= <A>, 3, '#21628 - $a .= <A> , A eof');
  close A; $a = 4;
  is($a .= <A>, 4, '#21628 - $a .= <A> , A closed');
  unlink "a";
}
