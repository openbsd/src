#!./perl
#
# Contributed by Graham Barr <Graham.Barr@tiuk.ti.com>
#
# So far there are tests for the following prototypes.
# none, () ($) ($@) ($%) ($;$) (&) (&\@) (&@) (%) (\%) (\@)
#
# It is impossible to test every prototype that can be specified, but
# we should test as many as we can.
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;

print "1..141\n";

my $i = 1;

sub testing (&$) {
    my $p = prototype(shift);
    my $c = shift;
    my $what = defined $c ? '(' . $p . ')' : 'no prototype';   
    print '#' x 25,"\n";
    print '# Testing ',$what,"\n";
    print '#' x 25,"\n";
    print "not "
	if((defined($p) && defined($c) && $p ne $c)
	   || (defined($p) != defined($c)));
    printf "ok %d\n",$i++;
}

@_ = qw(a b c d);
my @array;
my %hash;

##
##
##

testing \&no_proto, undef;

sub no_proto {
    print "# \@_ = (",join(",",@_),")\n";
    scalar(@_)
}

print "not " unless 0 == no_proto();
printf "ok %d\n",$i++;

print "not " unless 1 == no_proto(5);
printf "ok %d\n",$i++;

print "not " unless 4 == &no_proto;
printf "ok %d\n",$i++;

print "not " unless 1 == no_proto +6;
printf "ok %d\n",$i++;

print "not " unless 4 == no_proto(@_);
printf "ok %d\n",$i++;

##
##
##


testing \&no_args, '';

sub no_args () {
    print "# \@_ = (",join(",",@_),")\n";
    scalar(@_)
}

print "not " unless 0 == no_args();
printf "ok %d\n",$i++;

print "not " unless 0 == no_args;
printf "ok %d\n",$i++;

print "not " unless 5 == no_args +5;
printf "ok %d\n",$i++;

print "not " unless 4 == &no_args;
printf "ok %d\n",$i++;

print "not " unless 2 == &no_args(1,2);
printf "ok %d\n",$i++;

eval "no_args(1)";
print "not " unless $@;
printf "ok %d\n",$i++;

##
##
##

testing \&one_args, '$';

sub one_args ($) {
    print "# \@_ = (",join(",",@_),")\n";
    scalar(@_)
}

print "not " unless 1 == one_args(1);
printf "ok %d\n",$i++;

print "not " unless 1 == one_args +5;
printf "ok %d\n",$i++;

print "not " unless 4 == &one_args;
printf "ok %d\n",$i++;

print "not " unless 2 == &one_args(1,2);
printf "ok %d\n",$i++;

eval "one_args(1,2)";
print "not " unless $@;
printf "ok %d\n",$i++;

eval "one_args()";
print "not " unless $@;
printf "ok %d\n",$i++;

sub one_a_args ($) {
    print "# \@_ = (",join(",",@_),")\n";
    print "not " unless @_ == 1 && $_[0] == 4;
    printf "ok %d\n",$i++;
}

one_a_args(@_);

##
##
##

testing \&over_one_args, '$@';

sub over_one_args ($@) {
    print "# \@_ = (",join(",",@_),")\n";
    scalar(@_)
}

print "not " unless 1 == over_one_args(1);
printf "ok %d\n",$i++;

print "not " unless 2 == over_one_args(1,2);
printf "ok %d\n",$i++;

print "not " unless 1 == over_one_args +5;
printf "ok %d\n",$i++;

print "not " unless 4 == &over_one_args;
printf "ok %d\n",$i++;

print "not " unless 2 == &over_one_args(1,2);
printf "ok %d\n",$i++;

print "not " unless 5 == &over_one_args(1,@_);
printf "ok %d\n",$i++;

eval "over_one_args()";
print "not " unless $@;
printf "ok %d\n",$i++;

sub over_one_a_args ($@) {
    print "# \@_ = (",join(",",@_),")\n";
    print "not " unless @_ >= 1 && $_[0] == 4;
    printf "ok %d\n",$i++;
}

