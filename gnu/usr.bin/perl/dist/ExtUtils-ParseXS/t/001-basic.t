#!/usr/bin/perl

use strict;
use Test::More;
use Config;
use DynaLoader;
use ExtUtils::CBuilder;
use lib (-d 't' ? File::Spec->catdir(qw(t lib)) : 'lib');
use PrimitiveCapture;

my ($source_file, $obj_file, $lib_file);

require_ok( 'ExtUtils::ParseXS' );

# Borrow the useful heredoc quoting/indenting function.
*Q = \&ExtUtils::ParseXS::Q;


{
    # Minimal tie package to capture output to a filehandle
    package Capture;
    sub TIEHANDLE { bless {} }
    sub PRINT { shift->{buf} .= join '', @_ }
    sub PRINTF    { my $obj = shift; my $fmt = shift;
                    $obj->{buf} .= sprintf $fmt, @_ }
    sub content { shift->{buf} }
}

chdir('t') if -d 't';
push @INC, '.';

package ExtUtils::ParseXS;
our $DIE_ON_ERROR = 1;
our $AUTHOR_WARNINGS = 1;
package main;

use Carp; #$SIG{__WARN__} = \&Carp::cluck;

# The linker on some platforms doesn't like loading libraries using relative
# paths. Android won't find relative paths, and system perl on macOS will
# refuse to load relative paths. The path that DynaLoader uses to load the
# .so or .bundle file is based on the @INC path that the library is loaded
# from. The XSTest module we're using for testing is in the current directory,
# so we need an absolute path in @INC rather than '.'. Just convert all of the
# paths to absolute for simplicity.
@INC = map { File::Spec->rel2abs($_) } @INC;



#########################

# test_many(): test a list of XSUB bodies with a common XS preamble.
# $prefix is the prefix of the XSUB's name, in order to be able to extract
# out the C function definition. Typically the generated C subs look like:
#
#    XS_EXTERNAL(XS_Foo_foo)
#    {
#    ...
#    }
# So setting prefix to 'XS_Foo' will match any fn declared in the Foo
# package, while 'boot_Foo' will extract the boot fn.
#
# For each body, a series of regexes is matched against the STDOUT or
# STDERR produced.
#
# $test_fns is an array ref, where each element is an array ref consisting
# of:
#  
# [
#     "common prefix for test descriptions",
#     [ ... lines to be ...
#       ... used as ...
#       ... XSUB body...
#     ],
#     [ check_stderr, expect_nomatch, qr/expected/, "test description"],
#     [ ... and more tests ..]
#     ....
# ]
#
#  where:
#  check_stderr:   boolean: test STDERR against regex rather than STDOUT
#  expect_nomatch: boolean: pass if the regex *doesn't* match

sub test_many {
    my ($preamble, $prefix, $test_fns) = @_;
    for my $test_fn (@$test_fns) {
        my ($desc_prefix, $xsub_lines, @tests) = @$test_fn;

        my $text = $preamble;
        $text .= "$_\n" for @$xsub_lines;

        tie *FH, 'Capture';
        my $pxs = ExtUtils::ParseXS->new;
        my $err;
        my $stderr = PrimitiveCapture::capture_stderr(sub {
            eval {
                $pxs->process_file( filename => \$text, output => \*FH);
            };
            $err = $@;
        });

        my $out = tied(*FH)->content;
        untie *FH;

        if (length $err) {
            die "$desc_prefix: eval error, aborting:\n$err\n";
        }

        # trim the output to just the function in question to make
        # test diagnostics smaller.
        if ($out =~ /\S/) {
            $out =~ s/\A.*? (^\w+\(${prefix} .*? ^}).*\z/$1/xms
                or do {
                    # print STDERR $out;
                    die "$desc_prefix: couldn't trim output to only function starting '$prefix'\n";
                }
        }

        my $err_tested;
        for my $test (@tests) {
            my ($is_err, $exp_nomatch, $qr, $desc) = @$test;
            $desc = "$desc_prefix: $desc" if length $desc_prefix;
            my $str;
            if ($is_err) {
                $err_tested = 1;
                $str = $stderr;
            }
            else {
                $str = $out;
            }
            if ($exp_nomatch) {
                unlike $str, $qr, $desc;
            }
            else {
                like $str, $qr, $desc;
            }
        }
        # if there were no tests that expect an error, test that there
        # were no errors
        if (!$err_tested) {
            is $stderr, undef, "$desc_prefix: no errors expected";
        }
    }
}

#########################


{ # first block: try without linenumbers
my $pxs = ExtUtils::ParseXS->new;
# Try sending to filehandle
tie *FH, 'Capture';
$pxs->process_file( filename => 'XSTest.xs', output => \*FH, prototypes => 1 );
like tied(*FH)->content, '/is_even/', "Test that output contains some text";

$source_file = 'XSTest.c';

# Try sending to file
$pxs->process_file(filename => 'XSTest.xs', output => $source_file, prototypes => 0);
ok -e $source_file, "Create an output file";

my $quiet = $ENV{PERL_CORE} && !$ENV{HARNESS_ACTIVE};
my $b = ExtUtils::CBuilder->new(quiet => $quiet);

SKIP: {
  skip "no compiler available", 2
    if ! $b->have_compiler;
  $obj_file = $b->compile( source => $source_file );
  ok $obj_file, "ExtUtils::CBuilder::compile() returned true value";
  ok -e $obj_file, "Make sure $obj_file exists";
}

SKIP: {
  skip "no dynamic loading", 5
    if !$b->have_compiler || !$Config{usedl};
  my $module = 'XSTest';
  $lib_file = $b->link( objects => $obj_file, module_name => $module );
  ok $lib_file, "ExtUtils::CBuilder::link() returned true value";
  ok -e $lib_file,  "Make sure $lib_file exists";

  eval {require XSTest};
  is $@, '', "No error message recorded, as expected";
  ok  XSTest::is_even(8),
    "Function created thru XS returned expected true value";
  ok !XSTest::is_even(9),
    "Function created thru XS returned expected false value";

  # Win32 needs to close the DLL before it can unlink it, but unfortunately
  # dl_unload_file was missing on Win32 prior to perl change #24679!
  if ($^O eq 'MSWin32' and defined &DynaLoader::dl_unload_file) {
    for (my $i = 0; $i < @DynaLoader::dl_modules; $i++) {
      if ($DynaLoader::dl_modules[$i] eq $module) {
        DynaLoader::dl_unload_file($DynaLoader::dl_librefs[$i]);
        last;
      }
    }
  }
}

my $seen = 0;
open my $IN, '<', $source_file
  or die "Unable to open $source_file: $!";
while (my $l = <$IN>) {
  $seen++ if $l =~ m/#line\s1\s/;
}
is( $seen, 1, "Line numbers created in output file, as intended" );
{
    #rewind .c file and regexp it to look for code generation problems
    local $/ = undef;
    seek($IN, 0, 0);
    my $filecontents = <$IN>;
    $filecontents =~ s/^#if defined\(__HP_cc\).*\n#.*\n#endif\n//gm;
    my $good_T_BOOL_re =
qr|\QXS_EUPXS(XS_XSTest_T_BOOL)\E
.+?
#line \d+\Q "XSTest.c"
	ST(0) = boolSV(RETVAL);
    }
    XSRETURN(1);
}
\E|s;
    like($filecontents, $good_T_BOOL_re, "T_BOOL doesn\'t have an extra sv_newmortal or sv_2mortal");

    my $good_T_BOOL_2_re =
qr|\QXS_EUPXS(XS_XSTest_T_BOOL_2)\E
.+?
#line \d+\Q "XSTest.c"
	sv_setsv(ST(0), boolSV(in));
	SvSETMAGIC(ST(0));
    }
    XSRETURN(1);
}
\E|s;
    like($filecontents, $good_T_BOOL_2_re, 'T_BOOL_2 doesn\'t have an extra sv_newmortal or sv_2mortal');
    my $good_T_BOOL_OUT_re =
qr|\QXS_EUPXS(XS_XSTest_T_BOOL_OUT)\E
.+?
#line \d+\Q "XSTest.c"
	sv_setsv(ST(0), boolSV(out));
	SvSETMAGIC(ST(0));
    }
    XSRETURN_EMPTY;
}
\E|s;
    like($filecontents, $good_T_BOOL_OUT_re, 'T_BOOL_OUT doesn\'t have an extra sv_newmortal or sv_2mortal');

}
close $IN or die "Unable to close $source_file: $!";

unless ($ENV{PERL_NO_CLEANUP}) {
  for ( $obj_file, $lib_file, $source_file) {
    next unless defined $_;
    1 while unlink $_;
  }
}
}

#####################################################################

{ # second block: try with linenumbers
my $pxs = ExtUtils::ParseXS->new;
# Try sending to filehandle
tie *FH, 'Capture';
$pxs->process_file(
    filename => 'XSTest.xs',
    output => \*FH,
    prototypes => 1,
    linenumbers => 0,
);
like tied(*FH)->content, '/is_even/', "Test that output contains some text";

$source_file = 'XSTest.c';

# Try sending to file
$pxs->process_file(
    filename => 'XSTest.xs',
    output => $source_file,
    prototypes => 0,
    linenumbers => 0,
);
ok -e $source_file, "Create an output file";


my $seen = 0;
open my $IN, '<', $source_file
  or die "Unable to open $source_file: $!";
while (my $l = <$IN>) {
  $seen++ if $l =~ m/#line\s1\s/;
}
close $IN or die "Unable to close $source_file: $!";
is( $seen, 0, "No linenumbers created in output file, as intended" );

unless ($ENV{PERL_NO_CLEANUP}) {
  for ( $obj_file, $lib_file, $source_file) {
    next unless defined $_;
    1 while unlink $_;
  }
}
}
#####################################################################

{ # third block: broken typemap
my $pxs = ExtUtils::ParseXS->new;
tie *FH, 'Capture';
my $stderr = PrimitiveCapture::capture_stderr(sub {
  $pxs->process_file(filename => 'XSBroken.xs', output => \*FH);
});
like $stderr, '/No INPUT definition/', "Exercise typemap error";
}
#####################################################################

{ # fourth block: https://github.com/Perl/perl5/issues/19661
  my $pxs = ExtUtils::ParseXS->new;
  tie *FH, 'Capture';
  my ($stderr, $filename);
  {
    $filename = 'XSFalsePositive.xs';
    $stderr = PrimitiveCapture::capture_stderr(sub {
      $pxs->process_file(filename => $filename, output => \*FH, prototypes => 1);
    });
    TODO: {
      local $TODO = 'GH 19661';
      unlike $stderr,
        qr/Warning: duplicate function definition 'do' detected in \Q$filename\E/,
        "No 'duplicate function definition' warning observed in $filename";
    }
  }
  {
    $filename = 'XSFalsePositive2.xs';
    $stderr = PrimitiveCapture::capture_stderr(sub {
      $pxs->process_file(filename => $filename, output => \*FH, prototypes => 1);
    });
    TODO: {
      local $TODO = 'GH 19661';
      unlike $stderr,
        qr/Warning: duplicate function definition 'do' detected in \Q$filename\E/,
        "No 'duplicate function definition' warning observed in $filename";
      }
  }
}

#####################################################################

