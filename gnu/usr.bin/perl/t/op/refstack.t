#!./perl
#
# Tests for a (non) reference-counted stack
#
# This file checks the test cases of tickets where having the stack not
# reference-counted caused a crash or unexpected behaviour.
# Some of tickets no longer failed in blead, but I added them as tests
# anyway.
# Many of the tests are just to ensure that there's no panic, SEGV or
# ASAN errors, and so they are happy for the output to be "" rather
# than any specific value.
#
# The tickets these test cases initially came from were either:
#
# - those linked on RT by the meta ticket:
#    RT #77706: "[META] stack not reference counted issues"
#
# - or on GH tagged as label:leak/refcount/malloc and which appear to
#    be stack-related


BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    skip_all('not built with PERL_RC_STACK')
        unless defined &Internals::stack_refcounted
            && (Internals::stack_refcounted() & 1);
    set_up_inc( qw(. ../lib) );
}

use warnings;
use strict;


# GH #2157: "coredump in map modifying input array"

fresh_perl_is(
    q{my @a = 1..3; @a = map { splice( @a, 0 ); $_ } (@a); print "@a\n";},
    "1 2 3",
    {stderr => 1},
    "GH #2157"
);


# GH #4924: "@_ gets corrupted when F(@X) shortens @X"

{
    my @x;

    sub f4924 {
        @x = ();
        my @y = 999;
        "@_";
    }

    @x = 1..3;
    # used to get "0 999   4"
    is f4924(0, @x, 4), "0 1 2 3 4", "GH #4924";
}


# GH #6079: "Segfault when assigning to array that is being iterated over"

fresh_perl_is(
    q{@a = 1..2; for (@a, 3) { $t = 'x'; $t =~ s/x/@a = ()/e; }},
    "",
    {stderr => 1},
    "GH #6079"
);


# GH #6533: "Another self-modifyingloop bug"
#
# This failed an assertion prior to 5.26.0

fresh_perl_is(
    q{map { @a = ($_+=0) x $_ } @a=/\B./g for 100;},
    "",
    {stderr => 1},
    "GH #6533"
);


# GH #6874: "Coredump when shortening an array during use"

fresh_perl_is(
    q{$a=@F[4,7]-=@F=3},
    "",
    {stderr => 1},
    "GH #6874"
);


# GH #6957: "Bizarre array copy: ???"

fresh_perl_is(
    q{sub f { my $x; *G = \1; sub { package DB; ()=caller 1; @a = @DB::args; $x; }->(); } f($G)},
    "",
    {stderr => 1},
    "GH #6957"
);


# GH #7251: "Manipulating hash in SIGCHLD handler causes "Segmentation fault""
#
# Doesn't have a simple reproducer.



# GH #7483: "Assignments inside lists misbehave"

{
    my @a = 1..5;
    my @b = (@a, (@a = (8, 9)));
    is "@b", "1 2 3 4 5 8 9", "GH #7483";
}


# GH #8520: "Mortality of objects (e.g. %$_) passed as args... - bug or
#            feature?"

fresh_perl_is(
    q{sub foo { $x=0; \@_; } $x = { qw( a 1 b 2) }; foo(%$x);},
    "",
    {stderr => 1},
    "GH #8520"
);


# GH #8842: "Combination of tie() and loop aliasing can cause perl to
#            crash"
#
# This appears to have been fixed in 5.14.0

fresh_perl_is(
    q{sub TIEARRAY {bless []} sub FETCH {[1]} tie my @a, 'main'; my $p = \$a[0]; my @h = ($$p->[0], $$p->[0]);},
    "",
    {stderr => 1},
    "GH #8842"
);


# GH #8852: "panic copying freed scalar in Carp::Heavy"
#
# This appears to have been fixed in 5.14.0

fresh_perl_like(
    q{use Carp; @a=(1); f(@a); sub f { my $x = shift(@a); carp($x)}},
    qr/^1 at /,
    {stderr => 1},
    "GH #8852"
);


