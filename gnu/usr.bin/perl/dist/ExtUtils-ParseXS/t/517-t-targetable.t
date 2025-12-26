#!/usr/bin/perl
use strict;
use warnings;
use Carp;
use Cwd;
use File::Spec;
use Test::More;
use lib qw( lib );
use ExtUtils::Typemaps;

# Test the  targetable() and targetable_legacy() methods from
# ExtUtils::Typemaps::OutputMap


# First, test the targetable_legacy() method

my $output_expr_ref = {
  'T_CALLBACK' => '	sv_setpvn($arg, $var.context.value().chp(),
		$var.context.value().size());
',
  'T_OUT' => '	{
	    GV *gv = newGVgen("$Package");
	    if ( do_open(gv, "+>&", 3, FALSE, 0, 0, $var) )
		sv_setsv($arg, sv_bless(newRV((SV*)gv), gv_stashpv("$Package",1)));
	    else
		$arg = &PL_sv_undef;
	}
',
  'T_REF_IV_PTR' => '	sv_setref_pv($arg, \\"${ntype}\\", (void*)$var);
',
  'T_U_LONG' => '	sv_setuv($arg, (UV)$var);
',
  'T_U_CHAR' => '	sv_setuv($arg, (UV)$var);
',
  'T_U_INT' => '	sv_setuv($arg, (UV)$var);
',
  'T_ARRAY' => '        {
	    U32 ix_$var;
	    EXTEND(SP,size_$var);
	    for (ix_$var = 0; ix_$var < size_$var; ix_$var++) {
		ST(ix_$var) = sv_newmortal();
	DO_ARRAY_ELEM
	    }
        }
',
  'T_NV' => '	sv_setnv($arg, (NV)$var);
',
  'T_SHORT' => '	sv_setiv($arg, (IV)$var);
',
  'T_OPAQUE' => '	sv_setpvn($arg, (char *)&$var, sizeof($var));
',
  'T_PTROBJ' => '	sv_setref_pv($arg, \\"${ntype}\\", (void*)$var);
',
  'T_HVREF' => '	$arg = newRV((SV*)$var);
',
  'T_PACKEDARRAY' => '	XS_pack_$ntype($arg, $var, count_$ntype);
',
  'T_INT' => '	sv_setiv($arg, (IV)$var);
',
  'T_OPAQUEPTR' => '	sv_setpvn($arg, (char *)$var, sizeof(*$var));
',
  'T_BOOL' => '	$arg = boolSV($var);
',
  'T_REFREF' => '	NOT_IMPLEMENTED
',
  'T_REF_IV_REF' => '	sv_setref_pv($arg, \\"${ntype}\\", (void*)new $ntype($var));
',
  'T_STDIO' => '	{
	    GV *gv = newGVgen("$Package");
	    PerlIO *fp = PerlIO_importFILE($var,0);
	    if ( fp && do_open(gv, "+<&", 3, FALSE, 0, 0, fp) )
		sv_setsv($arg, sv_bless(newRV((SV*)gv), gv_stashpv("$Package",1)));
	    else
		$arg = &PL_sv_undef;
	}
',
  'T_FLOAT' => '	sv_setnv($arg, (double)$var);
',
  'T_IN' => '	{
	    GV *gv = newGVgen("$Package");
	    if ( do_open(gv, "<&", 2, FALSE, 0, 0, $var) )
		sv_setsv($arg, sv_bless(newRV((SV*)gv), gv_stashpv("$Package",1)));
	    else
		$arg = &PL_sv_undef;
	}
',
  'T_PV' => '	sv_setpv((SV*)$arg, $var);
',
  'T_INOUT' => '	{
	    GV *gv = newGVgen("$Package");
	    if ( do_open(gv, "+<&", 3, FALSE, 0, 0, $var) )
		sv_setsv($arg, sv_bless(newRV((SV*)gv), gv_stashpv("$Package",1)));
	    else
		$arg = &PL_sv_undef;
	}
',
  'T_CHAR' => '	sv_setpvn($arg, (char *)&$var, 1);
',
  'T_LONG' => '	sv_setiv($arg, (IV)$var);
',
  'T_DOUBLE' => '	sv_setnv($arg, (double)$var);
',
  'T_PTR' => '	sv_setiv($arg, PTR2IV($var));
',
  'T_AVREF' => '	$arg = newRV((SV*)$var);
',
  'T_SV' => '	$arg = $var;
',
  'T_ENUM' => '	sv_setiv($arg, (IV)$var);
',
  'T_REFOBJ' => '	NOT IMPLEMENTED
',
  'T_CVREF' => '	$arg = newRV((SV*)$var);
',
  'T_UV' => '	sv_setuv($arg, (UV)$var);
',
  'T_PACKED' => '	XS_pack_$ntype($arg, $var);
',
  'T_SYSRET' => '	if ($var != -1) {
	    if ($var == 0)
		sv_setpvn($arg, "0 but true", 10);
	    else
		sv_setiv($arg, (IV)$var);
	}
',
  'T_IV' => '	sv_setiv($arg, (IV)$var);
',
  'T_PTRDESC' => '	sv_setref_pv($arg, \\"${ntype}\\", (void*)new\\U${type}_DESC\\E($var));
',
  'T_DATAUNIT' => '	sv_setpvn($arg, $var.chp(), $var.size());
',
  'T_U_SHORT' => '	sv_setuv($arg, (UV)$var);
',
  'T_SVREF' => '	$arg = newRV((SV*)$var);
',
  'T_PTRREF' => '	sv_setref_pv($arg, Nullch, (void*)$var);
