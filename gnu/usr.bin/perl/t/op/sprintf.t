#!./perl

# Tests sprintf, excluding handling of 64-bit integers or long
# doubles (if supported), of machine-specific short and long
# integers, machine-specific floating point exceptions (infinity,
# not-a-number ...), of the effects of locale, and of features
# specific to multi-byte characters (under the utf8 pragma and such).

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}   
use warnings;
# we do not load %Config since this test resides in op and needs
# to run under the minitest target even without Config.pm working.

# strictness
my @tests = ();
my ($i, $template, $data, $result, $comment, $w, $x, $evalData, $n, $p);

while (<DATA>) {
    s/^\s*>//; s/<\s*$//;
    push @tests, [split(/<\s*>/, $_, 4)]; 
}

print '1..', scalar @tests, "\n";

$SIG{__WARN__} = sub {
    if ($_[0] =~ /^Invalid conversion/) {
	$w = ' INVALID';
    } elsif ($_[0] =~ /^Use of uninitialized value/) {
	$w = ' UNINIT';
    } else {
	warn @_;
    }
};

my $Is_VMS_VAX = 0;
# We use HW_MODEL since ARCH_NAME was not in VMS V5.*
if ($^O eq 'VMS') {
    my $hw_model;
    chomp($hw_model = `write sys\$output f\$getsyi("HW_MODEL")`);
    $Is_VMS_VAX = $hw_model < 1024 ? 1 : 0;
}

for ($i = 1; @tests; $i++) {
    ($template, $data, $result, $comment) = @{shift @tests};
    if ($^O eq 'os390' || $^O eq 's390') { # non-IEEE (s390 is UTS)
        $data   =~ s/([eE])96$/${1}63/;      # smaller exponents
        $result =~ s/([eE]\+)102$/${1}69/;   #  "       "
        $data   =~ s/([eE])\-101$/${1}-56/;  # larger exponents
        $result =~ s/([eE])\-102$/${1}-57/;  #  "       "
    }
    if ($Is_VMS_VAX) { # VAX DEC C 5.3 at least since there is no 
                       # ccflags =~ /float=ieee/ on VAX.
                       # AXP is unaffected whether or not it's using ieee.
        $data   =~ s/([eE])96$/${1}26/;      # smaller exponents
        $result =~ s/([eE]\+)102$/${1}32/;   #  "       "
        $data   =~ s/([eE])\-101$/${1}-24/;  # larger exponents
        $result =~ s/([eE])\-102$/${1}-25/;  #  "       "
    }
    $evalData = eval $data;
    $w = undef;
    $x = sprintf(">$template<",
                 defined @$evalData ? @$evalData : $evalData);
    substr($x, -1, 0) = $w if $w;
    # $x may have 3 exponent digits, not 2
    my $y = $x;
    if ($y =~ s/([Ee][-+])0(\d)/$1$2/) {
        # if result is left-adjusted, append extra space
        if ($template =~ /%\+?\-/ and $result =~ / $/) {
	    $y =~ s/<$/ </;
	}
        # if result is zero-filled, add extra zero
	elsif ($template =~ /%\+?0/ and $result =~ /^0/) {
	    $y =~ s/^>0/>00/;
	}
        # if result is right-adjusted, prepend extra space
	elsif ($result =~ /^ /) {
	    $y =~ s/^>/> /;
	}
    }

    if ($x eq ">$result<") {
        print "ok $i\n";
    }
    elsif ($y eq ">$result<")	# Some C libraries always give
    {				# three-digit exponent
		print("ok $i # >$result< $x three-digit exponent accepted\n");
    }
	elsif ($result =~ /[-+]\d{3}$/ &&
		   # Suppress tests with modulo of exponent >= 100 on platforms
		   # which can't handle such magnitudes (or where we can't tell).
		   ((!eval {require POSIX}) || # Costly: only do this if we must!
			(length(&POSIX::DBL_MAX) - rindex(&POSIX::DBL_MAX, '+')) == 3))
	{
		print("ok $i # >$template< >$data< >$result<",
			  " Suppressed: exponent out of range?\n") 
	}
    else {
	$y = ($x eq $y ? "" : " => $y");
	print("not ok $i >$template< >$data< >$result< $x$y",
	    $comment ? " # $comment\n" : "\n");
    }
}

