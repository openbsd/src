#!perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;
use warnings;
use Test::More 'no_plan'; # otherwise it would 38401 tests, which is, uh, a lot. :-)
my @pats=(
            "\\w",
	    "\\W",
	    "\\s",
	    "\\S",
	    "\\d",
	    "\\D",
	    "[:alnum:]",
	    "[:^alnum:]",
	    "[:alpha:]",
	    "[:^alpha:]",
	    "[:ascii:]",
	    "[:^ascii:]",
	    "[:cntrl:]",
	    "[:^cntrl:]",
	    "[:graph:]",
	    "[:^graph:]",
	    "[:lower:]",
	    "[:^lower:]",
	    "[:print:]",
	    "[:^print:]",
	    "[:punct:]",
	    "[:^punct:]",
	    "[:upper:]",
	    "[:^upper:]",
	    "[:xdigit:]",
	    "[:^xdigit:]",
	    "[:space:]",
	    "[:^space:]",
	    "[:blank:]",
	    "[:^blank:]" );
if (not $ENV{REAL_POSIX_CC}) {
    $TODO = "Only works under PERL_LEGACY_UNICODE_CHARCLASS_MAPPINGS = 0";
}

sub rangify {
    my $ary= shift;
    my $fmt= shift || '%d';
    my $sep= shift || ' ';
    my $rng= shift || '..';
    
    
    my $first= $ary->[0];
    my $last= $ary->[0];
    my $ret= sprintf $fmt, $first;
    for my $idx (1..$#$ary) {
        if ( $ary->[$idx] != $last + 1) {
            if ($last!=$first) {
                $ret.=sprintf "%s$fmt",$rng, $last;
            }             
            $first= $last= $ary->[$idx];
            $ret.=sprintf "%s$fmt",$sep,$first;
         } else {
            $last= $ary->[$idx];
         }
    }
    if ( $last != $first) {
        $ret.=sprintf "%s$fmt",$rng, $last;
    }
    return $ret;
}

my $description = "";
while (@pats) {
    my ($yes,$no)= splice @pats,0,2;
    
    my %err_by_type;
    my %singles;
    my %complements;
    foreach my $b (0..255) {
        my %got;
        for my $type ('unicode','not-unicode') {
            my $str=chr($b).chr($b);
            if ($type eq 'unicode') {
                $str.=chr(256);
                chop $str;
            }
            if ($str=~/[$yes][$no]/){
                TODO: {
                    unlike($str,qr/[$yes][$no]/,
                        "chr($b)=~/[$yes][$no]/ should not match under $type");
                }
                push @{$err_by_type{$type}},$b;
            }
            $got{"[$yes]"}{$type} = $str=~/[$yes]/ ? 1 : 0;
            $got{"[$no]"}{$type} = $str=~/[$no]/ ? 1 : 0;
            $got{"[^$yes]"}{$type} = $str=~/[^$yes]/ ? 1 : 0;
            $got{"[^$no]"}{$type} = $str=~/[^$no]/ ? 1 : 0;
        }
        foreach my $which ("[$yes]","[$no]","[^$yes]","[^$no]") {
            if ($got{$which}{'unicode'} != $got{$which}{'not-unicode'}){
                TODO: {
                    is($got{$which}{'unicode'},$got{$which}{'not-unicode'},
                        "chr($b)=~/$which/ should have the same results regardless of internal string encoding");
                }
                push @{$singles{$which}},$b;
            }
        }
        foreach my $which ($yes,$no) {
            foreach my $strtype ('unicode','not-unicode') {
                if ($got{"[$which]"}{$strtype} == $got{"[^$which]"}{$strtype}) {
                    TODO: {
                        isnt($got{"[$which]"}{$strtype},$got{"[^$which]"}{$strtype},
                            "chr($b)=~/[$which]/ should not have the same result as chr($b)=~/[^$which]/");
                    }
                    push @{$complements{$which}{$strtype}},$b;
                }
            }
        }
    }
    
    
    if (%err_by_type || %singles || %complements) {
        $description||=" Error:\n";
        $description .= "/[$yes][$no]/\n";
        if (%err_by_type) {
            foreach my $type (sort keys %err_by_type) {
                $description .= "\tmatches $type codepoints:\t";
                $description .= rangify($err_by_type{$type});
                $description .= "\n";
            }
            $description .= "\n";
        }
        if (%singles) {
            $description .= "Unicode/Nonunicode mismatches:\n";
            foreach my $type (sort keys %singles) {
                $description .= "\t$type:\t";
                $description .= rangify($singles{$type});
                $description .= "\n";
            }
            $description .= "\n";
        }
        if (%complements) {
            foreach my $class (sort keys %complements) {
                foreach my $strtype (sort keys %{$complements{$class}}) {
                    $description .= "\t$class has complement failures under $strtype for:\t";
                    $description .= rangify($complements{$class}{$strtype});
                    $description .= "\n";
                }
            }
        }
    }
}
TODO: {
    is( $description, "", "POSIX and perl charclasses should not depend on string type");
}

__DATA__
