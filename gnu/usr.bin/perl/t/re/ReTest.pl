#!./perl
#
# This is the test subs used for regex testing. 
# This used to be part of re/pat.t
use warnings;
use strict;
use 5.010;
use base qw/Exporter/;
use Carp;
use vars qw(
    $EXPECTED_TESTS 
    $TODO
    $Message
    $Error
    $DiePattern
    $WarnPattern
    $BugId
    $PatchId
    $running_as_thread
    $IS_ASCII
    $IS_EBCDIC
    $ordA
);

$| = 1;

$Message ||= "Noname test";

our $ordA = ord ('A');  # This defines ASCII/UTF-8 vs EBCDIC/UTF-EBCDIC
# This defined the platform.
our $IS_ASCII  = $ordA ==  65;
our $IS_EBCDIC = $ordA == 193;

use vars '%Config';
eval 'use Config';          #  Defaults assumed if this fails

my $test = 0;
my $done_plan;
sub plan {
    my (undef,$tests)= @_;
    if (defined $tests) {
        die "Number of tests already defined! ($EXPECTED_TESTS)"
            if $EXPECTED_TESTS;
        $EXPECTED_TESTS= $tests;
    }
    if ($EXPECTED_TESTS) {
        print "1..$EXPECTED_TESTS\n" if !$done_plan++;
    } else {
        print "Number of tests not declared!";
    }
}

sub pretty {
    my ($mess) = @_;
    $mess =~ s/\n/\\n/g;
    $mess =~ s/\r/\\r/g;
    $mess =~ s/\t/\\t/g;
    $mess =~ s/([\00-\37\177])/sprintf '\%03o', ord $1/eg;
    $mess =~ s/#/\\#/g;
    $mess;
}

sub safe_globals {
    defined($_) and s/#/\\#/g for $BugId, $PatchId, $TODO;
}

sub _ok {
    my ($ok, $mess, $error) = @_;
    plan();
    safe_globals();
    $mess    = pretty ($mess // $Message);
    $mess   .= "; Bug $BugId"     if defined $BugId;
    $mess   .= "; Patch $PatchId" if defined $PatchId;
    $mess   .= " # TODO $TODO"     if defined $TODO;

    my $line_nr = (caller(1)) [2];

    printf "%sok %d - %s\n",
              ($ok ? "" : "not "),
              ++ $test,
              "$mess\tLine $line_nr";

    unless ($ok) {
        print "# Failed test at line $line_nr\n" unless defined $TODO;
        if ($error //= $Error) {
            no warnings 'utf8';
            chomp $error;
            $error = join "\n#", map {pretty $_} split /\n\h*#/ => $error;
            $error = "# $error" unless $error =~ /^\h*#/;
            print $error, "\n";
        }
    }

    return $ok;
}

# Force scalar context on the pattern match
sub  ok ($;$$) {_ok  $_ [0], $_ [1], $_ [2]}
sub nok ($;$$) {_ok !$_ [0], "Failed: " . ($_ [1] // $Message), $_ [2]}


sub skip {
    my $why = shift;
    safe_globals();
    $why =~ s/\n.*//s;
    $why .= "; Bug $BugId" if defined $BugId;
    # seems like the new harness code doesnt like todo and skip to be mixed.
    # which seems like a bug in the harness to me. -- dmq
    #$why .= " # TODO $TODO" if defined $TODO;
    
    my $n = shift // 1;
    my $line_nr = (caller(0)) [2];
    for (1 .. $n) {
        ++ $test;
        #print "not " if defined $TODO;
        print "ok $test # skip $why\tLine $line_nr\n";
    }
    no warnings "exiting";
    last SKIP;
}

sub iseq ($$;$) { 
    my ($got, $expect, $name) = @_;
    
    $_ = defined ($_) ? "'$_'" : "undef" for $got, $expect;
        
    my $ok    = $got eq $expect;
    my $error = "# expected: $expect\n" .
                "#   result: $got";

    _ok $ok, $name, $error;
}   

sub isneq ($$;$) { 
    my ($got, $expect, $name) = @_;
    my $todo = $TODO ? " # TODO $TODO" : '';
    
    $_ = defined ($_) ? "'$_'" : "undef" for $got, $expect;
        
    my $ok    = $got ne $expect;
    my $error = "# results are equal ($got)";

    _ok $ok, $name, $error;
}   


sub eval_ok ($;$) {
    my ($code, $name) = @_;
    local $@;
    if (ref $code) {
        _ok eval {&$code} && !$@, $name;
    }
    else {
        _ok eval  ($code) && !$@, $name;
    }
}

sub must_die {
    my ($code, $pattern, $name) = @_;
    $pattern //= $DiePattern
        or Carp::confess("Bad pattern");
    undef $@;
    ref $code ? &$code : eval $code;
    my  $r = $@ && $@ =~ /$pattern/;
    _ok $r, $name // $Message // "\$\@ =~ /$pattern/";
}

sub must_warn {
    my ($code, $pattern, $name) = @_;
    $pattern //= $WarnPattern;
    my $w;
    local $SIG {__WARN__} = sub {$w .= join "" => @_};
    use warnings 'all';
    ref $code ? &$code : eval $code;
    my $r = $w && $w =~ /$pattern/;
    $w //= "UNDEF";
    _ok $r, $name // $Message // "Got warning /$pattern/",
            "# expected: /$pattern/\n" .
            "#   result: $w";
}

sub may_not_warn {
    my ($code, $name) = @_;
    my $w;
    local $SIG {__WARN__} = sub {$w .= join "" => @_};
    use warnings 'all';
    ref $code ? &$code : eval $code;
    _ok !$w, $name // ($Message ? "$Message (did not warn)"
                                : "Did not warn"),
             "Got warning '$w'";
}

1;
