#!/usr/bin/perl
use strict;
use warnings;
use Carp;
use Cwd;
use File::Spec;
use File::Temp qw( tempdir );
use Test::More qw(no_plan); # tests =>  7;
use lib qw( lib );
use ExtUtils::ParseXS::Utilities qw(
    make_targetable
);

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

my %targetable;
%targetable = make_targetable($output_expr_ref);

ok(! exists $targetable{'T_AVREF'},
    "Element found in 'output_expr' not found in \%targetable: not an 'sv_set'" );

ok(exists $targetable{'T_CALLBACK'},
    "Element found in 'output_expr' found in \%targetable as expected" );

pass("Passed all tests in $0");
