use strict;
use warnings;

package PLXML;

sub DESTROY { }

sub walk {
    print "walk(" . join(',', @_) . ")\n";
    my $self = shift;
    for my $key (sort keys %$self) {
	print "\t$key = <$$self{$key}>\n";
    }
    foreach my $kid (@{$$self{Kids}}) {
	$kid->walk(@_);
    }
}

package PLXML::Characters;

our @ISA = ('PLXML');
sub walk {}

package PLXML::madprops;

our @ISA = ('PLXML');

package PLXML::mad_op;

our @ISA = ('PLXML');

package PLXML::mad_pv;

our @ISA = ('PLXML');

package PLXML::baseop;

our @ISA = ('PLXML');

package PLXML::baseop_unop;

our @ISA = ('PLXML');

package PLXML::binop;

our @ISA = ('PLXML');

package PLXML::cop;

our @ISA = ('PLXML');

package PLXML::filestatop;

our @ISA = ('PLXML::baseop_unop');

package PLXML::listop;

our @ISA = ('PLXML');

package PLXML::logop;

our @ISA = ('PLXML');

package PLXML::loop;

our @ISA = ('PLXML');

package PLXML::loopexop;

our @ISA = ('PLXML');

package PLXML::padop;

our @ISA = ('PLXML');

package PLXML::padop_svop;

our @ISA = ('PLXML');

package PLXML::pmop;

our @ISA = ('PLXML');

package PLXML::pvop_svop;

our @ISA = ('PLXML');

package PLXML::unop;

our @ISA = ('PLXML');


# New ops always go at the end, just before 'custom'

# A recapitulation of the format of this file:
# The file consists of five columns: the name of the op, an English
# description, the name of the "check" routine used to optimize this
# operation, some flags, and a description of the operands.

# The flags consist of options followed by a mandatory op class signifier

# The classes are:
# baseop      - 0            unop     - 1            binop      - 2
# logop       - |            listop   - @            pmop       - /
# padop/svop  - $            padop    - # (unused)   loop       - {
# baseop/unop - %            loopexop - }            filestatop - -
# pvop/svop   - "            cop      - ;

# Other options are:
#   needs stack mark                    - m
#   needs constant folding              - f
#   produces a scalar                   - s
#   produces an integer                 - i
#   needs a target                      - t
#   target can be in a pad              - T
#   has a corresponding integer version - I
#   has side effects                    - d
#   uses $_ if no argument given        - u

# Values for the operands are:
# scalar      - S            list     - L            array     - A
# hash        - H            sub (CV) - C            file      - F
# socket      - Fs           filetest - F-           reference - R
# "?" denotes an optional operand.

# Nothing.

package PLXML::op_null;

our @ISA = ('PLXML::baseop');

