open (MINIMAIN, "<../miniperlmain.c") || die "failed to open miniperlmain.c" . $!;

while (<MINIMAIN>) {
	if (/Do not delete this line--writemain depends on it/) {
		last;
		}
	else {
		print $_;
		}
	};

close(MINIMAIN);

print "char *staticlinkmodules[]={\n";
foreach (@ARGV) {
	print "\t\"".$_."\",\n";
	}
print "\tNULL,\n";
print "\t};\n";
print "\n";
foreach (@ARGV) {
	print "EXTERN_C void boot_$_ _((CV* cv));\n"
	}

print <<EOP;

static void
xs_init()
{
	dXSUB_SYS;
	char *file = __FILE__;
EOP

foreach (@ARGV) {
	if (/DynaLoader/) {
	    print "\tnewXS(\"$_\:\:boot_$_\", boot_$_, file);\n";
		}
	else {
	    print "\tnewXS(\"$_\:\:bootstrap\", boot_$_, file);\n";
		};
	}

print <<EOP;
}
EOP
