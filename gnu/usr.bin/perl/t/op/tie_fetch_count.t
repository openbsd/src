#!./perl
# Tests counting number of FETCHes.
#
# See Bugs #76814 and #87708.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
    plan (tests => 312);
}

use strict;
use warnings;

my $count = 0;

# Usage:
#   tie $var, "main", $val;          # FETCH returns $val
#   tie $var, "main", $val1, $val2;  # FETCH returns the values in order,
#                                    # one at a time, repeating the last
#                                    # when the list is exhausted.
sub TIESCALAR {my $pack = shift; bless [@_], $pack;}
sub FETCH {$count ++; @{$_ [0]} == 1 ? ${$_ [0]}[0] : shift @{$_ [0]}}
sub STORE { unshift @{$_[0]}, $_[1] }


sub check_count {
    my $op = shift;
    my $expected = shift() // 1;
    local $::Level = $::Level + 1;
    is $count, $expected,
        "FETCH called " . (
          $expected == 1 ? "just once" : 
          $expected == 2 ? "twice"     :
                           "$count times"
        ) . " using '$op'";
    $count = 0;
}

my ($dummy, @dummy);

tie my $var => 'main', 1;

# Assignment.
$dummy  =  $var         ; check_count "=";
*dummy  =  $var         ; check_count '*glob = $tied';

# Unary +/-
$dummy  = +$var         ; check_count "unary +";
$dummy  = -$var         ; check_count "unary -";

# Basic arithmetic and string operators.
$dummy  =  $var   +   1 ; check_count '+';
$dummy  =  $var   -   1 ; check_count '-';
$dummy  =  $var   /   1 ; check_count '/';
$dummy  =  $var   *   1 ; check_count '*';
$dummy  =  $var   %   1 ; check_count '%';
$dummy  =  $var  **   1 ; check_count '**';
$dummy  =  $var  <<   1 ; check_count '<<';
$dummy  =  $var  >>   1 ; check_count '>>';
$dummy  =  $var   x   1 ; check_count 'x';
@dummy  = ($var)  x   1 ; check_count 'x';
$dummy  =  $var   .   1 ; check_count '.';
@dummy  =  $var  ..   1 ; check_count '$tied..1';
@dummy  =   1    .. $var; check_count '1..$tied';
tie my $v42 => 'main', "z";
@dummy  =  $v42  ..  "a"; check_count '$tied.."a"';
@dummy  =  "a"   .. $v42; check_count '"a"..$tied';
 
# Pre/post in/decrement
           $var ++      ; check_count 'post ++';
           $var --      ; check_count 'post --';
        ++ $var         ; check_count 'pre ++';
        -- $var         ; check_count 'pre --';

# Numeric comparison
$dummy  =  $var  <    1 ; check_count '<';
$dummy  =  $var  <=   1 ; check_count '<=';
$dummy  =  $var  ==   1 ; check_count '==';
$dummy  =  $var  >=   1 ; check_count '>=';
$dummy  =  $var  >    1 ; check_count '>';
$dummy  =  $var  !=   1 ; check_count '!=';
$dummy  =  $var <=>   1 ; check_count '<=>';

# String comparison
$dummy  =  $var  lt   1 ; check_count 'lt';
$dummy  =  $var  le   1 ; check_count 'le';
$dummy  =  $var  eq   1 ; check_count 'eq';
$dummy  =  $var  ge   1 ; check_count 'ge';
$dummy  =  $var  gt   1 ; check_count 'gt';
$dummy  =  $var  ne   1 ; check_count 'ne';
$dummy  =  $var cmp   1 ; check_count 'cmp';

# Bitwise operators
$dummy  =  $var   &   1 ; check_count '&';
$dummy  =  $var   ^   1 ; check_count '^';
$dummy  =  $var   |   1 ; check_count '|';
$dummy  = ~$var         ; check_count '~';

# Logical operators
$dummy  = !$var         ; check_count '!';
tie my $v_1, "main", 0;
$dummy  =  $v_1  ||   1 ; check_count '||';
$dummy  = ($v_1  or   1); check_count 'or';
$dummy  =  $var  &&   1 ; check_count '&&';
$dummy  = ($var and   1); check_count 'and';
$dummy  = ($var xor   1); check_count 'xor';
$dummy  =  $var ? 1 : 1 ; check_count '?:';

# Overloadable functions
$dummy  =   sin $var    ; check_count 'sin';
$dummy  =   cos $var    ; check_count 'cos';
$dummy  =   exp $var    ; check_count 'exp';
$dummy  =   abs $var    ; check_count 'abs';
$dummy  =   log $var    ; check_count 'log';
$dummy  =  sqrt $var    ; check_count 'sqrt';
$dummy  =   int $var    ; check_count 'int';
$dummy  = atan2 $var, 1 ; check_count 'atan2';

