# Changelog

All notable changes to simdzone will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.8] - 2026-10-21

### Added
- The UNECE and ISO RR types

### Fixed

- Fix the calculation of the rdata needed for rdata in RFC 3597 Unknown Type
  format.
  Thanks to Zanya <tea915656@gmail.com> for the report.
- Fix preventing size\_t length to wrap below 0 when scanning an end-of-string
  backslash character, resulting in out-of-bounds read.
  Thanks to Zanya <tea915656@gmail.com> for the report.
- Fix to determine the correct rdata length for WKS RRs without bitmap.
  Thanks to Alwaleed Ashi (LXR) <waleed.9998@hotmail.com> for the report.
- Fix to build simdzone also with NDEBUG by default, like NSD and only disable
  it when `--enable-check ing` is given to configure.
  Thanks to Alwaleed Ashi (LXR) <waleed.9998@hotmail.com> for the report.
- Fix to let configure detect ar as it needs to match the --host arch
  Thanks Haelwenn (lanodan) Monnier
- Fix asm usage in strict ISO C builds
  Thanks Jared Mauch

## [0.2.7] - 2026-10-14

### Fixed

- Fix for CVE-2026-101337: Reserve correct number of bytes for base64 sequence
  tokens, regardless of stream decoder phase.
  Thanks to Qifan Zhang, Palo Alto Networks, and Gaurang Maheta for the report.
- Fix for CVE-2026-101338: No SVCB and/or HTTPS parameters are parsed when the
  RDATA is already filled to the brim.
  Thanks to Qifan Zhang, Palo Alto Networks, for the report.
- Fix CVE-2026-101339: Fix for dname possibility to overwrite the stack when
  near the RDATA limit.
  Thanks to Qifan Zhang, Palo Alto Networks, and Alwaleed Ashi (LXR)
  <waleed.9998@hotmail.com> for the report.
- Fix for CVE-2026-101341: Also check that RDATA stays within bounds while
  parsing string tokens under 32 bytes.
  Thanks to Alwaleed Ashi (LXR) <waleed.9998@hotmail.com> for the report

## [0.2.6] - 2026-09-02

### Added

- Support for the HHIT and BRID RR types.
- Support for the "docpath", "pvd" and "oots" SVCB Service Parameters

### Fixed

- TXT records with more than 6465 rdata elements, crashes simdzone parsing.
  Thanks to Qifan Zhang, Palo Alto Networks, for the report.
- Fix unit test implicit conversion change of signedness warning.
- Fix unit test delimiters_overflow_txt to add block size padding for simd
  operations.

## [0.2.5] - 2026-07-07

### Added

- Add riscv fallback detection (#261)

### Fixed

- Fix to parse SVCB and HTTPS svcparam ech=0.

## [0.2.4] - 2025-12-04

### Fixed

- Correct lengths for GOST R 34.10-2012 and SM3 delegation signer (DS) digest
  algorithms
- Require the AMTRELAY relay field to be `.` for the no gateway relay type as
  specified by RFC 8777 (#257)

## [0.2.3] - 2025-09-03

### Added

- check_pie: match nsd support (#253).

### Fixed

- Fix tests to initialize padding (#252).
- Fix for #253, add acx_nlnetlabs.m4 in the repo and allow CFLAGS passed to
  configure to set the flags.

## [0.2.2] - 2025-04-24

### Added

- Support for EID, NIMLOC, SINK, TALINK, DSYNC, DOA, AMTRELAY and IPN RR types.

### Fixed

- Empty base16 and base64 in CDS and CDNSKEY can be represented with a '0'.
  As specified in Section 4 of RFC 8078.
- Initialise padding after the file buffer (#249).
- Fix type NSAP-PTR (#250).
- Fix LOC poweroften lookup (#251).

## [0.2.1] - 2025-01-17

### Fixed

- Cleanup westmere and haswell object files (#244) Thanks @fobser
- Out of tree builds (NLnetLabs/nsd#415)
- Fix function declarations for fallback detection routine in isadetection.h.

## [0.2.0] - 2024-12-12

### Added

- Add semantic checks for DS and ZONEMD digests (NLnetLabs/nsd#205).
- Support registering a callback for $INCLUDE entries (NLnetLabs/nsd#229).
- Add tls-supported-groups SvcParam support.
- Check iana registries for unimplemented (new) RR types and SvcParamKeys.
- Add support for NINFO, RKEY, RESINFO, WALLET, CLA and TA RR types.

### Fixed

- Prepend -march to CFLAGS to fix architecture detection (NLnetLabs/nsd#372).
- Fix propagation of implicit TTLs (NLnetLabs/nsd#375).
- Fix detection of Westmere architecture by checking for CLMUL too.
- Fix compilation on NetBSD (#233).
- Fix reading specialized symbolic links (NLnetLabs/nsd#380).

## [0.1.1] - 2024-07-19

### Added

- Test to verify configure.ac and Makefile.in are correct.
- Add support for reading from stdin if filename is "-".
- Add support for building with Oracle Developer Studio 12.6.
- Add support for "time" service for Well-Know Services (WKS) RR.

### Fixed

- Fix makefile dependencies.
- Fix makefile to use source directory for build dependencies.
- Fix changelog to reflect v0.1.0 release.
- Update makefile to not use target-specific variables.
- Fix makefile clean targets.
- Fix state keeping in fallback scanner for contiguous and quoted.
- Fix bug in name scanner.
- Fix type mnemonic parsing in fallback parser.
- Fix endian.h to include machine/endian.h on OpenBSD releases before 5.6.
- Fix use after free on buffer resize.
- Fix parsing of numeric protocols in WKS RRs.
- Make devclean target depend on realclean target.
- Fix detection of AVX2 support by checking generic AVX support by the
  processor and operating system (#222).

### Changed

- Make relative includes relative to current working directory.
- Split Autoconf and CMake compiler tests for supported SIMD instructions.

## [0.1.0] - 2024-04-16

### Added

- Initial release.