# GH #8955 "Bug in orassign"
#
# Caused a panic.

fresh_perl_is(
    q{my @a = (1); sub f { @a = () } $a[1] ||= f();},
    "",
    {stderr => 1},
    "GH #8955"
);


# GH #9166: "$_[0] seems to get reused inappropriately"
#
# Duplicate of GH #9282 ?



# GH #9203: "panic: attempt to copy freed scalar"

fresh_perl_is(
    q{@a = (1); foo(@a); sub foo { my $x = shift(@a); my $y = shift; }},
    "",
    {stderr => 1},
    "GH #9203"
);


# GH #9282: "Bizarre copy of ARRAY in sassign at Carp/Heavy.pm"

fresh_perl_is(
    q{@a = (1); sub { @a = (); package DB; () = caller(0); 1 for @DB::args; }->(@a);},
    "",
    {stderr => 1},
    "GH #9282"
);


# GH #9776: "segmentation fault modifying array ref during push"

fresh_perl_is(
    q{push @$x, f(); sub f { $x = 1; 2; }},
    "",
    {stderr => 1},
    "GH #9776"
);


# GH #10533: "segmentation fault in pure perl"

fresh_perl_is(
    q{my @a = ({},{}); sub f { my ($x) = @_; @a =  ( {}, {} ); 0 for (); } map { f $_ } @a;},
    "",
    {stderr => 1},
    "GH #10533"
);


# GH #10687: "Bizarre copy of ARRAY in list assignment"

{
    my @a = (8);
    sub f10687 {
        @a = ();
        package DB;
        () = caller(0);
        $DB::args[0];
    }
    is f10687(@a), "8", "GH #10687";
}

# GH #11287: "Use of freed value in iteration at perlbug line 6"

fresh_perl_is(
    q{my $a = my $b = { qw(a 1 b 2) }; for (values %$a, values %$b) { %$b=() }},
    "",
    {stderr => 1},
    "GH #11287"
);


# GH #11758: "@DB::args freed entries"

fresh_perl_is(
    q{my @a = qw(a v); sub f { shift @a; package DB; my @p = caller(0); print "[@DB::args]\n"; } f(@a);},
    "[a v]",
    {stderr => 1},
    "GH #11758"
);


# GH #11844: "SegFault in perl 5.010 -5.14.1"
#
# This was fixed in 5.16.0 by 9f71cfe6ef2 and 60edcf09a5cb0
# and tests were already added.



# GH #12315: "Panic in pure-Perl code with vanilla perl-5.16.0 from perlbrew"
#
# (This is the ticket that first got sprout and zefram talking seriously
# about how to transition to a ref-counted stack, which indirectly led
# to the work that included this test file - albeit using a slightly
# different approach.)

fresh_perl_is(
    q{@h{ @x = (1) } = @x for 1,2; print for %h;},
    "11",
    {stderr => 1},
    "GH #12315"
);


# GH #12952: "[5.16] Unreferenced scalar in recursion"

fresh_perl_is(
    q{@a = (1,1,1,1); map { [shift @a, shift @a] } @a;},
    "",
    {stderr => 1},
    "GH #12952"
);


# GH #13622: "Perl fails with message 'panic: attempt to copy freed scalar'"

fresh_perl_is(
    q{my @a = (8); sub g { shift @{$_[0]}; } sub f { g(\@a); return @_; } my @b = f(@a);},
    "",
    {stderr => 1},
    "GH #13622"
);


# GH #14630: "Perl_sv_clear: Assertion `((svtype)((sv)->sv_flags & 0xff))
#              != (svtype)0xff' failed (perl: sv.c:6537) "

