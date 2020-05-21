#   $OpenBSD: tlsfuzzer.py,v 1.2 2020/05/21 19:08:32 tb Exp $
#
# Copyright (c) 2020 Theo Buehler <tb@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import getopt
import os
import subprocess
import sys
from timeit import default_timer as timer

tlsfuzzer_scriptdir = "/usr/local/share/tlsfuzzer/scripts/"

class Test:
    """
    Represents a tlsfuzzer test script.
        name:           the script's name
        args:           arguments to feed to the script
        tls12_args:     override args for a TLSv1.2 server
        tls13_args:     override args for a TLSv1.3 server

    XXX Add client cert support.
    """
    def __init__(self, name="", args=[], tls12_args=[], tls13_args=[]):
        self.name = name
        self.tls12_args = args
        self.tls13_args = args
        if tls12_args:
            self.tls12_args = tls12_args
        if tls13_args:
            self.tls13_args = tls13_args

    def args(self, has_tls1_3: True):
        if has_tls1_3:
            return self.tls13_args
        else:
            return self.tls12_args

    def __repr__(self):
        return "<Test: %s tls12_args: %s tls13_args: %s>" % (
                self.name, self.tls12_args, tls13_args
            )

class TestGroup:
    """ A group of Test objects to be run by TestRunner."""
    def __init__(self, title="Tests", tests=[]):
        self.title = title
        self.tests = tests

    def __iter__(self):
        return iter(self.tests)

# argument to pass to several tests
tls13_unsupported_ciphers = [
    "-e", "TLS 1.3 with ffdhe2048",
    "-e", "TLS 1.3 with ffdhe3072",
    "-e", "TLS 1.3 with secp521r1",   # XXX: why is this curve problematic?
    "-e", "TLS 1.3 with x448",
]

tls13_tests = TestGroup("TLSv1.3 tests", [
    Test("test-tls13-ccs.py"),
    Test("test-tls13-conversation.py"),
    Test("test-tls13-count-tickets.py"),
    Test("test-tls13-empty-alert.py"),
    Test("test-tls13-finished-plaintext.py"),
    Test("test-tls13-hrr.py"),
    Test("test-tls13-legacy-version.py"),
    Test("test-tls13-nociphers.py"),
    Test("test-tls13-record-padding.py"),
])

# Tests that take a lot of time (> ~30s on an x280)
tls13_slow_tests = TestGroup("slow TLSv1.3 tests", [
    # XXX: Investigate the occasional message
    # "Got shared secret with 1 most significant bytes equal to zero."
    Test("test-tls13-dhe-shared-secret-padding.py", tls13_unsupported_ciphers),

    Test("test-tls13-invalid-ciphers.py"),
    Test("test-tls13-serverhello-random.py", tls13_unsupported_ciphers),
])

tls13_extra_cert_tests = TestGroup("TLSv1.3 certificate tests", [
    # need to set up client certs to run these
    Test("test-tls13-certificate-request.py"),
    Test("test-tls13-certificate-verify.py"),
    Test("test-tls13-ecdsa-in-certificate-verify.py"),

    # Test expects the server to have installed three certificates:
    # with P-256, P-384 and P-521 curve. Also SHA1+ECDSA is verified
    # to not work.
    Test("test-tls13-ecdsa-support.py"),
])

tls13_failing_tests = TestGroup("failing TLSv1.3 tests", [
    # Some tests fail because we fail later than the scripts expect us to.
    # With X25519, we accept weak peer public keys and fail when we actually
    # compute the keyshare.  Other tests seem to indicate that we could be
    # stricter about what keyshares we accept.
    Test("test-tls13-crfg-curves.py"),
    Test("test-tls13-ecdhe-curves.py"),

    # Expects a missing_extensions alert
    # AssertionError: Unexpected message from peer: Handshake(server_hello)
    Test("test-tls13-keyshare-omitted.py"),

    # https://github.com/openssl/openssl/issues/8369
    Test("test-tls13-obsolete-curves.py"),

    # 3 failing rsa_pss_pss tests
    Test("test-tls13-rsa-signatures.py"),

    # AssertionError: Unexpected message from peer: ChangeCipherSpec()
    # Most failing tests expect the CCS right before finished.
    # What's up with that?
    Test("test-tls13-version-negotiation.py"),

    # The following three tests fail due to broken pipe.
    # AssertionError: Unexpected closure from peer:
    # 'zero-length app data'
    # 'zero-length app data with large padding'
    # 'zero-length app data with padding'
    Test("test-tls13-zero-length-data.py"),
])

