BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib' if -d '../lib';
    require Config;
    if (($Config::Config{'extensions'} !~ m!\bDevel/PPPort\b!) ){
        print "1..0 # Skip -- Perl configured without Devel::PPPort module\n";
        exit 0;
    }
}

use Devel::PPPort;
use strict;

print "1..17\n";

my $total = 0;
my $good = 0;

my $test = 0;   
sub ok {
    my ($name, $test_sub) = @_;
    my $line = (caller)[2];
    my $value;

    eval { $value = &{ $test_sub }() } ;

    ++ $test ;

    if ($@) {
        printf "not ok $test # Testing '$name', line $line $@\n";
    }
    elsif ($value != 1){
        printf "not ok $test # Testing '$name', line $line, value != 1 ($value)\n";
    }
    else {
        print "ok $test\n";
    }

} 

ok "Static newCONSTSUB()", 
   sub { Devel::PPPort::test1(); Devel::PPPort::test_value_1() == 1} ;

ok "Global newCONSTSUB()", 
   sub { Devel::PPPort::test2(); Devel::PPPort::test_value_2() == 2} ;

ok "Extern newCONSTSUB()", 
   sub { Devel::PPPort::test3(); Devel::PPPort::test_value_3() == 3} ;

ok "newRV_inc()", sub { Devel::PPPort::test4()} ;

ok "newRV_noinc()", sub { Devel::PPPort::test5()} ;

ok "PL_sv_undef", sub { not defined Devel::PPPort::test6()} ;

ok "PL_sv_yes", sub { Devel::PPPort::test7()} ;

ok "PL_sv_no", sub { !Devel::PPPort::test8()} ;

ok "PL_na", sub { Devel::PPPort::test9("abcd") == 4} ;

ok "boolSV 1", sub { Devel::PPPort::test10(1) } ;

ok "boolSV 0", sub { ! Devel::PPPort::test10(0) } ;

ok "newSVpvn", sub { Devel::PPPort::test11("abcde", 3) eq "abc" } ;

ok "DEFSV", sub { $_ = "Fred"; Devel::PPPort::test12() eq "Fred" } ;

ok "ERRSV", sub { eval { 1; }; ! Devel::PPPort::test13() };

ok "ERRSV", sub { eval { fred() }; Devel::PPPort::test13() };

ok "CXT 1", sub { Devel::PPPort::test14()} ;

ok "CXT 2", sub { Devel::PPPort::test15()} ;

__END__
# TODO

PERL_VERSION
PERL_BCDVERSION

PL_stdingv
PL_hints
PL_curcop
PL_curstash
PL_copline
PL_Sv
PL_compiling
PL_dirty

PTR2IV
INT2PTR

dTHR
gv_stashpvn
NOOP
SAVE_DEFSV
PERL_UNUSED_DECL
dNOOP

call_argv
call_method
call_pv
call_sv

get_cv
get_av
get_hv
get_sv

grok_hex
grok_oct
grok_bin

grok_number
grok_numeric_radix