# In each of the the following lines, there are three required fields:
# printf template, data to be formatted (as a Perl expression), and
# expected result of formatting.  An optional fourth field can contain
# a comment.  Each field is delimited by a starting '>' and a
# finishing '<'; any whitespace outside these start and end marks is
# not part of the field.  If formatting requires more than one data
# item (for example, if variable field widths are used), the Perl data
# expression should return a reference to an array having the requisite
# number of elements.  Even so, subterfuge is sometimes required: see
# tests for %n and %p.
#
# The following tests are not currently run, for the reasons stated:

=pod

=begin problematic

>%.0f<      >-0.1<        >-0<  >C library bug: no minus on VMS, HP-UX<
>%.0f<      >1.5<         >2<   >Standard vague: no rounding rules<
>%.0f<      >2.5<         >2<   >Standard vague: no rounding rules<
>%G<        >1234567e96<  >1.23457E+102<	>exponent too big for OS/390<
>%G<        >.1234567e-101< >1.23457E-102<	>exponent too small for OS/390<
>%e<        >1234567E96<  >1.234567e+102<	>exponent too big for OS/390<
>%e<        >.1234567E-101< >1.234567e-102<	>exponent too small for OS/390<
>%g<        >.1234567E-101< >1.23457e-102<	>exponent too small for OS/390<
>%g<        >1234567E96<  >1.23457e+102<	>exponent too big for OS/390<

=end problematic

=cut

