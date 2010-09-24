package MakeMaker::Test::Setup::PL_FILES;

@ISA = qw(Exporter);
require Exporter;
@EXPORT = qw(setup teardown);

use strict;
use File::Path;
use File::Basename;
use File::Spec;
use MakeMaker::Test::Utils;

my %Files = (
             'PL_FILES-Module/Makefile.PL'   => <<'END',
use ExtUtils::MakeMaker;

# A module for testing PL_FILES
WriteMakefile(
    NAME     => 'PL_FILES::Module',
    PL_FILES => { 'single.PL' => 'single.out',
                  'multi.PL'  => [qw(1.out 2.out)],
                  'Bar_pm.PL' => '$(INST_LIB)/PL/Bar.pm',
    }
);
END

	     'PL_FILES-Module/single.PL'        => _gen_pl_files(),
	     'PL_FILES-Module/multi.PL'         => _gen_pl_files(),
	     'PL_FILES-Module/Bar_pm.PL'        => _gen_pm_files(),
	     'PL_FILES-Module/lib/PL/Foo.pm' => <<'END',
# Module to load to ensure PL_FILES have blib in @INC.
package PL::Foo;
sub bar { 42 }
1;
END

);


sub _gen_pl_files {
    my $test = <<'END';
#!/usr/bin/perl -w

# Ensure we have blib in @INC
use PL::Foo;
die unless PL::Foo::bar() == 42;

# Had a bug where PL_FILES weren't sent the file to generate
die "argv empty\n" unless @ARGV;
die "too many in argv: @ARGV\n" unless @ARGV == 1;

my $file = $ARGV[0];
open OUT, ">$file" or die $!;

print OUT "Testing\n";
close OUT
END

    $test =~ s/^\n//;

    return $test;
}


sub _gen_pm_files {
    my $test = <<'END';
#!/usr/bin/perl -w

# Ensure we do NOT have blib in @INC when building a module
eval { require PL::Foo; };
#die $@ unless $@ =~ m{^Can't locate PL/Foo.pm in \@INC };

# Had a bug where PL_FILES weren't sent the file to generate
die "argv empty\n" unless @ARGV;
die "too many in argv: @ARGV\n" unless @ARGV == 1;

my $file = $ARGV[0];
open OUT, ">$file" or die $!;

print OUT "Testing\n";
close OUT
END

    $test =~ s/^\n//;

    return $test;
}


sub setup {
    setup_mm_test_root();
    chdir 'MM_TEST_ROOT:[t]' if $^O eq 'VMS';

    while(my($file, $text) = each %Files) {
        # Convert to a relative, native file path.
        $file = File::Spec->catfile(File::Spec->curdir, split m{\/}, $file);

        my $dir = dirname($file);
        mkpath $dir;
        open(FILE, ">$file") || die "Can't create $file: $!";
        print FILE $text;
        close FILE;

        # ensure file at least 1 second old for makes that assume
        # files with the same time are out of date.
        my $time = calibrate_mtime();
        utime $time, $time - 1, $file;
    }

    return 1;
}

sub teardown { 
    foreach my $file (keys %Files) {
        my $dir = dirname($file);
        if( -e $dir ) {
            rmtree($dir) || return;
        }
    }
    return 1;
}
