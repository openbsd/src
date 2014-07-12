BEGIN {
	print("/* THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT. */");
	print("static const struct pci_matchid i915_devices[] = {");
}
/INTEL_VGA_DEVICE\(0x/ {
	val = substr($0, 19, 6);
	print "\t{ 0x8086,", val " },";
}
END {
	print("};");
}
