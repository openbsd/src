#!perl
use strict;
use warnings;

BEGIN {
    chdir 't';
    require './test.pl';
}

plan(tests => 17);

my $nonfile = tempfile();

@INC = qw(Perl Rules);

# The tests for ' ' and '.h' never did fail, but previously the error reporting
# code would read memory before the start of the SV's buffer

for my $file ($nonfile, ' ') {
    eval {
	require $file;
    };

    like $@, qr/^Can't locate $file in \@INC \(\@INC contains: @INC\) at/,
	"correct error message for require '$file'";
}

eval "require $nonfile";

like $@, qr/^Can't locate $nonfile\.pm in \@INC \(you may need to install the $nonfile module\) \(\@INC contains: @INC\) at/,
    "correct error message for require $nonfile";

eval {
    require "$nonfile.ph";
};

like $@, qr/^Can't locate $nonfile\.ph in \@INC \(did you run h2ph\?\) \(\@INC contains: @INC\) at/;

for my $file ("$nonfile.h", ".h") {
    eval {
	require $file
    };

    like $@, qr/^Can't locate \Q$file\E in \@INC \(change \.h to \.ph maybe\?\) \(did you run h2ph\?\) \(\@INC contains: @INC\) at/,
	"correct error message for require '$file'";
}

for my $file ("$nonfile.ph", ".ph") {
    eval {
	require $file
    };

    like $@, qr/^Can't locate \Q$file\E in \@INC \(did you run h2ph\?\) \(\@INC contains: @INC\) at/,
	"correct error message for require '$file'";
}

eval 'require <foom>';
like $@, qr/^<> should be quotes at /, 'require <> error';

my $module   = tempfile();
my $mod_file = "$module.pm";

open my $module_fh, ">", $mod_file or die $!;
print { $module_fh } "print 1; 1;\n";
close $module_fh;

chmod 0333, $mod_file;

SKIP: {
    skip_if_miniperl("these modules may not be available to miniperl", 2);

    push @INC, '../lib';
    require Cwd;
    require File::Spec::Functions;
    if ($^O eq 'cygwin') {
        require Win32;
    }

    # Going to try to switch away from root.  Might not work.
    # (stolen from t/op/stat.t)
    my $olduid = $>;
    eval { $> = 1; };
    skip "Can't test permissions meaningfully if you're superuser", 2
        if ($^O eq 'cygwin' ? Win32::IsAdminUser() : $> == 0);

    local @INC = ".";
    eval "use $module";
    like $@,
        qr<^\QCan't locate $mod_file:>,
        "special error message if the file exists but can't be opened";

    SKIP: {
        skip "Can't make the path absolute", 1
            if !defined(Cwd::getcwd());

        my $file = File::Spec::Functions::catfile(Cwd::getcwd(), $mod_file);
        eval {
            require($file);
        };
        like $@,
            qr<^\QCan't locate $file:>,
            "...even if we use a full path";
    }

    # switch uid back (may not be implemented)
    eval { $> = $olduid; };
}

1 while unlink $mod_file;

# I can't see how to test the EMFILE case
# I can't see how to test the case of not displaying @INC in the message.
# (and does that only happen on VMS?)

# fail and print the full filename
eval { no warnings 'syscalls'; require "strict.pm\0invalid"; };
like $@, qr/^Can't locate strict\.pm\\0invalid: /, 'require nul check [perl #117265]';
eval { no warnings 'syscalls'; do "strict.pm\0invalid"; };
like $@, qr/^Can't locate strict\.pm\\0invalid: /, 'do nul check';
{
  my $WARN;
  local $SIG{__WARN__} = sub { $WARN = shift };
  eval { require "strict.pm\0invalid"; };
  like $WARN, qr{^Invalid \\0 character in pathname for require: strict\.pm\\0invalid at }, 'nul warning';
  like $@, qr{^Can't locate strict\.pm\\0invalid: }, 'nul error';

  $WARN = '';
  local @INC = @INC;
  unshift @INC, "lib\0invalid";
  eval { require "unknown.pm" };
  like $WARN, qr{^Invalid \\0 character in \@INC entry for require: lib\\0invalid at }, 'nul warning';
}
eval "require strict\0::invalid;";
like $@, qr/^syntax error at \(eval \d+\) line 1/, 'parse error with \0 in barewords module names';