over_one_a_args(@_);
over_one_a_args(@_,1);
over_one_a_args(@_,1,2);
over_one_a_args(@_,@_);

##
##
##

testing \&scalar_and_hash, '$%';

sub scalar_and_hash ($%) {
    print "# \@_ = (",join(",",@_),")\n";
    scalar(@_)
}

print "not " unless 1 == scalar_and_hash(1);
printf "ok %d\n",$i++;

print "not " unless 3 == scalar_and_hash(1,2,3);
printf "ok %d\n",$i++;

print "not " unless 1 == scalar_and_hash +5;
printf "ok %d\n",$i++;

print "not " unless 4 == &scalar_and_hash;
printf "ok %d\n",$i++;

print "not " unless 2 == &scalar_and_hash(1,2);
printf "ok %d\n",$i++;

print "not " unless 5 == &scalar_and_hash(1,@_);
printf "ok %d\n",$i++;

eval "scalar_and_hash()";
print "not " unless $@;
printf "ok %d\n",$i++;

sub scalar_and_hash_a ($@) {
    print "# \@_ = (",join(",",@_),")\n";
    print "not " unless @_ >= 1 && $_[0] == 4;
    printf "ok %d\n",$i++;
}

scalar_and_hash_a(@_);
scalar_and_hash_a(@_,1);
scalar_and_hash_a(@_,1,2);
scalar_and_hash_a(@_,@_);

##
##
##

testing \&one_or_two, '$;$';

sub one_or_two ($;$) {
    print "# \@_ = (",join(",",@_),")\n";
    scalar(@_)
}

print "not " unless 1 == one_or_two(1);
printf "ok %d\n",$i++;

print "not " unless 2 == one_or_two(1,3);
printf "ok %d\n",$i++;

print "not " unless 1 == one_or_two +5;
printf "ok %d\n",$i++;

print "not " unless 4 == &one_or_two;
printf "ok %d\n",$i++;

print "not " unless 3 == &one_or_two(1,2,3);
printf "ok %d\n",$i++;

print "not " unless 5 == &one_or_two(1,@_);
printf "ok %d\n",$i++;

eval "one_or_two()";
print "not " unless $@;
printf "ok %d\n",$i++;

eval "one_or_two(1,2,3)";
print "not " unless $@;
printf "ok %d\n",$i++;

sub one_or_two_a ($;$) {
    print "# \@_ = (",join(",",@_),")\n";
    print "not " unless @_ >= 1 && $_[0] == 4;
    printf "ok %d\n",$i++;
}

one_or_two_a(@_);
one_or_two_a(@_,1);
one_or_two_a(@_,@_);

##
##
##

testing \&a_sub, '&';

sub a_sub (&) {
    print "# \@_ = (",join(",",@_),")\n";
    &{$_[0]};
}

sub tmp_sub_1 { printf "ok %d\n",$i++ }

a_sub { printf "ok %d\n",$i++ };
a_sub \&tmp_sub_1;

@array = ( \&tmp_sub_1 );
eval 'a_sub @array';
print "not " unless $@;
printf "ok %d\n",$i++;

##
##
##

testing \&a_subx, '\&';

sub a_subx (\&) {
    print "# \@_ = (",join(",",@_),")\n";
    &{$_[0]};
}

sub tmp_sub_2 { printf "ok %d\n",$i++ }
a_subx &tmp_sub_2;

@array = ( \&tmp_sub_2 );
eval 'a_subx @array';
print "not " unless $@;
printf "ok %d\n",$i++;

##
##
##

testing \&sub_aref, '&\@';

sub sub_aref (&\@) {
    print "# \@_ = (",join(",",@_),")\n";
    my($sub,$array) = @_;
    print "not " unless @_ == 2 && @{$array} == 4;
    print map { &{$sub}($_) } @{$array}
}

@array = (qw(O K)," ", $i++);
sub_aref { lc shift } @array;
print "\n";

##
##
##

testing \&sub_array, '&@';