tls13_slow_failing_tests = TestGroup("slow, failing TLSv1.3 tests", [
    # After adding record overflow alert, 14 tests fail because
    # they expect ExpectNewSessionTicket().
    Test("test-tls13-record-layer-limits.py" ),

    # Other test failures bugs in keyshare/tlsext negotiation?
    Test("test-tls13-shuffled-extentions.py"),    # should reject 2nd CH
    Test("test-tls13-unrecognised-groups.py"),    # unexpected closure

    # 5 failures:
    #   'app data split, conversation with KeyUpdate msg'
    #   'fragmented keyupdate msg'
    #   'multiple KeyUpdate messages'
    #   'post-handshake KeyUpdate msg with update_not_request'
    #   'post-handshake KeyUpdate msg with update_request'
    Test("test-tls13-keyupdate.py"),
 
    Test("test-tls13-symetric-ciphers.py"),       # unexpected message from peer

    # 70 fail and 644 pass. For some reason the tests expect a decode_error
    # but we send a decrypt_error after the CBS_mem_equal() fails in
    # tls13_server_finished_recv() (which is correct).
    Test("test-tls13-finished.py"),               # decrypt_error -> decode_error?

    # The following two tests fail Test (skip empty extensions for the first one):
    # 'empty unassigned extensions, ids in range from 2 to 4118'
    # 'unassigned extensions with random payload, ids in range from 2 to 1046'
    Test("test-tls13-large-number-of-extensions.py"), # 2 fail: empty/random payload

    # 6 tests fail: 'rsa_pkcs1_{md5,sha{1,224,256,384,512}} signature'
    # We send server hello, but the test expects handshake_failure
    Test("test-tls13-pkcs-signature.py"),
    # 8 tests fail: 'tls13 signature rsa_pss_{pss,rsae}_sha{256,384,512}
    Test("test-tls13-rsapss-signatures.py"),

    # ExpectNewSessionTicket
    Test("test-tls13-session-resumption.py"),
])

tls13_unsupported_tests = TestGroup("TLSv1.3 tests for unsupported features", [
    # Tests for features we don't support
    Test("test-tls13-0rtt-garbage.py"),
    Test("test-tls13-ffdhe-groups.py"),
    Test("test-tls13-ffdhe-sanity.py"),
    Test("test-tls13-psk_dhe_ke.py"),
    Test("test-tls13-psk_ke.py"),

    # need server to react to HTTP GET for /keyupdate
    Test("test-tls13-keyupdate-from-server.py"),

    # Weird test: tests servers that don't support 1.3
    Test("test-tls13-non-support.py"),

    # broken test script
    # UnboundLocalError: local variable 'cert' referenced before assignment
    Test("test-tls13-post-handshake-auth.py"),

    # Server must be configured to support only rsa_pss_rsae_sha512
    Test("test-tls13-signature-algorithms.py"),
])

tls12_exclude_legacy_protocols = [
    # all these have BIO_read timeouts against TLSv1.3
    "-e", "Protocol (3, 0)",
    "-e", "Protocol (3, 0) in SSLv2 compatible ClientHello",
    # the following only fail with TLSv1.3
    "-e", "Protocol (3, 1) in SSLv2 compatible ClientHello",
    "-e", "Protocol (3, 2) in SSLv2 compatible ClientHello",
    "-e", "Protocol (3, 3) in SSLv2 compatible ClientHello",
    "-e", "Protocol (3, 1) with secp521r1 group",   # XXX
    "-e", "Protocol (3, 1) with x448 group",
    "-e", "Protocol (3, 2) with secp521r1 group",   # XXX
    "-e", "Protocol (3, 2) with x448 group",
    "-e", "Protocol (3, 3) with secp521r1 group",   # XXX
    "-e", "Protocol (3, 3) with x448 group",
]

