#!perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc( qw(../lib) );
}

use strict;
use warnings;

plan(tests => 57);

my $nonfile = tempfile();

# The tests for ' ' and '.h' never did fail, but previously the error reporting
# code would read memory before the start of the SV's buffer

for my $file ($nonfile, ' ') {
    eval {
	require $file;
    };

    like $@, qr/^Can't locate $file in \@INC \(\@INC contains: @INC\) at/,
	"correct error message for require '$file'";
}

# Check that the "(you may need to install..) hint is included in the
# error message where (and only where) appropriate.
#
# Basically the hint should be issued for any filename where converting
# back from Foo/Bar.pm to Foo::Bar gives you a legal bare word which could
# follow "require" in source code.

{

    # may be any letter of an identifier
    my $I = "\x{393}";  # "\N{GREEK CAPITAL LETTER GAMMA}"
    # Continuation char: may only be 2nd+ letter of an identifier
    my $C = "\x{387}";  # "\N{GREEK ANO TELEIA}"

    for my $test_data (
        # thing to require        pathname in err mesg     err includes hint?
        [ "No::Such::Module1",          "No/Such/Module1.pm",       1 ],
        [ "'No/Such/Module1.pm'",       "No/Such/Module1.pm",       1 ],
        [ "_No::Such::Module1",         "_No/Such/Module1.pm",      1 ],
        [ "'_No/Such/Module1.pm'",      "_No/Such/Module1.pm",      1 ],
        [ "'No/Such./Module.pm'",       "No/Such./Module.pm",       0 ],
        [ "No::1Such::Module",          "No/1Such/Module.pm",       1 ],
        [ "'No/1Such/Module.pm'",       "No/1Such/Module.pm",       1 ],
        [ "1No::Such::Module",           undef,                     0 ],
        [ "'1No/Such/Module.pm'",       "1No/Such/Module.pm",       0 ],

        # utf8 variants
        [ "No::Such${I}::Module1",      "No/Such${I}/Module1.pm",   1 ],
        [ "'No/Such${I}/Module1.pm'",   "No/Such${I}/Module1.pm",   1 ],
        [ "_No::Such${I}::Module1",     "_No/Such${I}/Module1.pm",  1 ],
        [ "'_No/Such${I}/Module1.pm'",  "_No/Such${I}/Module1.pm",  1 ],
        [ "'No/Such${I}./Module.pm'",   "No/Such${I}./Module.pm",   0 ],
        [ "No::1Such${I}::Module",      "No/1Such${I}/Module.pm",   1 ],
        [ "'No/1Such${I}/Module.pm'",   "No/1Such${I}/Module.pm",   1 ],
        [ "1No::Such${I}::Module",       undef,                     0 ],
        [ "'1No/Such${I}/Module.pm'",   "1No/Such${I}/Module.pm",   0 ],

        # utf8 with continuation char in 1st position
        [ "No::${C}Such::Module1",      undef,                      0 ],
        [ "'No/${C}Such/Module1.pm'",   "No/${C}Such/Module1.pm",   0 ],
        [ "_No::${C}Such::Module1",     undef,                      0 ],
        [ "'_No/${C}Such/Module1.pm'",  "_No/${C}Such/Module1.pm",  0 ],
        [ "'No/${C}Such./Module.pm'",   "No/${C}Such./Module.pm",   0 ],
        [ "No::${C}1Such::Module",      undef,                      0 ],
        [ "'No/${C}1Such/Module.pm'",   "No/${C}1Such/Module.pm",   0 ],
        [ "1No::${C}Such::Module",      undef,                      0 ],
        [ "'1No/${C}Such/Module.pm'",   "1No/${C}Such/Module.pm",   0 ],

    ) {
        my ($require_arg, $err_path, $has_hint) = @$test_data;

        my $exp;
        if (defined $err_path) {
            $exp = "Can't locate $err_path in \@INC";
            if ($has_hint) {
                my $hint = $err_path;
                $hint =~ s{/}{::}g;
                $hint =~ s/\.pm$//;
                $exp .= " (you may need to install the $hint module)";
            }
            $exp .= " (\@INC contains: @INC) at";
        }
        else {
            # undef implies a require which doesn't compile,
            # rather than one which triggers a run-time error.
            # We'll set exp to a suitable value later;
            $exp = "";
        }

        my $err;
        {
            no warnings qw(syntax utf8);
            if ($require_arg =~ /[^\x00-\xff]/) {
                eval "require $require_arg";
                $err = $@;
                utf8::decode($err);
            }
            else {
                eval "require $require_arg";
                $err = $@;
            }
        }

        for ($err, $exp, $require_arg) {
            s/([^\x00-\xff])/sprintf"\\x{%x}",ord($1)/ge;
        }
        if (length $exp) {
            $exp = qr/^\Q$exp\E/;
        }
        else {
            $exp = qr/syntax error at|Unrecognized character/;
        }
        like $err, $exp,
                "err for require $require_arg";
    }
}