# Readline/glob
tie my $var0, "main", \*DATA;
$dummy  = <$var0>       ; check_count '<readline>';
$var    = \1;
$var   .= <DATA>        ; check_count '$tiedref .= <rcatline>';
$var    = "tied";
$var   .= <DATA>        ; check_count '$tiedstr .= <rcatline>';
$var    = *foo;
$var   .= <DATA>        ; check_count '$tiedglob .= <rcatline>';
{   no warnings "glob";
    $dummy  = <${var}>      ; check_count '<glob>';
}

# File operators
for (split //, 'rwxoRWXOezsfdpSbctugkTBMAC') {
    no warnings 'unopened';
    $dummy  = eval "-$_ \$var"; check_count "-$_";
    # Make $var hold a glob:
    $var = *dummy; $dummy = $var; $count = 0;
    $dummy  = eval "-$_ \$var"; check_count "-$_ \$tied_glob";
    next if /[guk]/;
    $var = *dummy; $dummy = $var; $count = 0;
    eval "\$dummy = -$_ \\\$var";
    check_count "-$_ \\\$tied_glob";
}
$dummy  = -l $var       ; check_count '-l';
$var = "test.pl";
$dummy  = -e -e -e $var ; check_count '-e -e';

# Matching
$_ = "foo";
$dummy  =  $var =~ m/ / ; check_count 'm//';
$dummy  =  $var =~ s/ //; check_count 's///';
{
    no warnings 'experimental::smartmatch';
    $dummy  =  $var ~~    1 ; check_count '~~';
}
$dummy  =  $var =~ y/ //; check_count 'y///';
           $var = \1;
$dummy  =  $var =~y/ /-/; check_count '$ref =~ y///';
           /$var/       ; check_count 'm/pattern/';
           /$var foo/   ; check_count 'm/$tied foo/';
          s/$var//      ; check_count 's/pattern//';
          s/$var foo//  ; check_count 's/$tied foo//';
          s/./$var/     ; check_count 's//replacement/';

# Dereferencing
tie my $var1 => 'main', \1;
$dummy  = $$var1        ; check_count '${}';
tie my $var2 => 'main', [];
$dummy  = @$var2        ; check_count '@{}';
$dummy  = shift $var2   ; check_count 'shift arrayref';
tie my $var3 => 'main', {};
$dummy  = %$var3        ; check_count '%{}';
$dummy  = keys $var3    ; check_count 'keys hashref';
{
    no strict 'refs';
    tie my $var4 => 'main', *];
    $dummy  = *$var4        ; check_count '*{}';
}

tie my $var5 => 'main', sub {1};
$dummy  = &$var5        ; check_count '&{}';

{
    no strict 'refs';
    tie my $var1 => 'main', 1;
    $dummy  = $$var1        ; check_count 'symbolic ${}';
    $dummy  = @$var1        ; check_count 'symbolic @{}';
    $dummy  = %$var1        ; check_count 'symbolic %{}';
    $dummy  = *$var1        ; check_count 'symbolic *{}';
    local *1 = sub{};
    $dummy  = &$var1        ; check_count 'symbolic &{}';

    # This test will not be a complete test if *988 has been created
    # already.  If this dies, change it to use another built-in variable.
    # In 5.10-14, rv2gv calls get-magic more times for built-in vars, which
    # is why we need the test this way.
    if (exists $::{988}) {
	die "*988 already exists. Please adjust this test"
    }
    tie my $var6 => main => 988;
    no warnings;
    readdir $var6           ; check_count 'symbolic readdir';
    if (exists $::{973}) { # Need a different variable here
	die "*973 already exists. Please adjust this test"
    }
    tie my $var7 => main => 973;
    defined $$var7          ; check_count 'symbolic defined ${}';
}

tie my $var8 => 'main', 'main';
sub bolgy {}
$var8->bolgy            ; check_count '->method';
{
    no warnings 'once';
    () = *swibble;
    # This must be the name of an existing glob to trigger the maximum
    # number of fetches in 5.14:
    tie my $var9 => 'main', 'swibble';
    no strict 'refs';
    use constant glumscrin => 'shreggleboughet';
    *$var9 = \&{"glumscrin"}; check_count '*$tied = \&{"name of const"}';
}

# Functions that operate on filenames or filehandles
for ([chdir=>''],[chmod=>'0,'],[chown=>'0,0,'],[utime=>'0,0,'],
     [truncate=>'',',0'],[stat=>''],[lstat=>''],[open=>'my $fh,"<&",'],
     ['()=sort'=>'',' 1,2,3']) {
    my($op,$args,$postargs) = @$_; $postargs //= '';
    # This line makes $var8 hold a glob:
    $var8 = *dummy; $dummy = $var8; $count = 0;
    eval "$op $args \$var8 $postargs";
    check_count "$op $args\$tied_glob$postargs";
    $var8 = *dummy; $dummy = $var8; $count = 0;
    my $ref = \$var8;
    eval "$op $args \$ref $postargs";
    check_count "$op $args\\\$tied_glob$postargs";
}