sub key { 'null' }
sub desc { 'null operation' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_stub;

our @ISA = ('PLXML::baseop');

sub key { 'stub' }
sub desc { 'stub' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_scalar;

our @ISA = ('PLXML::baseop_unop');

sub key { 'scalar' }
sub desc { 'scalar' }
sub check { 'ck_fun' }
sub flags { 's%' }
sub args { 'S' }



# Pushy stuff.

package PLXML::op_pushmark;

our @ISA = ('PLXML::baseop');

sub key { 'pushmark' }
sub desc { 'pushmark' }
sub check { 'ck_null' }
sub flags { 's0' }
sub args { '' }


package PLXML::op_wantarray;

our @ISA = ('PLXML::baseop');

sub key { 'wantarray' }
sub desc { 'wantarray' }
sub check { 'ck_null' }
sub flags { 'is0' }
sub args { '' }



package PLXML::op_const;

our @ISA = ('PLXML::padop_svop');

sub key { 'const' }
sub desc { 'constant item' }
sub check { 'ck_svconst' }
sub flags { 's$' }
sub args { '' }



package PLXML::op_gvsv;

our @ISA = ('PLXML::padop_svop');

sub key { 'gvsv' }
sub desc { 'scalar variable' }
sub check { 'ck_null' }
sub flags { 'ds$' }
sub args { '' }


package PLXML::op_gv;

our @ISA = ('PLXML::padop_svop');

sub key { 'gv' }
sub desc { 'glob value' }
sub check { 'ck_null' }
sub flags { 'ds$' }
sub args { '' }


package PLXML::op_gelem;

our @ISA = ('PLXML::binop');

sub key { 'gelem' }
sub desc { 'glob elem' }
sub check { 'ck_null' }
sub flags { 'd2' }
sub args { 'S S' }


package PLXML::op_padsv;

our @ISA = ('PLXML::baseop');

sub key { 'padsv' }
sub desc { 'private variable' }
sub check { 'ck_null' }
sub flags { 'ds0' }
sub args { '' }


package PLXML::op_padav;

our @ISA = ('PLXML::baseop');

sub key { 'padav' }
sub desc { 'private array' }
sub check { 'ck_null' }
sub flags { 'd0' }
sub args { '' }


package PLXML::op_padhv;

our @ISA = ('PLXML::baseop');

sub key { 'padhv' }
sub desc { 'private hash' }
sub check { 'ck_null' }
sub flags { 'd0' }
sub args { '' }


package PLXML::op_padany;

our @ISA = ('PLXML::baseop');

sub key { 'padany' }
sub desc { 'private value' }
sub check { 'ck_null' }
sub flags { 'd0' }
sub args { '' }



package PLXML::op_pushre;

our @ISA = ('PLXML::pmop');

sub key { 'pushre' }
sub desc { 'push regexp' }
sub check { 'ck_null' }
sub flags { 'd/' }
sub args { '' }



# References and stuff.

package PLXML::op_rv2gv;

our @ISA = ('PLXML::unop');

sub key { 'rv2gv' }
sub desc { 'ref-to-glob cast' }
sub check { 'ck_rvconst' }
sub flags { 'ds1' }
sub args { '' }


package PLXML::op_rv2sv;

our @ISA = ('PLXML::unop');

sub key { 'rv2sv' }
sub desc { 'scalar dereference' }
sub check { 'ck_rvconst' }
sub flags { 'ds1' }
sub args { '' }


package PLXML::op_av2arylen;

our @ISA = ('PLXML::unop');

sub key { 'av2arylen' }
sub desc { 'array length' }
sub check { 'ck_null' }
sub flags { 'is1' }
sub args { '' }


package PLXML::op_rv2cv;

our @ISA = ('PLXML::unop');

sub key { 'rv2cv' }
sub desc { 'subroutine dereference' }
sub check { 'ck_rvconst' }
sub flags { 'd1' }
sub args { '' }


package PLXML::op_anoncode;

our @ISA = ('PLXML::padop_svop');

sub key { 'anoncode' }
sub desc { 'anonymous subroutine' }
sub check { 'ck_anoncode' }
sub flags { '$' }
sub args { '' }


package PLXML::op_prototype;

our @ISA = ('PLXML::baseop_unop');

sub key { 'prototype' }
sub desc { 'subroutine prototype' }
sub check { 'ck_null' }
sub flags { 's%' }
sub args { 'S' }


package PLXML::op_refgen;

our @ISA = ('PLXML::unop');

sub key { 'refgen' }
sub desc { 'reference constructor' }
sub check { 'ck_spair' }
sub flags { 'm1' }
sub args { 'L' }


package PLXML::op_srefgen;

our @ISA = ('PLXML::unop');

sub key { 'srefgen' }
sub desc { 'single ref constructor' }
sub check { 'ck_null' }
sub flags { 'fs1' }
sub args { 'S' }


package PLXML::op_ref;

our @ISA = ('PLXML::baseop_unop');

sub key { 'ref' }
sub desc { 'reference-type operator' }
sub check { 'ck_fun' }
sub flags { 'stu%' }
sub args { 'S?' }


package PLXML::op_bless;

our @ISA = ('PLXML::listop');

sub key { 'bless' }
sub desc { 'bless' }
sub check { 'ck_fun' }
sub flags { 's@' }
sub args { 'S S?' }



# Pushy I/O.

package PLXML::op_backtick;

our @ISA = ('PLXML::baseop_unop');

sub key { 'backtick' }
sub desc { 'quoted execution (``, qx)' }
sub check { 'ck_open' }
sub flags { 't%' }
sub args { '' }


# glob defaults its first arg to $_
package PLXML::op_glob;

our @ISA = ('PLXML::listop');

sub key { 'glob' }
sub desc { 'glob' }
sub check { 'ck_glob' }
sub flags { 't@' }
sub args { 'S?' }


package PLXML::op_readline;

our @ISA = ('PLXML::baseop_unop');

sub key { 'readline' }
sub desc { '<HANDLE>' }
sub check { 'ck_null' }
sub flags { 't%' }
sub args { 'F?' }


package PLXML::op_rcatline;

our @ISA = ('PLXML::padop_svop');

sub key { 'rcatline' }
sub desc { 'append I/O operator' }
sub check { 'ck_null' }
sub flags { 't$' }
sub args { '' }



# Bindable operators.

package PLXML::op_regcmaybe;

our @ISA = ('PLXML::unop');

sub key { 'regcmaybe' }
sub desc { 'regexp internal guard' }
sub check { 'ck_fun' }
sub flags { 's1' }
sub args { 'S' }


package PLXML::op_regcreset;

our @ISA = ('PLXML::unop');

sub key { 'regcreset' }
sub desc { 'regexp internal reset' }
sub check { 'ck_fun' }
sub flags { 's1' }
sub args { 'S' }


package PLXML::op_regcomp;

our @ISA = ('PLXML::logop');

sub key { 'regcomp' }
sub desc { 'regexp compilation' }
sub check { 'ck_null' }
sub flags { 's|' }
sub args { 'S' }


package PLXML::op_match;

our @ISA = ('PLXML::pmop');

sub key { 'match' }
sub desc { 'pattern match (m//)' }
sub check { 'ck_match' }
sub flags { 'd/' }
sub args { '' }


package PLXML::op_qr;

our @ISA = ('PLXML::pmop');

sub key { 'qr' }
sub desc { 'pattern quote (qr//)' }
sub check { 'ck_match' }
sub flags { 's/' }
sub args { '' }


package PLXML::op_subst;

our @ISA = ('PLXML::pmop');

sub key { 'subst' }
sub desc { 'substitution (s///)' }
sub check { 'ck_match' }
sub flags { 'dis/' }
sub args { 'S' }


package PLXML::op_substcont;

our @ISA = ('PLXML::logop');

sub key { 'substcont' }
sub desc { 'substitution iterator' }
sub check { 'ck_null' }
sub flags { 'dis|' }
sub args { '' }


package PLXML::op_trans;

our @ISA = ('PLXML::pvop_svop');

sub key { 'trans' }
sub desc { 'transliteration (tr///)' }
sub check { 'ck_match' }
sub flags { 'is"' }
sub args { 'S' }



# Lvalue operators.
# sassign is special-cased for op class

package PLXML::op_sassign;

our @ISA = ('PLXML::baseop');

sub key { 'sassign' }
sub desc { 'scalar assignment' }
sub check { 'ck_sassign' }
sub flags { 's0' }
sub args { '' }


package PLXML::op_aassign;

our @ISA = ('PLXML::binop');

sub key { 'aassign' }
sub desc { 'list assignment' }
sub check { 'ck_null' }
sub flags { 't2' }
sub args { 'L L' }



package PLXML::op_chop;

our @ISA = ('PLXML::baseop_unop');

sub key { 'chop' }
sub desc { 'chop' }
sub check { 'ck_spair' }
sub flags { 'mts%' }
sub args { 'L' }


package PLXML::op_schop;

our @ISA = ('PLXML::baseop_unop');

sub key { 'schop' }
sub desc { 'scalar chop' }
sub check { 'ck_null' }
sub flags { 'stu%' }
sub args { 'S?' }


package PLXML::op_chomp;

our @ISA = ('PLXML::baseop_unop');

sub key { 'chomp' }
sub desc { 'chomp' }
sub check { 'ck_spair' }
sub flags { 'mTs%' }
sub args { 'L' }


package PLXML::op_schomp;

our @ISA = ('PLXML::baseop_unop');

sub key { 'schomp' }
sub desc { 'scalar chomp' }
sub check { 'ck_null' }
sub flags { 'sTu%' }
sub args { 'S?' }


package PLXML::op_defined;

our @ISA = ('PLXML::baseop_unop');

sub key { 'defined' }
sub desc { 'defined operator' }
sub check { 'ck_defined' }
sub flags { 'isu%' }
sub args { 'S?' }


package PLXML::op_undef;

our @ISA = ('PLXML::baseop_unop');

sub key { 'undef' }
sub desc { 'undef operator' }
sub check { 'ck_lfun' }
sub flags { 's%' }
sub args { 'S?' }


package PLXML::op_study;

our @ISA = ('PLXML::baseop_unop');

sub key { 'study' }
sub desc { 'study' }
sub check { 'ck_fun' }
sub flags { 'su%' }
sub args { 'S?' }


package PLXML::op_pos;

our @ISA = ('PLXML::baseop_unop');

sub key { 'pos' }
sub desc { 'match position' }
sub check { 'ck_lfun' }
sub flags { 'stu%' }
sub args { 'S?' }



package PLXML::op_preinc;

our @ISA = ('PLXML::unop');

sub key { 'preinc' }
sub desc { 'preincrement (++)' }
sub check { 'ck_lfun' }
sub flags { 'dIs1' }
sub args { 'S' }


package PLXML::op_i_preinc;

our @ISA = ('PLXML::unop');

sub key { 'i_preinc' }
sub desc { 'integer preincrement (++)' }
sub check { 'ck_lfun' }
sub flags { 'dis1' }
sub args { 'S' }


package PLXML::op_predec;

our @ISA = ('PLXML::unop');

sub key { 'predec' }
sub desc { 'predecrement (--)' }
sub check { 'ck_lfun' }
sub flags { 'dIs1' }
sub args { 'S' }


package PLXML::op_i_predec;

our @ISA = ('PLXML::unop');

sub key { 'i_predec' }
sub desc { 'integer predecrement (--)' }
sub check { 'ck_lfun' }
sub flags { 'dis1' }
sub args { 'S' }


package PLXML::op_postinc;

our @ISA = ('PLXML::unop');

sub key { 'postinc' }
sub desc { 'postincrement (++)' }
sub check { 'ck_lfun' }
sub flags { 'dIst1' }
sub args { 'S' }


package PLXML::op_i_postinc;

our @ISA = ('PLXML::unop');

sub key { 'i_postinc' }
sub desc { 'integer postincrement (++)' }
sub check { 'ck_lfun' }
sub flags { 'disT1' }
sub args { 'S' }


package PLXML::op_postdec;

our @ISA = ('PLXML::unop');

sub key { 'postdec' }
sub desc { 'postdecrement (--)' }
sub check { 'ck_lfun' }
sub flags { 'dIst1' }
sub args { 'S' }


package PLXML::op_i_postdec;

our @ISA = ('PLXML::unop');

sub key { 'i_postdec' }
sub desc { 'integer postdecrement (--)' }
sub check { 'ck_lfun' }
sub flags { 'disT1' }
sub args { 'S' }



# Ordinary operators.

package PLXML::op_pow;

our @ISA = ('PLXML::binop');

sub key { 'pow' }
sub desc { 'exponentiation (**)' }
sub check { 'ck_null' }
sub flags { 'fsT2' }
sub args { 'S S' }



package PLXML::op_multiply;

our @ISA = ('PLXML::binop');

sub key { 'multiply' }
sub desc { 'multiplication (*)' }
sub check { 'ck_null' }
sub flags { 'IfsT2' }
sub args { 'S S' }


package PLXML::op_i_multiply;

our @ISA = ('PLXML::binop');

sub key { 'i_multiply' }
sub desc { 'integer multiplication (*)' }
sub check { 'ck_null' }
sub flags { 'ifsT2' }
sub args { 'S S' }


package PLXML::op_divide;

our @ISA = ('PLXML::binop');

sub key { 'divide' }
sub desc { 'division (/)' }
sub check { 'ck_null' }
sub flags { 'IfsT2' }
sub args { 'S S' }


package PLXML::op_i_divide;

our @ISA = ('PLXML::binop');

sub key { 'i_divide' }
sub desc { 'integer division (/)' }
sub check { 'ck_null' }
sub flags { 'ifsT2' }
sub args { 'S S' }


package PLXML::op_modulo;

our @ISA = ('PLXML::binop');

sub key { 'modulo' }
sub desc { 'modulus (%)' }
sub check { 'ck_null' }
sub flags { 'IifsT2' }
sub args { 'S S' }


package PLXML::op_i_modulo;

our @ISA = ('PLXML::binop');

sub key { 'i_modulo' }
sub desc { 'integer modulus (%)' }
sub check { 'ck_null' }
sub flags { 'ifsT2' }
sub args { 'S S' }


package PLXML::op_repeat;

our @ISA = ('PLXML::binop');

sub key { 'repeat' }
sub desc { 'repeat (x)' }
sub check { 'ck_repeat' }
sub flags { 'mt2' }
sub args { 'L S' }



package PLXML::op_add;

our @ISA = ('PLXML::binop');

sub key { 'add' }
sub desc { 'addition (+)' }
sub check { 'ck_null' }
sub flags { 'IfsT2' }
sub args { 'S S' }


package PLXML::op_i_add;

our @ISA = ('PLXML::binop');

sub key { 'i_add' }
sub desc { 'integer addition (+)' }
sub check { 'ck_null' }
sub flags { 'ifsT2' }
sub args { 'S S' }


package PLXML::op_subtract;

our @ISA = ('PLXML::binop');

sub key { 'subtract' }
sub desc { 'subtraction (-)' }
sub check { 'ck_null' }
sub flags { 'IfsT2' }
sub args { 'S S' }


package PLXML::op_i_subtract;

our @ISA = ('PLXML::binop');

sub key { 'i_subtract' }
sub desc { 'integer subtraction (-)' }
sub check { 'ck_null' }
sub flags { 'ifsT2' }
sub args { 'S S' }


package PLXML::op_concat;

our @ISA = ('PLXML::binop');

sub key { 'concat' }
sub desc { 'concatenation (.) or string' }
sub check { 'ck_concat' }
sub flags { 'fsT2' }
sub args { 'S S' }


package PLXML::op_stringify;

our @ISA = ('PLXML::listop');

sub key { 'stringify' }
sub desc { 'string' }
sub check { 'ck_fun' }
sub flags { 'fsT@' }
sub args { 'S' }



package PLXML::op_left_shift;

our @ISA = ('PLXML::binop');

sub key { 'left_shift' }
sub desc { 'left bitshift (<<)' }
sub check { 'ck_bitop' }
sub flags { 'fsT2' }
sub args { 'S S' }


package PLXML::op_right_shift;

our @ISA = ('PLXML::binop');

sub key { 'right_shift' }
sub desc { 'right bitshift (>>)' }
sub check { 'ck_bitop' }
sub flags { 'fsT2' }
sub args { 'S S' }



package PLXML::op_lt;

our @ISA = ('PLXML::binop');

sub key { 'lt' }
sub desc { 'numeric lt (<)' }
sub check { 'ck_null' }
sub flags { 'Iifs2' }
sub args { 'S S' }


package PLXML::op_i_lt;

our @ISA = ('PLXML::binop');

sub key { 'i_lt' }
sub desc { 'integer lt (<)' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_gt;

our @ISA = ('PLXML::binop');

sub key { 'gt' }
sub desc { 'numeric gt (>)' }
sub check { 'ck_null' }
sub flags { 'Iifs2' }
sub args { 'S S' }


package PLXML::op_i_gt;

our @ISA = ('PLXML::binop');

sub key { 'i_gt' }
sub desc { 'integer gt (>)' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_le;

our @ISA = ('PLXML::binop');

sub key { 'le' }
sub desc { 'numeric le (<=)' }
sub check { 'ck_null' }
sub flags { 'Iifs2' }
sub args { 'S S' }


package PLXML::op_i_le;

our @ISA = ('PLXML::binop');

sub key { 'i_le' }
sub desc { 'integer le (<=)' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_ge;

our @ISA = ('PLXML::binop');

sub key { 'ge' }
sub desc { 'numeric ge (>=)' }
sub check { 'ck_null' }
sub flags { 'Iifs2' }
sub args { 'S S' }


package PLXML::op_i_ge;

our @ISA = ('PLXML::binop');

sub key { 'i_ge' }
sub desc { 'integer ge (>=)' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_eq;

our @ISA = ('PLXML::binop');

sub key { 'eq' }
sub desc { 'numeric eq (==)' }
sub check { 'ck_null' }
sub flags { 'Iifs2' }
sub args { 'S S' }


package PLXML::op_i_eq;

our @ISA = ('PLXML::binop');

sub key { 'i_eq' }
sub desc { 'integer eq (==)' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_ne;

our @ISA = ('PLXML::binop');

sub key { 'ne' }
sub desc { 'numeric ne (!=)' }
sub check { 'ck_null' }
sub flags { 'Iifs2' }
sub args { 'S S' }


package PLXML::op_i_ne;

our @ISA = ('PLXML::binop');

sub key { 'i_ne' }
sub desc { 'integer ne (!=)' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_ncmp;

our @ISA = ('PLXML::binop');

sub key { 'ncmp' }
sub desc { 'numeric comparison (<=>)' }
sub check { 'ck_null' }
sub flags { 'Iifst2' }
sub args { 'S S' }


package PLXML::op_i_ncmp;

our @ISA = ('PLXML::binop');

sub key { 'i_ncmp' }
sub desc { 'integer comparison (<=>)' }
sub check { 'ck_null' }
sub flags { 'ifst2' }
sub args { 'S S' }



package PLXML::op_slt;

our @ISA = ('PLXML::binop');

sub key { 'slt' }
sub desc { 'string lt' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_sgt;

our @ISA = ('PLXML::binop');

sub key { 'sgt' }
sub desc { 'string gt' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_sle;

our @ISA = ('PLXML::binop');

sub key { 'sle' }
sub desc { 'string le' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_sge;

our @ISA = ('PLXML::binop');

sub key { 'sge' }
sub desc { 'string ge' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_seq;

our @ISA = ('PLXML::binop');

sub key { 'seq' }
sub desc { 'string eq' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_sne;

our @ISA = ('PLXML::binop');

sub key { 'sne' }
sub desc { 'string ne' }
sub check { 'ck_null' }
sub flags { 'ifs2' }
sub args { 'S S' }


package PLXML::op_scmp;

our @ISA = ('PLXML::binop');

sub key { 'scmp' }
sub desc { 'string comparison (cmp)' }
sub check { 'ck_null' }
sub flags { 'ifst2' }
sub args { 'S S' }



package PLXML::op_bit_and;

our @ISA = ('PLXML::binop');

sub key { 'bit_and' }
sub desc { 'bitwise and (&)' }
sub check { 'ck_bitop' }
sub flags { 'fst2' }
sub args { 'S S' }


package PLXML::op_bit_xor;

our @ISA = ('PLXML::binop');

sub key { 'bit_xor' }
sub desc { 'bitwise xor (^)' }
sub check { 'ck_bitop' }
sub flags { 'fst2' }
sub args { 'S S' }


package PLXML::op_bit_or;

our @ISA = ('PLXML::binop');

sub key { 'bit_or' }
sub desc { 'bitwise or (|)' }
sub check { 'ck_bitop' }
sub flags { 'fst2' }
sub args { 'S S' }



package PLXML::op_negate;

our @ISA = ('PLXML::unop');

sub key { 'negate' }
sub desc { 'negation (-)' }
sub check { 'ck_null' }
sub flags { 'Ifst1' }
sub args { 'S' }


package PLXML::op_i_negate;

our @ISA = ('PLXML::unop');

sub key { 'i_negate' }
sub desc { 'integer negation (-)' }
sub check { 'ck_null' }
sub flags { 'ifsT1' }
sub args { 'S' }


package PLXML::op_not;

our @ISA = ('PLXML::unop');

sub key { 'not' }
sub desc { 'not' }
sub check { 'ck_null' }
sub flags { 'ifs1' }
sub args { 'S' }


package PLXML::op_complement;

our @ISA = ('PLXML::unop');

sub key { 'complement' }
sub desc { '1\'s complement (~)' }
sub check { 'ck_bitop' }
sub flags { 'fst1' }
sub args { 'S' }



# High falutin' math.

package PLXML::op_atan2;

our @ISA = ('PLXML::listop');

sub key { 'atan2' }
sub desc { 'atan2' }
sub check { 'ck_fun' }
sub flags { 'fsT@' }
sub args { 'S S' }


package PLXML::op_sin;

our @ISA = ('PLXML::baseop_unop');

sub key { 'sin' }
sub desc { 'sin' }
sub check { 'ck_fun' }
sub flags { 'fsTu%' }
sub args { 'S?' }


package PLXML::op_cos;

our @ISA = ('PLXML::baseop_unop');

sub key { 'cos' }
sub desc { 'cos' }
sub check { 'ck_fun' }
sub flags { 'fsTu%' }
sub args { 'S?' }


package PLXML::op_rand;

our @ISA = ('PLXML::baseop_unop');

sub key { 'rand' }
sub desc { 'rand' }
sub check { 'ck_fun' }
sub flags { 'sT%' }
sub args { 'S?' }


package PLXML::op_srand;

our @ISA = ('PLXML::baseop_unop');

sub key { 'srand' }
sub desc { 'srand' }
sub check { 'ck_fun' }
sub flags { 's%' }
sub args { 'S?' }


package PLXML::op_exp;

our @ISA = ('PLXML::baseop_unop');

sub key { 'exp' }
sub desc { 'exp' }
sub check { 'ck_fun' }
sub flags { 'fsTu%' }
sub args { 'S?' }


package PLXML::op_log;

our @ISA = ('PLXML::baseop_unop');

sub key { 'log' }
sub desc { 'log' }
sub check { 'ck_fun' }
sub flags { 'fsTu%' }
sub args { 'S?' }


package PLXML::op_sqrt;

our @ISA = ('PLXML::baseop_unop');

sub key { 'sqrt' }
sub desc { 'sqrt' }
sub check { 'ck_fun' }
sub flags { 'fsTu%' }
sub args { 'S?' }



# Lowbrow math.

package PLXML::op_int;

our @ISA = ('PLXML::baseop_unop');

sub key { 'int' }
sub desc { 'int' }
sub check { 'ck_fun' }
sub flags { 'fsTu%' }
sub args { 'S?' }


package PLXML::op_hex;

our @ISA = ('PLXML::baseop_unop');

sub key { 'hex' }
sub desc { 'hex' }
sub check { 'ck_fun' }
sub flags { 'fsTu%' }
sub args { 'S?' }


package PLXML::op_oct;

our @ISA = ('PLXML::baseop_unop');

sub key { 'oct' }
sub desc { 'oct' }
sub check { 'ck_fun' }
sub flags { 'fsTu%' }
sub args { 'S?' }


package PLXML::op_abs;

our @ISA = ('PLXML::baseop_unop');

sub key { 'abs' }
sub desc { 'abs' }
sub check { 'ck_fun' }
sub flags { 'fsTu%' }
sub args { 'S?' }



# String stuff.

package PLXML::op_length;

our @ISA = ('PLXML::baseop_unop');

sub key { 'length' }
sub desc { 'length' }
sub check { 'ck_lengthconst' }
sub flags { 'isTu%' }
sub args { 'S?' }


package PLXML::op_substr;

our @ISA = ('PLXML::listop');

sub key { 'substr' }
sub desc { 'substr' }
sub check { 'ck_substr' }
sub flags { 'st@' }
sub args { 'S S S? S?' }


package PLXML::op_vec;

our @ISA = ('PLXML::listop');

sub key { 'vec' }
sub desc { 'vec' }
sub check { 'ck_fun' }
sub flags { 'ist@' }
sub args { 'S S S' }



package PLXML::op_index;

our @ISA = ('PLXML::listop');

sub key { 'index' }
sub desc { 'index' }
sub check { 'ck_index' }
sub flags { 'isT@' }
sub args { 'S S S?' }


package PLXML::op_rindex;

our @ISA = ('PLXML::listop');

sub key { 'rindex' }
sub desc { 'rindex' }
sub check { 'ck_index' }
sub flags { 'isT@' }
sub args { 'S S S?' }



package PLXML::op_sprintf;

our @ISA = ('PLXML::listop');

sub key { 'sprintf' }
sub desc { 'sprintf' }
sub check { 'ck_fun' }
sub flags { 'mfst@' }
sub args { 'S L' }


package PLXML::op_formline;

our @ISA = ('PLXML::listop');

sub key { 'formline' }
sub desc { 'formline' }
sub check { 'ck_fun' }
sub flags { 'ms@' }
sub args { 'S L' }


package PLXML::op_ord;

our @ISA = ('PLXML::baseop_unop');

sub key { 'ord' }
sub desc { 'ord' }
sub check { 'ck_fun' }
sub flags { 'ifsTu%' }
sub args { 'S?' }


package PLXML::op_chr;

our @ISA = ('PLXML::baseop_unop');

sub key { 'chr' }
sub desc { 'chr' }
sub check { 'ck_fun' }
sub flags { 'fsTu%' }
sub args { 'S?' }


package PLXML::op_crypt;

our @ISA = ('PLXML::listop');

sub key { 'crypt' }
sub desc { 'crypt' }
sub check { 'ck_fun' }
sub flags { 'fsT@' }
sub args { 'S S' }


package PLXML::op_ucfirst;

our @ISA = ('PLXML::baseop_unop');

sub key { 'ucfirst' }
sub desc { 'ucfirst' }
sub check { 'ck_fun' }
sub flags { 'fstu%' }
sub args { 'S?' }


package PLXML::op_lcfirst;

our @ISA = ('PLXML::baseop_unop');

sub key { 'lcfirst' }
sub desc { 'lcfirst' }
sub check { 'ck_fun' }
sub flags { 'fstu%' }
sub args { 'S?' }


package PLXML::op_uc;

our @ISA = ('PLXML::baseop_unop');

sub key { 'uc' }
sub desc { 'uc' }
sub check { 'ck_fun' }
sub flags { 'fstu%' }
sub args { 'S?' }


package PLXML::op_lc;

our @ISA = ('PLXML::baseop_unop');

sub key { 'lc' }
sub desc { 'lc' }
sub check { 'ck_fun' }
sub flags { 'fstu%' }
sub args { 'S?' }


package PLXML::op_quotemeta;

our @ISA = ('PLXML::baseop_unop');

sub key { 'quotemeta' }
sub desc { 'quotemeta' }
sub check { 'ck_fun' }
sub flags { 'fstu%' }
sub args { 'S?' }



# Arrays.

package PLXML::op_rv2av;

our @ISA = ('PLXML::unop');

sub key { 'rv2av' }
sub desc { 'array dereference' }
sub check { 'ck_rvconst' }
sub flags { 'dt1' }
sub args { '' }


package PLXML::op_aelemfast;

our @ISA = ('PLXML::padop_svop');

sub key { 'aelemfast' }
sub desc { 'constant array element' }
sub check { 'ck_null' }
sub flags { 's$' }
sub args { 'A S' }


package PLXML::op_aelem;

our @ISA = ('PLXML::binop');

sub key { 'aelem' }
sub desc { 'array element' }
sub check { 'ck_null' }
sub flags { 's2' }
sub args { 'A S' }


package PLXML::op_aslice;

our @ISA = ('PLXML::listop');

sub key { 'aslice' }
sub desc { 'array slice' }
sub check { 'ck_null' }
sub flags { 'm@' }
sub args { 'A L' }



# Hashes.

package PLXML::op_each;

our @ISA = ('PLXML::baseop_unop');

sub key { 'each' }
sub desc { 'each' }
sub check { 'ck_fun' }
sub flags { '%' }
sub args { 'H' }


package PLXML::op_values;

our @ISA = ('PLXML::baseop_unop');

sub key { 'values' }
sub desc { 'values' }
sub check { 'ck_fun' }
sub flags { 't%' }
sub args { 'H' }


package PLXML::op_keys;

our @ISA = ('PLXML::baseop_unop');

sub key { 'keys' }
sub desc { 'keys' }
sub check { 'ck_fun' }
sub flags { 't%' }
sub args { 'H' }


package PLXML::op_delete;

our @ISA = ('PLXML::baseop_unop');

sub key { 'delete' }
sub desc { 'delete' }
sub check { 'ck_delete' }
sub flags { '%' }
sub args { 'S' }


package PLXML::op_exists;

our @ISA = ('PLXML::baseop_unop');

sub key { 'exists' }
sub desc { 'exists' }
sub check { 'ck_exists' }
sub flags { 'is%' }
sub args { 'S' }


package PLXML::op_rv2hv;

our @ISA = ('PLXML::unop');

sub key { 'rv2hv' }
sub desc { 'hash dereference' }
sub check { 'ck_rvconst' }
sub flags { 'dt1' }
sub args { '' }


package PLXML::op_helem;

our @ISA = ('PLXML::listop');

sub key { 'helem' }
sub desc { 'hash element' }
sub check { 'ck_null' }
sub flags { 's2@' }
sub args { 'H S' }


package PLXML::op_hslice;

our @ISA = ('PLXML::listop');

sub key { 'hslice' }
sub desc { 'hash slice' }
sub check { 'ck_null' }
sub flags { 'm@' }
sub args { 'H L' }



# Explosives and implosives.

package PLXML::op_unpack;

our @ISA = ('PLXML::listop');

sub key { 'unpack' }
sub desc { 'unpack' }
sub check { 'ck_unpack' }
sub flags { '@' }
sub args { 'S S?' }


package PLXML::op_pack;

our @ISA = ('PLXML::listop');

sub key { 'pack' }
sub desc { 'pack' }
sub check { 'ck_fun' }
sub flags { 'mst@' }
sub args { 'S L' }


package PLXML::op_split;

our @ISA = ('PLXML::listop');

sub key { 'split' }
sub desc { 'split' }
sub check { 'ck_split' }
sub flags { 't@' }
sub args { 'S S S' }


package PLXML::op_join;

our @ISA = ('PLXML::listop');

sub key { 'join' }
sub desc { 'join or string' }
sub check { 'ck_join' }
sub flags { 'mst@' }
sub args { 'S L' }



# List operators.

package PLXML::op_list;

our @ISA = ('PLXML::listop');

sub key { 'list' }
sub desc { 'list' }
sub check { 'ck_null' }
sub flags { 'm@' }
sub args { 'L' }


package PLXML::op_lslice;

our @ISA = ('PLXML::binop');

sub key { 'lslice' }
sub desc { 'list slice' }
sub check { 'ck_null' }
sub flags { '2' }
sub args { 'H L L' }


package PLXML::op_anonlist;

our @ISA = ('PLXML::listop');

sub key { 'anonlist' }
sub desc { 'anonymous list ([])' }
sub check { 'ck_fun' }
sub flags { 'ms@' }
sub args { 'L' }


package PLXML::op_anonhash;

our @ISA = ('PLXML::listop');

sub key { 'anonhash' }
sub desc { 'anonymous hash ({})' }
sub check { 'ck_fun' }
sub flags { 'ms@' }
sub args { 'L' }



package PLXML::op_splice;

our @ISA = ('PLXML::listop');

sub key { 'splice' }
sub desc { 'splice' }
sub check { 'ck_fun' }
sub flags { 'm@' }
sub args { 'A S? S? L' }


package PLXML::op_push;

our @ISA = ('PLXML::listop');

sub key { 'push' }
sub desc { 'push' }
sub check { 'ck_fun' }
sub flags { 'imsT@' }
sub args { 'A L' }


package PLXML::op_pop;

our @ISA = ('PLXML::baseop_unop');

sub key { 'pop' }
sub desc { 'pop' }
sub check { 'ck_shift' }
sub flags { 's%' }
sub args { 'A?' }


package PLXML::op_shift;

our @ISA = ('PLXML::baseop_unop');

sub key { 'shift' }
sub desc { 'shift' }
sub check { 'ck_shift' }
sub flags { 's%' }
sub args { 'A?' }


package PLXML::op_unshift;

our @ISA = ('PLXML::listop');

sub key { 'unshift' }
sub desc { 'unshift' }
sub check { 'ck_fun' }
sub flags { 'imsT@' }
sub args { 'A L' }


package PLXML::op_sort;

our @ISA = ('PLXML::listop');

sub key { 'sort' }
sub desc { 'sort' }
sub check { 'ck_sort' }
sub flags { 'm@' }
sub args { 'C? L' }


package PLXML::op_reverse;

our @ISA = ('PLXML::listop');

sub key { 'reverse' }
sub desc { 'reverse' }
sub check { 'ck_fun' }
sub flags { 'mt@' }
sub args { 'L' }



package PLXML::op_grepstart;

our @ISA = ('PLXML::listop');

sub key { 'grepstart' }
sub desc { 'grep' }
sub check { 'ck_grep' }
sub flags { 'dm@' }
sub args { 'C L' }


package PLXML::op_grepwhile;

our @ISA = ('PLXML::logop');

sub key { 'grepwhile' }
sub desc { 'grep iterator' }
sub check { 'ck_null' }
sub flags { 'dt|' }
sub args { '' }



package PLXML::op_mapstart;

our @ISA = ('PLXML::listop');

sub key { 'mapstart' }
sub desc { 'map' }
sub check { 'ck_grep' }
sub flags { 'dm@' }
sub args { 'C L' }


package PLXML::op_mapwhile;

our @ISA = ('PLXML::logop');

sub key { 'mapwhile' }
sub desc { 'map iterator' }
sub check { 'ck_null' }
sub flags { 'dt|' }
sub args { '' }



# Range stuff.

package PLXML::op_range;

our @ISA = ('PLXML::logop');

sub key { 'range' }
sub desc { 'flipflop' }
sub check { 'ck_null' }
sub flags { '|' }
sub args { 'S S' }


package PLXML::op_flip;

our @ISA = ('PLXML::unop');

sub key { 'flip' }
sub desc { 'range (or flip)' }
sub check { 'ck_null' }
sub flags { '1' }
sub args { 'S S' }


package PLXML::op_flop;

our @ISA = ('PLXML::unop');

sub key { 'flop' }
sub desc { 'range (or flop)' }
sub check { 'ck_null' }
sub flags { '1' }
sub args { '' }



# Control.

package PLXML::op_and;

our @ISA = ('PLXML::logop');

sub key { 'and' }
sub desc { 'logical and (&&)' }
sub check { 'ck_null' }
sub flags { '|' }
sub args { '' }


package PLXML::op_or;

our @ISA = ('PLXML::logop');

sub key { 'or' }
sub desc { 'logical or (||)' }
sub check { 'ck_null' }
sub flags { '|' }
sub args { '' }


package PLXML::op_xor;

our @ISA = ('PLXML::binop');

sub key { 'xor' }
sub desc { 'logical xor' }
sub check { 'ck_null' }
sub flags { 'fs2' }
sub args { 'S S	' }


package PLXML::op_cond_expr;

our @ISA = ('PLXML::logop');

sub key { 'cond_expr' }
sub desc { 'conditional expression' }
sub check { 'ck_null' }
sub flags { 'd|' }
sub args { '' }


package PLXML::op_andassign;

our @ISA = ('PLXML::logop');

sub key { 'andassign' }
sub desc { 'logical and assignment (&&=)' }
sub check { 'ck_null' }
sub flags { 's|' }
sub args { '' }


package PLXML::op_orassign;

our @ISA = ('PLXML::logop');

sub key { 'orassign' }
sub desc { 'logical or assignment (||=)' }
sub check { 'ck_null' }
sub flags { 's|' }
sub args { '' }



package PLXML::op_method;

our @ISA = ('PLXML::unop');

sub key { 'method' }
sub desc { 'method lookup' }
sub check { 'ck_method' }
sub flags { 'd1' }
sub args { '' }


package PLXML::op_entersub;

our @ISA = ('PLXML::unop');

sub key { 'entersub' }
sub desc { 'subroutine entry' }
sub check { 'ck_subr' }
sub flags { 'dmt1' }
sub args { 'L' }


package PLXML::op_leavesub;

our @ISA = ('PLXML::unop');

sub key { 'leavesub' }
sub desc { 'subroutine exit' }
sub check { 'ck_null' }
sub flags { '1' }
sub args { '' }


package PLXML::op_leavesublv;

our @ISA = ('PLXML::unop');

sub key { 'leavesublv' }
sub desc { 'lvalue subroutine return' }
sub check { 'ck_null' }
sub flags { '1' }
sub args { '' }


package PLXML::op_caller;

our @ISA = ('PLXML::baseop_unop');

sub key { 'caller' }
sub desc { 'caller' }
sub check { 'ck_fun' }
sub flags { 't%' }
sub args { 'S?' }


package PLXML::op_warn;

our @ISA = ('PLXML::listop');

sub key { 'warn' }
sub desc { 'warn' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'L' }


package PLXML::op_die;

our @ISA = ('PLXML::listop');

sub key { 'die' }
sub desc { 'die' }
sub check { 'ck_die' }
sub flags { 'dimst@' }
sub args { 'L' }


package PLXML::op_reset;

our @ISA = ('PLXML::baseop_unop');

sub key { 'reset' }
sub desc { 'symbol reset' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'S?' }



package PLXML::op_lineseq;

our @ISA = ('PLXML::listop');

sub key { 'lineseq' }
sub desc { 'line sequence' }
sub check { 'ck_null' }
sub flags { '@' }
sub args { '' }


package PLXML::op_nextstate;

our @ISA = ('PLXML::cop');

sub key { 'nextstate' }
sub desc { 'next statement' }
sub check { 'ck_null' }
sub flags { 's;' }
sub args { '' }


package PLXML::op_dbstate;

our @ISA = ('PLXML::cop');

sub key { 'dbstate' }
sub desc { 'debug next statement' }
sub check { 'ck_null' }
sub flags { 's;' }
sub args { '' }


package PLXML::op_unstack;

our @ISA = ('PLXML::baseop');

sub key { 'unstack' }
sub desc { 'iteration finalizer' }
sub check { 'ck_null' }
sub flags { 's0' }
sub args { '' }


package PLXML::op_enter;

our @ISA = ('PLXML::baseop');

sub key { 'enter' }
sub desc { 'block entry' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_leave;

our @ISA = ('PLXML::listop');

sub key { 'leave' }
sub desc { 'block exit' }
sub check { 'ck_null' }
sub flags { '@' }
sub args { '' }


package PLXML::op_scope;

our @ISA = ('PLXML::listop');

sub key { 'scope' }
sub desc { 'block' }
sub check { 'ck_null' }
sub flags { '@' }
sub args { '' }


package PLXML::op_enteriter;

our @ISA = ('PLXML::loop');

sub key { 'enteriter' }
sub desc { 'foreach loop entry' }
sub check { 'ck_null' }
sub flags { 'd{' }
sub args { '' }


package PLXML::op_iter;

our @ISA = ('PLXML::baseop');

sub key { 'iter' }
sub desc { 'foreach loop iterator' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_enterloop;

our @ISA = ('PLXML::loop');

sub key { 'enterloop' }
sub desc { 'loop entry' }
sub check { 'ck_null' }
sub flags { 'd{' }
sub args { '' }


package PLXML::op_leaveloop;

our @ISA = ('PLXML::binop');

sub key { 'leaveloop' }
sub desc { 'loop exit' }
sub check { 'ck_null' }
sub flags { '2' }
sub args { '' }


package PLXML::op_return;

our @ISA = ('PLXML::listop');

sub key { 'return' }
sub desc { 'return' }
sub check { 'ck_return' }
sub flags { 'dm@' }
sub args { 'L' }


package PLXML::op_last;

our @ISA = ('PLXML::loopexop');

sub key { 'last' }
sub desc { 'last' }
sub check { 'ck_null' }
sub flags { 'ds}' }
sub args { '' }


package PLXML::op_next;

our @ISA = ('PLXML::loopexop');

sub key { 'next' }
sub desc { 'next' }
sub check { 'ck_null' }
sub flags { 'ds}' }
sub args { '' }


package PLXML::op_redo;

our @ISA = ('PLXML::loopexop');

sub key { 'redo' }
sub desc { 'redo' }
sub check { 'ck_null' }
sub flags { 'ds}' }
sub args { '' }


package PLXML::op_dump;

our @ISA = ('PLXML::loopexop');

sub key { 'dump' }
sub desc { 'dump' }
sub check { 'ck_null' }
sub flags { 'ds}' }
sub args { '' }


package PLXML::op_goto;

our @ISA = ('PLXML::loopexop');

sub key { 'goto' }
sub desc { 'goto' }
sub check { 'ck_null' }
sub flags { 'ds}' }
sub args { '' }


package PLXML::op_exit;

our @ISA = ('PLXML::baseop_unop');

sub key { 'exit' }
sub desc { 'exit' }
sub check { 'ck_exit' }
sub flags { 'ds%' }
sub args { 'S?' }


# continued below

#nswitch	numeric switch		ck_null		d	
#cswitch	character switch	ck_null		d	

# I/O.

package PLXML::op_open;

our @ISA = ('PLXML::listop');

sub key { 'open' }
sub desc { 'open' }
sub check { 'ck_open' }
sub flags { 'ismt@' }
sub args { 'F S? L' }


package PLXML::op_close;

our @ISA = ('PLXML::baseop_unop');

sub key { 'close' }
sub desc { 'close' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'F?' }


package PLXML::op_pipe_op;

our @ISA = ('PLXML::listop');

sub key { 'pipe_op' }
sub desc { 'pipe' }
sub check { 'ck_fun' }
sub flags { 'is@' }
sub args { 'F F' }



package PLXML::op_fileno;

our @ISA = ('PLXML::baseop_unop');

sub key { 'fileno' }
sub desc { 'fileno' }
sub check { 'ck_fun' }
sub flags { 'ist%' }
sub args { 'F' }


package PLXML::op_umask;

our @ISA = ('PLXML::baseop_unop');

sub key { 'umask' }
sub desc { 'umask' }
sub check { 'ck_fun' }
sub flags { 'ist%' }
sub args { 'S?' }


package PLXML::op_binmode;

our @ISA = ('PLXML::listop');

sub key { 'binmode' }
sub desc { 'binmode' }
sub check { 'ck_fun' }
sub flags { 's@' }
sub args { 'F S?' }



package PLXML::op_tie;

our @ISA = ('PLXML::listop');

sub key { 'tie' }
sub desc { 'tie' }
sub check { 'ck_fun' }
sub flags { 'idms@' }
sub args { 'R S L' }


package PLXML::op_untie;

our @ISA = ('PLXML::baseop_unop');

sub key { 'untie' }
sub desc { 'untie' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'R' }


package PLXML::op_tied;

our @ISA = ('PLXML::baseop_unop');

sub key { 'tied' }
sub desc { 'tied' }
sub check { 'ck_fun' }
sub flags { 's%' }
sub args { 'R' }


package PLXML::op_dbmopen;

our @ISA = ('PLXML::listop');

sub key { 'dbmopen' }
sub desc { 'dbmopen' }
sub check { 'ck_fun' }
sub flags { 'is@' }
sub args { 'H S S' }


package PLXML::op_dbmclose;

our @ISA = ('PLXML::baseop_unop');

sub key { 'dbmclose' }
sub desc { 'dbmclose' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'H' }



package PLXML::op_sselect;

our @ISA = ('PLXML::listop');

sub key { 'sselect' }
sub desc { 'select system call' }
sub check { 'ck_select' }
sub flags { 't@' }
sub args { 'S S S S' }


package PLXML::op_select;

our @ISA = ('PLXML::listop');

sub key { 'select' }
sub desc { 'select' }
sub check { 'ck_select' }
sub flags { 'st@' }
sub args { 'F?' }



package PLXML::op_getc;

our @ISA = ('PLXML::baseop_unop');

sub key { 'getc' }
sub desc { 'getc' }
sub check { 'ck_eof' }
sub flags { 'st%' }
sub args { 'F?' }


package PLXML::op_read;

our @ISA = ('PLXML::listop');

sub key { 'read' }
sub desc { 'read' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'F R S S?' }


package PLXML::op_enterwrite;

our @ISA = ('PLXML::baseop_unop');

sub key { 'enterwrite' }
sub desc { 'write' }
sub check { 'ck_fun' }
sub flags { 'dis%' }
sub args { 'F?' }


package PLXML::op_leavewrite;

our @ISA = ('PLXML::unop');

sub key { 'leavewrite' }
sub desc { 'write exit' }
sub check { 'ck_null' }
sub flags { '1' }
sub args { '' }



package PLXML::op_prtf;

our @ISA = ('PLXML::listop');

sub key { 'prtf' }
sub desc { 'printf' }
sub check { 'ck_listiob' }
sub flags { 'ims@' }
sub args { 'F? L' }


package PLXML::op_print;

our @ISA = ('PLXML::listop');

sub key { 'print' }
sub desc { 'print' }
sub check { 'ck_listiob' }
sub flags { 'ims@' }
sub args { 'F? L' }


package PLXML::op_say;

our @ISA = ('PLXML::listop');

sub key { 'say' }
sub desc { 'say' }
sub check { 'ck_listiob' }
sub flags { 'ims@' }
sub args { 'F? L' }


package PLXML::op_sysopen;

our @ISA = ('PLXML::listop');

sub key { 'sysopen' }
sub desc { 'sysopen' }
sub check { 'ck_fun' }
sub flags { 's@' }
sub args { 'F S S S?' }


package PLXML::op_sysseek;

our @ISA = ('PLXML::listop');

sub key { 'sysseek' }
sub desc { 'sysseek' }
sub check { 'ck_fun' }
sub flags { 's@' }
sub args { 'F S S' }


package PLXML::op_sysread;

our @ISA = ('PLXML::listop');

sub key { 'sysread' }
sub desc { 'sysread' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'F R S S?' }


package PLXML::op_syswrite;

our @ISA = ('PLXML::listop');

sub key { 'syswrite' }
sub desc { 'syswrite' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'F S S? S?' }



package PLXML::op_send;

our @ISA = ('PLXML::listop');

sub key { 'send' }
sub desc { 'send' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'Fs S S S?' }


package PLXML::op_recv;

our @ISA = ('PLXML::listop');

sub key { 'recv' }
sub desc { 'recv' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'Fs R S S' }



package PLXML::op_eof;

our @ISA = ('PLXML::baseop_unop');

sub key { 'eof' }
sub desc { 'eof' }
sub check { 'ck_eof' }
sub flags { 'is%' }
sub args { 'F?' }


package PLXML::op_tell;

our @ISA = ('PLXML::baseop_unop');

sub key { 'tell' }
sub desc { 'tell' }
sub check { 'ck_fun' }
sub flags { 'st%' }
sub args { 'F?' }


package PLXML::op_seek;

our @ISA = ('PLXML::listop');

sub key { 'seek' }
sub desc { 'seek' }
sub check { 'ck_fun' }
sub flags { 's@' }
sub args { 'F S S' }


# truncate really behaves as if it had both "S S" and "F S"
package PLXML::op_truncate;

our @ISA = ('PLXML::listop');

sub key { 'truncate' }
sub desc { 'truncate' }
sub check { 'ck_trunc' }
sub flags { 'is@' }
sub args { 'S S' }



package PLXML::op_fcntl;

our @ISA = ('PLXML::listop');

sub key { 'fcntl' }
sub desc { 'fcntl' }
sub check { 'ck_fun' }
sub flags { 'st@' }
sub args { 'F S S' }


package PLXML::op_ioctl;

our @ISA = ('PLXML::listop');

sub key { 'ioctl' }
sub desc { 'ioctl' }
sub check { 'ck_fun' }
sub flags { 'st@' }
sub args { 'F S S' }


package PLXML::op_flock;

our @ISA = ('PLXML::listop');

sub key { 'flock' }
sub desc { 'flock' }
sub check { 'ck_fun' }
sub flags { 'isT@' }
sub args { 'F S' }



# Sockets.

package PLXML::op_socket;

our @ISA = ('PLXML::listop');

sub key { 'socket' }
sub desc { 'socket' }
sub check { 'ck_fun' }
sub flags { 'is@' }
sub args { 'Fs S S S' }


package PLXML::op_sockpair;

our @ISA = ('PLXML::listop');

sub key { 'sockpair' }
sub desc { 'socketpair' }
sub check { 'ck_fun' }
sub flags { 'is@' }
sub args { 'Fs Fs S S S' }



package PLXML::op_bind;

our @ISA = ('PLXML::listop');

sub key { 'bind' }
sub desc { 'bind' }
sub check { 'ck_fun' }
sub flags { 'is@' }
sub args { 'Fs S' }


package PLXML::op_connect;

our @ISA = ('PLXML::listop');

sub key { 'connect' }
sub desc { 'connect' }
sub check { 'ck_fun' }
sub flags { 'is@' }
sub args { 'Fs S' }


package PLXML::op_listen;

our @ISA = ('PLXML::listop');

sub key { 'listen' }
sub desc { 'listen' }
sub check { 'ck_fun' }
sub flags { 'is@' }
sub args { 'Fs S' }


package PLXML::op_accept;

our @ISA = ('PLXML::listop');

sub key { 'accept' }
sub desc { 'accept' }
sub check { 'ck_fun' }
sub flags { 'ist@' }
sub args { 'Fs Fs' }


package PLXML::op_shutdown;

our @ISA = ('PLXML::listop');

sub key { 'shutdown' }
sub desc { 'shutdown' }
sub check { 'ck_fun' }
sub flags { 'ist@' }
sub args { 'Fs S' }



package PLXML::op_gsockopt;

our @ISA = ('PLXML::listop');

sub key { 'gsockopt' }
sub desc { 'getsockopt' }
sub check { 'ck_fun' }
sub flags { 'is@' }
sub args { 'Fs S S' }


package PLXML::op_ssockopt;

our @ISA = ('PLXML::listop');

sub key { 'ssockopt' }
sub desc { 'setsockopt' }
sub check { 'ck_fun' }
sub flags { 'is@' }
sub args { 'Fs S S S' }



package PLXML::op_getsockname;

our @ISA = ('PLXML::baseop_unop');

sub key { 'getsockname' }
sub desc { 'getsockname' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'Fs' }


package PLXML::op_getpeername;

our @ISA = ('PLXML::baseop_unop');

sub key { 'getpeername' }
sub desc { 'getpeername' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'Fs' }



# Stat calls.

package PLXML::op_lstat;

our @ISA = ('PLXML::filestatop');

sub key { 'lstat' }
sub desc { 'lstat' }
sub check { 'ck_ftst' }
sub flags { 'u-' }
sub args { 'F' }


package PLXML::op_stat;

our @ISA = ('PLXML::filestatop');

sub key { 'stat' }
sub desc { 'stat' }
sub check { 'ck_ftst' }
sub flags { 'u-' }
sub args { 'F' }


package PLXML::op_ftrread;

our @ISA = ('PLXML::filestatop');

sub key { 'ftrread' }
sub desc { '-R' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftrwrite;

our @ISA = ('PLXML::filestatop');

sub key { 'ftrwrite' }
sub desc { '-W' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftrexec;

our @ISA = ('PLXML::filestatop');

sub key { 'ftrexec' }
sub desc { '-X' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_fteread;

our @ISA = ('PLXML::filestatop');

sub key { 'fteread' }
sub desc { '-r' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftewrite;

our @ISA = ('PLXML::filestatop');

sub key { 'ftewrite' }
sub desc { '-w' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_fteexec;

our @ISA = ('PLXML::filestatop');

sub key { 'fteexec' }
sub desc { '-x' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftis;

our @ISA = ('PLXML::filestatop');

sub key { 'ftis' }
sub desc { '-e' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_fteowned;

our @ISA = ('PLXML::filestatop');

sub key { 'fteowned' }
sub desc { '-O' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftrowned;

our @ISA = ('PLXML::filestatop');

sub key { 'ftrowned' }
sub desc { '-o' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftzero;

our @ISA = ('PLXML::filestatop');

sub key { 'ftzero' }
sub desc { '-z' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftsize;

our @ISA = ('PLXML::filestatop');

sub key { 'ftsize' }
sub desc { '-s' }
sub check { 'ck_ftst' }
sub flags { 'istu-' }
sub args { 'F-' }


package PLXML::op_ftmtime;

our @ISA = ('PLXML::filestatop');

sub key { 'ftmtime' }
sub desc { '-M' }
sub check { 'ck_ftst' }
sub flags { 'stu-' }
sub args { 'F-' }


package PLXML::op_ftatime;

our @ISA = ('PLXML::filestatop');

sub key { 'ftatime' }
sub desc { '-A' }
sub check { 'ck_ftst' }
sub flags { 'stu-' }
sub args { 'F-' }


package PLXML::op_ftctime;

our @ISA = ('PLXML::filestatop');

sub key { 'ftctime' }
sub desc { '-C' }
sub check { 'ck_ftst' }
sub flags { 'stu-' }
sub args { 'F-' }


package PLXML::op_ftsock;

our @ISA = ('PLXML::filestatop');

sub key { 'ftsock' }
sub desc { '-S' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftchr;

our @ISA = ('PLXML::filestatop');

sub key { 'ftchr' }
sub desc { '-c' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftblk;

our @ISA = ('PLXML::filestatop');

sub key { 'ftblk' }
sub desc { '-b' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftfile;

our @ISA = ('PLXML::filestatop');

sub key { 'ftfile' }
sub desc { '-f' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftdir;

our @ISA = ('PLXML::filestatop');

sub key { 'ftdir' }
sub desc { '-d' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftpipe;

our @ISA = ('PLXML::filestatop');

sub key { 'ftpipe' }
sub desc { '-p' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftlink;

our @ISA = ('PLXML::filestatop');

sub key { 'ftlink' }
sub desc { '-l' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftsuid;

our @ISA = ('PLXML::filestatop');

sub key { 'ftsuid' }
sub desc { '-u' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftsgid;

our @ISA = ('PLXML::filestatop');

sub key { 'ftsgid' }
sub desc { '-g' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftsvtx;

our @ISA = ('PLXML::filestatop');

sub key { 'ftsvtx' }
sub desc { '-k' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_fttty;

our @ISA = ('PLXML::filestatop');

sub key { 'fttty' }
sub desc { '-t' }
sub check { 'ck_ftst' }
sub flags { 'is-' }
sub args { 'F-' }


package PLXML::op_fttext;

our @ISA = ('PLXML::filestatop');

sub key { 'fttext' }
sub desc { '-T' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }


package PLXML::op_ftbinary;

our @ISA = ('PLXML::filestatop');

sub key { 'ftbinary' }
sub desc { '-B' }
sub check { 'ck_ftst' }
sub flags { 'isu-' }
sub args { 'F-' }



# File calls.

package PLXML::op_chdir;

our @ISA = ('PLXML::baseop_unop');

sub key { 'chdir' }
sub desc { 'chdir' }
sub check { 'ck_fun' }
sub flags { 'isT%' }
sub args { 'S?' }


package PLXML::op_chown;

our @ISA = ('PLXML::listop');

sub key { 'chown' }
sub desc { 'chown' }
sub check { 'ck_fun' }
sub flags { 'imsT@' }
sub args { 'L' }


package PLXML::op_chroot;

our @ISA = ('PLXML::baseop_unop');

sub key { 'chroot' }
sub desc { 'chroot' }
sub check { 'ck_fun' }
sub flags { 'isTu%' }
sub args { 'S?' }


package PLXML::op_unlink;

our @ISA = ('PLXML::listop');

sub key { 'unlink' }
sub desc { 'unlink' }
sub check { 'ck_fun' }
sub flags { 'imsTu@' }
sub args { 'L' }


package PLXML::op_chmod;

our @ISA = ('PLXML::listop');

sub key { 'chmod' }
sub desc { 'chmod' }
sub check { 'ck_fun' }
sub flags { 'imsT@' }
sub args { 'L' }


package PLXML::op_utime;

our @ISA = ('PLXML::listop');

sub key { 'utime' }
sub desc { 'utime' }
sub check { 'ck_fun' }
sub flags { 'imsT@' }
sub args { 'L' }


package PLXML::op_rename;

our @ISA = ('PLXML::listop');

sub key { 'rename' }
sub desc { 'rename' }
sub check { 'ck_fun' }
sub flags { 'isT@' }
sub args { 'S S' }


package PLXML::op_link;

our @ISA = ('PLXML::listop');

sub key { 'link' }
sub desc { 'link' }
sub check { 'ck_fun' }
sub flags { 'isT@' }
sub args { 'S S' }


package PLXML::op_symlink;

our @ISA = ('PLXML::listop');

sub key { 'symlink' }
sub desc { 'symlink' }
sub check { 'ck_fun' }
sub flags { 'isT@' }
sub args { 'S S' }


package PLXML::op_readlink;

our @ISA = ('PLXML::baseop_unop');

sub key { 'readlink' }
sub desc { 'readlink' }
sub check { 'ck_fun' }
sub flags { 'stu%' }
sub args { 'S?' }


package PLXML::op_mkdir;

our @ISA = ('PLXML::listop');

sub key { 'mkdir' }
sub desc { 'mkdir' }
sub check { 'ck_fun' }
sub flags { 'isT@' }
sub args { 'S S?' }


package PLXML::op_rmdir;

our @ISA = ('PLXML::baseop_unop');

sub key { 'rmdir' }
sub desc { 'rmdir' }
sub check { 'ck_fun' }
sub flags { 'isTu%' }
sub args { 'S?' }



# Directory calls.

package PLXML::op_open_dir;

our @ISA = ('PLXML::listop');

sub key { 'open_dir' }
sub desc { 'opendir' }
sub check { 'ck_fun' }
sub flags { 'is@' }
sub args { 'F S' }


package PLXML::op_readdir;

our @ISA = ('PLXML::baseop_unop');

sub key { 'readdir' }
sub desc { 'readdir' }
sub check { 'ck_fun' }
sub flags { '%' }
sub args { 'F' }


package PLXML::op_telldir;

our @ISA = ('PLXML::baseop_unop');

sub key { 'telldir' }
sub desc { 'telldir' }
sub check { 'ck_fun' }
sub flags { 'st%' }
sub args { 'F' }


package PLXML::op_seekdir;

our @ISA = ('PLXML::listop');

sub key { 'seekdir' }
sub desc { 'seekdir' }
sub check { 'ck_fun' }
sub flags { 's@' }
sub args { 'F S' }


package PLXML::op_rewinddir;

our @ISA = ('PLXML::baseop_unop');

sub key { 'rewinddir' }
sub desc { 'rewinddir' }
sub check { 'ck_fun' }
sub flags { 's%' }
sub args { 'F' }


package PLXML::op_closedir;

our @ISA = ('PLXML::baseop_unop');

sub key { 'closedir' }
sub desc { 'closedir' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'F' }



# Process control.

package PLXML::op_fork;

our @ISA = ('PLXML::baseop');

sub key { 'fork' }
sub desc { 'fork' }
sub check { 'ck_null' }
sub flags { 'ist0' }
sub args { '' }


package PLXML::op_wait;

our @ISA = ('PLXML::baseop');

sub key { 'wait' }
sub desc { 'wait' }
sub check { 'ck_null' }
sub flags { 'isT0' }
sub args { '' }


package PLXML::op_waitpid;

our @ISA = ('PLXML::listop');

sub key { 'waitpid' }
sub desc { 'waitpid' }
sub check { 'ck_fun' }
sub flags { 'isT@' }
sub args { 'S S' }


package PLXML::op_system;

our @ISA = ('PLXML::listop');

sub key { 'system' }
sub desc { 'system' }
sub check { 'ck_exec' }
sub flags { 'imsT@' }
sub args { 'S? L' }


package PLXML::op_exec;

our @ISA = ('PLXML::listop');

sub key { 'exec' }
sub desc { 'exec' }
sub check { 'ck_exec' }
sub flags { 'dimsT@' }
sub args { 'S? L' }


package PLXML::op_kill;

our @ISA = ('PLXML::listop');

sub key { 'kill' }
sub desc { 'kill' }
sub check { 'ck_fun' }
sub flags { 'dimsT@' }
sub args { 'L' }


package PLXML::op_getppid;

our @ISA = ('PLXML::baseop');

sub key { 'getppid' }
sub desc { 'getppid' }
sub check { 'ck_null' }
sub flags { 'isT0' }
sub args { '' }


package PLXML::op_getpgrp;

our @ISA = ('PLXML::baseop_unop');

sub key { 'getpgrp' }
sub desc { 'getpgrp' }
sub check { 'ck_fun' }
sub flags { 'isT%' }
sub args { 'S?' }


package PLXML::op_setpgrp;

our @ISA = ('PLXML::listop');

sub key { 'setpgrp' }
sub desc { 'setpgrp' }
sub check { 'ck_fun' }
sub flags { 'isT@' }
sub args { 'S? S?' }


package PLXML::op_getpriority;

our @ISA = ('PLXML::listop');

sub key { 'getpriority' }
sub desc { 'getpriority' }
sub check { 'ck_fun' }
sub flags { 'isT@' }
sub args { 'S S' }


package PLXML::op_setpriority;

our @ISA = ('PLXML::listop');

sub key { 'setpriority' }
sub desc { 'setpriority' }
sub check { 'ck_fun' }
sub flags { 'isT@' }
sub args { 'S S S' }



# Time calls.

package PLXML::op_time;

our @ISA = ('PLXML::baseop');

sub key { 'time' }
sub desc { 'time' }
sub check { 'ck_null' }
sub flags { 'isT0' }
sub args { '' }


package PLXML::op_tms;

our @ISA = ('PLXML::baseop');

sub key { 'tms' }
sub desc { 'times' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_localtime;

our @ISA = ('PLXML::baseop_unop');

sub key { 'localtime' }
sub desc { 'localtime' }
sub check { 'ck_fun' }
sub flags { 't%' }
sub args { 'S?' }


package PLXML::op_gmtime;

our @ISA = ('PLXML::baseop_unop');

sub key { 'gmtime' }
sub desc { 'gmtime' }
sub check { 'ck_fun' }
sub flags { 't%' }
sub args { 'S?' }


package PLXML::op_alarm;

our @ISA = ('PLXML::baseop_unop');

sub key { 'alarm' }
sub desc { 'alarm' }
sub check { 'ck_fun' }
sub flags { 'istu%' }
sub args { 'S?' }


package PLXML::op_sleep;

our @ISA = ('PLXML::baseop_unop');

sub key { 'sleep' }
sub desc { 'sleep' }
sub check { 'ck_fun' }
sub flags { 'isT%' }
sub args { 'S?' }



# Shared memory.

package PLXML::op_shmget;

our @ISA = ('PLXML::listop');

sub key { 'shmget' }
sub desc { 'shmget' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S S' }


package PLXML::op_shmctl;

our @ISA = ('PLXML::listop');

sub key { 'shmctl' }
sub desc { 'shmctl' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S S' }


package PLXML::op_shmread;

our @ISA = ('PLXML::listop');

sub key { 'shmread' }
sub desc { 'shmread' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S S S' }


package PLXML::op_shmwrite;

our @ISA = ('PLXML::listop');

sub key { 'shmwrite' }
sub desc { 'shmwrite' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S S S' }



# Message passing.

package PLXML::op_msgget;

our @ISA = ('PLXML::listop');

sub key { 'msgget' }
sub desc { 'msgget' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S' }


package PLXML::op_msgctl;

our @ISA = ('PLXML::listop');

sub key { 'msgctl' }
sub desc { 'msgctl' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S S' }


package PLXML::op_msgsnd;

our @ISA = ('PLXML::listop');

sub key { 'msgsnd' }
sub desc { 'msgsnd' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S S' }


package PLXML::op_msgrcv;

our @ISA = ('PLXML::listop');

sub key { 'msgrcv' }
sub desc { 'msgrcv' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S S S S' }



# Semaphores.

package PLXML::op_semget;

our @ISA = ('PLXML::listop');

sub key { 'semget' }
sub desc { 'semget' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S S' }


package PLXML::op_semctl;

our @ISA = ('PLXML::listop');

sub key { 'semctl' }
sub desc { 'semctl' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S S S' }


package PLXML::op_semop;

our @ISA = ('PLXML::listop');

sub key { 'semop' }
sub desc { 'semop' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S S' }



# Eval.

package PLXML::op_require;

our @ISA = ('PLXML::baseop_unop');

sub key { 'require' }
sub desc { 'require' }
sub check { 'ck_require' }
sub flags { 'du%' }
sub args { 'S?' }


package PLXML::op_dofile;

our @ISA = ('PLXML::unop');

sub key { 'dofile' }
sub desc { 'do "file"' }
sub check { 'ck_fun' }
sub flags { 'd1' }
sub args { 'S' }


package PLXML::op_entereval;

our @ISA = ('PLXML::baseop_unop');

sub key { 'entereval' }
sub desc { 'eval "string"' }
sub check { 'ck_eval' }
sub flags { 'd%' }
sub args { 'S' }


package PLXML::op_leaveeval;

our @ISA = ('PLXML::unop');

sub key { 'leaveeval' }
sub desc { 'eval "string" exit' }
sub check { 'ck_null' }
sub flags { '1' }
sub args { 'S' }


#evalonce	eval constant string	ck_null		d1	S
package PLXML::op_entertry;

our @ISA = ('PLXML::logop');

sub key { 'entertry' }
sub desc { 'eval {block}' }
sub check { 'ck_null' }
sub flags { '|' }
sub args { '' }


package PLXML::op_leavetry;

our @ISA = ('PLXML::listop');

sub key { 'leavetry' }
sub desc { 'eval {block} exit' }
sub check { 'ck_null' }
sub flags { '@' }
sub args { '' }



# Get system info.

package PLXML::op_ghbyname;

our @ISA = ('PLXML::baseop_unop');

sub key { 'ghbyname' }
sub desc { 'gethostbyname' }
sub check { 'ck_fun' }
sub flags { '%' }
sub args { 'S' }


package PLXML::op_ghbyaddr;

our @ISA = ('PLXML::listop');

sub key { 'ghbyaddr' }
sub desc { 'gethostbyaddr' }
sub check { 'ck_fun' }
sub flags { '@' }
sub args { 'S S' }


package PLXML::op_ghostent;

our @ISA = ('PLXML::baseop');

sub key { 'ghostent' }
sub desc { 'gethostent' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_gnbyname;

our @ISA = ('PLXML::baseop_unop');

sub key { 'gnbyname' }
sub desc { 'getnetbyname' }
sub check { 'ck_fun' }
sub flags { '%' }
sub args { 'S' }


package PLXML::op_gnbyaddr;

our @ISA = ('PLXML::listop');

sub key { 'gnbyaddr' }
sub desc { 'getnetbyaddr' }
sub check { 'ck_fun' }
sub flags { '@' }
sub args { 'S S' }


package PLXML::op_gnetent;

our @ISA = ('PLXML::baseop');

sub key { 'gnetent' }
sub desc { 'getnetent' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_gpbyname;

our @ISA = ('PLXML::baseop_unop');

sub key { 'gpbyname' }
sub desc { 'getprotobyname' }
sub check { 'ck_fun' }
sub flags { '%' }
sub args { 'S' }


package PLXML::op_gpbynumber;

our @ISA = ('PLXML::listop');

sub key { 'gpbynumber' }
sub desc { 'getprotobynumber' }
sub check { 'ck_fun' }
sub flags { '@' }
sub args { 'S' }


package PLXML::op_gprotoent;

our @ISA = ('PLXML::baseop');

sub key { 'gprotoent' }
sub desc { 'getprotoent' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_gsbyname;

our @ISA = ('PLXML::listop');

sub key { 'gsbyname' }
sub desc { 'getservbyname' }
sub check { 'ck_fun' }
sub flags { '@' }
sub args { 'S S' }


package PLXML::op_gsbyport;

our @ISA = ('PLXML::listop');

sub key { 'gsbyport' }
sub desc { 'getservbyport' }
sub check { 'ck_fun' }
sub flags { '@' }
sub args { 'S S' }


package PLXML::op_gservent;

our @ISA = ('PLXML::baseop');

sub key { 'gservent' }
sub desc { 'getservent' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_shostent;

our @ISA = ('PLXML::baseop_unop');

sub key { 'shostent' }
sub desc { 'sethostent' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'S' }


package PLXML::op_snetent;

our @ISA = ('PLXML::baseop_unop');

sub key { 'snetent' }
sub desc { 'setnetent' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'S' }


package PLXML::op_sprotoent;

our @ISA = ('PLXML::baseop_unop');

sub key { 'sprotoent' }
sub desc { 'setprotoent' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'S' }


package PLXML::op_sservent;

our @ISA = ('PLXML::baseop_unop');

sub key { 'sservent' }
sub desc { 'setservent' }
sub check { 'ck_fun' }
sub flags { 'is%' }
sub args { 'S' }


package PLXML::op_ehostent;

our @ISA = ('PLXML::baseop');

sub key { 'ehostent' }
sub desc { 'endhostent' }
sub check { 'ck_null' }
sub flags { 'is0' }
sub args { '' }


package PLXML::op_enetent;

our @ISA = ('PLXML::baseop');

sub key { 'enetent' }
sub desc { 'endnetent' }
sub check { 'ck_null' }
sub flags { 'is0' }
sub args { '' }


package PLXML::op_eprotoent;

our @ISA = ('PLXML::baseop');

sub key { 'eprotoent' }
sub desc { 'endprotoent' }
sub check { 'ck_null' }
sub flags { 'is0' }
sub args { '' }


package PLXML::op_eservent;

our @ISA = ('PLXML::baseop');

sub key { 'eservent' }
sub desc { 'endservent' }
sub check { 'ck_null' }
sub flags { 'is0' }
sub args { '' }


package PLXML::op_gpwnam;

our @ISA = ('PLXML::baseop_unop');

sub key { 'gpwnam' }
sub desc { 'getpwnam' }
sub check { 'ck_fun' }
sub flags { '%' }
sub args { 'S' }


package PLXML::op_gpwuid;

our @ISA = ('PLXML::baseop_unop');

sub key { 'gpwuid' }
sub desc { 'getpwuid' }
sub check { 'ck_fun' }
sub flags { '%' }
sub args { 'S' }


package PLXML::op_gpwent;

our @ISA = ('PLXML::baseop');

sub key { 'gpwent' }
sub desc { 'getpwent' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_spwent;

our @ISA = ('PLXML::baseop');

sub key { 'spwent' }
sub desc { 'setpwent' }
sub check { 'ck_null' }
sub flags { 'is0' }
sub args { '' }


package PLXML::op_epwent;

our @ISA = ('PLXML::baseop');

sub key { 'epwent' }
sub desc { 'endpwent' }
sub check { 'ck_null' }
sub flags { 'is0' }
sub args { '' }


package PLXML::op_ggrnam;

our @ISA = ('PLXML::baseop_unop');

sub key { 'ggrnam' }
sub desc { 'getgrnam' }
sub check { 'ck_fun' }
sub flags { '%' }
sub args { 'S' }


package PLXML::op_ggrgid;

our @ISA = ('PLXML::baseop_unop');

sub key { 'ggrgid' }
sub desc { 'getgrgid' }
sub check { 'ck_fun' }
sub flags { '%' }
sub args { 'S' }


package PLXML::op_ggrent;

our @ISA = ('PLXML::baseop');

sub key { 'ggrent' }
sub desc { 'getgrent' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


package PLXML::op_sgrent;

our @ISA = ('PLXML::baseop');

sub key { 'sgrent' }
sub desc { 'setgrent' }
sub check { 'ck_null' }
sub flags { 'is0' }
sub args { '' }


package PLXML::op_egrent;

our @ISA = ('PLXML::baseop');

sub key { 'egrent' }
sub desc { 'endgrent' }
sub check { 'ck_null' }
sub flags { 'is0' }
sub args { '' }


package PLXML::op_getlogin;

our @ISA = ('PLXML::baseop');

sub key { 'getlogin' }
sub desc { 'getlogin' }
sub check { 'ck_null' }
sub flags { 'st0' }
sub args { '' }



# Miscellaneous.

package PLXML::op_syscall;

our @ISA = ('PLXML::listop');

sub key { 'syscall' }
sub desc { 'syscall' }
sub check { 'ck_fun' }
sub flags { 'imst@' }
sub args { 'S L' }



# For multi-threading
package PLXML::op_lock;

our @ISA = ('PLXML::baseop_unop');

sub key { 'lock' }
sub desc { 'lock' }
sub check { 'ck_rfun' }
sub flags { 's%' }
sub args { 'R' }


package PLXML::op_threadsv;

our @ISA = ('PLXML::baseop');

sub key { 'threadsv' }
sub desc { 'per-thread value' }
sub check { 'ck_null' }
sub flags { 'ds0' }
sub args { '' }



# Control (contd.)
package PLXML::op_setstate;

our @ISA = ('PLXML::cop');

sub key { 'setstate' }
sub desc { 'set statement info' }
sub check { 'ck_null' }
sub flags { 's;' }
sub args { '' }


package PLXML::op_method_named;

our @ISA = ('PLXML::padop_svop');

sub key { 'method_named' }
sub desc { 'method with known name' }
sub check { 'ck_null' }
sub flags { 'd$' }
sub args { '' }



package PLXML::op_dor;

our @ISA = ('PLXML::logop');

sub key { 'dor' }
sub desc { 'defined or (//)' }
sub check { 'ck_null' }
sub flags { '|' }
sub args { '' }


package PLXML::op_dorassign;

our @ISA = ('PLXML::logop');

sub key { 'dorassign' }
sub desc { 'defined or assignment (//=)' }
sub check { 'ck_null' }
sub flags { 's|' }
sub args { '' }



# Add new ops before this, the custom operator.

package PLXML::op_custom;

our @ISA = ('PLXML::baseop');

sub key { 'custom' }
sub desc { 'unknown custom operator' }
sub check { 'ck_null' }
sub flags { '0' }
sub args { '' }


