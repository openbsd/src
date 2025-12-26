#!perl -w
BEGIN {
    chdir "t" if -d "t";
    require './test.pl';
    @INC = "../lib";
}

use v5.38;
use Config;
use Cwd "getcwd";
use File::Temp;
use File::Spec;
use Text::ParseWords qw(shellwords);

my $cwd = getcwd;
my $devnull = File::Spec->devnull;
my %sources = load_sources();

# we chdir around a bit below, which breaks relative paths and Carp
@INC = map File::Spec->rel2abs($_), @INC;

# the intent is the compiler detection done here will move into a module,
# EU::CB doesn't provide what I need here, EU::CppGuess does have some of
# it but isn't core, and has its own limitations

my $cc = $Config{cc};
$cc = shift if @ARGV;

my $ccflags = $Config{ccflags};

# we add a similar C++ -std
$ccflags  =~ s/-std[:=]\S+//;

my ($ccpp_cfg, $diag) = find_ccpp($cc);

note @$diag;

$ccpp_cfg
  or skip_all("Cannot find a C++ compiler corresponding to $cc");

my $perl_headers = <<'HEADERS';
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

HEADERS

$ccflags .= " " . join " ", map { "-I$_" }
  File::Spec->catdir($cwd, ".."),
  # win32 has special config.h handling during the build
  File::Spec->catdir($cwd, "..", "lib", "CORE");