fresh_perl_is(
    q{map $z=~s/x//, 0, $$z; grep 1, @b=1, @b=();},
    "",
    {stderr => 1},
    "GH #14630"
);


# GH #14716: "perls (including bleadperl) segfault/etc. with
#             recursion+sub{}+map pure-Perl code"

fresh_perl_is(
    q{sub f { my($n)=@_; print $n; @a = $n ? (sub { f(0); }, 0) : (); map { ref$_ ? &$_ :$_ } @a; } f(1);},
    "10",
    {stderr => 1},
    "GH #14716"
);


# GH #14785: "Perl_sv_clear: Assertion `((svtype)((sv)->sv_flags & 0xff))
#             != (svtype)0xff' failed (sv.c:6395)"

fresh_perl_is(
    q{map{%0=map{0}m 0 0}%0=map{0}0},
    "",
    {stderr => 1},
    "GH #14785"
);


# GH #14873: "v5.23.1-199-ga5f4850 breaks something badly"
#
# Doesn't have a simple reproducer.



# GH #14912: "undefing function argument references: "Attempt to free
#             unreferenced scalar""

fresh_perl_is(
    q[sub f { $r = 1; my ($x) = @_; } $r = \{}; f($$r);],
    "",
    {stderr => 1},
    "GH #14912"
);


# GH #14943: "Double-free in Perl_free_tmps"

fresh_perl_is(
    q{$[ .= *[ = 'y';},
    "",
    {stderr => 1},
    "GH #14943:"
);


# GH #15186: "Access to freed SV"

fresh_perl_is(
    q{@a=[0,0];map { $_=5; pop @$_ for @a } @{$a[0]}},
    "",
    {stderr => 1},
    "GH #15186"
);


# GH #15283: "Perl_sv_setnv: Assertion
#             `PL_valid_types_NV_set[((svtype)((sv)->sv_flags & 0xff)) & 0xf]'
#             failed."

fresh_perl_is(
    q{$z *= *z=0;},
    "",
    {stderr => 1},
    "GH #15283"
);


# GH #15287: "null pointer dereference in Perl_sv_setpvn at sv.c:4896"

fresh_perl_is(
    q{$x ^= *x = 0},
    "",
    {stderr => 1},
    "GH #15287"
);


# GH #15398: "Specific array shifting causes panic"
#
# Seems to have been fixed in 5.26

fresh_perl_is(
    q{sub o { shift; @a = (shift,shift); } o(@a); o(@a);},
    "",
    {stderr => 1},
    "GH #15398"
);


# GH #15447: "Unexpected: Use of freed value in iteration at ..."

fresh_perl_is(
    q{my $h = {qw(a 1 b 2)}; for (sort values %$h) { delete $h->{ b }; }},
    "",
    {stderr => 1},
    "GH #15447"
);


# GH #15556: "null ptr deref, segfault Perl_sv_setsv_flags (sv.c:4558)"
#
# Seems to have been fixed in 5.26

fresh_perl_is(
    q{*z=%::=$a=@b=0},
    "",
    {stderr => 1},
    "GH #15556"
);


# GH #15607: " null ptr deref, segfault in S_rv2gv (pp.c:296)"
# This still fails on  an ASAN on a PERL_RC_STACK build
# Since its a bit unlreliable as to whether it fails or not,
# just ignore for now.
#
# fresh_perl_is(
#     q{no warnings 'experimental'; use feature "refaliasing"; \$::{foo} = \undef; *{"foo"};},
#     "",
#     {stderr => 1},
#     "GH #15607"
# );


# GH #15663: " gv.c:1492: HV *S_gv_stashsvpvn_cached(
#                                SV *, const char *, U32, I32):
#              Assertion
#              `PL_valid_types_IVX[((svtype)((_svivx)->sv_flags & 0xff)) &
#              0xf]' failed"

fresh_perl_like(
    q{map xx->yy, (@z = 1), (@z = ());},
    qr/^Can't locate object method "yy"/,
    {stderr => 1},
    "GH #15663"
);


# GH #15684: "heap-use-after-free in Perl_sv_setpv (sv.c:4990)"
#
# Seems to have been fixed in 5.24

fresh_perl_is(
    q{($0+=(*0)=@0=($0)=N)=@0=(($0)=0)=@0=()},
    "",
    {stderr => 1},
    "GH #15684"
);


# GH #15687: "heap-use-after-free in S_unshare_hek_or_pvn (hv.c:2857)"

fresh_perl_like(
    q{*p= *$p= $| = *$p = $p |= *$p = *p = $p = \p},
    qr/^Can't use an undefined value as a symbol reference/,
    {stderr => 1},
    "GH #15687"
);


# GH #15740: "null ptr deref + segfault in Perl_sv_setpv_bufsize (sv.c:4956)"
#
# Seems to have been fixed in 5.36

fresh_perl_is(
    q{$$.=$A=*$=0},
    "",
    {stderr => 1},
    "GH #15740"
);


# GH #15747: "heap-use-after-free Perl_sv_setpv_bufsize (sv.c:4956)"
#
# Seems to have been fixed in 5.36

fresh_perl_is(
    q{@0=$0|=*0=H or()},
    "",
    {stderr => 1},
    "GH #15747"
);


# GH #15752: "fuzzing testcase triggers LeakSanitizer
#             over 101 byte memory leak"
#
# Seems to have been fixed in 5.36

fresh_perl_is(
    q{$$0 ^= ($0 |= (*0 = *H)), *& = ($$0 ^= ($0 |= (*0 = *H = *& = *a6))) for 'a9', 'a9'},
    "",
    {stderr => 1},
    "GH #15752"
);


# GH #15755: "Perl_sv_clear(SV *const): Assertion
#             `((svtype)((sv)->sv_flags & 0xff)) != (svtype)0xff'
#             failed (sv.c:6540)"

fresh_perl_is(
    q{map@0=%0=0,%0=D..T;},
    "",
    {stderr => 1},
    "GH #15755"
);


# GH #15756: "Null pointer dereference + segfault in Perl_pp_subst
#             (pp_hot.c:3368)"

fresh_perl_is(
    q{map 1, (%x) = (1..3), (%x) = ();},
    "",
    {stderr => 1},
    "GH #15756"
);


# GH #15757: "Perl_sv_backoff(SV *const): Assertion
#             `((svtype)((sv)->sv_flags & 0xff)) != SVt_PVHV'
#             failed (sv.c:1516)"

fresh_perl_is(
    q{map( ($_ = $T % 1), ((%x) = 'T'), ((%x) = 'T'), %$T);},
    "",
    {stderr => 1},
    "GH #15757"
);


# GH #15758: "Perl_sv_2nv_flags(SV *const, const I32): Assertion
#             `((svtye)((sv)->sv_flags & 0xff)) != SVt_PVAV
#             && ((svtype)((sv)->sv_flags & 0xff)) != SVt_PVHV
#             && ((svtype)((sv)->sv_flags & 0xff)) != SVt_PVFM'
#             fail"

fresh_perl_is(
    q{map( 1, (%_) = ('D', 'E'), (%_) = (),);},
    "",
    {stderr => 1},
    "GH #15758"
);


# GH #15759: "segfault in Perl_mg_magical (mg.c:144)"

fresh_perl_is(
    q{map( ((%^H) = ('D'..'FT')), (%_) = ('D'..'G'), (%_) = ());},
    "",
    {stderr => 1},
    "GH #15759"
);


# GH #15762: "heap-buffer-overflow Perl_vivify_ref (pp_hot.c:4362)"

fresh_perl_is(
    q{map$$_=0,%$T=%::},
    "",
    {stderr => 1},
    "GH #15762"
);


# GH #15765: "double-free affecting multiple Perl versions"

fresh_perl_like(
    q{map*$_= $#$_=8,%_=D.. FD,%_=D.. F},
    qr/^Not a GLOB reference at/,
    {stderr => 1},
    "GH #15765"
);


# GH #15769: "attempting free on address which was not malloc()-ed"

SKIP: {
    skip_if_miniperl('miniperl: ERRNO hash is read only');
    fresh_perl_is(
        # this combines both failing statements from this ticket
        q{map%$_= %_= %$_,%::;  map %$_ = %_, *::, $::{Internals::};},
        "",
        {stderr => 1},
        "GH #15769"
    );
}


# GH #15770: "Perl_sv_pvn_force_flags(SV *const, STRLEN *const, const I32):
#             Assertion
#             `PL_valid_types_PVX[((svtype)((_svpvx)->sv_flags & 0xff)) & 0xf]'
#             failed (sv.c:10056)"

fresh_perl_is(
    q{map 1, %x = (a => 1, b => undef), %x = (Y => 'Z');},
    "",
    {stderr => 1},
    "GH #15770"
);


# GH #15772: "heap-use-after-free S_gv_fetchmeth_internal (gv.c:782)"

fresh_perl_like(
    q{f { $s=1, @x=2, @x=() } 9},
    qr/^Can't locate object method .* line \d+\.$/,
    {stderr => 1},
    "GH #15772"
);


# GH #15807: "Coredump in Perl_sv_cmp_flags type-core"

fresh_perl_is(
    q{@0=s//0/; @0=sort(0,@t00=0,@t00=0,@0=s///);},
    "",
    {stderr => 1},
    "GH #15807"
);


# GH #15847: "sv.c:6545: void Perl_sv_clear(SV *const): Assertion
#             `SvTYPE(sv) != (svtype)SVTYPEMASK' failed"

fresh_perl_is(
    q{sub X::f{} f{'X',%0=local$0,%0=0}},
    "",
    {stderr => 1},
    "GH #15847"
);


# GH #15894: "AddressSanitizer: attempting free on address in Perl_safesysfree"

fresh_perl_is(
    q{map $p[0][0],@z=z,@z=z,@z=z,@z=z,@z=z,@z= ~9},
    "",
    {stderr => 1},
    "GH #15894"
);


# GH #15912: "AddressSanitizer: attempting free in Perl_vivify_ref"

fresh_perl_is(
    q{map $a[0][0], @a = 0, @a = 1;},
    "",
    {stderr => 1},
    "GH #15912"
);


# GH #15930: "Perl 5.24 makes nama FTBFS due to segfault"

fresh_perl_is(
    q{my @a = 0..1; sub f { my $x = shift; my @b = @a; @a = @b; 1; } map{ f($_) } @a;},
    "",
    {stderr => 1},
    "GH #15930"
);


# GH #15942: "segfault in S_mg_findext_flags()"

fresh_perl_is(
    q{map /x/g, (%h = ("y", 0)), (%h = ("y", 0))},
    "",
    {stderr => 1},
    "GH #15942"
);


# GH #15959: "panic: attempt to copy freed scalar via @ARGV on stack,
#           Getopt::Long + Carp::longmess"
#
# Too much like hard work to reduce the bug report to a simple test case,
# but the full script doesn't crash under PERL_RC_STACK



# GH #16103: "perl: sv.c:6566: void Perl_sv_clear(SV *const):
#             Assertion `SvTYPE(sv) != (svtype)SVTYPEMASK' failed"
#
# Reproducing script had too many random control and unicode chars in
# it to make a simple test which could be included here, but
# the full script doesn't crash under PERL_RC_STACK



# GH #16104: "Null Pointer Dereference in Perl_sv_setpv_bufsize"
#
# Seems to have been fixed in 5.36

fresh_perl_is(
    q{$_.=*_='x';},
    "",
    {stderr => 1},
    "GH #16104"
);


# GH #16120: "heap-use-after-free in Perl_sv_setpv_bufsize"
#
# Seems to have been fixed in 5.36

fresh_perl_is(
    q{$~|=*~='a';},
    "",
    {stderr => 1},
    "GH #16120"
);


# GH #16320: "PERL-5.26.1 heap_buffer_overflow READ of size 8"
#
# This crashed prior to 5.36.0

fresh_perl_like(
    q{*^V = "*main::"; 1 for Y $\ = $\ = $~ = *\ = $\ = *^ = %^V = *^V;},
    qr/^Can't locate object method "Y"/,
    {stderr => 1},
    "GH #16320"
);


# GH #16321: "PERL-5.26.1 heap_use_after_free READ of size 8"
#
# This failed under ASAN

fresh_perl_like(
    q{"x" . $x . pack "Wu", ~qr{}, !~"" = "x" . $x . pack "Wu", ~"", !~"" = $^V .= *^V = ""},
    qr/^Modification of a read-only value/,
    {stderr => 1},
    "GH #16321"
);


# GH #16322: "PERL-5.26.1 heap_use_after_free WRITE of size 1"
#
# This failed under ASAN, but doesn't seem to on 5.38.0

fresh_perl_is(
    q{$^A .= *^A = $^A .= ""},
    "",
    {stderr => 1},
    "GH #16322"
);


# GH #16323: "PERL-5.26.1 heap_use_after_free WRITE of size 1"

fresh_perl_is(
    q{$$W += $W = 0;},
    "",
    {stderr => 1},
    "GH #16323"
);


# GH #16324: "PERL-5.26.1 heap_use_after_free READ of size 8"
#
# This used $*, which is no longer supported



# GH #16325: "PERL-5.26.1 heap_buffer_overflow READ of size 1"
#
# This failed under ASAN, but doesn't seem to on 5.38.0

fresh_perl_is(
    q{$T .= *: = *T = "*main::"},
    
    "",
    {stderr => 1},
    "GH #16325"
);


# GH #16326: "PERL-5.26.1 heap_buffer_overflow READ of size 8"
#
# This used $*, which is no longer supported


# GH #16443: "Assertion `SvTYPE(sv) != (svtype)SVTYPEMASK' failed"

fresh_perl_is(
    q{($a)=map[split//],G0;$0=map abs($0[$a++]),@$a;},
    "",
    {stderr => 1},
    "GH #16443"
);


# GH #16455: "Fwd: [rt.cpan.org #124716] Use after free in sv.c:4860"
#
# Seems to have been fixed in 5.36

fresh_perl_is(
    q{$a ^= (*a = 'b');},
    "",
    {stderr => 1},
    "GH #16455"
);


# GH #16576: "Reporting a use-after-free vulnerability in function
#             Perl_sv_setpv_bufsize"
#
# This failed under ASAN, but doesn't seem to on 5.38.0

fresh_perl_is(
    q{$~ |= *~ = $~;},
    "",
    {stderr => 1},
    "GH #16576"
);


# GH #16613: "#10 AddressSanitizer: heap-use-after-free on address
#             0x604000000990 at pc 0x00000114d184 bp 0x7fffdb11d170
#             sp 0x7fffdb11d168 WRITE of size 1 at 0x604000000990"

fresh_perl_is(
    q{$A .= $$B .= $B = 0},
    "",
    {stderr => 1},
    "GH #16613"
);


# GH #16622: "Segfault on invalid script"
#
# This crashed prior to 5.36.0

fresh_perl_like(
    q{'A'->A($A .= *A = @5 = *A * 'A');},
    qr/^Can't locate object method "A"/,
    {stderr => 1},
    "GH #16622"
);


# GH #16727: "NULL pointer deference in Perl_sv_setpv_bufsize
#
# Seems to have been fixed in 5.36

fresh_perl_is(
    q{$^ ^= *: = ** = *^= *: = ** = *^= *: = ** = *:;},
    "",
    {stderr => 1},
    "GH #16727"
);


# GH #16742: "segfault triggered by invalid read in S_mg_findext_flags"
#
# Seems to have been fixed in 5.36.
# The test case is very noisy, so I've skipped including here.



# GH #17333: "map modifying its own LIST causes segfault in perl-5.16 and
#             later versions"

fresh_perl_is(
    q{my @a = 1..5; map { pop @a } @a;},
    "",
    {stderr => 1},
    "GH #17333"
);



done_testing();