sub sub_array (&@) {
    print "# \@_ = (",join(",",@_),")\n";
    print "not " unless @_ == 5;
    my $sub = shift;
    print map { &{$sub}($_) } @_
}

@array = (qw(O K)," ", $i++);
sub_array { lc shift } @array;
sub_array { lc shift } ('O', 'K', ' ', $i++);
print "\n";

##
##
##

testing \&a_hash, '%';

sub a_hash (%) {
    print "# \@_ = (",join(",",@_),")\n";
    scalar(@_);
}

print "not " unless 1 == a_hash 'a';
printf "ok %d\n",$i++;

print "not " unless 2 == a_hash 'a','b';
printf "ok %d\n",$i++;

##
##
##

testing \&a_hash_ref, '\%';

sub a_hash_ref (\%) {
    print "# \@_ = (",join(",",@_),")\n";
    print "not " unless ref($_[0]) && $_[0]->{'a'};
    printf "ok %d\n",$i++;
    $_[0]->{'b'} = 2;
}

%hash = ( a => 1);
a_hash_ref %hash;
print "not " unless $hash{'b'} == 2;
printf "ok %d\n",$i++;

##
##
##

testing \&array_ref_plus, '\@@';

sub array_ref_plus (\@@) {
    print "# \@_ = (",join(",",@_),")\n";
    print "not " unless @_ == 2 && ref($_[0]) && 1 == @{$_[0]} && $_[1] eq 'x';
    printf "ok %d\n",$i++;
    @{$_[0]} = (qw(ok)," ",$i++,"\n");
}

@array = ('a');
{ my @more = ('x');
  array_ref_plus @array, @more; }
print "not " unless @array == 4;
print @array;

my $p;
print "not " if defined prototype('CORE::print');
print "ok ", $i++, "\n";

print "not " if defined prototype('CORE::system');
print "ok ", $i++, "\n";

print "# CORE::open => ($p)\nnot " if ($p = prototype('CORE::open')) ne '*;$@';
print "ok ", $i++, "\n";

print "# CORE:Foo => ($p), \$@ => `$@'\nnot " 
    if defined ($p = eval { prototype('CORE::Foo') or 1 }) or $@ !~ /^Can't find an opnumber/;
print "ok ", $i++, "\n";

# correctly note too-short parameter lists that don't end with '$',
#  a possible regression.

sub foo1 ($\@);
eval q{ foo1 "s" };
print "not " unless $@ =~ /^Not enough/;
print "ok ", $i++, "\n";

sub foo2 ($\%);
eval q{ foo2 "s" };
print "not " unless $@ =~ /^Not enough/;
print "ok ", $i++, "\n";

sub X::foo3;
*X::foo3 = sub {'ok'};
print "# $@not " unless eval {X->foo3} eq 'ok';
print "ok ", $i++, "\n";

sub X::foo4 ($);
*X::foo4 = sub ($) {'ok'};
print "not " unless X->foo4 eq 'ok';
print "ok ", $i++, "\n";

# test if the (*) prototype allows barewords, constants, scalar expressions,
# globs and globrefs (just as CORE::open() does), all under stricture
sub star (*&) { &{$_[1]} }
sub star2 (**&) { &{$_[2]} }
sub BAR { "quux" }
sub Bar::BAZ { "quuz" }
my $star = 'FOO';
star FOO, sub {
    print "not " unless $_[0] eq 'FOO';
    print "ok $i - star FOO\n";
}; $i++;
star(FOO, sub {
	print "not " unless $_[0] eq 'FOO';
	print "ok $i - star(FOO)\n";
    }); $i++;
star "FOO", sub {
    print "not " unless $_[0] eq 'FOO';
    print qq/ok $i - star "FOO"\n/;
}; $i++;
star("FOO", sub {
	print "not " unless $_[0] eq 'FOO';
	print qq/ok $i - star("FOO")\n/;
    }); $i++;
