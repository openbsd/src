#!/usr/bin/perl
use ExtUtils::MakeMaker;
use Getopt::Std;
use Config;
$ARCHNAME = $Config{archname};
use File::Basename;

getopts('e'); # embedding?

#require "JNIConfig";

if ($^O eq 'solaris') {
    $LIBPATH = " -R$Config{archlib}/CORE -L$Config{archlib}/CORE";
} elsif ($^O eq 'MSWin32') {
    $LIBPATH = " -L$Config{archlib}\\CORE";
} else {
    $LIBPATH = " -L$Config{archlib}/CORE";
}
#$LIBS = " -lperl";

# Figure out where Java might live
#
my @JAVA_HOME_GUESSES = qw(/usr/local/java /usr/java /usr/local/jdk117_v3
                           /usr/local/lib/kaffe C:\\JDK1.1.8
                           C:\\JDK1.2.1 );
if (! defined $ENV{JAVA_HOME}) {
        print "You didn't define JAVA_HOME, so I'm trying a few guesses.\n";
        print "If this fails, you might want to try setting JAVA_HOME and\n";
        print "running me again.\n";
} else {
        @JAVA_HOME_GUESSES = ( $ENV{JAVA_HOME} );
}

# Let's find out where jni.h lives
#
my @INCLUDE = find_includes();
$INC = join(" -I", ("", @INCLUDE));

# Let's find out the name of the Java shared library
#
my @JAVALIBS = find_libs();

# Find out some defines based on the library we are linking to
#
foreach (@JAVALIBS) {
    if ( /javai.lib$/ or /jvm.lib$/) { # We're on Win32
        $INC =~ s#/#\\#g;
        $INC =~ s#\\$##;
        $CCFLAGS .= "-DWIN32 -Z7 -D_DEBUG";
        $MYEXTLIB = $libjava;
    } elsif (/libkaffevm.so$/) {
        $CCFLAGS .= "-DKAFFE";
    }
}

# Let's find out the path of the library we need to link against.
#
foreach (@JAVALIBS) {
    if ( /javai.lib$/ or /jvm.lib$/) { # We're on Win32
        $_ =~ s#/#\\\\#g;
    }
    my ($libname, $libpath, $libsuffix) = fileparse($_, ("\.so", "\.lib"));
    $libname =~ s/^lib//;
    if ($^O eq 'solaris') {
        $LIBPATH .= " -R$libpath -L$libpath"
    } else {
        $LIBPATH .= " -L$libpath"
    }
    $LIBS .= " -l$libname";
}

# Do we need -D_REENTRANT?
if ($LIBPATH =~ /native/) {
    print "Looks like native threads...\n";
    $CCFLAGS .= " -D_REENTRANT";
}

if ($opt_e) {
    print "We're embedding Perl in Java via libPerlInterpreter.so.\n";
    eval `../setvars -perl`;
    $CCFLAGS .= " -DEMBEDDEDPERL";
    $LIBPATH .= " -R$ENV{JPL_HOME}/lib/$ARCHNAME -L$ENV{JPL_HOME}/lib/$ARCHNAME";
    $LIBS    .= " -lPerlInterpreter";
}

# Needed for JNI.
if ($^O eq 'solaris') {
    $LIBS = " -lthread -lc $LIBS"; #-lthread must be first!!!
    $CCFLAGS .= " -D_REENTRANT";
}

my %Makefile = (
    NAME        => 'JNI',
    VERSION_FROM => 'JNI.pm',
    DEFINE        => '',
    LINKTYPE => 'dynamic',
    INC        => $INC,
    CCFLAGS => "$Config{ccflags} $CCFLAGS", 
    ($Config{archname} =~ /mswin32.*-object/i ? ('CAPI' => 'TRUE') : ()),
);

$Makefile{LIBS} = ["$LIBPATH $LIBS"];
if ($MYEXTLIB) {
    $Makefile{MYEXTLIB} = $MYEXTLIB;
}

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
#
WriteMakefile(%Makefile);

# subroutine to find a library
#
sub find_stuff {

    my ($candidates, $locations) = @_;

    my $lib;
    $wanted = sub {
        foreach my $name (@$candidates) {
            if (/$name$/ and ! /green_threads/ and !/include-old/) {
                    $lib = $File::Find::name;
            }
        }
    };
    
    use File::Find;
    foreach my $guess (@$locations) {
        next unless -d $guess;
        find (\&$wanted, $guess);
    }
    if (! $lib) {
        print "Could not find @$candidates\n";
    } else {
        print "Found @$candidates as $lib\n\n";
    }
    return $lib;
}

# Extra lib for Java 1.2
#
sub find_libs {

    my $libjava = find_stuff(['libjava.so', 'libkaffevm.so', 'javai.lib', 'jvm.lib'],
                        \@JAVA_HOME_GUESSES);
    my $libjvm  = find_stuff(['libjvm.so'],  \@JAVA_HOME_GUESSES);
    if ($libjvm) { # JDK 1.2
        my $libhpi  = find_stuff(['libhpi.so'], \@JAVA_HOME_GUESSES);
        my $libawt  = find_stuff(['libawt.so'], \@JAVA_HOME_GUESSES);
        return($libjava, $libjvm, $libhpi, $libawt);
    } else {
        return($libjava);
    }

}

# We need to find jni.h and jni_md.h
#
sub find_includes {

    my @CANDIDATES = qw(jni.h jni_md.h);
    my @includes;

    sub find_inc {
        foreach my $name (@CANDIDATES) {
            if (/$name$/) {
                my ($hname, $hpath, $hsuffix) = 
                    fileparse($File::Find::name, ("\.h", "\.H"));
                unless ($hpath =~ /include-old/) {
                    print "Found $hname$hsuffix in $hpath\n";
                    push @includes, $hpath;
                }
            }
        }
    }
    
    use File::Find;
    foreach my $guess (@JAVA_HOME_GUESSES) {
        next unless -d $guess;
        find (\&find_inc, $guess);
    }
    if (! @includes) {
        die "Could not find Java includes!";
    } else {
        print join("\n", @includes), "\n";
    }
    return @includes;
}