{
    no warnings;
    $var = *foo;
    $dummy  =  select $var, undef, undef, 0
                            ; check_count 'select $tied_glob, ...';
    $var = \1;
    $dummy  =  select $var, undef, undef, 0
                            ; check_count 'select $tied_ref, ...';
    $var = undef;
    $dummy  =  select $var, undef, undef, 0
                            ; check_count 'select $tied_undef, ...';
}

chop(my $u = "\xff\x{100}");
tie $var, "main", $u;
$dummy  = pack "u", $var; check_count 'pack "u", $utf8';

tie $var, "main", "\x{100}";
pos$var = 0             ; check_count 'lvalue pos $utf8';
$dummy=sprintf"%1s",$var; check_count 'sprintf "%1s", $utf8';
$dummy=sprintf"%.1s",$var; check_count 'sprintf "%.1s", $utf8';
$dummy  = substr$var,0,1; check_count 'substr $utf8';
my $l   =\substr$var,0,1;
$dummy  = $$l           ; check_count 'reading lvalue substr($utf8)';
$$l     = 0             ; check_count 'setting lvalue substr($utf8)';
tie $var, "main", "a";
$$l     = "\x{100}"     ; check_count 'assigning $utf8 to lvalue substr';
tie $var1, "main", "a";
substr$var1,0,0,"\x{100}"; check_count '4-arg substr with utf8 replacement';

{
    local $SIG{__WARN__} = sub {};
    $dummy  =  warn $var    ; check_count 'warn $tied';
    tie $@, => 'main', 1;
    $dummy  =  warn         ; check_count 'warn() with $@ tied (num)';
    tie $@, => 'main', \1;
    $dummy  =  warn         ; check_count 'warn() with $@ tied (ref)';
    tie $@, => 'main', "foo\n";
    $dummy  =  warn         ; check_count 'warn() with $@ tied (str)';
    untie $@;
}

###############################################
#        Tests for  $foo binop $foo           #
###############################################

# These test that binary ops call FETCH twice if the same scalar is used
# for both operands. They also test that both return values from
# FETCH are used.

my %mutators = map { ($_ => 1) } qw(. + - * / % ** << >> & | ^);


sub _bin_test {
    my $int = shift;
    my $op = shift;
    my $exp = pop;
    my @fetches = @_;

    $int = $int ? 'use integer; ' : '';

    tie my $var, "main", @fetches;
    is(eval "$int\$var $op \$var", $exp, "retval of $int\$var $op \$var");
    check_count "$int$op", 2;

    return unless $mutators{$op};

    tie my $var2, "main", @fetches;
    is(eval "$int \$var2 $op= \$var2", $exp, "retval of $int \$var2 $op= \$var2");
    check_count "$int$op=", 3;
}

sub bin_test {
    _bin_test(0, @_);
}

sub bin_int_test {
    _bin_test(1, @_);
}

bin_test '**',  2, 3, 8;
bin_test '*' ,  2, 3, 6;
bin_test '/' , 10, 2, 5;
bin_test '%' , 11, 2, 1;
bin_test 'x' , 11, 2, 1111;
bin_test '-' , 11, 2, 9;
bin_test '<<', 11, 2, 44;
bin_test '>>', 44, 2, 11;
bin_test '<' ,  1, 2, 1;
bin_test '>' , 44, 2, 1;
bin_test '<=', 44, 2, "";
bin_test '>=',  1, 2, "";
bin_test '!=',  1, 2, 1;
bin_test '<=>', 1, 2, -1;
bin_test 'le',  4, 2, "";
bin_test 'lt',  1, 2, 1;
bin_test 'gt',  4, 2, 1;
bin_test 'ge',  1, 2, "";
bin_test 'eq',  1, 2, "";
bin_test 'ne',  1, 2, 1;
bin_test 'cmp', 1, 2, -1;
bin_test '&' ,  1, 2, 0;
bin_test '|' ,  1, 2, 3;
bin_test '^' ,  3, 5, 6;
bin_test '.' ,  1, 2, 12;
bin_test '==',  1, 2, "";
bin_test '+' ,  1, 2, 3;
bin_int_test '*' ,  2, 3, 6;
bin_int_test '/' , 10, 2, 5;
bin_int_test '%' , 11, 2, 1;
bin_int_test '+' ,  1, 2, 3;
bin_int_test '-' , 11, 2, 9;
bin_int_test '<' ,  1, 2, 1;
bin_int_test '>' , 44, 2, 1;
bin_int_test '<=', 44, 2, "";
bin_int_test '>=',  1, 2, "";
bin_int_test '==',  1, 2, "";
bin_int_test '!=',  1, 2, 1;
bin_int_test '<=>', 1, 2, -1;
tie $var, "main", 1, 4;
cmp_ok(atan2($var, $var), '<', .3, 'retval of atan2 $var, $var');
check_count 'atan2',  2;

__DATA__