star $star, sub {
    print "not " unless $_[0] eq 'FOO';
    print "ok $i - star \$star\n";
}; $i++;
star($star, sub {
	print "not " unless $_[0] eq 'FOO';
	print "ok $i - star(\$star)\n";
    }); $i++;
star *FOO, sub {
    print "not " unless $_[0] eq \*FOO;
    print "ok $i - star *FOO\n";
}; $i++;
star(*FOO, sub {
	print "not " unless $_[0] eq \*FOO;
	print "ok $i - star(*FOO)\n";
    }); $i++;
star \*FOO, sub {
    print "not " unless $_[0] eq \*FOO;
    print "ok $i - star \\*FOO\n";
}; $i++;
star(\*FOO, sub {
	print "not " unless $_[0] eq \*FOO;
	print "ok $i - star(\\*FOO)\n";
    }); $i++;
star2 FOO, BAR, sub {
    print "not " unless $_[0] eq 'FOO' and $_[1] eq 'BAR';
    print "ok $i - star2 FOO, BAR\n";
}; $i++;
star2(Bar::BAZ, FOO, sub {
	print "not " unless $_[0] eq 'Bar::BAZ' and $_[1] eq 'FOO';
	print "ok $i - star2(Bar::BAZ, FOO)\n"
    }); $i++;
star2 BAR(), FOO, sub {
    print "not " unless $_[0] eq 'quux' and $_[1] eq 'FOO';
    print "ok $i - star2 BAR(), FOO\n"
}; $i++;
star2(FOO, BAR(), sub {
	print "not " unless $_[0] eq 'FOO' and $_[1] eq 'quux';
	print "ok $i - star2(FOO, BAR())\n";
    }); $i++;
star2 "FOO", "BAR", sub {
    print "not " unless $_[0] eq 'FOO' and $_[1] eq 'BAR';
    print qq/ok $i - star2 "FOO", "BAR"\n/;
}; $i++;
star2("FOO", "BAR", sub {
	print "not " unless $_[0] eq 'FOO' and $_[1] eq 'BAR';
	print qq/ok $i - star2("FOO", "BAR")\n/;
    }); $i++;
star2 $star, $star, sub {
    print "not " unless $_[0] eq 'FOO' and $_[1] eq 'FOO';
    print "ok $i - star2 \$star, \$star\n";
}; $i++;
star2($star, $star, sub {
	print "not " unless $_[0] eq 'FOO' and $_[1] eq 'FOO';
	print "ok $i - star2(\$star, \$star)\n";
    }); $i++;
star2 *FOO, *BAR, sub {
    print "not " unless $_[0] eq \*FOO and $_[1] eq \*BAR;
    print "ok $i - star2 *FOO, *BAR\n";
}; $i++;
star2(*FOO, *BAR, sub {
	print "not " unless $_[0] eq \*FOO and $_[1] eq \*BAR;
	print "ok $i - star2(*FOO, *BAR)\n";
    }); $i++;
star2 \*FOO, \*BAR, sub {
    no strict 'refs';
    print "not " unless $_[0] eq \*{'FOO'} and $_[1] eq \*{'BAR'};
    print "ok $i - star2 \*FOO, \*BAR\n";
}; $i++;
star2(\*FOO, \*BAR, sub {
	no strict 'refs';
	print "not " unless $_[0] eq \*{'FOO'} and $_[1] eq \*{'BAR'};
	print "ok $i - star2(\*FOO, \*BAR)\n";
    }); $i++;

# test scalarref prototype
sub sreftest (\$$) {
    print "not " unless ref $_[0];
    print "ok $_[1] - sreftest\n";
}
{
    no strict 'vars';
    sreftest my $sref, $i++;
    sreftest($helem{$i}, $i++);
    sreftest $aelem[0], $i++;
}

