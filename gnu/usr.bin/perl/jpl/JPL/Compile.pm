#!/usr/bin/perl -w

# Copyright 1997, O'Reilly & Associate, Inc.
#
# This package may be copied under the same terms as Perl itself.

package JPL::Compile;
use Exporter ();
@ISA = qw(Exporter);
@EXPORT = qw(files file);

use strict;


warn "You don't have a recent JDK kit your PATH, so this may fail.\n"
    unless $ENV{PATH} =~ /(java|jdk1.[1-9])/;

sub emit;

my $PERL = "";
my $LASTCLASS = "";
my $PERLLINE = 0;
my $PROTO;

my @protos;

my $plfile;
my $jpfile;
my $hfile;
my $h_file;
my $cfile;
my $jfile;
my $classfile;

my $DEBUG = $ENV{JPLDEBUG};

my %ptype = qw(
	Z boolean
	B byte
	C char
	S short
	I int
	J long
	F float
	D double
);

$ENV{CLASSPATH} =~ s/^/.:/ unless $ENV{CLASSPATH} =~ /^\.(?::|$)/;

unless (caller) {
    files(@ARGV);
}

#######################################################################

sub files {
    foreach my $jpfile (@_) {
	file($jpfile);
    }
    print "make\n";
    system "make";
}