tls12_tests = TestGroup("TLSv1.2 tests", [
    # Tests that pass as they are.
    Test("test-TLSv1_2-rejected-without-TLSv1_2.py"),
    Test("test-aes-gcm-nonces.py"),
    Test("test-chacha20.py"),
    Test("test-conversation.py"),
    Test("test-cve-2016-2107.py"),
    Test("test-dhe-rsa-key-exchange.py"),
    Test("test-early-application-data.py"),
    Test("test-empty-extensions.py"),
    Test("test-fuzzed-MAC.py"),
    Test("test-fuzzed-ciphertext.py"),
    Test("test-fuzzed-finished.py"),
    Test("test-fuzzed-padding.py"),
    Test("test-hello-request-by-client.py"),
    Test("test-invalid-cipher-suites.py"),
    Test("test-invalid-content-type.py"),
    Test("test-invalid-session-id.py"),
    Test("test-invalid-version.py"),
    Test("test-message-skipping.py"),
    Test("test-no-heartbeat.py"),
    Test("test-sessionID-resumption.py"),
    Test("test-sslv2-connection.py"),
    Test("test-truncating-of-finished.py"),
    Test("test-truncating-of-kRSA-client-key-exchange.py"),
    Test("test-unsupported-curve-fallback.py"),
    Test("test-version-numbers.py"),
    Test("test-zero-length-data.py"),

    # Tests that need tweaking for unsupported features and ciphers.
    Test(
        "test-atypical-padding.py", [
            "-e", "sanity - encrypt then MAC",
            "-e", "2^14 bytes of AppData with 256 bytes of padding (SHA1 + Encrypt then MAC)",
        ]
    ),
    Test(
        "test-dhe-rsa-key-exchange-signatures.py", [
            "-e", "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA sha224 signature",
            "-e", "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 sha224 signature",
            "-e", "TLS_DHE_RSA_WITH_AES_128_CBC_SHA sha224 signature",
            "-e", "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256 sha224 signature",
            "-e", "TLS_DHE_RSA_WITH_AES_256_CBC_SHA sha224 signature",
        ]
    ),
    Test("test-dhe-key-share-random.py", tls12_exclude_legacy_protocols),
    Test("test-export-ciphers-rejected.py", ["--min-ver", "TLSv1.0"]),
    Test(
        "test-downgrade-protection.py",
        tls12_args = ["--server-max-protocol", "TLSv1.2"],
        tls13_args = ["--server-max-protocol", "TLSv1.3"],
    ),
    Test("test-fallback-scsv.py", tls13_args = ["--tls-1.3"] ),
    Test("test-serverhello-random.py", args = tls12_exclude_legacy_protocols),
])

tls12_slow_tests = TestGroup("slow TLSv1.2 tests", [
    Test("test-cve-2016-7054.py"),
    Test("test-dhe-no-shared-secret-padding.py", tls12_exclude_legacy_protocols),
    Test("test-ecdhe-padded-shared-secret.py", tls12_exclude_legacy_protocols),
    Test("test-ecdhe-rsa-key-share-random.py", tls12_exclude_legacy_protocols),
    # This test has some failures once in a while.
    Test("test-fuzzed-plaintext.py"),
])