# test prototypes when they are evaled and there is a syntax error
# Byacc generates the string "syntax error".  Bison gives the
# string "parse error".
#
for my $p ( "", qw{ () ($) ($@) ($%) ($;$) (&) (&\@) (&@) (%) (\%) (\@) } ) {
  no warnings 'prototype';
  my $eval = "sub evaled_subroutine $p { &void *; }";
  eval $eval;
  print "# eval[$eval]\nnot " unless $@ && $@ =~ /(parse|syntax) error/i;
  print "ok ", $i++, "\n";
}

# Not $$;$;$
print "not " unless prototype "CORE::substr" eq '$$;$$';
print "ok ", $i++, "\n";

# recv takes a scalar reference for its second argument
print "not " unless prototype "CORE::recv" eq '*\\$$$';
print "ok ", $i++, "\n";

{
    my $myvar;
    my @myarray;
    my %myhash;
    sub mysub { print "not calling mysub I hope\n" }
    local *myglob;

    sub myref (\[$@%&*]) { print "# $_[0]\n"; return "$_[0]" }

    print "not " unless myref($myvar)   =~ /^SCALAR\(/;
    print "ok ", $i++, "\n";
    print "not " unless myref(@myarray) =~ /^ARRAY\(/;
    print "ok ", $i++, "\n";
    print "not " unless myref(%myhash)  =~ /^HASH\(/;
    print "ok ", $i++, "\n";
    print "not " unless myref(&mysub)   =~ /^CODE\(/;
    print "ok ", $i++, "\n";
    print "not " unless myref(*myglob)  =~ /^GLOB\(/;
    print "ok ", $i++, "\n";

    eval q/sub multi1 (\[%@]) { 1 } multi1 $myvar;/;
    print "not " unless $@ =~ /Type of arg 1 to main::multi1 must be one of/;
    print "ok ", $i++, "\n";
    eval q/sub multi2 (\[$*&]) { 1 } multi2 @myarray;/;
    print "not " unless $@ =~ /Type of arg 1 to main::multi2 must be one of/;
    print "ok ", $i++, "\n";
    eval q/sub multi3 (\[$@]) { 1 } multi3 %myhash;/;
    print "not " unless $@ =~ /Type of arg 1 to main::multi3 must be one of/;
    print "ok ", $i++, "\n";
    eval q/sub multi4 ($\[%]) { 1 } multi4 1, &mysub;/;
    print "not " unless $@ =~ /Type of arg 2 to main::multi4 must be one of/;
    print "ok ", $i++, "\n";
    eval q/sub multi5 (\[$@]$) { 1 } multi5 *myglob;/;
    print "not " unless $@ =~ /Type of arg 1 to main::multi5 must be one of/
		     && $@ =~ /Not enough arguments/;
    print "ok ", $i++, "\n";
}

# check that obviously bad prototypes are getting warnings
{
  use warnings 'syntax';
  my $warn = "";
  local $SIG{__WARN__} = sub { $warn .= join("",@_) };
  
  eval 'sub badproto (@bar) { 1; }';
  print "not " unless $warn =~ /Illegal character in prototype for main::badproto : \@bar/;
  print "ok ", $i++, "\n";

  eval 'sub badproto2 (bar) { 1; }';
  print "not " unless $warn =~ /Illegal character in prototype for main::badproto2 : bar/;
  print "ok ", $i++, "\n";
  
  eval 'sub badproto3 (&$bar$@) { 1; }';
  print "not " unless $warn =~ /Illegal character in prototype for main::badproto3 : &\$bar\$\@/;
  print "ok ", $i++, "\n";
  
  eval 'sub badproto4 (@ $b ar) { 1; }';
  print "not " unless $warn =~ /Illegal character in prototype for main::badproto4 : \@\$bar/;
  print "ok ", $i++, "\n";
}

# make sure whitespace in prototypes works
eval "sub good (\$\t\$\n\$) { 1; }";
print "not " if $@;
print "ok ", $i++, "\n";

eval 'sub bug (\[%@]) {  } my $array = [0 .. 1]; bug %$array;';
print "not " unless $@ =~ /Not a HASH reference/;
print "ok ", $i++, " # TODO Ought to fail, doesn't in 5.8.2\n";