# template    data          result
__END__
>%6. 6s<    >''<          >%6. 6s INVALID< >(See use of $w in code above)<
>%6 .6s<    >''<          >%6 .6s INVALID<
>%6.6 s<    >''<          >%6.6 s INVALID<
>%A<        >''<          >%A INVALID<
>%B<        >''<          >%B INVALID<
>%C<        >''<          >%C INVALID<
>%D<        >0x7fffffff<  >2147483647<     >Synonym for %ld<
>%E<        >123456.789<  >1.234568E+05<   >Like %e, but using upper-case "E"<
>%F<        >123456.789<  >123456.789000<  >Synonym for %f<
>%G<        >1234567.89<  >1.23457E+06<    >Like %g, but using upper-case "E"<
>%G<        >1234567e96<  >1.23457E+102<
>%G<        >.1234567e-101< >1.23457E-102<
>%G<        >12345.6789<  >12345.7<
>%H<        >''<          >%H INVALID<
>%I<        >''<          >%I INVALID<
>%J<        >''<          >%J INVALID<
>%K<        >''<          >%K INVALID<
>%L<        >''<          >%L INVALID<
>%M<        >''<          >%M INVALID<
>%N<        >''<          >%N INVALID<
>%O<        >2**32-1<     >37777777777<    >Synonym for %lo<
>%P<        >''<          >%P INVALID<
>%Q<        >''<          >%Q INVALID<
>%R<        >''<          >%R INVALID<
>%S<        >''<          >%S INVALID<
>%T<        >''<          >%T INVALID<
>%U<        >2**32-1<     >4294967295<     >Synonym for %lu<
>%V<        >''<          >%V INVALID<
>%W<        >''<          >%W INVALID<
>%X<        >2**32-1<     >FFFFFFFF<       >Like %x, but with u/c letters<
>%#X<       >2**32-1<     >0XFFFFFFFF<
>%Y<        >''<          >%Y INVALID<
>%Z<        >''<          >%Z INVALID<
>%a<        >''<          >%a INVALID<
>%b<        >2**32-1<     >11111111111111111111111111111111<
>%+b<       >2**32-1<     >11111111111111111111111111111111<
>%#b<       >2**32-1<     >0b11111111111111111111111111111111<
>%34b<      >2**32-1<     >  11111111111111111111111111111111<
>%034b<     >2**32-1<     >0011111111111111111111111111111111<
>%-34b<     >2**32-1<     >11111111111111111111111111111111  <
>%-034b<    >2**32-1<     >11111111111111111111111111111111  <
>%c<        >ord('A')<    >A<
>%10c<      >ord('A')<    >         A<
>%#10c<     >ord('A')<    >         A<     ># modifier: no effect<
>%010c<     >ord('A')<    >000000000A<
>%10lc<     >ord('A')<    >         A<     >l modifier: no effect<
>%10hc<     >ord('A')<    >         A<     >h modifier: no effect<
>%10.5c<    >ord('A')<    >         A<     >precision: no effect<
>%-10c<     >ord('A')<    >A         <
>%d<        >123456.789<  >123456<
>%d<        >-123456.789< >-123456<
>%d<        >0<           >0<
>%+d<       >0<           >+0<
>%0d<       >0<           >0<
>%.0d<      >0<           ><
>%+.0d<     >0<           >+<
>%.0d<      >1<           >1<
>%d<        >1<           >1<
>%+d<       >1<           >+1<
>%#3.2d<    >1<           > 01<            ># modifier: no effect<
>%3.2d<     >1<           > 01<
>%03.2d<    >1<           >001<
>%-3.2d<    >1<           >01 <
>%-03.2d<   >1<           >01 <            >zero pad + left just.: no effect<
>%d<        >-1<          >-1<
>%+d<       >-1<          >-1<
>%hd<       >1<           >1<              >More extensive testing of<
>%ld<       >1<           >1<              >length modifiers would be<
>%Vd<       >1<           >1<              >platform-specific<
>%vd<       >chr(1)<      >1<
>%+vd<      >chr(1)<      >+1<
>%#vd<      >chr(1)<      >1<
>%vd<       >"\01\02\03"< >1.2.3<
>%v.3d<     >"\01\02\03"< >001.002.003<
>%0v3d<     >"\01\02\03"< >001.002.003<
>%-v3d<     >"\01\02\03"< >1  .2  .3  <
>%+-v3d<    >"\01\02\03"< >+1 .2  .3  <
>%v4.3d<    >"\01\02\03"< > 001. 002. 003<
>%0v4.3d<   >"\01\02\03"< >0001.0002.0003<
>%0*v2d<    >['-', "\0\7\14"]< >00-07-12<
>%v.*d<     >["\01\02\03", 3]< >001.002.003<
>%0v*d<     >["\01\02\03", 3]< >001.002.003<
>%-v*d<     >["\01\02\03", 3]< >1  .2  .3  <
>%+-v*d<    >["\01\02\03", 3]< >+1 .2  .3  <
>%v*.*d<    >["\01\02\03", 4, 3]< > 001. 002. 003<
>%0v*.*d<   >["\01\02\03", 4, 3]< >0001.0002.0003<
>%0*v*d<    >['-', "\0\7\13", 2]< >00-07-11<
>%e<        >1234.875<    >1.234875e+03<
>%e<        >0.000012345< >1.234500e-05<
>%e<        >1234567E96<  >1.234567e+102<
>%e<        >0<           >0.000000e+00<
>%e<        >.1234567E-101< >1.234567e-102<
>%+e<       >1234.875<    >+1.234875e+03<
>%#e<       >1234.875<    >1.234875e+03<
>%e<        >-1234.875<   >-1.234875e+03<
>%+e<       >-1234.875<   >-1.234875e+03<
>%#e<       >-1234.875<   >-1.234875e+03<
>%.0e<      >1234.875<    >1e+03<
>%#.0e<     >1234.875<    >1.e+03<
>%.*e<      >[0, 1234.875]< >1e+03<
>%.1e<      >1234.875<    >1.2e+03<
>%-12.4e<   >1234.875<    >1.2349e+03  <
>%12.4e<    >1234.875<    >  1.2349e+03<
>%+-12.4e<  >1234.875<    >+1.2349e+03 <
>%+12.4e<   >1234.875<    > +1.2349e+03<
>%+-12.4e<  >-1234.875<   >-1.2349e+03 <
>%+12.4e<   >-1234.875<   > -1.2349e+03<
>%f<        >1234.875<    >1234.875000<
>%+f<       >1234.875<    >+1234.875000<
>%#f<       >1234.875<    >1234.875000<
>%f<        >-1234.875<   >-1234.875000<
>%+f<       >-1234.875<   >-1234.875000<
>%#f<       >-1234.875<   >-1234.875000<
>%6f<       >1234.875<    >1234.875000<
>%*f<       >[6, 1234.875]< >1234.875000<
>%.0f<      >1234.875<    >1235<
>%.1f<      >1234.875<    >1234.9<
>%-8.1f<    >1234.875<    >1234.9  <
>%8.1f<     >1234.875<    >  1234.9<
>%+-8.1f<   >1234.875<    >+1234.9 <
>%+8.1f<    >1234.875<    > +1234.9<
>%+-8.1f<   >-1234.875<   >-1234.9 <
>%+8.1f<    >-1234.875<   > -1234.9<
>%*.*f<     >[5, 2, 12.3456]< >12.35<
>%f<        >0<           >0.000000<
>%.0f<      >0<           >0<
>%.0f<      >2**38<       >274877906944<   >Should have exact int'l rep'n<
>%.0f<      >0.1<         >0<
>%.0f<      >0.6<         >1<              >Known to fail with sfio and (irix|nonstop-ux|powerux)<
>%.0f<      >-0.6<        >-1<             >Known to fail with sfio and (irix|nonstop-ux|powerux)<
>%.0f<      >1<           >1<
>%#.0f<     >1<           >1.<
>%g<        >12345.6789<  >12345.7<
>%+g<       >12345.6789<  >+12345.7<
>%#g<       >12345.6789<  >12345.7<
>%.0g<      >12345.6789<  >1e+04<
>%#.0g<     >12345.6789<  >1.e+04<
>%.2g<      >12345.6789<  >1.2e+04<
>%.*g<      >[2, 12345.6789]< >1.2e+04<
>%.9g<      >12345.6789<  >12345.6789<
>%12.9g<    >12345.6789<  >  12345.6789<
>%012.9g<   >12345.6789<  >0012345.6789<
>%-12.9g<   >12345.6789<  >12345.6789  <
>%*.*g<     >[-12, 9, 12345.6789]< >12345.6789  <
>%-012.9g<  >12345.6789<  >12345.6789  <
>%g<        >-12345.6789< >-12345.7<
>%+g<       >-12345.6789< >-12345.7<
>%g<        >1234567.89<  >1.23457e+06<
>%+g<       >1234567.89<  >+1.23457e+06<
>%#g<       >1234567.89<  >1.23457e+06<
>%g<        >-1234567.89< >-1.23457e+06<
>%+g<       >-1234567.89< >-1.23457e+06<
>%#g<       >-1234567.89< >-1.23457e+06<
>%g<        >0.00012345<  >0.00012345<
>%g<        >0.000012345< >1.2345e-05<
>%g<        >1234567E96<  >1.23457e+102<
>%g<        >.1234567E-101< >1.23457e-102<
>%g<        >0<           >0<
>%13g<      >1234567.89<  >  1.23457e+06<
>%+13g<     >1234567.89<  > +1.23457e+06<
>%013g<      >1234567.89< >001.23457e+06<
>%-13g<      >1234567.89< >1.23457e+06  <
>%h<        >''<          >%h INVALID<
>%i<        >123456.789<  >123456<         >Synonym for %d<
>%j<        >''<          >%j INVALID<
>%k<        >''<          >%k INVALID<
>%l<        >''<          >%l INVALID<
>%m<        >''<          >%m INVALID<
>%s< >sprintf('%%n%n %d', $n, $n)< >%n 2< >Slight sneakiness to test %n<
>%o<        >2**32-1<     >37777777777<
>%+o<       >2**32-1<     >37777777777<
>%#o<       >2**32-1<     >037777777777<
>%o<        >642<         >1202<          >check smaller octals across platforms<
>%+o<       >642<         >1202<
>%#o<       >642<         >01202<
>%d< >$p=sprintf('%p',$p);$p=~/^[0-9a-f]+$/< >1< >Coarse hack: hex from %p?<
>%#p<       >''<          >%#p INVALID<
>%q<        >''<          >%q INVALID<
>%r<        >''<          >%r INVALID<
>%s<        >'string'<    >string<
>%10s<      >'string'<    >    string<
>%+10s<     >'string'<    >    string<
>%#10s<     >'string'<    >    string<
>%010s<     >'string'<    >0000string<
>%0*s<      >[10, 'string']< >0000string<
>%-10s<     >'string'<    >string    <
>%3s<       >'string'<    >string<
>%.3s<      >'string'<    >str<
>%.*s<      >[3, 'string']< >str<
>%t<        >''<          >%t INVALID<
>%u<        >2**32-1<     >4294967295<
>%+u<       >2**32-1<     >4294967295<
>%#u<       >2**32-1<     >4294967295<
>%12u<      >2**32-1<     >  4294967295<
>%012u<     >2**32-1<     >004294967295<
>%-12u<     >2**32-1<     >4294967295  <
>%-012u<    >2**32-1<     >4294967295  <
>%v<        >''<          >%v INVALID<
>%w<        >''<          >%w INVALID<
>%x<        >2**32-1<     >ffffffff<
>%+x<       >2**32-1<     >ffffffff<
>%#x<       >2**32-1<     >0xffffffff<
>%10x<      >2**32-1<     >  ffffffff<
>%010x<     >2**32-1<     >00ffffffff<
>%-10x<     >2**32-1<     >ffffffff  <
>%-010x<    >2**32-1<     >ffffffff  <
>%0-10x<    >2**32-1<     >ffffffff  <
>%0*x<      >[-10, ,2**32-1]< >ffffffff  <
>%y<        >''<          >%y INVALID<
>%z<        >''<          >%z INVALID<
>%2$d %1$d<	>[12, 34]<	>34 12<
>%*2$d<		>[12, 3]<	> 12<
>%2$d %d<	>[12, 34]<	>34 12<
>%2$d %d %d<	>[12, 34]<	>34 12 34<
>%3$d %d %d<	>[12, 34, 56]<	>56 12 34<
>%2$*3$d %d<	>[12, 34, 3]<	> 34 12<
>%*3$2$d %d<	>[12, 34, 3]<	>%*3$2$d 34 INVALID<
>%2$d<		>12<	>0 UNINIT<
>%0$d<		>12<	>%0$d INVALID<
>%1$$d<		>12<	>%1$$d INVALID<
>%1$1$d<	>12<	>%1$1$d INVALID<
>%*2$*2$d<	>[12, 3]<	>%*2$*2$d INVALID<
>%*2*2$d<	>[12, 3]<	>%*2*2$d INVALID<
>%0v2.2d<	>''<	><
>%vc,%d<	>[63, 64, 65]<	>?,64<
>%vd,%d<	>[1, 2, 3]<	>49,2<
>%vf,%d<	>[1, 2, 3]<	>1.000000,2<
>%vp<	>''<	>%vp INVALID<
>%vs,%d<	>[1, 2, 3]<	>1,2<
>%v_<	>''<	>%v_ INVALID<