tls12_failing_tests = TestGroup("failing TLSv1.2 tests", [
    # no shared cipher
    Test("test-aesccm.py"),
    # need server to set up alpn
    Test("test-alpn-negotiation.py"),
    # many tests fail due to unexpected server_name extension
    Test("test-bleichenbacher-workaround.py"),

    # need client key and cert plus extra server setup
    Test("test-certificate-malformed.py"),
    Test("test-certificate-request.py"),
    Test("test-certificate-verify-malformed-sig.py"),
    Test("test-certificate-verify-malformed.py"),
    Test("test-certificate-verify.py"),
    Test("test-ecdsa-in-certificate-verify.py"),
    Test("test-renegotiation-disabled-client-cert.py"),
    Test("test-rsa-pss-sigs-on-certificate-verify.py"),
    Test("test-rsa-sigs-on-certificate-verify.py"),

    # test doesn't expect session ticket
    Test("test-client-compatibility.py"),
    # abrupt closure
    Test("test-client-hello-max-size.py"),
    # unknown signature algorithms
    Test("test-clienthello-md5.py"),
    # abrupt closure
    Test("test-cve-2016-6309.py"),

    # failing tests are fixed by sending illegal_parameter alert after
    # DH_compute_keyTest() in ssl_srvr.c
    Test("test-dhe-rsa-key-exchange-with-bad-messages.py"),
    # Tests expect an illegal_parameter alert
    Test("test-ecdhe-rsa-key-exchange-with-bad-messages.py"),

    # We send a handshake_failure while the test expects to succeed
    Test("test-ecdhe-rsa-key-exchange.py"),

    # unsupported?
    Test("test-extended-master-secret-extension-with-client-cert.py"),

    # no shared cipher
    Test("test-ecdsa-sig-flexibility.py"),

    # unsupported
    Test("test-encrypt-then-mac-renegotiation.py"),
    Test("test-encrypt-then-mac.py"),
    Test("test-extended-master-secret-extension.py"),
    Test("test-ffdhe-negotiation.py"),
    # unsupported. Expects the server to send the heartbeat extension
    Test("test-heartbeat.py"),

    # 29 succeed, 263 fail:
    #   'n extensions', 'n extensions last empty' n in 4086, 4096, 8192, 16383
    #   'fuzz ext length to n' n in [0..255] with the exception of 41...
    Test("test-extensions.py"),

    # Tests expect SH but we send unexpected_message or handshake_failure
    #   'Application data inside Client Hello'
    #   'Application data inside Client Key Exchange'
    #   'Application data inside Finished'
    Test("test-interleaved-application-data-and-fragmented-handshakes-in-renegotiation.py"),
    # Tests expect SH but we send handshake_failure
    #   'Application data before Change Cipher Spec'
    #   'Application data before Client Key Exchange'
    #   'Application data before Finished'
    Test("test-interleaved-application-data-in-renegotiation.py"),

    # broken test script
    # TypeError: '<' not supported between instances of 'int' and 'NoneType'
    Test("test-invalid-client-hello-w-record-overflow.py"),

    # Lots of failures. abrupt closure
    Test("test-invalid-client-hello.py"),

    # Test expects illegal_parameter, we send decode_error in ssl_srvr.c:1016
    # Need to check that this is correct.
    Test("test-invalid-compression-methods.py"),

    # abrupt closure
    # 'encrypted premaster set to all zero (n)' n in 256 384 512
    Test("test-invalid-rsa-key-exchange-messages.py"),

    # test expects illegal_parameter, we send unrecognized_name (which seems
    # correct according to rfc 6066?)
    Test("test-invalid-server-name-extension-resumption.py"),
    # let through some server names without sending an alert
    # again illegal_parameter vs unrecognized_name
    Test("test-invalid-server-name-extension.py"),

    Test("test-large-hello.py"),

    # 14 pass
    # 7 fail
    # 'n extensions', n in 4095, 4096, 4097, 8191, 8192, 8193, 16383,
    Test("test-large-number-of-extensions.py"),

    # 4 failures:
    #   'insecure (legacy) renegotiation with GET after 2nd handshake'
    #   'insecure (legacy) renegotiation with incomplete GET'
    #   'secure renegotiation with GET after 2nd handshake'
    #   'secure renegotiation with incomplete GET'
    Test("test-legacy-renegotiation.py"),

    # 1 failure (timeout): we don't send the unexpected_message alert
    # 'duplicate change cipher spec after Finished'
    Test("test-message-duplication.py"),

    # server should send status_request
    Test("test-ocsp-stapling.py"),

    # unexpected closure
    Test("test-openssl-3712.py"),

    # 3 failures:
    # 'big, needs fragmentation: max fragment - 16336B extension'
    # 'big, needs fragmentation: max fragment - 32768B extension'
    # 'maximum size: max fragment - 65531B extension'
    Test("test-record-layer-fragmentation.py"),

    # wants --reply-AD-size
    Test("test-record-size-limit.py"),

    # failed: 3 (expect an alert, we send AD)
    # 'try insecure (legacy) renegotiation with incomplete GET'
    # 'try secure renegotiation with GET after 2nd CH'
    # 'try secure renegotiation with incomplete GET'
    Test("test-renegotiation-disabled.py"),

    # 'resumption of safe session with NULL cipher'
    # 'resumption with cipher from old CH but not selected by server'
    Test("test-resumption-with-wrong-ciphers.py"),

    # 5 failures:
    #   'empty sigalgs'
    #   'only undefined sigalgs'
    #   'rsa_pss_pss_sha256 only'
    #   'rsa_pss_pss_sha384 only'
    #   'rsa_pss_pss_sha512 only'
    Test("test-sig-algs.py"),

    # 13 failures:
    #   'duplicated n non-rsa schemes' for n in 202 2342 8119 23741 32744
    #   'empty list of signature methods'
    #   'tolerance n RSA or ECDSA methods' for n in 215 2355 8132 23754
    #   'tolerance 32758 methods with sig_alg_cert'
    #   'tolerance max 32744 number of methods with sig_alg_cert'
    #   'tolerance max (32760) number of methods'
    Test("test-signature-algorithms.py"),

    # times out
    Test("test-ssl-death-alert.py"),

    # 17 pass, 13 fail. padding and truncation
    Test("test-truncating-of-client-hello.py"),

    # x448 tests need disabling plus x25519 corner cases need sorting out
    Test("test-x25519.py"),
])