{ # tight cpp directives
  my $pxs = ExtUtils::ParseXS->new;
  tie *FH, 'Capture';
  my $stderr = PrimitiveCapture::capture_stderr(sub { eval {
    $pxs->process_file(
      filename => 'XSTightDirectives.xs',
      output => \*FH,
      prototypes => 1);
  } or warn $@ });
  my $content = tied(*FH)->{buf};
  my $count = 0;
  $count++ while $content=~/^XS_EUPXS\(XS_My_do\)\n\{/mg;
  is $stderr, undef, "No error expected from TightDirectives.xs";
  is $count, 2, "Saw XS_MY_do definition the expected number of times";
}

{ # Alias check
  my $pxs = ExtUtils::ParseXS->new;
  tie *FH, 'Capture';
  my $stderr = PrimitiveCapture::capture_stderr(sub {
    $pxs->process_file(
      filename => 'XSAlias.xs',
      output => \*FH,
      prototypes => 1);
  });
  my $content = tied(*FH)->{buf};
  my $count = 0;
  $count++ while $content=~/^XS_EUPXS\(XS_My_do\)\n\{/mg;
  is $stderr,
    "Warning: Aliases 'pox' and 'dox', 'lox' have"
    . " identical values of 1 in XSAlias.xs, line 9\n"
    . "    (If this is deliberate use a symbolic alias instead.)\n"
    . "Warning: Conflicting duplicate alias 'pox' changes"
    . " definition from '1' to '2' in XSAlias.xs, line 10\n"
    . "Warning: Aliases 'docks' and 'dox', 'lox' have"
    . " identical values of 1 in XSAlias.xs, line 11\n"
    . "Warning: Aliases 'xunx' and 'do' have identical values"
    . " of 0 - the base function in XSAlias.xs, line 13\n"
    . "Warning: Aliases 'do' and 'xunx', 'do' have identical values"
    . " of 0 - the base function in XSAlias.xs, line 14\n"
    . "Warning: Aliases 'xunx2' and 'do', 'xunx' have"
    . " identical values of 0 - the base function in XSAlias.xs, line 15\n"
    ,
    "Saw expected warnings from XSAlias.xs in AUTHOR_WARNINGS mode";

  my $expect = quotemeta(<<'EOF_CONTENT');
         cv = newXSproto_portable("My::dachs", XS_My_do, file, "$");
         XSANY.any_i32 = 1;
         cv = newXSproto_portable("My::do", XS_My_do, file, "$");
         XSANY.any_i32 = 0;
         cv = newXSproto_portable("My::docks", XS_My_do, file, "$");
         XSANY.any_i32 = 1;
         cv = newXSproto_portable("My::dox", XS_My_do, file, "$");
         XSANY.any_i32 = 1;
         cv = newXSproto_portable("My::lox", XS_My_do, file, "$");
         XSANY.any_i32 = 1;
         cv = newXSproto_portable("My::pox", XS_My_do, file, "$");
         XSANY.any_i32 = 2;
         cv = newXSproto_portable("My::xukes", XS_My_do, file, "$");
         XSANY.any_i32 = 0;
         cv = newXSproto_portable("My::xunx", XS_My_do, file, "$");
         XSANY.any_i32 = 0;
EOF_CONTENT
  $expect=~s/(?:\\[ ])+/\\s+/g;
  $expect=qr/$expect/;
  like $content, $expect, "Saw expected alias initialization";

  #diag $content;
}
{ # Alias check with no dev warnings.
  my $pxs = ExtUtils::ParseXS->new;
  tie *FH, 'Capture';
  my $stderr = PrimitiveCapture::capture_stderr(sub {
    $pxs->process_file(
      filename => 'XSAlias.xs',
      output => \*FH,
      prototypes => 1,
      author_warnings => 0);
  });
  my $content = tied(*FH)->{buf};
  my $count = 0;
  $count++ while $content=~/^XS_EUPXS\(XS_My_do\)\n\{/mg;
  is $stderr,
    "Warning: Conflicting duplicate alias 'pox' changes"
    . " definition from '1' to '2' in XSAlias.xs, line 10\n",
    "Saw expected warnings from XSAlias.xs";

  my $expect = quotemeta(<<'EOF_CONTENT');
         cv = newXSproto_portable("My::dachs", XS_My_do, file, "$");
         XSANY.any_i32 = 1;
         cv = newXSproto_portable("My::do", XS_My_do, file, "$");
         XSANY.any_i32 = 0;
         cv = newXSproto_portable("My::docks", XS_My_do, file, "$");
         XSANY.any_i32 = 1;
         cv = newXSproto_portable("My::dox", XS_My_do, file, "$");
         XSANY.any_i32 = 1;
         cv = newXSproto_portable("My::lox", XS_My_do, file, "$");
         XSANY.any_i32 = 1;
         cv = newXSproto_portable("My::pox", XS_My_do, file, "$");
         XSANY.any_i32 = 2;
         cv = newXSproto_portable("My::xukes", XS_My_do, file, "$");
         XSANY.any_i32 = 0;
         cv = newXSproto_portable("My::xunx", XS_My_do, file, "$");
         XSANY.any_i32 = 0;
EOF_CONTENT
  $expect=~s/(?:\\[ ])+/\\s+/g;
  $expect=qr/$expect/;
  like $content, $expect, "Saw expected alias initialization";

  #diag $content;
}
{
    my $file = $INC{"ExtUtils/ParseXS.pm"};
    $file=~s!ExtUtils/ParseXS\.pm\z!perlxs.pod!;
    open my $fh, "<", $file
        or die "Failed to open '$file' for read:$!";
    my $pod_version = "";
    while (defined(my $line= readline($fh))) {
        if ($line=~/\(also known as C<xsubpp>\)\s+(\d+\.\d+)/) {
            $pod_version = $1;
            last;
        }
    }
    close $fh;
    ok($pod_version, "Found the version from perlxs.pod");
    is($pod_version, $ExtUtils::ParseXS::VERSION,
        "The version in perlxs.pod should match the version of ExtUtils::ParseXS");
}

# Basic test of the death() method.
# Run some code which will trigger a call to death(). Check that we get
# the expected error message (and as an exception rather than being on
# stderr.)
{
    my $pxs = ExtUtils::ParseXS->new;
    tie *FH, 'Capture';
    my $exception;
    my $stderr = PrimitiveCapture::capture_stderr(sub {
        eval {
            $pxs->process_file(
                filename => "XSNoMap.xs",
                output => \*FH,
               );
            1;
        } or $exception = $@;
    });
    is($stderr, undef, "should fail to parse");
    like($exception, qr/Error: Unterminated TYPEMAP section/,
         "check we throw rather than trying to deref '2'");
}


{
    # Basic test of using a string ref as the input file

    my $pxs = ExtUtils::ParseXS->new;
    tie *FH, 'Capture';
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void f(int a)
        |    CODE:
        |        mycode;
EOF

    $pxs->process_file( filename => \$text, output => \*FH);

    my $out = tied(*FH)->content;

    # We should have got some content, and the generated '#line' lines
    # should be sensible rather than '#line 1 SCALAR(0x...)'.
    like($out, qr/XS_Foo_f/,               "string ref: fn name");
    like($out, qr/#line \d+ "\(input\)"/,  "string ref input #line");
    like($out, qr/#line \d+ "\(output\)"/, "string ref output #line");
}


{
    # Test [=+;] on INPUT lines (including embedded double quotes
    # within expression which get evalled)

    my $pxs = ExtUtils::ParseXS->new;
    tie *FH, 'Capture';
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void f(mymarker1, a, b, c, d)
        |        int mymarker1
        |        int a = ($var"$var\"$type);
        |        int b ; blah($var"$var\"$type);
        |        int c + blurg($var"$var\"$type);
        |        int d
        |    CODE:
        |        mymarker2;
EOF

    $pxs->process_file( filename => \$text, output => \*FH);

    # Those INPUT lines should have produced something like:
    #
    #    int    mymarker1 = (int)SvIV(ST(0));
    #    int    a = (a"a\"int);
    #    int    b;
    #    int    c = (int)SvIV(ST(3))
    #    int    d = (int)SvIV(ST(4))
    #    blah(b"b\"int);
    #    blurg(c"c\"int);
    #    mymarker2;

    my $out = tied(*FH)->content;

    # trim the output to just the function in question to make
    # test diagnostics smaller.
    $out =~ s/\A .*? (int \s+ mymarker1 .*? mymarker2 ) .* \z/$1/xms
        or die "couldn't trim output";

    like($out, qr/^ \s+ int \s+ a\ =\ \Q(a"a"int);\E $/xm,
                        "INPUT '=' expands custom typemap");

    like($out, qr/^ \s+ int \s+ b;$/xm,
                        "INPUT ';' suppresses typemap");

    like($out, qr/^ \s+ int \s+ c\ =\ \Q(int)SvIV(ST(3))\E $/xm,
                        "INPUT '+' expands standard typemap");

    like($out,
        qr/^ \s+ int \s+ d\ = .*? blah\Q(b"b"int)\E .*? blurg\Q(c"c"int)\E .*? mymarker2/xms,
                        "INPUT '+' and ';' append expanded code");
}


{
    # Check that function pointer types are supported

    my $pxs = ExtUtils::ParseXS->new;
    tie *FH, 'Capture';
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |TYPEMAP: <<EOF
        |int (*)(char *, long)   T_INT_FN_PTR
        |
        |INPUT
        |
        |T_INT_FN_PTR
        |    $var = ($type)INT2PTR(SvIV($arg))
        |EOF
        |
        |void foo(mymarker1, fn_ptr)
        |    int                   mymarker1
        |    int (*)(char *, long) fn_ptr
EOF

    $pxs->process_file( filename => \$text, output => \*FH);

    my $out = tied(*FH)->content;

    # trim the output to just the function in question to make
    # test diagnostics smaller.
    $out =~ s/\A .*? (int \s+ mymarker1 .*? XSRETURN ) .* \z/$1/xms
        or die "couldn't trim output";

    # remove all spaces for easier matching
    my $sout = $out;
    $sout =~ s/[ \t]+//g;

    like($sout,
        qr/\Qint(*fn_ptr)(char*,long)=(int(*)(char*,long))INT2PTR(SvIV(ST(1)))/,
        "function pointer declared okay");
}

{
    # Check that default expressions are template-expanded.
    # Whether this is sensible or not, Dynaloader and other distributions
    # rely on it

    my $pxs = ExtUtils::ParseXS->new;
    tie *FH, 'Capture';
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void foo(int mymarker1, char *pkg = "$Package")
        |    CODE:
        |        mymarker2;
EOF

    $pxs->process_file( filename => \$text, output => \*FH);

    my $out = tied(*FH)->content;

    # trim the output to just the function in question to make
    # test diagnostics smaller.
    $out =~ s/\A .*? (int \s+ mymarker1 .*? mymarker2 ) .* \z/$1/xms
        or die "couldn't trim output";

    # remove all spaces for easier matching
    my $sout = $out;
    $sout =~ s/[ \t]+//g;

    like($sout, qr/pkg.*=.*"Foo"/, "default expression expanded");
}

{
    # Test 'alien' INPUT parameters: ones which are declared in an INPUT
    # section but don't appear in the XSUB's signature. This ought to be
    # a compile error, but people rely on it to declare and initialise
    # variables which ought to be in a PREINIT or CODE section.

    my $pxs = ExtUtils::ParseXS->new;
    tie *FH, 'Capture';
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void foo(mymarker1)
        |        int mymarker1
        |        long alien1
        |        int  alien2 = 123;
        |    CODE:
        |        mymarker2;
EOF

    $pxs->process_file( filename => \$text, output => \*FH);

    my $out = tied(*FH)->content;

    # trim the output to just the function in question to make
    # test diagnostics smaller.
    $out =~ s/\A .*? (int \s+ mymarker1 .*? mymarker2 ) .* \z/$1/xms
        or die "couldn't trim output";

    # remove all spaces for easier matching
    my $sout = $out;
    $sout =~ s/[ \t]+//g;

    like($sout, qr/longalien1;\nintalien2=123;/, "alien INPUT parameters");
}

{
    # Test for 'No INPUT definition' error, particularly that the
    # type is output correctly in the error message.

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |TYPEMAP: <<EOF
        |Foo::Bar   T_FOOBAR
        |EOF
        |
        |void foo(fb)
        |        Foo::Bar fb
EOF

    tie *FH, 'Capture';
    my $stderr = PrimitiveCapture::capture_stderr(sub {
        $pxs->process_file( filename => \$text, output => \*FH);
    });

    like($stderr, qr/No INPUT definition for type 'Foo::Bar'/,
                    "No INPUT definition");
}

{
    # Test for default arg mixed with initialisers

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void foo(mymarker1, aaa = 111, bbb = 222, ccc = 333, ddd = NO_INIT, eee = NO_INIT, fff = NO_INIT)
        |    int mymarker1
        |    int aaa = 777;
        |    int bbb + 888;
        |    int ccc ; 999;
        |    int ddd = AAA;
        |    int eee + BBB;
        |    int fff ; CCC;
        |  CODE:
        |    mymarker2
EOF

    tie *FH, 'Capture';
    $pxs->process_file( filename => \$text, output => \*FH);

    my $out = tied(*FH)->content;

    # trim the output to just the function in question to make
    # test diagnostics smaller.
    $out =~ s/\A .*? (int \s+ mymarker1 .*? mymarker2 ) .* \z/$1/xms
        or die "couldn't trim output";

    # remove all spaces for easier matching
    my $sout = $out;
    $sout =~ s/[ \t]+//g;

    like($sout, qr/if\(items<3\)\nbbb=222;\nelse\{\nbbb=.*ST\(2\)\)\n;\n\}\n/,
                    "default with +init");

    like($sout, qr/\Qif(items>=6){\E\n\Qeee=(int)SvIV(ST(5))\E\n;\n\}/,
                "NO_INIT default with +init");

    {
        local $TODO = "default is lost in presence of initialiser";

        like($sout, qr/if\(items<2\)\naaa=111;\nelse\{\naaa=777;\n\}\n/,
                    "default with =init");

        like($sout, qr/if\(items<4\)\nccc=333;\n999;\n/,
                    "default with ;init");

        like($sout, qr/if\(items>=5\)\{\nddd=AAA;\n\}/,
                    "NO_INIT default with =init");
      unlike($sout, qr/^intddd=AAA;\n/m,
                    "NO_INIT default with =init no stray");

    }


    like($sout, qr/^$/m,
                    "default with +init deferred expression");
    like($sout, qr/^888;$/m,
                    "default with +init deferred expression");
    like($sout, qr/^999;$/m,
                    "default with ;init deferred expression");
    like($sout, qr/^BBB;$/m,
                    "NO_INIT default with +init deferred expression");
    like($sout, qr/^CCC;$/m,
                    "NO_INIT default with ;init deferred expression");

}

{
    # C++ methods: check that a sub name including a class auto-generates
    # a THIS or CLASS parameter

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |TYPEMAP: <<EOF
        |X::Y *    T_XY
        |INPUT
        |T_XY
        |   $var = my_xy($arg)
        |EOF
        |
        |int
        |X::Y::new(marker1)
        |    int mymarker1
        |  CODE:
        |
        |int
        |X::Y::f()
        |  CODE:
        |    mymarker2
        |
EOF

    tie *FH, 'Capture';
    $pxs->process_file( filename => \$text, output => \*FH);

    my $out = tied(*FH)->content;

    # trim the output to just the function in question to make
    # test diagnostics smaller.
    $out =~ s/\A .*? (int \s+ mymarker1 .*? mymarker2 ) .* \z/$1/xms
        or die "couldn't trim output";

    like($out, qr/^\s*\Qchar *\E\s+CLASS = \Q(char *)SvPV_nolen(ST(0))\E$/m,
                    "CLASS auto-generated");
    like($out, qr/^\s*\QX__Y *\E\s+THIS = \Qmy_xy(ST(0))\E$/m,
                    "THIS auto-generated");

}

{
    # Test for 'length(foo)' not legal in INPUT section

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void foo(s)
        |        char *s
        |        int  length(s)
EOF

    tie *FH, 'Capture';
    my $stderr = PrimitiveCapture::capture_stderr(sub {
        $pxs->process_file( filename => \$text, output => \*FH);
    });

    like($stderr, qr/./,
                    "No length() in INPUT section");
}

{
    # Test for initialisers with unknown variable type.
    # This previously died.

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void foo(a, b, c)
        |    UnknownType a = NO_INIT
        |    UnknownType b = bar();
        |    UnknownType c = baz($arg);
EOF

    tie *FH, 'Capture';
    my $stderr = PrimitiveCapture::capture_stderr(sub {
        $pxs->process_file( filename => \$text, output => \*FH);
    });

    is($stderr, undef, "Unknown type with initialiser: no errors");
}

{
    # Test for "duplicate definition of argument" errors

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void foo(a, b, int c)
        |    int a;
        |    int a;
        |    int b;
        |    int b;
        |    int c;
        |    int alien;
        |    int alien;
EOF

    tie *FH, 'Capture';
    my $stderr = PrimitiveCapture::capture_stderr(sub {
        $pxs->process_file( filename => \$text, output => \*FH);
    });

    for my $var (qw(a b c alien)) {
        my $count = () =
            $stderr =~ /duplicate definition of parameter '$var'/g;
        is($count, 1, "One dup error for \"$var\"");
    }
}

{
    # Basic check of an OUT parameter where the type is specified either
    # in the signature or in an INPUT line

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |int
        |f(marker1, OUT a, OUT int b)
        |    int mymarker1
        |    int a
        |  CODE:
        |    mymarker2
        |
EOF

    tie *FH, 'Capture';
    $pxs->process_file( filename => \$text, output => \*FH);

    my $out = tied(*FH)->content;

    # trim the output to just the function in question to make
    # test diagnostics smaller.
    $out =~ s/\A .*? (int \s+ mymarker1 .*? mymarker2 ) .* \z/$1/xms
        or die "couldn't trim output";

    like($out, qr/^\s+int\s+a;\s*$/m, "OUT a");
    like($out, qr/^\s+int\s+b;\s*$/m, "OUT b");

}

{
    # Basic check of a "usage: ..." string.
    # In particular, it should strip away type and IN/OUT class etc.
    # Also, some distros include a test of their usage strings which
    # are sensitive to variations in white space, so this test
    # confirms that the exact white space is preserved, especially
    # with regards to space (or not) around the '=' of a default value.

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |int
        |foo(  a   ,  char   * b  , OUT  int  c  ,  OUTLIST int  d   ,    \
        |      IN_OUT char * * e    =   1  + 2 ,   long length(e)   ,    \
        |      char* f="abc"  ,     g  =   0  ,   ...     )
EOF

    tie *FH, 'Capture';
    $pxs->process_file( filename => \$text, output => \*FH);

    my $out = tied(*FH)->content;

    my $ok = $out =~ /croak_xs_usage\(cv,\s*(".*")\);\s*$/m;
    my $str = $ok ? $1 : '';
    ok $ok, "extract usage string";
    is $str, q("a, b, c, e=   1  + 2, f=\"abc\", g  =   0, ..."),
         "matched usage string";
}

{
    # Test for parameter parsing errors, including the effects of the
    # -noargtype and -noinout switches

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void
        |foo(char* a, length(a) = 0, IN c, +++)
EOF

    tie *FH, 'Capture';
    my $stderr = PrimitiveCapture::capture_stderr(sub {
        eval {
            $pxs->process_file( filename => \$text, output => \*FH,
                                argtypes => 0, inout => 0);
        }
    });

    like $stderr, qr{\Qparameter type not allowed under -noargtypes},
                 "no type under -noargtypes";
    like $stderr, qr{\Qlength() pseudo-parameter not allowed under -noargtypes},
                 "no length under -noargtypes";
    like $stderr, qr{\Qparameter IN/OUT modifier not allowed under -noinout},
                 "no IN/OUT under -noinout";
    like $stderr, qr{\QUnparseable XSUB parameter: '+++'},
                 "unparseable parameter";
}

{
    # Test for ellipis in the signature.

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void
        |foo(int mymarker1, char *b = "...", int c = 0, ...)
        |    POSTCALL:
        |      mymarker2;
EOF

    tie *FH, 'Capture';
    $pxs->process_file( filename => \$text, output => \*FH);

    my $out = tied(*FH)->content;

    # trim the output to just the function in question to make
    # test diagnostics smaller.
    $out =~ s/\A .*? (int \s+ mymarker1 .*? mymarker2 ) .* \z/$1/xms
        or die "couldn't trim output";

    like $out, qr/\Qb = "..."/, "ellipsis: b has correct default value";
    like $out, qr/b = .*SvPV/,  "ellipsis: b has correct non-default value";
    like $out, qr/\Qc = 0/,     "ellipsis: c has correct default value";
    like $out, qr/c = .*SvIV/,  "ellipsis: c has correct non-default value";
    like $out, qr/\Qfoo(mymarker1, b, c)/, "ellipsis: wrapped function args";
}

{
    # Test for bad ellipsis

    my $pxs = ExtUtils::ParseXS->new;
    my $text = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |void
        |foo(a, ..., b)
EOF

    tie *FH, 'Capture';
    my $stderr = PrimitiveCapture::capture_stderr(sub {
        eval {
            $pxs->process_file( filename => \$text, output => \*FH);
        }
    });

    like $stderr, qr{\Qfurther XSUB parameter seen after ellipsis},
                 "further XSUB parameter seen after ellipsis";
}

{
    # Test for C++ XSUB support: in particular,
    # - an XSUB function including a class in its name implies C++
    # - implicit CLASS/THIS first arg
    # - new and DESTROY methods handled specially
    # - 'static' return type implies class method
    # - 'const' can follow signature
    #

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |TYPEMAP: <<EOF
        |X::Y *        T_OBJECT
        |const X::Y *  T_OBJECT
        |
        |INPUT
        |T_OBJECT
        |    $var = my_in($arg);
        |
        |OUTPUT
        |T_OBJECT
        |    my_out($arg, $var)
        |EOF
        |
EOF

    my @test_fns = (
        # [
        #     "common prefix for test descriptions",
        #     [ ... lines to be ...
        #       ... used as ...
        #       ... XSUB body...
        #     ],
        #     [ check_stderr, expect_nomatch, qr/expected/, "test description"],
        #     [ ... and more tests ..]
        #     ....
        # ]

        [
            # test something that isn't actually C++
            "C++: plain new",
            [
                'X::Y*',
                'new(int aaa)',
            ],
            [ 0, 0, qr/usage\(cv,\s+"aaa"\)/,                "usage"    ],
            [ 0, 0, qr/\Qnew(aaa)/,                          "autocall" ],
        ],

        [
            # test something static that isn't actually C++
            "C++: plain static new",
            [
                'static X::Y*',
                'new(int aaa)',
            ],
            [ 0, 0, qr/usage\(cv,\s+"aaa"\)/,                "usage"    ],
            [ 0, 0, qr/\Qnew(aaa)/,                          "autocall" ],
            [ 1, 0, qr/Ignoring 'static' type modifier/,     "warning"  ],
        ],

        [
            # test something static that isn't actually C++ nor new
            "C++: plain static foo",
            [
                'static X::Y*',
                'foo(int aaa)',
            ],
            [ 0, 0, qr/usage\(cv,\s+"aaa"\)/,                "usage"    ],
            [ 0, 0, qr/\Qfoo(aaa)/,                          "autocall" ],
            [ 1, 0, qr/Ignoring 'static' type modifier/,     "warning"  ],
        ],

        [
            "C++: new",
            [
                'X::Y*',
                'X::Y::new(int aaa)',
            ],
            [ 0, 0, qr/usage\(cv,\s+"CLASS, aaa"\)/,         "usage"    ],
            [ 0, 0, qr/char\s*\*\s*CLASS\b/,                 "var decl" ],
            [ 0, 0, qr/\Qnew X::Y(aaa)/,                     "autocall" ],
        ],

        [
            "C++: static new",
            [
                'static X::Y*',
                'X::Y::new(int aaa)',
            ],
            [ 0, 0, qr/usage\(cv,\s+"CLASS, aaa"\)/,         "usage"    ],
            [ 0, 0, qr/char\s*\*\s*CLASS\b/,                 "var decl" ],
            [ 0, 0, qr/\QX::Y(aaa)/,                         "autocall" ],
        ],

        [
            "C++: fff",
            [
                'void',
                'X::Y::fff(int bbb)',
            ],
            [ 0, 0, qr/usage\(cv,\s+"THIS, bbb"\)/,          "usage"    ],
            [ 0, 0, qr/X__Y\s*\*\s*THIS\s*=\s*my_in/,        "var decl" ],
            [ 0, 0, qr/\QTHIS->fff(bbb)/,                    "autocall" ],
        ],

        [
            "C++: ggg",
            [
                'static int',
                'X::Y::ggg(int ccc)',
            ],
            [ 0, 0, qr/usage\(cv,\s+"CLASS, ccc"\)/,         "usage"    ],
            [ 0, 0, qr/char\s*\*\s*CLASS\b/,                 "var decl" ],
            [ 0, 0, qr/\QX::Y::ggg(ccc)/,                    "autocall" ],
        ],

        [
            "C++: hhh",
            [
                'int',
                'X::Y::hhh(int ddd) const',
            ],
            [ 0, 0, qr/usage\(cv,\s+"THIS, ddd"\)/,          "usage"    ],
            [ 0, 0, qr/const X__Y\s*\*\s*THIS\s*=\s*my_in/,  "var decl" ],
            [ 0, 0, qr/\QTHIS->hhh(ddd)/,                    "autocall" ],
        ],

        [
            "",
            [
                'int',
                'X::Y::f1(THIS, int i)',
            ],
            [ 1, 0, qr/\QError: duplicate definition of parameter 'THIS' /,
                 "C++: f1 dup THIS" ],
        ],

        [
            "",
            [
                'int',
                'X::Y::f2(int THIS, int i)',
            ],
            [ 1, 0, qr/\QError: duplicate definition of parameter 'THIS' /,
                 "C++: f2 dup THIS" ],
        ],

        [
            "",
            [
                'int',
                'X::Y::new(int CLASS, int i)',
            ],
            [ 1, 0, qr/\QError: duplicate definition of parameter 'CLASS' /,
                 "C++: new dup CLASS" ],
        ],

        [
            "C++: f3",
            [
                'int',
                'X::Y::f3(int i)',
                '    OUTPUT:',
                '        THIS',
            ],
            [ 0, 0, qr/usage\(cv,\s+"THIS, i"\)/,            "usage"    ],
            [ 0, 0, qr/X__Y\s*\*\s*THIS\s*=\s*my_in/,        "var decl" ],
            [ 0, 0, qr/\QTHIS->f3(i)/,                       "autocall" ],
            [ 0, 0, qr/^\s*\Qmy_out(ST(0), THIS)/m,          "set st0"  ],
        ],

        [
            # allow THIS's type to be overridden ...
            "C++: f4: override THIS type",
            [
                'int',
                'X::Y::f4(int i)',
                '    int THIS',
            ],
            [ 0, 0, qr/usage\(cv,\s+"THIS, i"\)/,       "usage"    ],
            [ 0, 0, qr/int\s*THIS\s*=\s*\(int\)/,       "var decl" ],
            [ 0, 1, qr/X__Y\s*\*\s*THIS/,               "no class var decl" ],
            [ 0, 0, qr/\QTHIS->f4(i)/,                  "autocall" ],
        ],

        [
            #  ... but not multiple times
            "C++: f5: dup override THIS type",
            [
                'int',
                'X::Y::f5(int i)',
                '    int THIS',
                '    long THIS',
            ],
            [ 1, 0, qr/\QError: duplicate definition of parameter 'THIS'/,
                    "dup err" ],
        ],

        [
            #  don't allow THIS in sig, with type
            "C++: f6: sig THIS type",
            [
                'int',
                'X::Y::f6(int THIS)',
            ],
            [ 1, 0, qr/\QError: duplicate definition of parameter 'THIS'/,
                    "dup err" ],
        ],

        [
            #  don't allow THIS in sig, without type
            "C++: f7: sig THIS no type",
            [
                'int',
                'X::Y::f7(THIS)',
            ],
            [ 1, 0, qr/\QError: duplicate definition of parameter 'THIS'/,
                    "dup err" ],
        ],

        [
            # allow CLASS's type to be overridden ...
            "C++: new: override CLASS type",
            [
                'int',
                'X::Y::new(int i)',
                '    int CLASS',
            ],
            [ 0, 0, qr/usage\(cv,\s+"CLASS, i"\)/,      "usage"    ],
            [ 0, 0, qr/int\s*CLASS\s*=\s*\(int\)/,      "var decl" ],
            [ 0, 1, qr/char\s*\*\s*CLASS/,              "no char* var decl" ],
            [ 0, 0, qr/\Qnew X::Y(i)/,                  "autocall" ],
        ],

        [
            #  ... but not multiple times
            "C++: new dup override CLASS type",
            [
                'int',
                'X::Y::new(int i)',
                '    int CLASS',
                '    long CLASS',
            ],
            [ 1, 0, qr/\QError: duplicate definition of parameter 'CLASS'/,
                    "dup err" ],
        ],

        [
            #  don't allow CLASS in sig, with type
            "C++: new sig CLASS type",
            [
                'int',
                'X::Y::new(int CLASS)',
            ],
            [ 1, 0, qr/\QError: duplicate definition of parameter 'CLASS'/,
                    "dup err" ],
        ],

        [
            #  don't allow CLASS in sig, without type
            "C++: new sig CLASS no type",
            [
                'int',
                'X::Y::new(CLASS)',
            ],
            [ 1, 0, qr/\QError: duplicate definition of parameter 'CLASS'/,
                    "dup err" ],
        ],

        [
            "C++: DESTROY",
            [
                'void',
                'X::Y::DESTROY()',
            ],
            [ 0, 0, qr/usage\(cv,\s+"THIS"\)/,               "usage"    ],
            [ 0, 0, qr/X__Y\s*\*\s*THIS\s*=\s*my_in/,        "var decl" ],
            [ 0, 0, qr/delete\s+THIS;/,                      "autocall" ],
        ]
    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # check that suitable "usage: " error strings are generated

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
EOF

    my @test_fns = (
        [
            "general usage",
            [
                'void',
                'foo(a, char *b,  int length(b), int d =  999, ...)',
                '    long a',
            ],
            [ 0, 0, qr/usage\(cv,\s+"a, b, d=  999, ..."\)/,     ""    ],
        ]
    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # check that args to an auto-called C function are correct

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
EOF

    my @test_fns = (
        [
            "autocall args normal",
            [
                'void',
                'foo( OUT int  a,   b   , char   *  c , int length(c), OUTLIST int d, IN_OUTLIST int e)',
                '    long &b',
                '    int alien',
            ],
            [ 0, 0, qr/\Qfoo(&a, &b, c, XSauto_length_of_c, &d, &e)/,  ""  ],
        ],
        [
            "autocall args normal",
            [
                'void',
                'foo( OUT int  a,   b   , char   *  c , size_t length(c) )',
                '    long &b',
                '    int alien',
            ],
            [ 0, 0, qr/\Qfoo(&a, &b, c, XSauto_length_of_c)/,     ""    ],
        ],

        [
            "autocall args C_ARGS",
            [
                'void',
                'foo( int  a,   b   , char   *  c  )',
                '    C_ARGS:     a,   b   , bar,  c? c : "boo!"    ',
                '    INPUT:',
                '        long &b',
            ],
            [ 0, 0, qr/\Qfoo(a,   b   , bar,  c? c : "boo!")/,     ""    ],
        ],

        [
            # Whether this is sensible or not is another matter.
            # For now, just check that it works as-is.
            "autocall args C_ARGS multi-line",
            [
                'void',
                'foo( int  a,   b   , char   *  c  )',
                '    C_ARGS: a,',
                '        b   , bar,',
                '        c? c : "boo!"',
                '    INPUT:',
                '        long &b',
            ],
            [ 0, 0, qr/\(a,\n        b   , bar,\n\Q        c? c : "boo!")/,
              ""  ],
        ],
    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # Test OUTLIST etc

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |TYPEMAP: <<EOF
        |mybool        T_MYBOOL
        |
        |OUTPUT
        |T_MYBOOL
        |    ${"$var" eq "RETVAL" ? \"$arg = boolSV($var);" : \"sv_setsv($arg, boolSV($var));"}
        |EOF
EOF

    my @test_fns = (
        [
            "IN OUT",
            [
                'void',
                'foo(IN int A, IN_OUT int B, OUT int C, OUTLIST int D, IN_OUTLIST int E)',
            ],
            [ 0, 0, qr/\Qusage(cv,  "A, B, C, E")/,    "usage"    ],

            [ 0, 0, qr/int\s+A\s*=\s*\(int\)SvIV\s*/,  "A decl"   ],
            [ 0, 0, qr/int\s+B\s*=\s*\(int\)SvIV\s*/,  "B decl"   ],
            [ 0, 0, qr/int\s+C\s*;/,                   "C decl"   ],
            [ 0, 0, qr/int\s+D\s*;/,                   "D decl"   ],
            [ 0, 0, qr/int\s+E\s*=\s*\(int\)SvIV\s*/,  "E decl"   ],

            [ 0, 0, qr/\Qfoo(A, &B, &C, &D, &E)/,      "autocall" ],

            [ 0, 0, qr/sv_setiv.*ST\(1\).*\bB\b/,      "set B"    ],
            [ 0, 0, qr/\QSvSETMAGIC(ST(1))/,           "set magic B" ],
            [ 0, 0, qr/sv_setiv.*ST\(2\).*\bC\b/,      "set C"    ],
            [ 0, 0, qr/\QSvSETMAGIC(ST(2))/,           "set magic C" ],

            [ 0, 1, qr/\bEXTEND\b/,                    "NO extend"       ],

            [ 0, 0, qr/\b\QTARGi((IV)D, 1);\E\s+\QST(0) = TARG;\E\s+\}\s+\Q++SP;/, "set D"    ],
            [ 0, 0, qr/\b\Qsv_setiv(RETVALSV, (IV)E);\E\s+\QST(1) = RETVALSV;\E\s+\}\s+\Q++SP;/, "set E"    ],
        ],

        # Various types of OUTLIST where the param is the only value to
        # be returned. Includes some types which might be optimised.

        [
            "OUTLIST void/bool",
            [
                'void',
                'foo(OUTLIST bool A)',
            ],
            [ 0, 0, qr/\bXSprePUSH;/,                    "XSprePUSH"       ],
            [ 0, 1, qr/\bEXTEND\b/,                      "NO extend"       ],
            [ 0, 0, qr/\b\QRETVALSV = sv_newmortal();/ , "create new mortal" ],
            [ 0, 0, qr/\b\Qsv_setsv(RETVALSV, boolSV(A));/, "set RETVALSV"   ],
            [ 0, 0, qr/\b\QST(0) = RETVALSV;\E\s+\}\s+\Q++SP;/, "store RETVALSV"],
            [ 0, 0, qr/\b\QXSRETURN(1);/,                "XSRETURN(1)"     ],
        ],
        [
            "OUTLIST void/mybool",
            [
                'void',
                'foo(OUTLIST mybool A)',
            ],
            [ 0, 0, qr/\bXSprePUSH;/,                    "XSprePUSH"       ],
            [ 0, 1, qr/\bEXTEND\b/,                      "NO extend"       ],
            [ 0, 0, qr/\b\QRETVALSV = sv_newmortal();/ , "create new mortal" ],
            [ 0, 0, qr/\b\Qsv_setsv(RETVALSV, boolSV(A));/, "set RETVALSV"   ],
            [ 0, 0, qr/\b\QST(0) = RETVALSV;\E\s+\}\s+\Q++SP;/, "store RETVALSV"],
            [ 0, 0, qr/\b\QXSRETURN(1);/,                "XSRETURN(1)"     ],
        ],
        [
            "OUTLIST void/int",
            [
                'void',
                'foo(OUTLIST int A)',
            ],
            [ 0, 0, qr/\bXSprePUSH;/,                    "XSprePUSH"       ],
            [ 0, 1, qr/\bEXTEND\b/,                      "NO extend"       ],
            [ 0, 1, qr/\bsv_newmortal\b;/,               "NO new mortal"   ],
            [ 0, 0, qr/\bdXSTARG;/,                      "dXSTARG"         ],
            [ 0, 0, qr/\b\QTARGi((IV)A, 1);/,            "set TARG"        ],
            [ 0, 0, qr/\b\QST(0) = TARG;\E\s+\}\s+\Q++SP;/, "store TARG"   ],
            [ 0, 0, qr/\b\QXSRETURN(1);/,                "XSRETURN(1)"     ],
        ],
        [
            "OUTLIST void/char*",
            [
                'void',
                'foo(OUTLIST char* A)',
            ],
            [ 0, 0, qr/\bXSprePUSH;/,                    "XSprePUSH"       ],
            [ 0, 1, qr/\bEXTEND\b/,                      "NO extend"       ],
            [ 0, 1, qr/\bsv_newmortal\b;/,               "NO new mortal"   ],
            [ 0, 0, qr/\bdXSTARG;/,                      "dXSTARG"         ],
            [ 0, 0, qr/\b\Qsv_setpv((SV*)TARG, A);/,     "set TARG"        ],
            [ 0, 0, qr/\b\QST(0) = TARG;\E\s+\}\s+\Q++SP;/, "store TARG"   ],
            [ 0, 0, qr/\b\QXSRETURN(1);/,                "XSRETURN(1)"     ],
        ],

        # Various types of OUTLIST where the param is the second value to
        # be returned. Includes some types which might be optimised.

        [
            "OUTLIST int/bool",
            [
                'int',
                'foo(OUTLIST bool A)',
            ],
            [ 0, 0, qr/\bXSprePUSH;/,                    "XSprePUSH"       ],
            [ 0, 0, qr/\b\QEXTEND(SP,2);/,               "extend 2"        ],
            [ 0, 0, qr/\b\QTARGi((IV)RETVAL, 1);/,       "TARGi RETVAL"    ],
            [ 0, 0, qr/\b\QST(0) = TARG;\E\s+\Q++SP;/,   "store RETVAL,SP++" ],
            [ 0, 0, qr/\b\QRETVALSV = sv_newmortal();/ , "create new mortal" ],
            [ 0, 0, qr/\b\Qsv_setsv(RETVALSV, boolSV(A));/, "set RETVALSV"   ],
            [ 0, 0, qr/\b\QST(1) = RETVALSV;\E\s+\}\s+\Q++SP;/, "store RETVALSV"],
            [ 0, 0, qr/\b\QXSRETURN(2);/,                "XSRETURN(2)"     ],
        ],
        [
            "OUTLIST int/mybool",
            [
                'int',
                'foo(OUTLIST mybool A)',
            ],
            [ 0, 0, qr/\bXSprePUSH;/,                    "XSprePUSH"       ],
            [ 0, 0, qr/\b\QEXTEND(SP,2);/,               "extend 2"        ],
            [ 0, 0, qr/\b\QTARGi((IV)RETVAL, 1);/,       "TARGi RETVAL"    ],
            [ 0, 0, qr/\b\QST(0) = TARG;\E\s+\Q++SP;/,   "store RETVAL,SP++" ],
            [ 0, 0, qr/\b\QRETVALSV = sv_newmortal();/ , "create new mortal" ],
            [ 0, 0, qr/\b\Qsv_setsv(RETVALSV, boolSV(A));/, "set RETVALSV"   ],
            [ 0, 0, qr/\b\QST(1) = RETVALSV;\E\s+\}\s+\Q++SP;/, "store RETVALSV"],
            [ 0, 0, qr/\b\QXSRETURN(2);/,                "XSRETURN(2)"     ],
        ],
        [
            "OUTLIST int/int",
            [
                'int',
                'foo(OUTLIST int A)',
            ],
            [ 0, 0, qr/\bXSprePUSH;/,                    "XSprePUSH"       ],
            [ 0, 0, qr/\b\QEXTEND(SP,2);/,               "extend 2"        ],
            [ 0, 0, qr/\b\QTARGi((IV)RETVAL, 1);/,       "TARGi RETVAL"    ],
            [ 0, 0, qr/\b\QST(0) = TARG;\E\s+\Q++SP;/,   "store RETVAL,SP++" ],
            [ 0, 0, qr/\b\QRETVALSV = sv_newmortal();/ , "create new mortal" ],
            [ 0, 0, qr/\b\Qsv_setiv(RETVALSV, (IV)A);/,  "set RETVALSV"   ],
            [ 0, 0, qr/\b\QST(1) = RETVALSV;\E\s+\}\s+\Q++SP;/, "store RETVALSV"],
            [ 0, 0, qr/\b\QXSRETURN(2);/,                "XSRETURN(2)"     ],
        ],
        [
            "OUTLIST int/char*",
            [
                'int',
                'foo(OUTLIST char* A)',
            ],
            [ 0, 0, qr/\bXSprePUSH;/,                    "XSprePUSH"       ],
            [ 0, 0, qr/\b\QEXTEND(SP,2);/,               "extend 2"        ],
            [ 0, 0, qr/\b\QTARGi((IV)RETVAL, 1);/,       "TARGi RETVAL"    ],
            [ 0, 0, qr/\b\QST(0) = TARG;\E\s+\Q++SP;/,   "store RETVAL,SP++" ],
            [ 0, 0, qr/\b\QRETVALSV = sv_newmortal();/ , "create new mortal" ],
            [ 0, 0, qr/\b\Qsv_setpv((SV*)RETVALSV, A);/, "set RETVALSV"   ],
            [ 0, 0, qr/\b\QST(1) = RETVALSV;\E\s+\}\s+\Q++SP;/, "store RETVALSV"],
            [ 0, 0, qr/\b\QXSRETURN(2);/,                "XSRETURN(2)"     ],
        ],
        [
            "OUTLIST int/opt int",
            [
                'int',
                'foo(IN_OUTLIST int A = 0)',
            ],
            [ 0, 0, qr/\bXSprePUSH;/,                    "XSprePUSH"       ],
            [ 0, 0, qr/\b\QEXTEND(SP,2);/,               "extend 2"        ],
            [ 0, 0, qr/\b\QTARGi((IV)RETVAL, 1);/,       "TARGi RETVAL"    ],
            [ 0, 0, qr/\b\QST(0) = TARG;\E\s+\Q++SP;/,   "store RETVAL,SP++" ],
            [ 0, 0, qr/\b\QRETVALSV = sv_newmortal();/ , "create new mortal" ],
            [ 0, 0, qr/\b\Qsv_setiv(RETVALSV, (IV)A);/,  "set RETVALSV"   ],
            [ 0, 0, qr/\b\QST(1) = RETVALSV;\E\s+\}\s+\Q++SP;/, "store RETVALSV"],
            [ 0, 0, qr/\b\QXSRETURN(2);/,                "XSRETURN(2)"     ],
        ],
        [
            "OUTLIST with OUTPUT override",
            [ Q(<<'EOF') ],
                |void
                |foo(IN_OUTLIST int A)
                |    OUTPUT:
                |        A    setA(ST[99], A);
EOF
            [ 0, 1, qr/\bEXTEND\b/,                      "NO extend"       ],
            [ 0, 0, qr/\b\QsetA(ST[99], A);/,            "set ST[99]"      ],
            [ 0, 0, qr/\b\QTARGi((IV)A, 1);/,            "set ST[0]"       ],
            [ 0, 0, qr/\b\QXSRETURN(1);/,                "XSRETURN(1)"     ],
        ],
    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # Test OUTLIST on 'assign' format typemaps.
    #
    # Test code for returning the value of OUTLIST vars for typemaps of
    # the form
    #
    #   $arg = $val;
    # or
    #   $arg = newFoo($arg);
    #
    # Includes whether RETVALSV ha been optimised away.
    #
    # Some of the typemaps don't expand to the 'assign' form yet for
    # OUTLIST vars; we test those too.

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: DISABLE
        |
        |TYPEMAP: <<EOF
        |
        |svref_fix   T_SVREF_REFCOUNT_FIXED
        |mysvref_fix T_MYSVREF_REFCOUNT_FIXED
        |mybool      T_MYBOOL
        |
        |OUTPUT
        |T_SV
        |    $arg = $var;
        |
        |T_MYSVREF_REFCOUNT_FIXED
        |    $arg = newRV_noinc((SV*)$var);
        |
        |T_MYBOOL
        |    $arg = boolSV($var);
        |
        |EOF
EOF

    my @test_fns = (
        [
            # This uses 'SV*' (handled specially by EU::PXS) but with the
            # output code overridden to use the direct $arg = $var assign,
            # which is normally only used for RETVAL return
            "OUTLIST T_SV",
            [
                'int',
                'foo(OUTLIST SV * A)',
            ],
            [ 0, 1, qr/\bRETVALSV\b/,                        "NO RETVALSV"    ],
            [ 0, 0, qr/\b\QA = sv_2mortal(A);/,              "mortalise A"    ],
            [ 0, 0, qr/\b\QST(1) = A;/,                      "store A"        ],
        ],

        [
            "OUTLIST T_SVREF",
            [
                'int',
                'foo(OUTLIST SVREF A)',
            ],
            [ 0, 0, qr/SV\s*\*\s*RETVALSV;/,                 "RETVALSV"       ],
            [ 0, 0, qr/\b\QRETVALSV = newRV((SV*)A)/,        "newREF(A)"      ],
            [ 0, 0, qr/\b\QRETVALSV = sv_2mortal(RETVALSV);/,"mortalise RSV"  ],
            [ 0, 0, qr/\b\QST(1) = RETVALSV;/,               "store RETVALSV" ],
        ],

        [
            # this one doesn't use assign for OUTLIST
            "OUTLIST T_SVREF_REFCOUNT_FIXED",
            [
                'int',
                'foo(OUTLIST svref_fix A)',
            ],
            [ 0, 0, qr/SV\s*\*\s*RETVALSV;/,                 "RETVALSV"       ],
            [ 0, 0, qr/\b\QRETVALSV = sv_newmortal();/ ,     "new mortal"     ],
            [ 0, 0, qr/\b\Qsv_setrv_noinc(RETVALSV, (SV*)A);/,"setrv()"       ],
            [ 0, 0, qr/\b\QST(1) = RETVALSV;/,               "store RETVALSV" ],
        ],
        [
            # while this one uses assign
            "OUTLIST T_MYSVREF_REFCOUNT_FIXED",
            [
                'int',
                'foo(OUTLIST mysvref_fix A)',
            ],
            [ 0, 0, qr/SV\s*\*\s*RETVALSV;/,                 "RETVALSV"       ],
            [ 0, 0, qr/\b\QRETVALSV = newRV_noinc((SV*)A)/,  "newRV(A)"       ],
            [ 0, 0, qr/\b\QRETVALSV = sv_2mortal(RETVALSV);/,"mortalise RSV"  ],
            [ 0, 0, qr/\b\QST(1) = RETVALSV;/,               "store RETVALSV" ],
        ],

        [
            # this one doesn't use assign for OUTLIST
            "OUTLIST T_BOOL",
            [
                'int',
                'foo(OUTLIST bool A)',
            ],
            [ 0, 0, qr/SV\s*\*\s*RETVALSV;/,                 "RETVALSV"       ],
            [ 0, 0, qr/\b\QRETVALSV = sv_newmortal();/ ,     "new mortal"     ],
            [ 0, 0, qr/\b\Qsv_setsv(RETVALSV, boolSV(A));/,  "setsv(boolSV())"],
            [ 0, 0, qr/\b\QST(1) = RETVALSV;/,               "store RETVALSV" ],
        ],
        [
            # while this one uses assign
            "OUTLIST T_MYBOOL",
            [
                'int',
                'foo(OUTLIST mybool A)',
            ],
            [ 0, 1, qr/\bRETVALSV\b/,                        "NO RETVALSV"    ],
            [ 0, 0, qr/\b\QST(1) = boolSV(A)/,               "store boolSV(A)"],
        ],
    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # Test prototypes

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES: ENABLE
        |
        |TYPEMAP: <<EOF
        |X::Y *        T_OBJECT
        |const X::Y *  T_OBJECT \&
        |
        |P::Q *        T_OBJECT @
        |const P::Q *  T_OBJECT %
        |
        |INPUT
        |T_OBJECT
        |    $var = my_in($arg);
        |
        |OUTPUT
        |T_OBJECT
        |    my_out($arg, $var)
        |EOF
EOF

    my @test_fns = (
        [
            "auto-generated proto basic",
            [
                'void',
                'foo(int a, int b, int c)',
            ],
            [ 0, 0, qr/"\$\$\$"/, "" ],
        ],

        [
            "auto-generated proto basic with default",
            [
                'void',
                'foo(int a, int b, int c = 0)',
            ],
            [ 0, 0, qr/"\$\$;\$"/, "" ],
        ],

        [
            "auto-generated proto complex",
            [
                'void',
                'foo(char *A, int length(A), int B, OUTLIST int C, int D)',
            ],
            [ 0, 0, qr/"\$\$\$"/, "" ],
        ],

        [
            "auto-generated proto  complex with default",
            [
                'void',
                'foo(char *A, int length(A), int B, IN_OUTLIST int C, int D = 0)',
            ],
            [ 0, 0, qr/"\$\$\$;\$"/, "" ],
        ],

        [
            "auto-generated proto with ellipsis",
            [
                'void',
                'foo(char *A, int length(A), int B, OUT int C, int D, ...)',
            ],
            [ 0, 0, qr/"\$\$\$\$;\@"/, "" ],
        ],

        [
            "auto-generated proto with default and ellipsis",
            [
                'void',
                'foo(char *A, int length(A), int B, IN_OUT int C, int D = 0, ...)',
            ],
            [ 0, 0, qr/"\$\$\$;\$\@"/, "" ],
        ],

        [
            "auto-generated proto with default and ellipsis and THIS",
            [
                'void',
                'X::Y::foo(char *A, int length(A), int B, IN_OUT int C, int D = 0, ...)',
            ],
            [ 0, 0, qr/"\$\$\$\$;\$\@"/, "" ],
        ],

        [
            "auto-generated proto with overridden THIS type",
            [
                'void',
                'P::Q::foo()',
                '    const P::Q * THIS'
            ],
            [ 0, 0, qr/"%"/, "" ],
        ],

        [
            "explicit prototype",
            [
                'void',
                'foo(int a, int b, int c = 0)',
                '    PROTOTYPE: $@%;$'
            ],
            [ 0, 0, qr/"\$\@%;\$"/, "" ],
        ],

        [
            "explicit prototype with backslash etc",
            [
                'void',
                'foo(int a, int b, int c = 0)',
                '    PROTOTYPE: \$\[@%]'
            ],
            # Note that the emitted C code will have escaped backslashes,
            # so the actual C code looks something like:
            #    newXS_some_variant(..., "\\$\\[@%]");
            # and so the regex below has to escape each backslash and
            # meta char its trying to match:
            [ 0, 0, qr/" \\  \\  \$  \\  \\ \[  \@  \%  \] "/x, "" ],
        ],

        [
            "explicit empty prototype",
            [
                'void',
                'foo(int a, int b, int c = 0)',
                '    PROTOTYPE:'
            ],
            [ 0, 0, qr/newXS.*, ""/, "" ],
        ],

        [
            "not overridden by typemap",
            [
                'void',
                'foo(X::Y * a, int b, int c = 0)',
            ],
            [ 0, 0, qr/"\$\$;\$"/, "" ],
        ],

        [
            "overridden by typemap",
            [
                'void',
                'foo(const X::Y * a, int b, int c = 0)',
            ],
            [ 0, 0, qr/" \\ \\ \& \$ ; \$ "/x, "" ],
        ],

        [
            # shady but legal - placeholder
            "auto-generated proto with no type",
            [
                'void',
                'foo(a, b, c = 0)',
            ],
            [ 0, 0, qr/"\$\$;\$"/, ""  ],
        ],

        [
            "auto-generated proto with backcompat SV* placeholder",
            [
                'void',
                'foo(int a, SV*, char *c = "")',
                'C_ARGS: a, c',
            ],
            [ 0, 0, qr/"\$\$;\$"/, ""  ],
        ],
    );

    test_many($preamble, 'boot_Foo', \@test_fns);
}

{
    # Test RETVAL with the dXSTARG optimisation. When the return type
    # corresponds to a simple sv_setXv($arg, $val) in the typemap,
    # use the OP_ENTERSUB's TARG if possible, rather than creating a new
    # mortal each time.

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES:  DISABLE
        |
        |TYPEMAP: <<EOF
        |const int     T_IV
        |const long    T_MYIV
        |const short   T_MYSHORT
        |undef_t       T_MYUNDEF
        |ivmg_t        T_MYIVMG
        |
        |INPUT
        |T_MYIV
        |    $var = ($type)SvIV($arg)
        |
        |OUTPUT
        |T_OBJECT
        |    sv_setiv($arg, (IV)$var);
        |
        |T_MYSHORT
        |    ${ "$var" eq "RETVAL" ? \"$arg = $var;" : \"sv_setiv($arg, $var);" }
        |
        |T_MYUNDEF
        |    sv_set_undef($arg);
        |
        |T_MYIVMG
        |    sv_setiv_mg($arg, (IV)RETVAL);
        |EOF
EOF

    my @test_fns = (
        [
            "dXSTARG int (IV)",
            [
                'int',
                'foo()',
            ],
            [ 0, 0, qr/\bdXSTARG;/,   "has targ def" ],
            [ 0, 0, qr/\bTARGi\b/,    "has TARGi" ],
            [ 0, 1, qr/sv_newmortal/, "doesn't have newmortal" ],
        ],

        [
            # same as int, but via custom typemap entry
            "dXSTARG const int (IV)",
            [
                'const int',
                'foo()',
            ],
            [ 0, 0, qr/\bdXSTARG;/,   "has targ def" ],
            [ 0, 0, qr/\bTARGi\b/,    "has TARGi" ],
            [ 0, 1, qr/sv_newmortal/, "doesn't have newmortal" ],
        ],

        [
            # same as int, but via custom typemap OUTPUT entry
            "dXSTARG const long (MYIV)",
            [
                'const int',
                'foo()',
            ],
            [ 0, 0, qr/\bdXSTARG;/,   "has targ def" ],
            [ 0, 0, qr/\bTARGi\b/,    "has TARGi" ],
            [ 0, 1, qr/sv_newmortal/, "doesn't have newmortal" ],
        ],

        [
            "dXSTARG unsigned long (UV)",
            [
                'unsigned long',
                'foo()',
            ],
            [ 0, 0, qr/\bdXSTARG;/,   "has targ def" ],
            [ 0, 0, qr/\bTARGu\b/,    "has TARGu" ],
            [ 0, 1, qr/sv_newmortal/, "doesn't have newmortal" ],
        ],

        [
            "dXSTARG time_t (NV)",
            [
                'time_t',
                'foo()',
            ],
            [ 0, 0, qr/\bdXSTARG;/,   "has targ def" ],
            [ 0, 0, qr/\bTARGn\b/,    "has TARGn" ],
            [ 0, 1, qr/sv_newmortal/, "doesn't have newmortal" ],
        ],

        [
            "dXSTARG char (pvn)",
            [
                'char',
                'foo()',
            ],
            [ 0, 0, qr/\bdXSTARG;/,   "has targ def" ],
            [ 0, 0, qr/\bsv_setpvn\b/,"has sv_setpvn()" ],
            [ 0, 1, qr/sv_newmortal/, "doesn't have newmortal" ],
        ],

        [
            "dXSTARG char * (PV)",
            [
                'char *',
                'foo()',
            ],
            [ 0, 0, qr/\bdXSTARG;/,   "has targ def" ],
            [ 0, 0, qr/\bsv_setpv\b/, "has sv_setpv" ],
            [ 0, 0, qr/\QST(0) = TARG;/, "has ST(0) = TARG" ],
            [ 0, 1, qr/sv_newmortal/, "doesn't have newmortal" ],
        ],

        [
            "dXSTARG int (IV) with outlist",
            [
                'int',
                'foo(OUTLIST int a, OUTLIST int b)',
            ],
            [ 0, 0, qr/\bdXSTARG;/,      "has targ def" ],
            [ 0, 0, qr/\bXSprePUSH;/,    "has XSprePUSH" ],
            [ 0, 1, qr/\bXSprePUSH\b.+\bXSprePUSH\b/s,
                                         "has only one XSprePUSH" ],

            [ 0, 0, qr/\bTARGi\b/,       "has TARGi" ],
            [ 0, 0, qr/\bsv_setiv\(RETVALSV.*sv_setiv\(RETVALSV/s,
                                         "has two setiv(RETVALSV,...)" ],

            [ 0, 0, qr/\bXSRETURN\(3\)/, "has XSRETURN(3)" ],
        ],

        # Test RETVAL with an overridden typemap template in OUTPUT
        [
            "RETVAL overridden typemap: non-TARGable",
            [
                'int',
                'foo()',
                '    OUTPUT:',
                '        RETVAL my_sv_setiv(ST(0), RETVAL);',
            ],
            [ 0, 0, qr/\bmy_sv_setiv\b/,   "has my_sv_setiv" ],
        ],

        [
            "RETVAL overridden typemap: TARGable",
            [
                'int',
                'foo()',
                '    OUTPUT:',
                '        RETVAL sv_setiv(ST(0), RETVAL);',
            ],
            # XXX currently the TARG optimisation isn't done
            # XXX when this is fixed, update the test
            [ 0, 0, qr/\bsv_setiv\b/,   "has sv_setiv" ],
        ],

        [
            "dXSTARG with variant typemap",
            [
                'void',
                'foo(OUTLIST const short a)',
            ],
            [ 0, 0, qr/\bdXSTARG;/,      "has targ def" ],
            [ 0, 0, qr/\bTARGi\b/,       "has TARGi" ],
            [ 0, 1, qr/\bsv_setiv\(/,    "has NO sv_setiv" ],
            [ 0, 0, qr/\bXSRETURN\(1\)/, "has XSRETURN(1)" ],
        ],

        [
            "dXSTARG with sv_set_undef",
            [
                'void',
                'foo(OUTLIST undef_t a)',
            ],
            [ 0, 0, qr/\bdXSTARG;/,          "has targ def" ],
            [ 0, 0, qr/\bsv_set_undef\(/,    "has sv_set_undef" ],
        ],

        [
            "dXSTARG with sv_setiv_mg",
            [
                'ivmg_t',
                'foo()',
            ],
            [ 0, 0, qr/\bdXSTARG;/,          "has targ def" ],
            [ 0, 0, qr/\bTARGi\(/,           "has TARGi" ],
        ],
    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # Test OUTPUT: keyword

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES:  DISABLE
        |
EOF

    my @test_fns = (
        [
            "OUTPUT RETVAL",
            [ Q(<<'EOF') ],
                |int
                |foo(int a)
                |    CODE:
                |      RETVAL = 99
                |    OUTPUT:
                |      RETVAL
EOF
            [ 0, 1, qr/\bSvSETMAGIC\b/,   "no set magic" ],
            [ 0, 0, qr/\bTARGi\b/,        "has TARGi" ],
            [ 0, 0, qr/\QXSRETURN(1)/,    "has XSRETURN" ],
        ],

        [
            "OUTPUT RETVAL with set magic ignored",
            [ Q(<<'EOF') ],
                |int
                |foo(int a)
                |    CODE:
                |      RETVAL = 99
                |    OUTPUT:
                |      SETMAGIC: ENABLE
                |      RETVAL
EOF
            [ 0, 1, qr/\bSvSETMAGIC\b/,   "no set magic" ],
            [ 0, 0, qr/\bTARGi\b/,        "has TARGi" ],
            [ 0, 0, qr/\QXSRETURN(1)/,    "has XSRETURN" ],
        ],

        [
            "OUTPUT RETVAL with code",
            [ Q(<<'EOF') ],
                |int
                |foo(int a)
                |    CODE:
                |      RETVAL = 99
                |    OUTPUT:
                |      RETVAL PUSHs(my_newsviv(RETVAL));
EOF
            [ 0, 0, qr/\QPUSHs(my_newsviv(RETVAL));/,   "uses code" ],
            [ 0, 0, qr/\QXSRETURN(1)/,                  "has XSRETURN" ],
        ],

        [
            "OUTPUT RETVAL with code and template-like syntax",
            [ Q(<<'EOF') ],
                |int
                |foo(int a)
                |    CODE:
                |      RETVAL = 99
                |    OUTPUT:
                |      RETVAL baz($arg,$val);
EOF
            # Check that the override code is *not* template-expanded.
            # This was probably originally an implementation error, but
            # keep that behaviour for now for backwards compatibility.
            [ 0, 0, qr'baz\(\$arg,\$val\);',            "vars not expanded" ],
        ],

        [
            "OUTPUT RETVAL with code on IN_OUTLIST param",
            [ Q(<<'EOF') ],
                |int
                |foo(IN_OUTLIST int abc)
                |    CODE:
                |      RETVAL = 99
                |    OUTPUT:
                |      RETVAL
                |      abc  my_set(ST[0], RETVAL);
EOF
            [ 0, 0, qr/\Qmy_set(ST[0], RETVAL)/,      "code used for st(0)" ],
            [ 0, 0, qr/\bXSprePUSH;/,                 "XSprePUSH" ],
            [ 0, 1, qr/\bEXTEND\b/,                   "NO extend"       ],
            [ 0, 0, qr/\QTARGi((IV)RETVAL, 1);/,      "push RETVAL" ],
            [ 0, 0, qr/\QRETVALSV = sv_newmortal();/, "create mortal" ],
            [ 0, 0, qr/\Qsv_setiv(RETVALSV, (IV)abc);/, "code not used for st(1)" ],
            [ 0, 0, qr/\QXSRETURN(2)/,                "has XSRETURN" ],
        ],

        [
            "OUTPUT RETVAL with code and unknown type",
            [ Q(<<'EOF') ],
                |blah
                |foo(int a)
                |    CODE:
                |      RETVAL = 99
                |    OUTPUT:
                |      RETVAL PUSHs(my_newsviv(RETVAL));
EOF
            [ 0, 0, qr/blah\s+RETVAL;/,                 "decl" ],
            [ 0, 0, qr/\QPUSHs(my_newsviv(RETVAL));/,   "uses code" ],
            [ 0, 0, qr/\QXSRETURN(1)/,                  "has XSRETURN" ],
        ],

        [
            "OUTPUT vars with set magic mixture",
            [ Q(<<'EOF') ],
                |int
                |foo(int aaa, int bbb, int ccc, int ddd)
                |    CODE:
                |      RETVAL = 99
                |    OUTPUT:
                |      RETVAL
                |      aaa
                |      SETMAGIC: ENABLE
                |      bbb
                |      SETMAGIC: DISABLE
                |      ccc
                |      SETMAGIC: ENABLE
                |      ddd  my_set(xyz)
EOF
            [ 0, 0, qr/\b\QSvSETMAGIC(ST(0))/,       "set magic ST(0)" ],
            [ 0, 0, qr/\b\QSvSETMAGIC(ST(1))/,       "set magic ST(1)" ],
            [ 0, 1, qr/\b\QSvSETMAGIC(ST(2))/,       "no set magic ST(2)" ],
            [ 0, 0, qr/\b\QSvSETMAGIC(ST(3))/,       "set magic ST(3)" ],
            [ 0, 0, qr/\b\Qsv_setiv(ST(0),\E.*aaa/,  "setiv(aaa)" ],
            [ 0, 0, qr/\b\Qsv_setiv(ST(1),\E.*bbb/,  "setiv(bbb)" ],
            [ 0, 0, qr/\b\Qsv_setiv(ST(2),\E.*ccc/,  "setiv(ccc)" ],
            [ 0, 1, qr/\b\Qsv_setiv(ST(3)/,          "no setiv(ddd)" ],
            [ 0, 0, qr/\b\Qmy_set(xyz)/,             "myset" ],
            [ 0, 0, qr/\bTARGi\b.*RETVAL/,           "has TARGi(RETVAL,1)" ],
            [ 0, 0, qr/\QXSRETURN(1)/,               "has XSRETURN" ],
        ],

        [
            "duplicate OUTPUT RETVAL",
            [ Q(<<'EOF') ],
                |int
                |foo(int aaa)
                |    CODE:
                |      RETVAL = 99
                |    OUTPUT:
                |      RETVAL
                |      RETVAL
EOF
            [ 1, 0, qr/Error: duplicate OUTPUT parameter 'RETVAL'/, "" ],
        ],

        [
            "duplicate OUTPUT parameter",
            [ Q(<<'EOF') ],
                |int
                |foo(int aaa)
                |    CODE:
                |      RETVAL = 99
                |    OUTPUT:
                |      RETVAL
                |      aaa
                |      aaa
EOF
            [ 1, 0, qr/Error: duplicate OUTPUT parameter 'aaa'/, "" ],
        ],

        [
            "RETVAL in CODE without OUTPUT section",
            [ Q(<<'EOF') ],
                |int
                |foo()
                |    CODE:
                |      RETVAL = 99
EOF
            [ 1, 0, qr/Warning: Found a 'CODE' section which seems to be using 'RETVAL' but no 'OUTPUT' section/, "" ],
        ],

        [
            # This one *shouldn't* warn. For a void XSUB, RETVAL
            # is just another local variable.
            "void RETVAL in CODE without OUTPUT section",
            [ Q(<<'EOF') ],
                |void
                |foo()
                |    PREINIT:
                |      int RETVAL;
                |    CODE:
                |      RETVAL = 99
EOF
            [ 1, 1, qr/Warning: Found a 'CODE' section which seems to be using 'RETVAL' but no 'OUTPUT' section/, "no warn" ],
        ],

        [
            "RETVAL in CODE without being in OUTPUT",
            [ Q(<<'EOF') ],
                |int
                |foo(int aaa)
                |    CODE:
                |      RETVAL = 99
                |    OUTPUT:
                |      aaa
EOF
            [ 1, 0, qr/Warning: Found a 'CODE' section which seems to be using 'RETVAL' but no 'OUTPUT' section/, "" ],
        ],

        [
            "OUTPUT RETVAL not a parameter",
            [ Q(<<'EOF') ],
                |void
                |foo(int aaa)
                |    CODE:
                |      xyz
                |    OUTPUT:
                |      RETVAL
EOF
            [ 1, 0, qr/\QError: OUTPUT RETVAL not a parameter/, "" ],
        ],

        [
            "OUTPUT RETVAL IS a parameter",
            [ Q(<<'EOF') ],
                |int
                |foo(int aaa)
                |    CODE:
                |      xyz
                |    OUTPUT:
                |      RETVAL
EOF
            [ 1, 1, qr/\QError: OUTPUT RETVAL not a parameter/, "" ],
        ],

        [
            "OUTPUT foo not a parameter",
            [ Q(<<'EOF') ],
                |void
                |foo(int aaa)
                |    CODE:
                |      xyz
                |    OUTPUT:
                |      bbb
EOF
            [ 1, 0, qr/\QError: OUTPUT bbb not a parameter/, "" ],
        ],

        [
            "OUTPUT length(foo) not a parameter",
            [ Q(<<'EOF') ],
                |void
                |foo(char* aaa, int length(aaa))
                |    CODE:
                |      xyz
                |    OUTPUT:
                |      length(aaa)
EOF
            [ 1, 0, qr/\QError: OUTPUT length(aaa) not a parameter/, "" ],
        ],

        [
            "OUTPUT with IN_OUTLIST",
            [ Q(<<'EOF') ],
                |char*
                |foo(IN_OUTLIST int abc)
                |    CODE:
                |        RETVAL=999
                |    OUTPUT:
                |        RETVAL
                |        abc
EOF
            # OUT var - update arg 0 on stack
            [ 0, 0, qr/\b\Qsv_setiv(ST(0),\E.*abc/,  "setiv(ST0, abc)" ],
            [ 0, 0, qr/\b\QSvSETMAGIC(ST(0))/,       "set magic ST(0)" ],
            # prepare stack for OUTLIST
            [ 0, 0, qr/\bXSprePUSH\b/,               "XSprePUSH" ],
            [ 0, 1, qr/\bEXTEND\b/,                  "NO extend"       ],
            # OUTPUT: RETVAL: push return value on stack
            [ 0, 0, qr/\bsv_setpv\(\(SV\*\)TARG,\s*RETVAL\)/,"sv_setpv(TARG, RETVAL)" ],
            [ 0, 0, qr/\QST(0) = TARG;/,             "has ST(0) = TARG" ],
            # OUTLIST: push abc on stack
            [ 0, 0, qr/\QRETVALSV = sv_newmortal();/, "create mortal" ],
            [ 0, 0, qr/\b\Qsv_setiv(RETVALSV, (IV)abc);/,"sv_setiv(RETVALSV, abc)" ],
            [ 0, 0, qr/\b\QST(1) = RETVALSV;\E\s+\}\s+\Q++SP;/, "store RETVALSV"],
            # and return RETVAL and abc
            [ 0, 0, qr/\QXSRETURN(2)/,               "has XSRETURN" ],

            # should only be one SvSETMAGIC
            [ 0, 1, qr/\bSvSETMAGIC\b.*\bSvSETMAGIC\b/s,"only one SvSETMAGIC" ],
        ],
    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # Test RETVAL as a parameter. This isn't well documented as to
    # how it should be interpreted, so these tests are more about checking
    # current behaviour so that inadvertent changes are detected, rather
    # than approving the current behaviour.

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES:  DISABLE
        |
EOF

    my @test_fns = (

        # First, with void return type.
        # Generally in this case, RETVAL is currently not special - it's
        # just another name for a parameter. If it doesn't have a type
        # specified, it's treated as a placeholder.

        [
            # XXX this generates an autocall using undeclared RETVAL,
            # which should be an error
            "void RETVAL no-type param autocall",
            [ Q(<<'EOF') ],
                |void
                |foo(RETVAL, short abc)
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,        "abc is ST1" ],
            [ 0, 0, qr/\Qfoo(RETVAL, abc)/,              "autocall" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,              "ret empty" ],
        ],

        [
            "void RETVAL no-type param",
            [ Q(<<'EOF') ],
                |void
                |foo(RETVAL, short abc)
                |    CODE:
                |        xyz
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,        "abc is ST1" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,              "ret empty" ],
        ],

        [
            "void RETVAL typed param autocall",
            [ Q(<<'EOF') ],
                |void
                |foo(int RETVAL, short abc)
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=.*\QST(0)/,     "declare and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,        "abc is ST1" ],
            [ 0, 0, qr/\Qfoo(RETVAL, abc)/,              "autocall" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,              "ret empty" ],
        ],

        [
            "void RETVAL INPUT typed param autocall",
            [ Q(<<'EOF') ],
                |void
                |foo(RETVAL, short abc)
                |   int RETVAL
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=.*\QST(0)/,     "declare and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,        "abc is ST1" ],
            [ 0, 0, qr/\Qfoo(RETVAL, abc)/,              "autocall" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,              "ret empty" ],
        ],

        [
            "void RETVAL typed param",
            [ Q(<<'EOF') ],
                |void
                |foo(int RETVAL, short abc)
                |    CODE:
                |        xyz
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=.*\QST(0)/,     "declare and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,        "abc is ST1" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,              "ret empty" ],
        ],

        [
            "void RETVAL INPUT typed param",
            [ Q(<<'EOF') ],
                |void
                |foo(RETVAL, short abc)
                |   int RETVAL
                |    CODE:
                |        xyz
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=.*\QST(0)/,     "declare and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,        "abc is ST1" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,              "ret empty" ],
        ],

        [
            "void RETVAL alien autocall",
            [ Q(<<'EOF') ],
                |void
                |foo(short abc)
                |   int RETVAL = 99
EOF
            [ 0, 0, qr/_usage\(cv,\s*"abc"\)/,           "usage" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=\s*99/,         "declare and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(0)/,        "abc is ST0" ],
            [ 0, 0, qr/\Qfoo(abc)/,                      "autocall" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,              "ret empty" ],
        ],

        [
            "void RETVAL alien",
            [ Q(<<'EOF') ],
                |void
                |foo(short abc)
                |   int RETVAL = 99
EOF
            [ 0, 0, qr/_usage\(cv,\s*"abc"\)/,           "usage" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=\s*99/,         "declare and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(0)/,        "abc is ST0" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,              "ret empty" ],
        ],


        # Next, with 'long' return type.
        # Generally, RETVAL is treated as a normal parameter, with
        # some bad behaviour (such as multiple definitions) when that
        # clashes with the implicit use of RETVAL

        [
            "long RETVAL no-type param autocall",
            [ Q(<<'EOF') ],
                |long
                |foo(RETVAL, short abc)
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            # XXX RETVAL is passed uninitialised to the autocall fn
            [ 0, 0, qr/long\s+RETVAL;/,                  "declare no init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,        "abc is ST1" ],
            [ 0, 0, qr/\Qfoo(RETVAL, abc)/,              "autocall" ],
            [ 0, 0, qr/\b\QXSRETURN(1)/,                 "ret 1" ],
        ],

        [
            "long RETVAL no-type param",
            [ Q(<<'EOF') ],
                |long
                |foo(RETVAL, short abc)
                |    CODE:
                |        xyz
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            [ 0, 0, qr/long\s+RETVAL;/,                  "declare no init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,        "abc is ST1" ],
            [ 0, 0, qr/\b\QXSRETURN(1)/,                 "ret 1" ],
        ],

        [
            "long RETVAL typed param autocall",
            [ Q(<<'EOF') ],
                |long
                |foo(int RETVAL, short abc)
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            # duplicate or malformed declarations used to be emitted
            [ 0, 1, qr/int\s+RETVAL;/,                   "no none init init" ],
            [ 0, 1, qr/long\s+RETVAL;/,                  "no none init long" ],

            [ 0, 0, qr/\bint\s+RETVAL\s*=.*\QST(0)/,     "int  decl and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,        "abc is ST1" ],
            [ 0, 0, qr/\bRETVAL\s*=\s*foo\(RETVAL, abc\)/,"autocall" ],
            [ 0, 0, qr/\b\QTARGi((IV)RETVAL, 1)/,        "TARGi" ],
            [ 0, 0, qr/\b\QXSRETURN(1)/,                 "ret 1" ],
        ],

        [
            "long RETVAL INPUT typed param autocall",
            [ Q(<<'EOF') ],
                |long
                |foo(RETVAL, short abc)
                |   int RETVAL
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            [ 0, 1, qr/long\s+RETVAL/,                   "no long decl" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=.*\QST(0)/,     "int  decl and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,        "abc is ST1" ],
            [ 0, 0, qr/\bRETVAL\s*=\s*foo\(RETVAL, abc\)/,"autocall" ],
            [ 0, 0, qr/\b\QTARGi((IV)RETVAL, 1)/,         "TARGi" ],
            [ 0, 0, qr/\b\QXSRETURN(1)/,                  "ret 1" ],
        ],

        [
            "long RETVAL INPUT typed param autocall 2nd pos",
            [ Q(<<'EOF') ],
                |long
                |foo(short abc, RETVAL)
                |   int RETVAL
EOF
            [ 0, 0, qr/_usage\(cv,\s*"abc,\s*RETVAL"\)/, "usage" ],
            [ 0, 1, qr/long\s+RETVAL/,                   "no long decl" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=.*\QST(1)/,     "int  decl and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(0)/,        "abc is ST0" ],
            [ 0, 0, qr/\bRETVAL\s*=\s*foo\(abc, RETVAL\)/,"autocall" ],
            [ 0, 0, qr/\b\QTARGi((IV)RETVAL, 1)/,         "TARGi" ],
            [ 0, 0, qr/\b\QXSRETURN(1)/,                  "ret 1" ],
        ],

        [
            "long RETVAL typed param",
            [ Q(<<'EOF') ],
                |long
                |foo(int RETVAL, short abc)
                |    CODE:
                |        xyz
                |    OUTPUT:
                |        RETVAL
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            # duplicate or malformed declarations used to be emitted
            [ 0, 1, qr/int\s+RETVAL;/,                "no none init init" ],
            [ 0, 1, qr/long\s+RETVAL;/,               "no none init long" ],

            [ 0, 0, qr/\bint\s+RETVAL\s*=.*\QST(0)/,  "int  decl and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,     "abc is ST1" ],
            [ 0, 0, qr/\b\QTARGi((IV)RETVAL, 1)/,     "TARGi" ],
            [ 0, 0, qr/\b\QXSRETURN(1)/,              "ret 1" ],
        ],

        [
            "long RETVAL INPUT typed param",
            [ Q(<<'EOF') ],
                |long
                |foo(RETVAL, short abc)
                |    int RETVAL
                |    CODE:
                |        xyz
                |    OUTPUT:
                |        RETVAL
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL,\s*abc"\)/, "usage" ],
            [ 0, 1, qr/long\s+RETVAL/,                "no long declare" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=.*\QST(0)/,  "int  declare and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(1)/,     "abc is ST1" ],
            [ 0, 0, qr/\b\QTARGi((IV)RETVAL, 1)/,     "TARGi" ],
            [ 0, 0, qr/\b\QXSRETURN(1)/,              "ret 1" ],
        ],

        [
            "long RETVAL alien autocall",
            [ Q(<<'EOF') ],
                |long
                |foo(short abc)
                |   int RETVAL = 99
EOF
            [ 0, 0, qr/_usage\(cv,\s*"abc"\)/,        "usage" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=\s*99/,      "declare and init" ],
            [ 0, 0, qr/short\s+abc\s*=.*\QST(0)/,     "abc is ST0" ],
            [ 0, 0, qr/\bRETVAL\s*=\s*foo\(abc\)/,    "autocall" ],
            [ 0, 0, qr/\b\QXSRETURN(1)/,              "ret 1" ],
        ],

        [
            "long RETVAL alien",
            [ Q(<<'EOF') ],
                |long
                |foo(abc, def)
                |   int def
                |   int RETVAL = 99
                |   int abc
                |  CODE:
                |    xyz
EOF
            [ 0, 0, qr/_usage\(cv,\s*"abc,\s*def"\)/, "usage" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=\s*99/,      "declare and init" ],
            [ 0, 0, qr/int\s+abc\s*=.*\QST(0)/,       "abc is ST0" ],
            [ 0, 0, qr/int\s+def\s*=.*\QST(1)/,       "def is ST1" ],
            [ 0, 0, qr/int\s+def.*int\s+RETVAL.*int\s+abc/s,  "ordering" ],
            [ 0, 0, qr/\b\QXSRETURN(1)/,              "ret 1" ],
        ],


        # Test NO_OUTPUT

        [
            "NO_OUTPUT autocall",
            [ Q(<<'EOF') ],
                |NO_OUTPUT long
                |foo(int abc)
EOF
            [ 0, 0, qr/_usage\(cv,\s*"abc"\)/,        "usage" ],
            [ 0, 0, qr/long\s+RETVAL;/,               "long declare  no init" ],
            [ 0, 0, qr/int\s+abc\s*=.*\QST(0)/,       "abc is ST0" ],
            [ 0, 0, qr/\bRETVAL\s*=\s*foo\(abc\)/,    "autocall" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,           "ret empty" ],
        ],

        [
            # NO_OUTPUT with void should be a NOOP, but check
            "NO_OUTPUT void autocall",
            [ Q(<<'EOF') ],
                |NO_OUTPUT void
                |foo(int abc)
EOF
            [ 0, 0, qr/_usage\(cv,\s*"abc"\)/,        "usage" ],
            [ 0, 1, qr/\s+RETVAL;/,                   "don't declare RETVAL" ],
            [ 0, 0, qr/int\s+abc\s*=.*\QST(0)/,       "abc is ST0" ],
            [ 0, 0, qr/^\s*foo\(abc\)/m,              "void autocall" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,           "ret empty" ],
        ],

        [
            "NO_OUTPUT with RETVAL autocall",
            [ Q(<<'EOF') ],
                |NO_OUTPUT long
                |foo(int RETVAL)
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL"\)/,     "usage" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=/,           "declare and init" ],
            [ 0, 0, qr/\bRETVAL\s*=\s*foo\(RETVAL\)/, "autocall" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,           "ret empty" ],
        ],

        [
            "NO_OUTPUT with CODE",
            [ Q(<<'EOF') ],
                |NO_OUTPUT long
                |foo(int abc)
                |   CODE:
                |      xyz
EOF
            [ 0, 0, qr/_usage\(cv,\s*"abc"\)/,        "usage" ],
            [ 0, 0, qr/long\s+RETVAL;/,               "long declare  no init" ],
            [ 0, 0, qr/int\s+abc\s*=.*\QST(0)/,       "abc is ST0" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,           "ret empty" ],
        ],

        [
            # NO_OUTPUT with void should be a NOOP, but check
            "NO_OUTPUT void with CODE",
            [ Q(<<'EOF') ],
                |NO_OUTPUT void
                |foo(int abc)
                |   CODE:
                |      xyz
EOF
            [ 0, 0, qr/_usage\(cv,\s*"abc"\)/,        "usage" ],
            [ 0, 1, qr/\s+RETVAL;/,                   "don't declare RETVAL" ],
            [ 0, 0, qr/int\s+abc\s*=.*\QST(0)/,       "abc is ST0" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,           "ret empty" ],
        ],

        [
            "NO_OUTPUT with RETVAL and CODE",
            [ Q(<<'EOF') ],
                |NO_OUTPUT long
                |foo(int RETVAL)
                |   CODE:
                |      xyz
EOF
            [ 0, 0, qr/_usage\(cv,\s*"RETVAL"\)/,     "usage" ],
            [ 0, 0, qr/\bint\s+RETVAL\s*=/,           "declare and init" ],
            [ 0, 0, qr/\bXSRETURN_EMPTY\b/,           "ret empty" ],
        ],


        [
            "NO_OUTPUT with CODE and OUTPUT",
            [ Q(<<'EOF') ],
                |NO_OUTPUT long
                |foo(int abc)
                |   CODE:
                |      xyz
                |   OUTPUT:
                |      RETVAL
EOF
            [ 1, 0, qr/Error: can't use RETVAL in OUTPUT when NO_OUTPUT declared/,  "OUTPUT err" ],
        ],

        [
            "NO_OUTPUT with RETVAL param and OUTPUT",
            [ Q(<<'EOF') ],
                |NO_OUTPUT long
                |foo(int RETVAL)
                |   OUTPUT:
                |      RETVAL
EOF
            [ 1, 0, qr/Error: can't use RETVAL in OUTPUT when NO_OUTPUT declared/,  "OUTPUT err" ],
        ],

        [
            "NO_OUTPUT with RETVAL param, CODE and OUTPUT",
            [ Q(<<'EOF') ],
                |NO_OUTPUT long
                |foo(int RETVAL)
                |   CODE:
                |      xyz
                |   OUTPUT:
                |      RETVAL
EOF
            [ 1, 0, qr/Error: can't use RETVAL in OUTPUT when NO_OUTPUT declared/,  "OUTPUT err" ],
        ],


        # Test duplicate RETVAL parameters

        [
            "void dup",
            [ Q(<<'EOF') ],
                |void
                |foo(RETVAL, RETVAL)
EOF
            [ 1, 0, qr/Error: duplicate definition of parameter 'RETVAL'/,  "" ],
        ],

        [
            "void dup typed",
            [ Q(<<'EOF') ],
                |void
                |foo(int RETVAL, short RETVAL)
EOF
            [ 1, 0, qr/Error: duplicate definition of parameter 'RETVAL'/,  "" ],
        ],

        [
            "void dup INPUT",
            [ Q(<<'EOF') ],
                |void
                |foo(RETVAL, RETVAL)
                |   int RETVAL
EOF
            [ 1, 0, qr/Error: duplicate definition of parameter 'RETVAL'/,  "" ],
        ],

        [
            "long dup",
            [ Q(<<'EOF') ],
                |long
                |foo(RETVAL, RETVAL)
EOF
            [ 1, 0, qr/Error: duplicate definition of parameter 'RETVAL'/,  "" ],
        ],

        [
            "long dup typed",
            [ Q(<<'EOF') ],
                |long
                |foo(int RETVAL, short RETVAL)
EOF
            [ 1, 0, qr/Error: duplicate definition of parameter 'RETVAL'/,  "" ],
        ],

        [
            "long dup INPUT",
            [ Q(<<'EOF') ],
                |long
                |foo(RETVAL, RETVAL)
                |   int RETVAL
EOF
            [ 1, 0, qr/Error: duplicate definition of parameter 'RETVAL'/,  "" ],
        ],


    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # Test RETVAL return mixed types.
    # Where the return type of the XSUB differs from the declared type
    # of the RETVAL var. For backwards compatibility, we should use the
    # XSUB type when returning.

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES:  DISABLE
        |
        |TYPEMAP: <<EOF
        |my_type    T_MY_TYPE
        |
        |OUTPUT
        |T_MY_TYPE
        |    sv_set_my_type($arg, (my_type)$var);
        |EOF
EOF

    my @test_fns = (

        [
            "RETVAL mixed type",
            [ Q(<<'EOF') ],
                |my_type
                |foo(int RETVAL)
EOF
            [ 0, 0, qr/int\s+RETVAL\s*=.*SvIV\b/,  "RETVAL is int" ],
            [ 0, 0, qr/sv_set_my_type\(/,          "return is my_type" ],
        ],

        [
            "RETVAL mixed type INPUT",
            [ Q(<<'EOF') ],
                |my_type
                |foo(RETVAL)
                |    int RETVAL
EOF
            [ 0, 0, qr/int\s+RETVAL\s*=.*SvIV\b/,  "RETVAL is int" ],
            [ 0, 0, qr/sv_set_my_type\(/,          "return is my_type" ],
        ],

        [
            "RETVAL mixed type alien",
            [ Q(<<'EOF') ],
                |my_type
                |foo()
                |  int RETVAL = 99;
EOF
            [ 0, 0, qr/int\s+RETVAL\s*=\s*99/,     "RETVAL is int" ],
            [ 0, 0, qr/sv_set_my_type\(/,          "return is my_type" ],
        ],

    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # Test CASE: blocks

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES:  DISABLE
        |
EOF

    my @test_fns = (

        [
            "CASE with dup INPUT and OUTPUT",
            [ Q(<<'EOF') ],
                |int
                |foo(abc, def)
                |    CASE: X
                |            int   abc;
                |            short def;
                |        CODE:
                |            RETVAL = abc + def;
                |        OUTPUT:
                |            RETVAL
                |
                |    CASE: Y
                |            long abc;
                |            long def;
                |        CODE:
                |            RETVAL = abc - def;
                |        OUTPUT:
                |            RETVAL
EOF
            [ 0, 0, qr/_usage\(cv,\s*"abc, def"\)/,     "usage" ],

            [ 0, 0, qr/
                       if \s* \(X\)
                       .*
                       int \s+ abc \s* = [^\n]* ST\(0\)
                       .*
                       else \s+ if \s* \(Y\)
                      /xs,                       "1st abc is int and ST(0)" ],
            [ 0, 0, qr/
                       else \s+ if \s* \(Y\)
                       .*
                       long \s+ abc \s* = [^\n]* ST\(0\)
                      /xs,                       "2nd abc is long and ST(0)" ],
            [ 0, 0, qr/
                       if \s* \(X\)
                       .*
                       short \s+ def \s* = [^\n]* ST\(1\)
                       .*
                       else \s+ if \s* \(Y\)
                      /xs,                       "1st def is short and ST(1)" ],
            [ 0, 0, qr/
                       else \s+ if \s* \(Y\)
                       .*
                       long \s+ def \s* = [^\n]* ST\(1\)
                      /xs,                       "2nd def is long and ST(1)" ],
            [ 0, 0, qr/
                       if \s* \(X\)
                       .*
                       int \s+ RETVAL;
                       .*
                       else \s+ if \s* \(Y\)
                      /xs,                       "1st RETVAL is int" ],
            [ 0, 0, qr/
                       else \s+ if \s* \(Y\)
                       .*
                       int \s+ RETVAL;
                       .*
                      /xs,                       "2nd RETVAL is int" ],

            [ 0, 0, qr/
                       if \s* \(X\)
                       .*
                       \QRETVAL = abc + def;\E
                       .*
                       else \s+ if \s* \(Y\)
                      /xs,                       "1st RETVAL assign" ],
            [ 0, 0, qr/
                       else \s+ if \s* \(Y\)
                       .*
                       \QRETVAL = abc - def;\E
                       .*
                      /xs,                       "2nd RETVAL assign" ],

            [ 0, 0, qr/\b\QXSRETURN(1)/,           "ret 1" ],
            [ 0, 1, qr/\bXSRETURN\b.*\bXSRETURN/s, "only a single XSRETURN" ],
        ],


    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # Test placeholders - various semi-official ways to to mark an
    # argument as 'unused'.

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES:  DISABLE
        |
EOF

    my @test_fns = (

        [
            "placeholder: typeless param with CODE",
            [ Q(<<'EOF') ],
                |int
                |foo(int AAA, BBB, int CCC)
                |   CODE:
                |      XYZ;
EOF
            [ 0, 0, qr/_usage\(cv,\s*"AAA, BBB, CCC"\)/,      "usage" ],
            [ 0, 0, qr/\bint\s+AAA\s*=\s*.*\Q(ST(0))/,        "AAA is ST(0)" ],
            [ 0, 0, qr/\bint\s+CCC\s*=\s*.*\Q(ST(2))/,        "CCC is ST(2)" ],
            [ 0, 1, qr/\bBBB;/,                               "no BBB decl" ],
        ],

        [
            "placeholder: typeless param bodiless",
            [ Q(<<'EOF') ],
                |int
                |foo(int AAA, BBB, int CCC)
EOF
            [ 0, 0, qr/_usage\(cv,\s*"AAA, BBB, CCC"\)/,      "usage" ],
            # Note that autocall uses the BBB var even though it isn't
            # declared. It would be up to the coder to use C_ARGS, or add
            # such a var via PREINIT.
            [ 0, 0, qr/\bRETVAL\s*=\s*\Qfoo(AAA, BBB, CCC);/, "autocall" ],
            [ 0, 0, qr/\bint\s+AAA\s*=\s*.*\Q(ST(0))/,        "AAA is ST(0)" ],
            [ 0, 0, qr/\bint\s+CCC\s*=\s*.*\Q(ST(2))/,        "CCC is ST(2)" ],
            [ 0, 1, qr/\bBBB;/,                               "no BBB decl" ],
        ],

        [
            # this is the only IN/OUT etc one which works, since IN is the
            # default.
            "placeholder: typeless IN param with CODE",
            [ Q(<<'EOF') ],
                |int
                |foo(int AAA, IN BBB, int CCC)
                |   CODE:
                |      XYZ;
EOF
            [ 0, 0, qr/_usage\(cv,\s*"AAA, BBB, CCC"\)/,      "usage" ],
            [ 0, 0, qr/\bint\s+AAA\s*=\s*.*\Q(ST(0))/,        "AAA is ST(0)" ],
            [ 0, 0, qr/\bint\s+CCC\s*=\s*.*\Q(ST(2))/,        "CCC is ST(2)" ],
            [ 0, 1, qr/\bBBB;/,                               "no BBB decl" ],
        ],


        [
            "placeholder: typeless OUT param with CODE",
            [ Q(<<'EOF') ],
                |int
                |foo(int AAA, OUT BBB, int CCC)
                |   CODE:
                |      XYZ;
EOF
            [ 1, 0, qr/Can't determine output type for 'BBB'/, "got type err" ],
        ],

        [
            "placeholder: typeless IN_OUT param with CODE",
            [ Q(<<'EOF') ],
                |int
                |foo(int AAA, IN_OUT BBB, int CCC)
                |   CODE:
                |      XYZ;
EOF
            [ 1, 0, qr/Can't determine output type for 'BBB'/, "got type err" ],
        ],

        [
            "placeholder: typeless OUTLIST param with CODE",
            [ Q(<<'EOF') ],
                |int
                |foo(int AAA, OUTLIST BBB, int CCC)
                |   CODE:
                |      XYZ;
EOF
            [ 1, 0, qr/Can't determine output type for 'BBB'/, "got type err" ],
        ],

        [
            # a placeholder with a default value may not seem to make much
            # sense, but it allows an argument to still be passed (or
            # not), even if it;s no longer used.
            "placeholder: typeless default param with CODE",
            [ Q(<<'EOF') ],
                |int
                |foo(int AAA, BBB = 888, int CCC = 999)
                |   CODE:
                |      XYZ;
EOF
            [ 0, 0, qr/_usage\(cv,\s*"AAA, BBB = 888, CCC\s*= 999"\)/,"usage" ],
            [ 0, 0, qr/\bint\s+AAA\s*=\s*.*\Q(ST(0))/,        "AAA is ST(0)" ],
            [ 0, 0, qr/\bCCC\s*=\s*.*\Q(ST(2))/,              "CCC is ST(2)" ],
            [ 0, 1, qr/\bBBB;/,                               "no BBB decl" ],
            [ 0, 1, qr/\b888\s*;/,                            "no 888 usage" ],
        ],

        [
            "placeholder: allow SV *",
            [ Q(<<'EOF') ],
                |int
                |foo(int AAA, SV *, int CCC)
                |   CODE:
                |      XYZ;
EOF
            [ 0, 0, qr/_usage\(cv,\s*\Q"AAA, SV *, CCC")/,    "usage" ],
            [ 0, 0, qr/\bint\s+AAA\s*=\s*.*\Q(ST(0))/,        "AAA is ST(0)" ],
            [ 0, 0, qr/\bint\s+CCC\s*=\s*.*\Q(ST(2))/,        "CCC is ST(2)" ],
        ],

        [
            # Bodiless XSUBs can't use SV* as a placeholder ...
            "placeholder: SV *, bodiless",
            [ Q(<<'EOF') ],
                |int
                |foo(int AAA, SV    *, int CCC)
EOF
            [ 1, 0, qr/Error: parameter 'SV \*' not valid as a C argument/,
                                                           "got arg err" ],
        ],

        [
            # ... unless they use C_ARGS to define how the C fn should
            # be called.
            "placeholder: SV *, bodiless C_ARGS",
            [ Q(<<'EOF') ],
                |int
                |foo(int AAA, SV    *, int CCC)
                |    C_ARGS: AAA, CCC
EOF
            [ 0, 0, qr/_usage\(cv,\s*\Q"AAA, SV *, CCC")/,    "usage" ],
            [ 0, 0, qr/\bint\s+AAA\s*=\s*.*\Q(ST(0))/,        "AAA is ST(0)" ],
            [ 0, 0, qr/\bint\s+CCC\s*=\s*.*\Q(ST(2))/,        "CCC is ST(2)" ],
            [ 0, 0, qr/\bRETVAL\s*=\s*\Qfoo(AAA, CCC);/,      "autocall" ],
        ],


    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # Test weird packing facility: return type array(type,nitems)

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES:  DISABLE
        |
EOF

    my @test_fns = (

        [
            "array(int,5)",
            [ Q(<<'EOF') ],
                |array(int,5)
                |foo()
EOF
            [ 0, 0, qr/int\s*\*\s+RETVAL;/,      "RETVAL is int*" ],
            [ 0, 0, qr/sv_setpvn\(.*,\s*5\s*\*\s*\Qsizeof(int));/,
                                                 "return packs 5 ints" ],
            [ 0, 0, qr/\bdXSTARG\b/,             "declares TARG" ],
            [ 0, 0, qr/sv_setpvn\(TARG\b/,       "uses TARG" ],

        ],

        [
            "array(int*, expr)",
            [ Q(<<'EOF') ],
                |array(int*, FOO_SIZE)
                |foo()
EOF
            [ 0, 0, qr/int\s*\*\s*\*\s+RETVAL;/, "RETVAL is int**" ],
            [ 0, 0, qr/sv_setpvn\(.*,\s*FOO_SIZE\s*\*\s*sizeof\(int\s*\*\s*\)\);/,
                                                "return packs FOO_SIZE int*s" ],
        ],

        [
            "array() as param type",
            [ Q(<<'EOF') ],
                |int
                |foo(abc)
                |    array(int,5) abc
EOF
            [ 1, 0, qr/Could not find a typemap for C type/, " no find type" ],
        ],

        [
            "array() can be overriden by OUTPUT",
            [ Q(<<'EOF') ],
                |array(int,5)
                |foo()
                |    OUTPUT:
                |        RETVAL my_setintptr(ST(0), RETVAL);
EOF
            [ 0, 0, qr/int\s*\*\s+RETVAL;/,             "RETVAL is int*" ],
            [ 0, 0, qr/\Qmy_setintptr(ST(0), RETVAL);/, "override honoured" ],
        ],

        [
            "array() in output override isn't special",
            [ Q(<<'EOF') ],
                |short
                |foo()
                |    OUTPUT:
                |        RETVAL array(int,5)
EOF
            [ 0, 0, qr/short\s+RETVAL;/,      "RETVAL is short" ],
            [ 0, 0, qr/\Qarray(int,5)/,       "return expression is unchanged" ],
        ],

        [
            "array() OUT",
            [ Q(<<'EOF') ],
                |int
                |foo(OUT array(int,5) AAA)
EOF
            [ 1, 0, qr/\QCan't use array(type,nitems) type for OUT parameter/,
                        "got err" ],
        ],

        [
            "array() OUTLIST",
            [ Q(<<'EOF') ],
                |int
                |foo(OUTLIST array(int,5) AAA)
EOF
            [ 1, 0, qr/\QCan't use array(type,nitems) type for OUTLIST parameter/,
                    "got err" ],
        ],
    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

{
    # Test weird packing facility: DO_ARRAY_ELEM

    my $preamble = Q(<<'EOF');
        |MODULE = Foo PACKAGE = Foo
        |
        |PROTOTYPES:  DISABLE
        |
        |TYPEMAP: <<EOF
        |intArray *        T_ARRAY
        |longArray *       T_ARRAY
        |
        |myiv              T_IV
        |myivArray *       T_ARRAY
        |
        |blah              T_BLAH
        |blahArray *       T_ARRAY
        |
        |nosuchtypeArray * T_ARRAY
        |
        |shortArray *       T_DAE
        |
        |INPUT
        |T_BLAH
        |   $var = my_get_blah($arg);
        |
        |T_DAE
        |   IN($var,$type,$ntype,$subtype,$arg,$argoff){DO_ARRAY_ELEM}
        |
        |OUTPUT
        |T_BLAH
        |   my_set_blah($arg, $var);
        |
        |T_DAE
        |   OUT($var,$type,$ntype,$subtype,$arg){DO_ARRAY_ELEM}
        |
        |EOF
EOF

    my @test_fns = (

        [
            "T_ARRAY long input",
            [ Q(<<'EOF') ],
                |char *
                |foo(longArray * abc)
EOF
            [ 0, 0, qr/longArray\s*\*\s*abc;/,      "abc is longArray*" ],
            [ 0, 0, qr/abc\s*=\s*longArrayPtr\(/,   "longArrayPtr called" ],
            [ 0, 0, qr/abc\[ix_abc.*\]\s*=\s*.*\QSvIV(ST(ix_abc))/,
                                                    "abc[i] set" ],
            [ 0, 1, qr/DO_ARRAY_ELEM/,              "no DO_ARRAY_ELEM" ],
        ],
        [
            "T_ARRAY long output",
            [ Q(<<'EOF') ],
                |longArray *
                |foo()
EOF
            [ 0, 0, qr/longArray\s*\*\s*RETVAL;/,   "RETVAL is longArray*" ],
            [ 0, 1, qr/longArrayPtr/,               "longArrayPtr NOT called" ],
            [ 0, 0, qr/\Qsv_setiv(ST(ix_RETVAL), (IV)RETVAL[ix_RETVAL]);/,
                                                    "ST(i) set" ],
            [ 0, 1, qr/DO_ARRAY_ELEM/,              "no DO_ARRAY_ELEM" ],
        ],

        [
            "T_ARRAY myiv input",
            [ Q(<<'EOF') ],
                |char *
                |foo(myivArray * abc)
EOF
            [ 0, 0, qr/myivArray\s*\*\s*abc;/,      "abc is myivArray*" ],
            [ 0, 0, qr/abc\s*=\s*myivArrayPtr\(/,   "myivArrayPtr called" ],
            [ 0, 0, qr/abc\[ix_abc.*\]\s*=\s*.*\QSvIV(ST(ix_abc))/,
                                                    "abc[i] set" ],
            [ 0, 1, qr/DO_ARRAY_ELEM/,              "no DO_ARRAY_ELEM" ],
        ],
        [
            "T_ARRAY myiv output",
            [ Q(<<'EOF') ],
                |myivArray *
                |foo()
EOF
            [ 0, 0, qr/myivArray\s*\*\s*RETVAL;/,   "RETVAL is myivArray*" ],
            [ 0, 1, qr/myivArrayPtr/,               "myivArrayPtr NOT called" ],
            [ 0, 0, qr/\Qsv_setiv(ST(ix_RETVAL), (IV)RETVAL[ix_RETVAL]);/,
                                                    "ST(i) set" ],
            [ 0, 1, qr/DO_ARRAY_ELEM/,              "no DO_ARRAY_ELEM" ],
        ],

        [
            "T_ARRAY blah input",
            [ Q(<<'EOF') ],
                |char *
                |foo(blahArray * abc)
EOF
            [ 0, 0, qr/blahArray\s*\*\s*abc;/,      "abc is blahArray*" ],
            [ 0, 0, qr/abc\s*=\s*blahArrayPtr\(/,   "blahArrayPtr called" ],
            [ 0, 0, qr/abc\[ix_abc.*\]\s*=\s*.*\Qmy_get_blah(ST(ix_abc))/,
                                                    "abc[i] set" ],
            [ 0, 1, qr/DO_ARRAY_ELEM/,              "no DO_ARRAY_ELEM" ],
        ],
        [
            "T_ARRAY blah output",
            [ Q(<<'EOF') ],
                |blahArray *
                |foo()
EOF
            [ 0, 0, qr/blahArray\s*\*\s+RETVAL;/,   "RETVAL is blahArray*" ],
            [ 0, 1, qr/blahArrayPtr/,               "blahArrayPtr NOT called" ],
            [ 0, 0, qr/\Qmy_set_blah(ST(ix_RETVAL), RETVAL[ix_RETVAL]);/,
                                                    "ST(i) set" ],
            [ 0, 1, qr/DO_ARRAY_ELEM/,              "no DO_ARRAY_ELEM" ],
        ],

        [
            "T_ARRAY nosuchtype input",
            [ Q(<<'EOF') ],
                |char *
                |foo(nosuchtypeArray * abc)
EOF
            [ 1, 0, qr/Could not find a typemap for C type 'nosuchtype'/,
                                                    "no such type" ],
        ],
        [
            "T_ARRAY nosuchtype output",
            [ Q(<<'EOF') ],
                |nosuchtypeArray *
                |foo()
EOF
            [ 1, 0, qr/Could not find a typemap for C type 'nosuchtype'/,
                                                    "no such type" ],
        ],

        # test DO_ARRAY_ELEM in a typemap other than T_ARRAY.
        #
        # XXX It's not clear whether DO_ARRAY_ELEM should be processed
        # in typemap definitions generally, rather than just in the
        # T_ARRAY definition. Currently it is, but DO_ARRAY_ELEM isn't
        # documented, and was clearly put into place as a hack to make
        # T_ARRAY work. So these tests represent the *current*
        # behaviour, but don't necessarily endorse that behaviour. These
        # tests ensure that any change in behaviour is deliberate rather
        # than accidental.
        [
            "T_DAE input",
            [ Q(<<'EOF') ],
                |char *
                |foo(shortArray * abc)
EOF
            [ 0, 0, qr/shortArray\s*\*\s*abc;/,      "abc is shortArray*" ],
            # calling fooArrayPtr() is part of the T_ARRAY typemap,
            # not part of the general mechanism
            [ 0, 1, qr/shortArrayPtr\(/,             "no shortArrayPtr call" ],
            [ 0, 0, qr/\{\s*abc\[ix_abc.*\]\s*=\s*.*\QSvIV(ST(ix_abc))\E\s*\n?\s*\}/,
                                                    "abc[i] set" ],
            [ 0, 0, qr/\QIN(abc,shortArray *,shortArrayPtr,short,ST(0),0)/,
                                                    "template vars ok" ],
            [ 0, 1, qr/DO_ARRAY_ELEM/,              "no DO_ARRAY_ELEM" ],
        ],
        [
            "T_DAE output",
            [ Q(<<'EOF') ],
                |shortArray *
                |foo()
EOF
            [ 0, 0, qr/shortArray\s*\*\s*RETVAL;/,  "RETVAL is shortArray*" ],
            [ 0, 1, qr/shortArrayPtr\(/,            "shortArrayPtr NOT called" ],
            [ 0, 0, qr/\Qsv_setiv(ST(ix_RETVAL), (IV)RETVAL[ix_RETVAL]);/,
                                                    "ST(i) set" ],
            [ 0, 0, qr/\QOUT(RETVAL,shortArray *,shortArrayPtr,short,ST(0))/,
                                                    "template vars ok" ],
            [ 0, 1, qr/DO_ARRAY_ELEM/,              "no DO_ARRAY_ELEM" ],
        ],

        # Use overridden return code with an OUTPUT line.
        [
            "T_ARRAY override output",
            [ Q(<<'EOF') ],
                |intArray *
                |foo()
                |    OUTPUT:
                |      RETVAL my_intptr_set(ST(0), RETVAL[0]);
EOF
            [ 0, 0, qr/intArray\s*\*\s*RETVAL;/,   "RETVAL is intArray*" ],
            [ 0, 1, qr/intArrayPtr/,               "intArrayPtr NOT called" ],
            [ 0, 0, qr/\Qmy_intptr_set(ST(0), RETVAL[0]);/, "ST(0) set" ],
            [ 0, 1, qr/DO_ARRAY_ELEM/,              "no DO_ARRAY_ELEM" ],
        ],

        # for OUT and OUTLIST arguments, don't process DO_ARRAY_ELEM
        [
            "T_ARRAY OUT",
            [ Q(<<'EOF') ],
                |int
                |foo(OUT intArray * abc)
EOF
            [ 1, 0, qr/Can't use typemap containing DO_ARRAY_ELEM for OUT parameter/,
                    "gives err" ],
        ],
        [
            "T_ARRAY OUT",
            [ Q(<<'EOF') ],
                |int
                |foo(OUTLIST intArray * abc)
EOF
            [ 1, 0, qr/Can't use typemap containing DO_ARRAY_ELEM for OUTLIST parameter/,
                    "gives err" ],
        ],
    );

    test_many($preamble, 'XS_Foo_', \@test_fns);
}

done_testing;
