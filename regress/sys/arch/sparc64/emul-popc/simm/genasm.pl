
print "/* AUTOMATICALLY GENERATED, DO NOT EDIT */\n";
print "#include <machine/asm.h>\n";
print "#include <machine/trap.h>\n";
print "#include <machine/frame.h>\n";
print "\n";

for ($i = -4096; $i <= 4095; $i++) {
	print "ENTRY(popc";
	if ($i < 0) {
		$v =  -$i;
		print "__$v";
	} else {
		print "_$i";
	}
	print ")\n";

	print "\tretl\n";
	print "\t popc $i, %o0\n\n";
}