tls12_unsupported_tests = TestGroup("TLSv1.2 for unsupported features", [
    # protocol_version
    Test("test-SSLv3-padding.py"),
])

# These tests take a ton of time to fail against an 1.3 server,
# so don't run them against 1.3 pending further investigation.
legacy_tests = TestGroup("Legacy protocol tests", [
    Test("test-sslv2-force-cipher-3des.py"),
    Test("test-sslv2-force-cipher-non3des.py"),
    Test("test-sslv2-force-cipher.py"),
    Test("test-sslv2-force-export-cipher.py"),
    Test("test-sslv2hello-protocol.py"),
])

all_groups = [
    tls13_tests,
    tls13_slow_tests,
    tls13_extra_cert_tests,
    tls13_failing_tests,
    tls13_slow_failing_tests,
    tls13_unsupported_tests,
    tls12_tests,
    tls12_slow_tests,
    tls12_failing_tests,
    tls12_unsupported_tests,
    legacy_tests,
]

failing_groups = [
    tls13_failing_tests,
    tls13_slow_failing_tests,
    tls12_failing_tests,
]

class TestRunner:
    """ Runs the given tests troups against a server and displays stats. """

    def __init__(
        self, timing=False, verbose=False, port=4433, use_tls1_3=True,
        dry_run=False, tests=[], scriptdir=tlsfuzzer_scriptdir,
    ):
        self.tests = []

        self.dryrun = dry_run
        self.use_tls1_3 = use_tls1_3
        self.port = str(port)
        self.scriptdir = scriptdir

        self.stats = []
        self.failed = []

        self.timing = timing
        self.verbose = verbose

    def add(self, title="tests", tests=[]):
        # tests.sort(key=lambda test: test.name)
        self.tests.append(TestGroup(title, tests))

    def add_group(self, group):
        self.tests.append(group)

    def run_script(self, test):
        script = test.name
        args = ["-p"] + [self.port] + test.args(self.use_tls1_3)

        if self.dryrun:
            if not self.verbose:
                args = []
            print(script , end=' ' if args else '')
            print(' '.join([f"\"{arg}\"" for arg in args]))
            return

        if self.verbose:
            print(script)
        else:
            print(f"{script[:68]:<72}", end=" ", flush=True)
        start = timer()
        test = subprocess.run(
            ["python3", os.path.join(self.scriptdir, script)] + args,
            capture_output=not self.verbose,
            text=True,
        )
        end = timer()
        self.stats.append((script, end - start))
        if test.returncode == 0:
            print("OK")
            return
        print("FAILED")
        self.failed.append(script)

        if self.verbose:
            return

        print('\n'.join(test.stdout.split("Test end\n", 1)[1:]), end="")

    def run(self):
        for group in self:
            print(f"Running {group.title} ...")
            for test in group:
                self.run_script(test)
        return not self.failed

    def __iter__(self):
        return iter(self.tests)

    def __del__(self):
        if self.timing and self.stats:
            total = 0.0
            for (script, time) in self.stats:
                print(f"{round(time, 2):6.2f} {script}")
                total += time
            print(f"{round(total, 2):6.2f} total")

        if self.failed:
            print("Failed tests:")
            print('\n'.join(self.failed))