sub file {
    my $jpfile = shift;
    my $JAVA = "";
    my $lastpos = 0;
    my $linenum = 2;
    my %classseen;
    my %fieldsig;
    my %staticfield;

    (my $file = $jpfile) =~ s/\.jpl$//;
    $jpfile = "$file.jpl";
    $jfile = "$file.java";
    $hfile = "$file.h";
    $cfile = "$file.c";
    $plfile = "$file.pl";
    $classfile = "$file.class";

    ($h_file = $hfile) =~ s/_/_0005f/g;

    emit_c_header();

    # Extract out arg names from .java file, since .class doesn't have 'em.

    open(JPFILE, $jpfile) or die "Can't open $jpfile: $!\n";
    undef $/;
    $_ = <JPFILE>;
    close JPFILE;

    die "$jpfile doesn't seem to define class $file!\n"
	unless /class\s+\b$file\b[\w\s.,]*{/;

    @protos = ();
    open(JFILE, ">$jfile") or die "Can't create $jfile: $!\n";

    while (m/\bperl\b([^\n]*?\b(\w+)\s*\(\s*(.*?)\s*\)[\s\w.,]*)\{\{(.*?)\}\}/sg) {
	$JAVA = substr($`, $lastpos);
	$lastpos = pos $_;
	$JAVA .= "native";
	$JAVA .= $1;

	my $method = $2;

	my $proto = $3;

	my $perl = $4;
	(my $repl = $4) =~ tr/\n//cd;
	$JAVA .= ';';
	$linenum += $JAVA =~ tr/\n/\n/;
	$JAVA .= $repl;
	print JFILE $JAVA;

	$proto =~ s/\s+/ /g;
	$perl =~ s/^[ \t]+\Z//m;
	$perl =~ s/^[ \t]*\n//;
	push(@protos, [$method, $proto, $perl, $linenum]);

	$linenum += $repl =~ tr/\n/\n/;
    }

    print JFILE <<"END";
    static {
	System.loadLibrary("$file");
        PerlInterpreter pi = new PerlInterpreter().fetch();
        // pi.eval("\$JPL::DEBUG = \$ENV{JPLDEBUG};");
	pi.eval("warn qq{loading $file\\n} if \$JPL::DEBUG");
	pi.eval("eval {require '$plfile'}; print \$@ if \$@;");
    }
END

    print JFILE substr($_, $lastpos);

    close JFILE;

    # Produce the corresponding .h file.  Should really use make...

    if (not -s $hfile or -M $hfile > -M $jfile) {
	if (not -s $classfile or -M $classfile > -M $jfile) {
	    unlink $classfile;
	    print  "javac $jfile\n";
	    system "javac $jfile" and die "Couldn't run javac: exit $?\n";
	    if (not -s $classfile or -M $classfile > -M $jfile) {
		die "Couldn't produce $classfile from $jfile!";
	    }
	}
	unlink $hfile;
	print  "javah -jni $file\n";
	system "javah -jni $file" and die "Couldn't run javah: exit $?\n";
	if (not -s $hfile and -s $h_file) {
	    rename $h_file, $hfile;
	}
	if (not -s $hfile or -M $hfile > -M $jfile) {
	    die "Couldn't produce $hfile from $classfile!";
	}
    }

    # Easiest place to get fields is from javap.

    print  "javap -s $file\n";
    open(JP, "javap -s $file|");
    $/ = "\n";
    while (<JP>) {
	if (/^\s+([A-Za-z_].*) (\w+)[\[\d\]]*;/) {
	    my $jtype = $1;
	    my $name = $2;
	    $_ = <JP>;
	    s!^\s*/\*\s*!!;
	    s!\s*\*/\s*!!;
	    print "Field $jtype $name $_\n" if $DEBUG;
	    $fieldsig{$name} = $_;
	    $staticfield{$name} = $jtype =~ /\bstatic\b/;
	}
	while (m/L([^;]*);/g) {
	    my $pclass = j2p_class($1);
	    $classseen{$pclass}++;
	}
    }
    close JP;

    open(HFILE, $hfile) or die "Couldn't open $hfile: $!\n";
    undef $/;
    $_ = <HFILE>;
    close HFILE;

    die "panic: native method mismatch" if @protos != s/^JNIEXPORT/JNIEXPORT/gm;

    $PROTO = 0;
    while (m{
	\*\s*Class:\s*(\w+)\s*
	\*\s*Method:\s*(\w+)\s*
	\*\s*Signature:\s*(\S+)\s*\*/\s*
	JNIEXPORT\s*(.*?)\s*JNICALL\s*(\w+)\s*\((.*?)\)
    }gx) {
	my $class = $1;
	my $method = $2;
	my $signature = $3;
	my $rettype = $4;
	my $cname = $5;
	my $ctypes = $6;
	$class =~ s/_0005f/_/g;
	if ($method ne $protos[$PROTO][0]) {
	    die "Method name mismatch: $method vs $protos[$PROTO][0]\n";
	}
	print "$class.$method($protos[$PROTO][1]) =>
	$signature
	$rettype $cname($ctypes)\n" if $DEBUG;

	# Insert argument names into parameter list.

	my $env = "env";
	my $obj = "obj";
	my @jargs = split(/\s*,\s*/, $protos[$PROTO][1]);
	foreach my $arg (@jargs) {
	    $arg =~ s/^.*\b(\w+).*$/${1}/;
	}
	my @tmpargs = @jargs;
	unshift(@tmpargs, $env, $obj);
	print "\t@tmpargs\n" if $DEBUG;
	$ctypes .= ",";
	$ctypes =~ s/,/' ' . shift(@tmpargs) . '_,'/eg;
	$ctypes =~ s/,$//;
	$ctypes =~ s/env_/env/;
	$ctypes =~ s/obj_/obj/;
	print "\t$ctypes\n" if $DEBUG;

	my $jlen = @jargs + 1;

	(my $mangclass = $class) =~ s/_/_1/g;
	(my $mangmethod = $method) =~ s/_/_1/g;
	my $plname = $cname;
	$plname =~ s/^Java_${mangclass}_${mangmethod}/JPL::${class}::${method}/;
	$plname =~ s/Ljava_lang_String_2/s/g;

	# Make glue code for each argument.

	(my $sig = $signature) =~ s/^\(//;

	my $decls = "";
	my $glue = "";

	foreach my $jarg (@jargs) {
	    if ($sig =~ s/^[ZBCSI]//) {
		$glue .= <<"";
!    /* $jarg */
!    PUSHs(sv_2mortal(newSViv(${jarg}_)));
!

	    }
	    elsif ($sig =~ s/^[JFD]//) {
		$glue .= <<"";
!    /* $jarg */
!    PUSHs(sv_2mortal(newSVnv(${jarg}_)));
!

	    }
	    elsif ($sig =~ s#^Ljava/lang/String;##) {
		$glue .= <<"";
!    /* $jarg */
!    tmpjb = (jbyte*)(*env)->GetStringUTFChars(env,${jarg}_,0);
!    PUSHs(sv_2mortal(newSVpv((char*)tmpjb,0)));
!    (*env)->ReleaseStringUTFChars(env,${jarg}_,tmpjb);
!

	    }
	    elsif ($sig =~ s/^L([^;]*);//) {
		my $pclass = j2p_class($1);
		$classseen{$pclass}++;
		$glue .= <<"";
!    /* $jarg */
!    if (!${jarg}_stashhv_)
!	${jarg}_stashhv_ = gv_stashpv("$pclass", TRUE);
! 
!    PUSHs(sv_bless(
!	sv_setref_iv(sv_newmortal(), Nullch, (IV)(void*)${jarg}_),
!	${jarg}_stashhv_));
!    if (jpldebug)
!	fprintf(stderr, "Done with $jarg\\n");
!

		$decls .= <<"";
!    static HV* ${jarg}_stashhv_ = 0;


	    }
	    elsif ($sig =~ s/^\[+([ZBCSIJFD]|L[^;]*;)//) {
		my $pclass = "jarray";
		$classseen{$pclass}++;
		$glue .= <<"";
!    /* $jarg */
!    if (!${jarg}_stashhv_)
!	${jarg}_stashhv_ = gv_stashpv("$pclass", TRUE);
! 
!    PUSHs(sv_bless(
!	sv_setref_iv(sv_newmortal(), Nullch, (IV)(void*)${jarg}_),
!	${jarg}_stashhv_));
!    if (jpldebug)
!	fprintf(stderr, "Done with $jarg\\n");
!

		$decls .= <<"";
!    static HV* ${jarg}_stashhv_ = 0;

	    }
	    else {
		die "Short signature: $signature\n" if $sig eq "";
		die "Unrecognized letter '" . substr($sig, 0, 1) . "' in signature $signature\n";
	    }
	}

	$sig =~ s/^\)// or die "Argument mismatch in signature: $method$signature\n";

	my $void = $signature =~ /\)V$/;

	$decls .= <<"" if $signature =~ m#java/lang/String#;
!    jbyte* tmpjb;

	$decls .= <<"" unless $void;
!    SV* retsv;
!    $rettype retval;
!
!    if (jpldebug)
!	fprintf(stderr, "Got to $cname\\n");
!    ENTER;
!    SAVETMPS;

	emit <<"";
!JNIEXPORT $rettype JNICALL
!$cname($ctypes)
!{
!    static SV* methodsv = 0;
!    static HV* stashhv = 0;
!    dSP;
$decls
!    PUSHMARK(sp);
!    EXTEND(sp,$jlen);
!
!    sv_setiv(perl_get_sv("JPL::_env_", 1), (IV)(void*)env);
!    jplcurenv = env;
!
!    if (jpldebug)
!	fprintf(stderr, "env = %lx\\n", (long)$env);
!
!    if (!methodsv)
!	methodsv = (SV*)perl_get_cv("$plname", TRUE);
!    if (!stashhv)
!	stashhv = gv_stashpv("JPL::$class", TRUE);
! 
!    if (jpldebug)
!	fprintf(stderr, "blessing obj = %lx\\n", obj);
!    PUSHs(sv_bless(
!	sv_setref_iv(sv_newmortal(), Nullch, (IV)(void*)obj),
!	stashhv));
!
$glue

	# Finally, call the subroutine.

	my $mod;
	$mod = "|G_DISCARD" if $void;

	if ($void) {
	    emit <<"";
!    PUTBACK;
!    perl_call_sv(methodsv, G_EVAL|G_KEEPERR|G_DISCARD);
!

	}
	else {
	    emit <<"";
!    PUTBACK;
!    if (perl_call_sv(methodsv, G_EVAL|G_KEEPERR))
!	retsv = *PL_stack_sp--;
!    else
!	retsv = &PL_sv_undef;
!

	}

	emit <<"";
!    if (SvTRUE(ERRSV)) {
!	jthrowable newExcCls;
!
!	(*env)->ExceptionDescribe(env);
!	(*env)->ExceptionClear(env);
!
!	newExcCls = (*env)->FindClass(env, "java/lang/RuntimeException");
!	if (newExcCls)
!	    (*env)->ThrowNew(env, newExcCls, SvPV(ERRSV,PL_na));
!    }
!

	# Fix up the return value, if any.

	if ($sig =~ s/^V//) {
	    emit <<"";
!    return;

	}
	elsif ($sig =~ s/^[ZBCSI]//) {
	    emit <<"";
!    retval = ($rettype)SvIV(retsv);
!    FREETMPS;
!    LEAVE;
!    return retval;

	}
	elsif ($sig =~ s/^[JFD]//) {
	    emit <<"";
!    retval = ($rettype)SvNV(retsv);
!    FREETMPS;
!    LEAVE;
!    return retval;

	}
	elsif ($sig =~ s#^Ljava/lang/String;##) {
	    emit <<"";
!    retval = (*env)->NewStringUTF(env, SvPV(retsv,PL_na));
!    FREETMPS;
!    LEAVE;
!    return retval;

	}
	elsif ($sig =~ s/^L[^;]*;//) {
	    emit <<"";
!    if (SvROK(retsv)) {
!	SV* rv = (SV*)SvRV(retsv);
!	if (SvOBJECT(rv))
!	    retval = ($rettype)(void*)SvIV(rv);
!	else
!	    retval = ($rettype)(void*)0;
!    }
!    else
!	retval = ($rettype)(void*)0;
!    FREETMPS;
!    LEAVE;
!    return retval;

	}
	elsif ($sig =~ s/^\[([ZBCSIJFD])//) {
	    my $elemtype = $1;
	    my $ptype = "\u$ptype{$elemtype}";
	    my $ntype = "j$ptype{$elemtype}";
	    my $in = $elemtype =~ /^[JFD]/ ? "N" : "I";
	    emit <<"";
!    if (SvROK(retsv)) {
!	SV* rv = (SV*)SvRV(retsv);
!	if (SvOBJECT(rv))
!	    retval = ($rettype)(void*)SvIV(rv);
!	else if (SvTYPE(rv) == SVt_PVAV) {
!	    jsize len = av_len((AV*)rv) + 1;
!	    $ntype* buf = ($ntype*)malloc(len * sizeof($ntype));
!	    int i;
!	    SV** esv;
!
!	    ${ntype}Array ja = (*env)->New${ptype}Array(env, len);
!	    for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
!		buf[i] = ($ntype)Sv${in}V(*esv);
!	    (*env)->Set${ptype}ArrayRegion(env, ja, 0, len, buf);
!	    free((void*)buf);
!	    retval = ($rettype)ja;
!	}
!	else
!	    retval = ($rettype)(void*)0;
!    }
!    else if (SvPOK(retsv)) {
!	jsize len = sv_len(retsv) / sizeof($ntype);
!
!	${ntype}Array ja = (*env)->New${ptype}Array(env, len);
!	(*env)->Set${ptype}ArrayRegion(env, ja, 0, len, ($ntype*)SvPV(retsv,PL_na));
!	retval = ($rettype)ja;
!    }
!    else
!	retval = ($rettype)(void*)0;
!    FREETMPS;
!    LEAVE;
!    return retval;

	}
	elsif ($sig =~ s!^\[Ljava/lang/String;!!) {
	    emit <<"";
!    if (SvROK(retsv)) {
!	SV* rv = (SV*)SvRV(retsv);
!	if (SvOBJECT(rv))
!	    retval = ($rettype)(void*)SvIV(rv);
!	else if (SvTYPE(rv) == SVt_PVAV) {
!	    jsize len = av_len((AV*)rv) + 1;
!	    int i;
!	    SV** esv;
!           static jclass jcl = 0;
!	    jarray ja;
!
!	    if (!jcl)
!		jcl = (*env)->FindClass(env, "java/lang/String");
!	    ja = (*env)->NewObjectArray(env, len, jcl, 0);
!	    for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++) {
!		jobject str = (jobject)(*env)->NewStringUTF(env, SvPV(*esv,PL_na));
!		(*env)->SetObjectArrayElement(env, ja, i, str);
!	    }
!	    retval = ($rettype)ja;
!	}
!	else
!	    retval = ($rettype)(void*)0;
!    }
!    else
!	retval = ($rettype)(void*)0;
!    FREETMPS;
!    LEAVE;
!    return retval;

	}
	elsif ($sig =~ s/^(\[+)([ZBCSIJFD]|L[^;]*;)//) {
	    my $arity = length $1;
	    my $elemtype = $2;
	    emit <<"";
!    if (SvROK(retsv)) {
!	SV* rv = (SV*)SvRV(retsv);
!	if (SvOBJECT(rv))
!	    retval = ($rettype)(void*)SvIV(rv);
!	else if (SvTYPE(rv) == SVt_PVAV) {
!	    jsize len = av_len((AV*)rv) + 1;
!	    int i;
!	    SV** esv;
!           static jclass jcl = 0;
!	    jarray ja;
!
!	    if (!jcl)
!		jcl = (*env)->FindClass(env, "java/lang/Object");
!	    ja = (*env)->NewObjectArray(env, len, jcl, 0);
!	    for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++) {
!		if (SvROK(*esv) && (rv = SvRV(*esv)) && SvOBJECT(rv)) {
!		    (*env)->SetObjectArrayElement(env, ja, i,
!			(jobject)(void*)SvIV(rv));
!		}
!		else {
!		    jobject str = (jobject)(*env)->NewStringUTF(env,
!			SvPV(*esv,PL_na));
!		    (*env)->SetObjectArrayElement(env, ja, i, str);
!		}
!	    }
!	    retval = ($rettype)ja;
!	}
!	else
!	    retval = ($rettype)(void*)0;
!    }
!    else
!	retval = ($rettype)(void*)0;
!    FREETMPS;
!    LEAVE;
!    return retval;

	}
	else {
	    die "No return type: $signature\n" if $sig eq "";
	    die "Unrecognized return type '" . substr($sig, 0, 1) . "' in signature $signature\n";
	}

	emit <<"";
!}
!

	my $perl = "";

	if ($class ne $LASTCLASS) {
	    $LASTCLASS = $class;
	    $perl .= <<"";
package JPL::${class};
use JNI;
use JPL::AutoLoader;
\@ISA = qw(jobject);
\$clazz = JNI::FindClass("$file");\n

	    foreach my $field (sort keys %fieldsig) {
		my $sig = $fieldsig{$field};
		my $ptype = $ptype{$sig};
		if ($ptype) {
		    $ptype = "\u$ptype";
		    if ($staticfield{$field}) {
			$perl .= <<"";
\$${field}_FieldID = JNI::GetStaticFieldID(\$clazz, "$field", "$sig");
sub $field (\$;\$) {
    my \$self = shift;
    if (\@_) {
	JNI::SetStatic${ptype}Field(\$clazz, \$${field}_FieldID, \$_[0]);
    }
    else {
	JNI::GetStatic${ptype}Field(\$clazz, \$${field}_FieldID);
    }
}\n

		    }
		    else {
			$perl .= <<"";
\$${field}_FieldID = JNI::GetFieldID(\$clazz, "$field", "$sig");
sub $field (\$;\$) {
    my \$self = shift;
    if (\@_) {
	JNI::Set${ptype}Field(\$self, \$${field}_FieldID, \$_[0]);
    }
    else {
	JNI::Get${ptype}Field(\$self, \$${field}_FieldID);
    }
}\n

		    }
		}
		else {
		    my $pltype = $sig;
		    if ($pltype =~ s/^L(.*);/$1/) {
			$pltype =~ s!/!::!g;
		    }
		    else {
			$pltype = 'jarray';
		    }
		    if ($pltype eq "java::lang::String") {
			if ($staticfield{$field}) {
			    $perl .= <<"";
\$${field}_FieldID = JNI::GetStaticFieldID(\$clazz, "$field", "$sig");
sub $field (\$;\$) {
    my \$self = shift;
    if (\@_) {
	JNI::SetStaticObjectField(\$clazz, \$${field}_FieldID,
	    ref \$_[0] ? \$_[0] : JNI::NewStringUTF(\$_[0]));
    }
    else {
	JNI::GetStringUTFChars(JNI::GetStaticObjectField(\$clazz, \$${field}_FieldID));
    }
}\n

			}
			else {
			    $perl .= <<"";
\$${field}_FieldID = JNI::GetFieldID(\$clazz, "$field", "$sig");
sub $field (\$;\$) {
    my \$self = shift;
    if (\@_) {
	JNI::SetObjectField(\$self, \$${field}_FieldID,
	    ref \$_[0] ? \$_[0] : JNI::NewStringUTF(\$_[0]));
    }
    else {
	JNI::GetStringUTFChars(JNI::GetObjectField(\$self, \$${field}_FieldID));
    }
}\n

			}
		    }
		    else {
			if ($staticfield{$field}) {
			    $perl .= <<"";
\$${field}_FieldID = JNI::GetStaticFieldID(\$clazz, "$field", "$sig");
sub $field (\$;\$) {
    my \$self = shift;
    if (\@_) {
	JNI::SetStaticObjectField(\$clazz, \$${field}_FieldID, \$_[0]);
    }
    else {
	bless JNI::GetStaticObjectField(\$clazz, \$${field}_FieldID), "$pltype";
    }
}\n

			}
			else {
			    $perl .= <<"";
\$${field}_FieldID = JNI::GetFieldID(\$clazz, "$field", "$sig");
sub $field (\$;\$) {
    my \$self = shift;
    if (\@_) {
	JNI::SetObjectField(\$self, \$${field}_FieldID, \$_[0]);
    }
    else {
	bless JNI::GetObjectField(\$self, \$${field}_FieldID), "$pltype";
    }
}\n

			}
		    }
		}
	    }
	}

	$plname =~ s/^JPL::${class}:://;

	my $proto = '$' x (@jargs + 1);
	$perl .= "sub $plname ($proto) {\n";
	$perl .= '    my ($self, ';
	foreach my $jarg (@jargs) {
	    $perl .= "\$$jarg, ";
	}
	$perl =~ s/, $/) = \@_;\n/;
	$perl .= <<"END";
	warn "JPL::${class}::$plname(\@_)\\n" if \$JPL::DEBUG;
#line $protos[$PROTO][3] "$jpfile"
$protos[$PROTO][2]}

END

	$PERLLINE += $perl =~ tr/\n/\n/ + 2;
	$perl .= <<"END";
#line $PERLLINE ""
END
	$PERLLINE--;

	$PERL .= $perl;
    }
    continue {
	$PROTO++;
	print "\n" if $DEBUG;
    }

    emit_c_footer();

    rename $cfile, "$cfile.old";
    rename "$cfile.new", $cfile;

    open(PLFILE, ">$plfile") or die "Can't create $plfile: $!\n";
    print PLFILE "BEGIN { \$JPL::_env_ ||= 1; }	# suppress bogus embedding\n\n";
    if (%classseen) {
	my @classes = sort keys %classseen;
	print PLFILE "use JPL::Class qw(@classes);\n\n";
    }
    print PLFILE $PERL;
    print PLFILE "1;\n";
    close PLFILE;

    print "perl -c $plfile\n";
    system "perl -c $plfile" and die "jpl stopped\n";
}

sub emit_c_header {
    open(CFILE, ">$cfile.new") or die "Can't create $cfile.new: $!\n";
    emit <<"";
!/* This file is automatically generated.  Do not modify! */
!
!#include "$hfile"
! 
!#include "EXTERN.h"
!#include "perl.h"
! 
!#ifndef EXTERN_C
!#  ifdef __cplusplus
!#    define EXTERN_C extern "C"
!#  else
!#    define EXTERN_C extern
!#  endif
!#endif
!
!extern int jpldebug;
!extern JNIEnv* jplcurenv;
!

}


sub emit_c_footer {
    close CFILE;
}

sub emit {
    my $string = shift;
    $string =~ s/^!//mg;
    print CFILE $string;
}

sub j2p_class {
    my $jclass = shift;
    $jclass =~ s#/#::#g;
    $jclass;
}