',
};


my %results = (
  T_UV        => { type => 'u', with_size => undef, what => '(UV)$var', what_size => undef },
  T_IV        => { type => 'i', with_size => undef, what => '(IV)$var', what_size => undef },
  T_NV        => { type => 'n', with_size => undef, what => '(NV)$var', what_size => undef },
  T_FLOAT     => { type => 'n', with_size => undef, what => '(double)$var', what_size => undef },
  T_PTR       => { type => 'i', with_size => undef, what => 'PTR2IV($var)', what_size => undef },
  T_PV        => { type => 'p', with_size => undef, what => '$var', what_size => undef },
  T_OPAQUE    => { type => 'p', with_size => 'n', what => '(char *)&$var', what_size => ', sizeof($var)' },
  T_OPAQUEPTR => { type => 'p', with_size => 'n', what => '(char *)$var', what_size => ', sizeof(*$var)' },
  T_CHAR      => { type => 'p', with_size => 'n', what => '(char *)&$var', what_size => ', 1' },
  T_CALLBACK  => { type => 'p', with_size => 'n', what => '$var.context.value().chp()',
                   what_size => ",\n		\$var.context.value().size()" }, # whitespace is significant here
  T_DATAUNIT  => { type => 'p', with_size => 'n', what => '$var.chp()', what_size => ', $var.size()' },
);

$results{$_} = $results{T_UV} for qw(T_U_LONG T_U_INT T_U_CHAR T_U_SHORT);
$results{$_} = $results{T_IV} for qw(T_LONG T_INT T_SHORT T_ENUM);
$results{$_} = $results{T_FLOAT} for qw(T_DOUBLE);

foreach my $xstype (sort keys %$output_expr_ref) {
  my $om = ExtUtils::Typemaps::OutputMap->new(
    xstype => $xstype,
    code => $output_expr_ref->{$xstype}
  );
  my $targetable = $om->targetable_legacy;
  if (not exists($results{$xstype})) {
    ok(not(defined($targetable)), "$xstype not targetable")
      or diag(join ", ", map {defined($_) ? $_ : "<undef>"} %$targetable);
  }
  else {
    my $res = $results{$xstype};
    is_deeply($targetable, $res, "$xstype targetable and has right output")
      or diag(join ", ", map {defined($_) ? $_ : "<undef>"} %$targetable);
  }
}


# Now, test the targetable() boolean class method

{
    my @tests = (
        # check that the basic set_foo functions are recognised
        1,  'sv_set_undef($arg);',
        1,  'sv_set_true($arg);',
        1,  'sv_set_false($arg);',

        1,  'sv_set_bool($arg,  (bool)RETVAL);',
        1,  'sv_setiv($arg,     (IV)RETVAL);',
        1,  'sv_setiv_mg($arg,  (IV)RETVAL);',
        1,  'sv_setuv($arg,     (UV)RETVAL);',
        1,  'sv_setuv_mg($arg,  (UV)RETVAL);',
        1,  'sv_setnv($arg,     (NV)RETVAL);',
        1,  'sv_setnv_mg($arg,  (NV)RETVAL);',
        1,  'sv_setpv($arg,     (char*)RETVAL);',
        1,  'sv_setpv_mg($arg,  (char*)RETVAL);',

        1,  'sv_setpvn($arg,    (char*)RETVAL, strlen(RETVAL));',
        1,  'sv_setpvn_mg($arg, (char*)RETVAL, strlen(RETVAL));',


        # variants of the SV to set

        1,  'sv_setiv((SV*)$arg,               (IV)RETVAL);',
        1,  'sv_setuv(  (  SV  *  )  $arg,     (UV)RETVAL);',
        1,  'sv_setnv((SV*)RETVALSV,           (NV)RETVAL);',
        1,  'sv_setpv(RETVAL,                  (char*)RETVAL);',
        1,  'sv_setpvn(ST(0),                  (char*)RETVAL, strlen(RETVAL));',

        # too few arguments

        '', 'sv_setiv($arg);',
        '', 'sv_setuv($arg);',
        '', 'sv_setnv($arg);',
        '', 'sv_setpv($arg);',
        '', 'sv_setpvn($arg, (char*)RETVAL);',

        # too many arguments

        '', 'sv_setiv($arg,     (IV)RETVAL, 1);',
        '', 'sv_setuv($arg,     (UV)RETVAL, 1);',
        '', 'sv_setnv($arg,     (NV)RETVAL, 1);',
        '', 'sv_setpv($arg,     (char*)RETVAL, 1);',
        '', 'sv_setpvn($arg,    (char*)RETVAL, strlen(RETVAL), 1);',

        # check that nested strings, parentheses etc are parsed
        1,  'sv_setpv($arg,     (char*) foo(a,(b,(c))));',
        1,  'sv_setpv($arg,     (char*)"a,\"b,c");',
        1,  'sv_setpv($arg,     (char*)"a,b,(c,(d,(e)))");',
        1,  'sv_setpvn($arg,    (char*)foo, "a,b,(c,(d,(e)))");',

        # RV setting is not allowed
        '', 'sv_setrv_inc($arg,   RETVAL);',
        '', 'sv_setrv_noinc($arg, RETVAL);',
        '', 'sv_setref_pv($arg,   RETVAL);',
    );

    while (@tests) {
        my $exp  = shift @tests;
        my $code = shift @tests;
        is(ExtUtils::Typemaps::OutputMap->targetable($code),
           $exp,
           "targetable($code)"
        );
    };
}

done_testing;
