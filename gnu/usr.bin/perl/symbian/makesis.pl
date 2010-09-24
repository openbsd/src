#!/usr/bin/perl -w

# Copyright (c) 2004-2005 Nokia.  All rights reserved.

use strict;
use lib "symbian";

do "sanity.pl" or die $@;

my %VERSION = %{ do "version.pl" or die $@ };
my $VERSION = "$VERSION{REVISION}$VERSION{VERSION}$VERSION{SUBVERSION}";
my $R_V_SV  = "$VERSION{REVISION}.$VERSION{VERSION}.$VERSION{SUBVERSION}";

my ($SYMBIAN_ROOT, $SYMBIAN_VERSION, $SDK_NAME, $SDK_VARIANT, $SDK_VERSION) =
    @{ do "sdk.pl" or die $@ };
my $UID  = do "uid.pl" or die $@;
my %PORT = %{ do "port.pl" or die $@ };

my $ARM = 'armv5';#'thumb'; # TODO
my $S60SDK = $ENV{S60SDK}; # from sdk.pl
my $S80SDK = $ENV{S80SDK}; # from sdk.pl
my $S90SDK = $ENV{S90SDK}; # from sdk.pl

my $UREL = $ENV{UREL}; # from sdk.pl
$UREL =~ s/-ARM-/$ARM/;

my $app = '!:\System\Apps\Perl';
my $lib = '!:\System\Libs';

my @target = @ARGV
  ? @ARGV
  : (
    "miniperl",          "perl",
    "perl${VERSION}dll", "perl${VERSION}lib",
    "perl${VERSION}ext"
  );

my %suffix;
@suffix{ "miniperl", "perl", "perl$VERSION" } = ( "exe", "exe", "dll", );

for my $target (@target) {
    $target = "perl${VERSION}" if $target eq "perl${VERSION}dll";

    my %copy;
    my $pkg = "$target.pkg";
    print "\nCreating $pkg...\n";

    my $suffix = $suffix{$target} || "";
    my $dst = $suffix eq "dll" ? $lib : $app;

    my $srctarget = "$UREL\\$target.$suffix";

    if ( $target =~ /^(miniperl|perl|perl${VERSION}(?:dll)?)$/ ) {
        $copy{$srctarget} = "$dst\\$target.$suffix";
        print "\t$target.$suffix\n";
    }
    if ( $target eq "perl${VERSION}lib" ) {
        print "Libraries...\n";

        print "\tConfig.pm\n";
        $copy{"lib\\Config.pm"} =
          "$lib\\Perl\\$R_V_SV\\thumb-symbian\\Config.pm";

        print "\tConfig_heavy.pl\n";
        $copy{"xlib\\symbian\\Config_heavy.pl"} =
          "$lib\\Perl\\$R_V_SV\\thumb-symbian\\Config_heavy.pl";

        print "\tDynaLoader.pm\n";
        $copy{"ext\\DynaLoader\\DynaLoader.pm"} =
          "$lib\\Perl\\$R_V_SV\\DynaLoader.pm";

        print "\tErrno.pm\n";
        $copy{"ext\\Errno\\Errno.pm"} = "$lib\\Perl\\$R_V_SV\\Errno.pm";

        open( my $cfg, "symbian/install.cfg" )
          or die "$!: symbian/install.cfg: $!\n";
        while (<$cfg>) {
            next unless /^lib\s+(.+)/;
            chomp;
            my $f = $1;
	    unless (-f "lib/$f") {
		warn qq[$0: No "lib/$f", skipping...\n];
		next;
	    }
            $f =~ s:/:\\:g;
            $copy{"lib\\$f"} = "$lib\\Perl\\$R_V_SV\\$f";
            print "\t$f\n";
        }
        close($cfg);
    }

    if ( $target eq "perl${VERSION}ext" ) {
        my @lst = glob("symbian/*.lst");
        print "Extensions...\n";
        print "\t(none found)\n" unless @lst;
        for my $lst (@lst) {
            $lst =~ m:^symbian/(.+)\.:;
            my $ext = $1;
            $ext =~ s!-!::!g;
            print "\t$ext\n";
            if ( open( my $pkg, $lst ) ) {
                while (<$pkg>) {
                    if (m!^"(.+)"-"(.+)"$!) {
                        my ( $src, $dst ) = ( $1, $2 );
                        $copy{$src} = $dst;
                    }
                    else {
                        warn "$0: $lst: $.: unknown syntax\n";
                    }
                }
                close($pkg);
            }
            else {
                warn "$0: $lst: $!\n";
            }
        }
    }

    for my $file ( keys %copy ) {
        warn "$0: $file does not exist\n" unless -f $file;
    }

    my @copy = map { qq["$_"-"$copy{$_}"] } sort keys %copy;
    my $copy = join( "\n", @copy );

    my %UID = (
        "miniperl"          => 0,
        "perl"              => 0,
        "perl${VERSION}"    => $UID + 0,
        "perl${VERSION}dll" => $UID + 0,
        "perl${VERSION}ext" => $UID + 1,
        "perl${VERSION}lib" => $UID + 2,
        "perlapp"           => $UID + 3,
        "perlrecog"         => $UID + 4,
        "perlappmin"        => $UID + 5,
    );

    die "$0: target has no UID\n" unless defined $UID{$target};

    my $uid = sprintf( "0x%08X", $UID{$target} );

    my ( $MAJOR, $MINOR, $PATCH ) = ( 0, 0, 0 );

    if ( $target =~ m:^perl$VERSION(dll|ext|lib)?$: ) {
        my $pkg = defined $1 ? $1 : "dll";
        $MAJOR = $PORT{$pkg}->{MAJOR};
        $MINOR = $PORT{$pkg}->{MINOR};
        $PATCH = $PORT{$pkg}->{PATCH};
    }

    die "$0: Bad version for $target\n"
      unless defined $MAJOR
      && ( $MAJOR eq 0 || $MAJOR > 0 )
      && defined $MINOR
      && ( $MINOR eq 0 || $MINOR > 0 )
      && defined $PATCH
      && ( $PATCH eq 0 || $PATCH > 0 );

     my $ProductId =
         defined $S60SDK ?
qq[;Supports Series 60 v0.9\n(0x101F6F88), 0, 0, 0, {"Series60ProductID"}\n] :
         defined $S80SDK ?
qq[;Supports Series 80 v2.0\n(0x101F8ED2), 0, 0, 0, {"Series80ProductID"}\n] :
         defined $S90SDK ?
qq[;Supports Series 90 v1.1\n(0x101FBE05), 0, 0, 0, {"Series90ProductID"}\n] :
         ";Supports Series NN";

    open PKG, ">$pkg" or die "$0: failed to create $pkg: $!\n";
    print PKG <<__EOF__;
; \u$target installation script
;
; The supported languages
&EN;
;
; The installation name and header data
;
#{"\u$target"},($uid),$MAJOR,$MINOR,$PATCH
;
;Localised Vendor name
%{"Vendor-EN"}
;
; Private key and certificate (unused)
;
;* "\u$target.key", "\u$target.cer"
;
$ProductId
; The files to install
;
$copy
__EOF__
    close PKG;

    print "Created $pkg\n";

    print "Running makesis...\n";

    unlink("$target.sis");

    system("makesis $pkg") == 0
      || die "$0: makesis $pkg failed: $!\n";
}

exit(0);