class TlsServer:
    """ Spawns an s_server listening on localhost:port if necessary. """

    def __init__(self, port=4433):
        self.spawn = True
        # Check whether a server is already listening on localhost:port
        self.spawn = subprocess.run(
            ["nc", "-c", "-z", "-T", "noverify", "localhost", str(port)],
            stderr=subprocess.DEVNULL,
        ).returncode != 0

        if self.spawn:
            self.server = subprocess.Popen(
                [
                    "openssl",
                    "s_server",
                    "-accept",
                    str(port),
                    "-key",
                    "localhost.key",
                    "-cert",
                    "localhost.crt",
                    "-www",
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
            )

        # Check whether the server talks TLSv1.3
        self.has_tls1_3 = subprocess.run(
            [
                "nc",
                "-c",
                "-z",
                "-T",
                "noverify",
                "-T",
                "protocols=TLSv1.3",
                "localhost",
                str(port),
            ],
            stderr=subprocess.DEVNULL,
        ).returncode == 0

        self.check()

    def check(self):
        if self.spawn and self.server.poll() is not None:
            print(self.server.stderr.read())
            raise RuntimeError(
                f"openssl s_server died. Return code: {self.server.returncode}."
            )
        if self.spawn:
            self.server.stderr.detach()

    def __del__(self):
        if self.spawn:
            self.server.terminate()

# Extract the arguments we pass to script
def defaultargs(script, has_tls1_3):
    return next(
        (test for group in all_groups for test in group if test.name == script),
        Test()
    ).args(has_tls1_3)

def list_or_missing(missing=True):
    tests = [test.name for group in all_groups for test in group]

    if missing:
        scripts = {
            f for f in os.listdir(tlsfuzzer_scriptdir) if f != "__pycache__"
        }
        missing = scripts - set(tests)
        if missing:
            print('\n'.join(sorted(missing)))
        exit(0)

    tests.sort()
    print('\n'.join(tests))
    exit(0)

def usage():
    print("Usage: python3 tlsfuzzer.py [-lmnstv] [-p port] [script [test...]]")
    print(" --help      help")
    print(" -f          run failing tests")
    print(" -l          list tests")
    print(" -m          list new tests after package update")
    print(" -n          do not run tests, but list the ones that would be run")
    print(" -p port     connect to this port - defaults to 4433")
    print(" -s          run slow tests")
    print(" -t          show timing stats at end")
    print(" -v          verbose output")
    exit(0)

def main():
    failing = False
    list = False
    missing = False
    dryrun = False
    port = 4433
    slow = False
    timing = False
    verbose = False

    argv = sys.argv[1:]
    opts, args = getopt.getopt(argv, "flmnp:stv", ["help"])
    for opt, arg in opts:
        if opt == '--help':
            usage()
        elif opt == '-f':
            failing = True
        elif opt == '-l':
            list = True
        elif opt == '-m':
            missing = True
        elif opt == '-n':
            dryrun = True
        elif opt == '-p':
            port = int(arg)
        elif opt == '-s':
            slow = True
        elif opt == '-t':
            timing = True
        elif opt == '-v':
            verbose = True
        else:
            raise ValueError(f"Unknown option: {opt}")

    if not os.path.exists(tlsfuzzer_scriptdir):
        print("package py3-tlsfuzzer is required for this regress")
        exit(1)

    if list and failing:
        failing = [test.name for group in failing_groups for test in group]
        failing.sort()
        print('\n'.join(failing))
        exit(0)

    if list or missing:
        list_or_missing(missing)

    tls_server = TlsServer(port)

    tests = TestRunner(timing, verbose, port, tls_server.has_tls1_3, dryrun)

    if args:
        (dir, script) = os.path.split(args[0])
        if dir and not dir == '.':
            tests.scriptdir = dir

        testargs = defaultargs(script, tls_server.has_tls1_3)

        tests.verbose = True
        tests.add("test from command line", [Test(script, testargs + args[1:])])

        exit(not tests.run())

    if failing:
        if tls_server.has_tls1_3:
            tests.add_group(tls13_failing_tests)
            if slow:
                tests.add_group(tls13_slow_failing_tests)
        tests.add_group(tls12_failing_tests)

    if tls_server.has_tls1_3:
        tests.add_group(tls13_tests)
        if slow:
            tests.add_group(tls13_slow_tests)
    else:
        tests.add_group(legacy_tests)

    tests.add_group(tls12_tests)
    if slow:
        tests.add_group(tls12_slow_tests)

    success = tests.run()
    del tests

    if not success:
        print("FAILED")
        exit(1)

if __name__ == "__main__":
    main()