for my $std ("base", sort keys $ccpp_cfg->{stdargs}->%*) {
    my $code = get_source("cpp$std");
    $code =~ s(^//PERLHEADERS$)($perl_headers)m
      or die "Couldn't insert headers in cpp$std";

    my %std_opt = $std eq "base" ? () : ( std => $std );
    ok_compile_only({ code => \$code, %std_opt, opts => $ccflags }, $ccpp_cfg, "test std $std")
      or diag "Code: $code";
}

done_testing();

sub shellquote (@words) {
    state $esc = $^O eq "MSWin32" ? qr/["]/ : qr/["\\]/;
    state $need_esc = $^O eq "MSWin32" ? qr/[ "]/ : qr/[ "\\]/;

    for my $word (@words) {
        if ($word =~ $need_esc) {
            $word =~ s/($esc)/\\$1/g;
            $word = qq("$word");
        }
    }

    return "@words";
}

sub find_ccpp ($cc) {
    my $ccpp;
    my $cfg;
    my $exe = $Config{_exe};
    my @pre = shellwords($cc);
    my @post;
    my @diag;

    # $Config{cc} is meant to be the name of the C compiler, but some people
    # supply switches too (which belong in ccflags, ldflags and/or lddlflags)
    #
    # Strip anything at the end starting with "-", this don't catch all possible
    # such options (an option may have a separate value without "-") but
    # once this goes into production we'll just (mostly) harmlessly skip
    # such configurations.
    #
    # cc may also include a wrapper like ccache or env, which we leave in @pre
    # here.
    while (@pre && $pre[-1] =~ /^-/) {
        unshift @post, pop @pre;
    }

    my $ccarg = pop @pre;
    unless ($ccarg) {
        push @diag, qq(Nothing left after stripping arguments from "$cc"\n);
        return (undef, \@diag);
    }

    # gcc
    if (($ccpp = $ccarg) =~ s/\bgcc((?:-\d+)?(?:\Q$exe\E)?)$/g++$1/aa
        && ($cfg = check_cpp_compiler(shellquote(@pre, $ccpp, @post), "gcc,unix", \@diag))) {
        return ( $cfg, \@diag );
    }
    # clang
    elsif (($ccpp = $ccarg) =~ s/\bclang((?:-\d+)?(?:\Q$exe\E)?)$/clang++$1/aa
           && ($cfg = check_cpp_compiler(shellquote(@pre, $ccpp, @post), "clang,unix", \@diag))) {
        return ( $cfg, \@diag );
    }
    # msvc
    # may need work if we ever support clang-cl
    elsif ($ccarg =~ m!([\\/]|^)cl(?:\Q$exe\E)?$!i
           && ($cfg = check_cpp_compiler(shellquote(@pre, $ccarg, @post), "msvc", \@diag))) {
        return ( $cfg, \@diag );
    }
    else {
        # intel C, Sun C
        # Sun C sends -V output to stderr
        my $ver = `$cc -V 2>&1`;
        if (!$ver || ($? && $ver =~ /\berror\b/)) {
            # gcc, clang
            $ver = `$cc --version 2>$devnull`;
        }

        if ($ver =~ /Intel(?:\(R\))? (?:.*)C.* Compiler/) {
            if (($ccpp = $ccarg) =~ s/\bicc((?:\Q$exe\E)?)$/icpc$1/iaa
                && ($cfg = check_cpp_compiler(shellquote(@pre, $ccpp, @post), "intel,unix", \@diag))) {
                return ( $cfg, \@diag );
            }
            # icx (Intel oneAPI DPC++/C++ compiler)
            elsif (($ccpp = $ccarg) =~ s/\bicx((?:\Q$exe\E)?)$/icpx$1/iaa
                   && ($cfg = check_cpp_compiler(shellquote(@pre, $ccpp, @post), "intel,unix", \@diag))) {
                return ( $cfg, \@diag );
            }
        }
        elsif ($ver =~ / Sun .*C/) {
            if (($ccpp = $ccarg) =~ s/\bcc$/CC/aa
                && ($cfg = check_cpp_compiler(shellquote(@pre, $ccpp, @post), "sunw,unix", \@diag))) {
                return ( $cfg, \@diag );
            }
        }
        # common naming, at least on Linux
        if (($ccpp = $ccarg) =~ s/\b(cc|c89|c99)$/c++/aa
           || ($ccpp = $ccarg) =~ /\+\+/) { # already a C++ compiler?
            my $type = "unix"; # something unix-like
            if ($ver =~ /Copyright .* Free Software Foundation/) {
                $type = "gcc,unix";
            }
            elsif ($ver =~ /clang version/) {
                $type = "clang,unix";
            }
            if ($cfg = check_cpp_compiler(shellquote(@pre, $ccpp, @post), $type, \@diag)) {
                return ( $cfg, \@diag );
            }
        }
    }
    return (undef, \@diag );
}

# does a simple check that the supplied compiler can compile C++
sub check_cpp_compiler ($ccpp, $type, $diag) {
    my $ccpp_test_code = get_source("cppbase");
    my $cfg =
      +{
          type => $type,
          ccpp => $ccpp,
      };

    # the test is done with ccflags since I had some strange results without it,
    # for now at least this tests the headers, not whether ccflags is sane
    # For example, the cpp11 code compiled without ccflags, but with ccflags,
    # without the perl headers produced from icc (Intel Classic):
    # /usr/include/c++/12/bits/utility.h(154): error: pack expansion does not make use of any argument packs
    #         using __type = _Index_tuple<__integer_pack(_Num)...>;
    # /usr/include/c++/12/cstdio(107): error: the global scope has no "fgetpos"
    #     using ::fgetpos;
    # /usr/include/c++/12/cstdio(109): error: the global scope has no "fopen"
    #     using ::fopen;

    push @$diag, "test run for $ccpp";
    my $out = test_run({ ccpp => $ccpp, code => \$ccpp_test_code, opts => $ccflags }, $cfg);
    unless ($out && $out->{run_stdout} && $out->{run_stdout} eq "OK\n") {
        push_run_diag($diag, $out);
        return;
    }

    # see if we can select different C++ standards
    # be aware that the default standard varies by compiler and
    # version of that compiler
    my %std_args;
    if ($type eq "msvc") {
        # https://learn.microsoft.com/en-us/cpp/build/reference/std-specify-language-standard-version?view=msvc-170
        %std_args = map {; $_ => "-std:c++$_" } qw(14 17 20);
    }
    elsif ($type =~ /\bsunw\b/) {
        # https://docs.oracle.com/cd/E77782_01/html/E77789/bkana.html#OSSCPgnaof
        %std_args = map {; $_ => "-std=c++$_" } qw(11 14);
    }
    elsif ($type =~ /\bunix\b/) {
        # Intel
        # https://www.intel.com/content/www/us/en/docs/dpcpp-cpp-compiler/developer-guide-reference/2024-1/std-qstd.html
        # gcc allows 23 but claims
        # "Support is highly experimental, and will almost certainly change in incompatible ways in future releases."
        # https://gcc.gnu.org/onlinedocs/gcc/C-Dialect-Options.html
        # clang don't document which values are permitted
        # https://clang.llvm.org/docs/ClangCommandLineReference.html
        %std_args = map {; $_ => "-std=c++$_" } qw(11 14 17 20 23);
    }
    else {
        die "Unknown compiler type $type\n";
    }

    my %stds;
    for my $std (sort keys %std_args) {
        my $arg = $std_args{$std};
        push @$diag, "probe $ccpp for standard C++$std with $arg";
        my $code = get_source("cpp$std");
        my $out = test_run({ ccpp => $ccpp, code => \$code, opts => "$ccflags $arg" }, $cfg);
        if ($out && $out->{run_stdout} && $out->{run_stdout} eq "OK\n") {
            push @$diag, "found $std with $arg";
            $stds{$std} = $arg;
        }
        else {
            push @$diag, "didn't find $std with $arg";
            push_run_diag($diag, $out);
        }
    }
    $cfg->{stdargs} = \%stds;

    return $cfg;
}

sub push_run_diag ($diag, $out) {
    push @$diag, <<DIAG;
build: $out->{build_cmd}
build output: $out->{build_out}
build exit: $out->{build_exit}
DIAG
    push @$diag, "run cmd: $out->{run_cmd}" if $out->{run_cmd};
    push @$diag, "run stdout: $out->{run_stdout}" if $out->{run_stdout};
    push @$diag, "run stderr: $out->{run_stderr}" if $out->{run_stderr};
    push @$diag, "run exit: $out->{run_exit}" if defined $out->{run_exit};
}

sub ok_compile_only($job, $conf, $name) {
    our $Level;
    local $Level = $Level + 1;
    my $result = _test_compile_only($job, $conf);

    if (ok($result->{ok}, $name)) {
        note "cmd: $result->{cmd}";
        note "out: $result->{out}";
    }
    else {
        diag "cmd: $result->{cmd}";
        diag "out: $result->{out}";
    }
    $result->{ok};
}

sub _test_compile_only ($job, $conf) {
    my $dir = File::Temp->newdir();
    chdir "$dir"
      or die "Cannot chdir to temp directory '$dir': $!";
    my $code = $job->{code};
    if (ref $code) {
        open my $cfh, ">", "source.cpp"
          or die "Cannot create source.cpp: $!";
        print $cfh $$code;
        close $cfh
          or die "Cannot close source.cpp: $!";
        $code = "source.cpp";
    }
    my $opts = $job->{opts} || '';
    $opts = "-c $opts";
    if (my $std = $job->{std}) {
        my $std_opt = $conf->{stdargs}{$std}
          or die "Unknown standard $std for $conf->{ccpp}\n";
        $opts .= " $std_opt";
    }

    my $cmd = "$conf->{ccpp} $opts $code 2>&1";
    my $out = `$cmd`;

    chdir $cwd;

    unless ($? == 0) {
        return
          +{
              cmd => $cmd,
              out => $out,
          };
    }

    return
      +{
          ok => 1,
          cmd => $cmd,
          out => $out,
      };
}

# perform a test run to see if a compiler works
# $conf can be empty to unix-like defaults, see test_build() for more
sub test_run ($job, $conf) {
    my $dir = File::Temp->newdir();
    chdir "$dir"
      or die "Cannot chdir to temp directory '$dir': $!";
    my $result = _test_build($job, $conf);
    if ($result->{exe}) {
        my $cmd = "$result->{exe} >stdout.txt 2>stderr.txt";
        my $exit = system $cmd;
        $result->{run_exit} = $exit;
        $result->{run_cmd} = $cmd;
        $result->{run_stdout} = scalar _slurp("stdout.txt");
        $result->{run_stderr} = scalar _slurp("stderr.txt");
    }
    chdir $cwd
      or die "Cannot chdir back to '$cwd': $!";

    $result;
}

# build the supplied code to test we can invoke the compiler
# and so the caller can run it
sub _test_build ($job, $conf) {
    $conf ||= { type => "unix" };

    my $code = $job->{code};
    if (ref $code) {
        open my $cfh, ">", "source.cpp"
          or die "Cannot create source.cpp: $!";
        print $cfh $$code;
        close $cfh
          or die "Cannot close source.cpp: $!";
        $code = "source.cpp";
    }
    my $opts = $job->{opts} || '';
    my $_exe = $Config{_exe};
    if ($conf->{type} =~ /\bunix\b/) {
        $opts = "-oa.out$_exe $opts";
    }
    elsif ($conf->{type} eq "msvc") {
        $opts = "/Fea.out$_exe $opts";
    }
    else {
        die "Unknown type $conf->{type}";
    }

    my $cmd = "$job->{ccpp} $opts $code 2>&1";
    my $result =
      +{
        build_cmd => "$cmd\n",
       };
    my $out = `$cmd` // "";
    $result->{build_out} = $out;
    $result->{build_exit} = $?;
    unless ($? == 0) {
        return $result;
    }

    my $exe = "a.out$_exe";
    unless ($^O eq "MSWin32") {
        $exe = "./$exe";
    }
    $result->{exe} = $exe;

    return $result;
}

sub _slurp ($filename) {
    open my $fh, "<", $filename
      or die "Cannot open $filename: $!";
    return do { local $/; <$fh> };
}

sub load_sources {
    my %code;

    my $name = '';
    local $_;
    while (<DATA>) {
        if (/^-- (\w+)$/a) {
            $name = $1;
        }
        elsif ($name) {
            $code{$name} .= $_;
        }
        else {
            die "No name seen for code line $_";
        }
    }

    return %code;
}

sub get_source ($keyword) {
    $sources{$keyword}
      or die "No source found for keyword $keyword\n";
    $sources{$keyword};
}

# the test code below tries to use at least one language feature
# specific to that version.
#
# For now we don't try to do anything real with perl here, but that may change.
#
# The perl headers need to be after the C++ headers since the perl headers
# define many macros that could conflict with the public and non-public
# like "std::__impl::somenamehere" names that the C++ headers use or define
__DATA__
-- cppbase
#include <iostream>

//PERLHEADERS

int main() {
  std::cout << "OK" << std::endl;
  return 0;
}
-- cpp11
#include <iostream>
#include <memory>

//PERLHEADERS

struct A {
    virtual const char *ok() { return "NOT OK\n"; };
    // = default C++11
    virtual ~A() = default;
};

struct B : A {
    // override C++11
    const char *ok() override { return "OK\n"; };
};

// unique ptr is C++11
std::unique_ptr<A> f() {
    return std::unique_ptr<A>{new B};
}

int main() {
  // auto as a placeholder type is C++11
  auto p = f();
  std::cout << p->ok();
  return 0;
}

-- cpp14
#include <iostream>
#include <memory>

//PERLHEADERS

struct A {
    virtual const char *ok() { return "NOT OK\n"; };
    // = default C++11
    virtual ~A() = default;
};

struct B : A {
    // override C++11
    const char *ok() override { return "OK\n"; };
};

// auto return type is C++14
auto f() {
    return std::unique_ptr<A>{new B{}};
}

// deprecated C++14
[[deprecated]] void g();

int main() {
  auto p = f();
  // binary literals and ' in numeric literals are C++14
  if (0b100'0000 == 64)
    std::cout << p->ok();
  return 0;
}

-- cpp17
#include <iostream>
#include <memory>
#include <string_view>

//PERLHEADERS

// for access to sv literals
using namespace std::literals;

struct A {
    // string_view c++17
    virtual std::string_view ok() { return "NOT OK\n"sv; };
    virtual ~A() = default;
};

struct B : A {
    std::string_view ok() override { return "OK\n"sv; };
};

// [[nodiscard]] is C++17
[[nodiscard]] auto f() {
    return std::unique_ptr<A>{new B{}};
}

int main() {
  auto p = f();
  // if constexpr C++17
  if constexpr (0b100'0000 == 64)
    std::cout << p->ok();
  return 0;
}
-- cpp20
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

//PERLHEADERS

// for access to sv literals
using namespace std::literals;

enum class isok {
  yes, no
};

auto f(isok x) {
  // using scoped enum c++20
  using enum isok;

  switch (x) {
  case yes:
    return "OK\n"sv;
  case no:
    return "NOT OK\n"sv;

  default:
    return "BAD\n"sv;
  }
}

int main() {
  std::cout << f(isok::yes);
  return 0;
}
-- cpp23
#include <string_view>
#include <print>

//PERLHEADERS

// for access to sv literals
using namespace std::literals;

struct A {
  // static operator () c++23
  static auto operator()() {
    return "OK"sv;
  }
};

int main() {
  // std::println() c++23
  // requires clang trunk or gcc trunk at time of writing
  std::println("{}", A{}());
}