eval "require ::$nonfile";

like $@, qr/^Bareword in require must not start with a double-colon:/,
        "correct error message for require ::$nonfile";

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
like $@, qr/^<> at require-statement should be quotes at /, 'require <> error';

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
{
  my $WARN;
  local $SIG{__WARN__} = sub { $WARN = shift };
  {
    my $ret = do "strict.pm\0invalid";
    my $exc = $@;
    my $err = $!;
    is $ret, undef, 'do nulstring returns undef';
    is $exc, '',    'do nulstring clears $@';
    $! = $err;
    ok $!{ENOENT},  'do nulstring fails with ENOENT';
    like $WARN, qr{^Invalid \\0 character in pathname for do: strict\.pm\\0invalid at }, 'do nulstring warning';
  }

  $WARN = '';
  eval { require "strict.pm\0invalid"; };
  like $WARN, qr{^Invalid \\0 character in pathname for require: strict\.pm\\0invalid at }, 'nul warning';
  like $@, qr{^Can't locate strict\.pm\\0invalid: }, 'nul error';

  $WARN = '';
  local @INC = @INC;
  set_up_inc( "lib\0invalid" );
  eval { require "unknown.pm" };
  like $WARN, qr{^Invalid \\0 character in \@INC entry for require: lib\\0invalid at }, 'nul warning';
}
eval "require strict\0::invalid;";
like $@, qr/^syntax error at \(eval \d+\) line 1/, 'parse error with \0 in barewords module names';

# Refs and globs that stringify with embedded nulls
# These crashed from 5.20 to 5.24 [perl #128182].
eval { no warnings 'syscalls'; require eval "qr/\0/" };
like $@, qr/^Can't locate \(\?\^:\\0\):/,
    'require ref that stringifies with embedded null';
eval { no strict; no warnings 'syscalls'; require *{"\0a"} };
like $@, qr/^Can't locate \*main::\\0a:/,
    'require ref that stringifies with embedded null';

eval { require undef };
like $@, qr/^Missing or undefined argument to require /;

eval { do undef };
like $@, qr/^Missing or undefined argument to do /;

eval { require "" };
like $@, qr/^Missing or undefined argument to require /;

eval { do "" };
like $@, qr/^Missing or undefined argument to do /;

# non-searchable pathnames shouldn't mention @INC in the error

my $nonsearch = "./no_such_file.pm";

eval "require \"$nonsearch\"";

like $@, qr/^Can't locate \Q$nonsearch\E at/,
        "correct error message for require $nonsearch";

{
    # make sure require doesn't treat a non-PL_sv_undef undef as
    # success in %INC
    # GH #17428
    push @INC, "lib";
    ok(!eval { require CannotParse; }, "should fail to load");
    local %INC = %INC; # copies \&PL_sv_undef into a new undef
    ok(!eval { require CannotParse; },
       "check the second attempt also fails");
    like $@, qr/Attempt to reload/, "check we failed for the right reason";
}
