/*	$OpenBSD: name_constraints_test.c,v 1.3 2026/09/18 18:24:13 beck Exp $ */
/*
 * Copyright (c) 2026 Bob Beck <beck@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <time.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include "test.h"

static const char dns_root_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBYzCCAQmgAwIBAgIJAJ/D/fOJFvPDMAoGCCqGSM49BAMCMBQxEjAQBgNVBAMM\n"
    "CVRlc3QgUm9vdDAgFw0yNjA5MTgwMjE5MzlaGA8yMTI2MDgyNTAyMTkzOVowFDES\n"
    "MBAGA1UEAwwJVGVzdCBSb290MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEIpna\n"
    "ShBnE5r+xTd9+LMifP2LMngmSfZibz69ektpb//LDZnjt6K6XfBvgRulGq4lhSR3\n"
    "JKZwS0+GjmNDVBPO/qNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
    "AQYwHQYDVR0OBBYEFOSJwXqcbWx4UKfgUSOj490UP/ErMAoGCCqGSM49BAMCA0gA\n"
    "MEUCICKXG3Itms1VrPKCqnm6Cu7nUwVv3/bkmUd4R5EjN7fZAiEA/nfTba6tjdEO\n"
    "lGZ7gWEajGFDCGaG/+BLJw1HrhNIYjo=\n"
    "-----END CERTIFICATE-----\n";

static const char dns_inter_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBsDCCAVegAwIBAgIBAjAKBggqhkjOPQQDAjAUMRIwEAYDVQQDDAlUZXN0IFJv\n"
    "b3QwIBcNMjYwOTE4MDIxOTM5WhgPMjEyNjA4MjUwMjE5MzlaMCgxJjAkBgNVBAMM\n"
    "HVRlc3QgQ29uc3RyYWluZWQgSW50ZXJtZWRpYXRlMFkwEwYHKoZIzj0CAQYIKoZI\n"
    "zj0DAQcDQgAEl0GDBN804RfCuoHzsVndoq+cma6LqzMNaipDirGn6qplmqKibBwF\n"
    "JlPnfWJBpVBSwjvG6mHTxXnd1zHSavhWgKOBgzCBgDAPBgNVHRMBAf8EBTADAQH/\n"
    "MA4GA1UdDwEB/wQEAwIBBjAdBgNVHQ4EFgQUy2/IuHbpHlhlZ3K44AOWzzoPut8w\n"
    "HwYDVR0jBBgwFoAU5InBepxtbHhQp+BRI6Pj3RQ/8SswHQYDVR0eAQH/BBMwEaAP\n"
    "MA2CC2V4YW1wbGUuY29tMAoGCCqGSM49BAMCA0cAMEQCICmgXrzoVNTbNRAC20gD\n"
    "BYVM1jnEo06m6xmHoP9HnuXPAiA2KLp3OlyuEPHwBxdjs8DHKfHzKiTtfIajBVlD\n"
    "89+/Ug==\n"
    "-----END CERTIFICATE-----\n";

static const char dns_leaf_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBxzCCAW2gAwIBAgIBAzAKBggqhkjOPQQDAjAoMSYwJAYDVQQDDB1UZXN0IENv\n"
    "bnN0cmFpbmVkIEludGVybWVkaWF0ZTAgFw0yNjA5MTgwMjE5MzlaGA8yMTI2MDgy\n"
    "NTAyMTkzOVowGzEZMBcGA1UEAwwQbGVhZi5leGFtcGxlLm9yZzBZMBMGByqGSM49\n"
    "AgEGCCqGSM49AwEHA0IABHgzZNJNzRHCViBpUc5fsvnt04orFSMuvAKUxNJvcS9/\n"
    "HYnXkytaa4rwoPdaNof2uW5nK8m0wW4y33kPJiu8pKCjgZIwgY8wDAYDVR0TAQH/\n"
    "BAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwEwHQYDVR0O\n"
    "BBYEFOKpxvj7I7kNvJmygh0RAyy1pgnBMB8GA1UdIwQYMBaAFMtvyLh26R5YZWdy\n"
    "uOADls86D7rfMBoGA1UdEQQTMBGCD3d3dy5leGFtcGxlLmNvbTAKBggqhkjOPQQD\n"
    "AgNIADBFAiB3lUinX2NLUvaeZdQ+dh4gyocE2KexDM+jpfVXeFBGKwIhAPWNJFZc\n"
    "oMxDJezkeiSkBxtNIXO4WUS/fkH3+1JdY6mr\n"
    "-----END CERTIFICATE-----\n";

static const char email_root_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBZDCCAQmgAwIBAgIJAJ6zZnV/dv80MAoGCCqGSM49BAMCMBQxEjAQBgNVBAMM\n"
    "CVRlc3QgUm9vdDAgFw0yNjA5MTgwMjM5MjNaGA8yMTI2MDgyNTAyMzkyM1owFDES\n"
    "MBAGA1UEAwwJVGVzdCBSb290MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEB1u0\n"
    "etxVYvFrG2A0b60YmYIhFm3bBRRiCBHJ9MX7MoGIp1rG+gWHzauDzGT9GtSX2hJ2\n"
    "uARhxSuKxbWjdqYr0KNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
    "AQYwHQYDVR0OBBYEFMdiUsYMFhSVHxfGsCvgPKLDU8IIMAoGCCqGSM49BAMCA0kA\n"
    "MEYCIQD2AGKXmW/ROSOzJo10XTqvXUwEODIl3uqnLb+saru3xgIhAPiC8KapHTT1\n"
    "vm1UPtZx/3l0iuaVCPcVlfE9CU2nv519\n"
    "-----END CERTIFICATE-----\n";

static const char email_inter_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBuDCCAV2gAwIBAgIBAjAKBggqhkjOPQQDAjAUMRIwEAYDVQQDDAlUZXN0IFJv\n"
    "b3QwIBcNMjYwOTE4MDIzOTIzWhgPMjEyNjA4MjUwMjM5MjNaMC4xLDAqBgNVBAMM\n"
    "I1Rlc3QgRW1haWwgQ29uc3RyYWluZWQgSW50ZXJtZWRpYXRlMFkwEwYHKoZIzj0C\n"
    "AQYIKoZIzj0DAQcDQgAEKTTN7AV8n6vriCrs7iTPqWExXvH893grPR8eo//KhgBS\n"
    "n9YYl9eGJlW25YnjEfphQzwg7oMYM2gJkIiAJN716aOBgzCBgDAPBgNVHRMBAf8E\n"
    "BTADAQH/MA4GA1UdDwEB/wQEAwIBBjAdBgNVHQ4EFgQUIMh1YE360lpqRkKxw2ey\n"
    "XFzUv9MwHwYDVR0jBBgwFoAUx2JSxgwWFJUfF8awK+A8osNTwggwHQYDVR0eAQH/\n"
    "BBMwEaAPMA2BC2V4YW1wbGUuY29tMAoGCCqGSM49BAMCA0kAMEYCIQCCNtcyCW88\n"
    "nMntal7XuC0juyjtwbcWK1E65J9sVipe6AIhAO+H877mEsVExqMhx8OsTu+YtF2J\n"
    "rFgKAD6ILBwyw0hU\n"
    "-----END CERTIFICATE-----\n";

static const char email_leaf_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBtTCCAVqgAwIBAgIBAzAKBggqhkjOPQQDAjAuMSwwKgYDVQQDDCNUZXN0IEVt\n"
    "YWlsIENvbnN0cmFpbmVkIEludGVybWVkaWF0ZTAgFw0yNjA5MTgwMjM5MjNaGA8y\n"
    "MTI2MDgyNTAyMzkyM1owNTESMBAGA1UEAwwJVGVzdCBVc2VyMR8wHQYJKoZIhvcN\n"
    "AQkBFhB1c2VyQGV4YW1wbGUub3JnMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE\n"
    "i/q8DtzJvuBQ4c2U9OXbieyL/+cfI1bRkUhHpMQVsbhonV2fjgfT33HRcjvorgaT\n"
    "e+vFxEGJBYuzu1LpYzPD0qNgMF4wDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMC\n"
    "B4AwHQYDVR0OBBYEFGGMF/eHr75J4fsiA3TyskXf8MmiMB8GA1UdIwQYMBaAFCDI\n"
    "dWBN+tJaakZCscNnslxc1L/TMAoGCCqGSM49BAMCA0kAMEYCIQDV5N8xeyrtQThp\n"
    "dGqxQVs5QyqFt0o3PGceR7KeiQQhowIhAOCHFKBjN2b7er8Moa319rMDbs3AN4rw\n"
    "YIEp9QRLgyOu\n"
    "-----END CERTIFICATE-----\n";

static const char dns_cn_leaf_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBsTCCAVegAwIBAgIBBDAKBggqhkjOPQQDAjAoMSYwJAYDVQQDDB1UZXN0IENv\n"
    "bnN0cmFpbmVkIEludGVybWVkaWF0ZTAgFw0yNjA5MTgwMjUzNDRaGA8yMTI2MDgy\n"
    "NTAyNTM0NFowGzEZMBcGA1UEAwwQbGVhZi5leGFtcGxlLm9yZzBZMBMGByqGSM49\n"
    "AgEGCCqGSM49AwEHA0IABBLyzECQc0VYC55Cb6jg3O1+0BbO7AIdJabNyHL7GNtV\n"
    "hmJJB59kkJNjzLX37N7Uy4j/NOxEcfoQrFF95UccMhyjfTB7MAwGA1UdEwEB/wQC\n"
    "MAAwDgYDVR0PAQH/BAQDAgeAMB0GA1UdDgQWBBRjn8rykjP1IkSBhUy7NN6fl/H0\n"
    "ejAfBgNVHSMEGDAWgBTLb8i4dukeWGVncrjgA5bPOg+63zAbBgNVHREEFDASgRB1\n"
    "c2VyQGV4YW1wbGUuY29tMAoGCCqGSM49BAMCA0gAMEUCIFgc+BAThnmPh5IRoc4c\n"
    "HCNmK365bDZrcLSz5XZ+nzbcAiEArDBXcFwG2F50tlDLfI5nXJZeETu6UWuvZbdj\n"
    "rTBBmTI=\n"
    "-----END CERTIFICATE-----\n";

static const char dns_excl_inter_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBrjCCAVSgAwIBAgIBBTAKBggqhkjOPQQDAjAUMRIwEAYDVQQDDAlUZXN0IFJv\n"
    "b3QwIBcNMjYwOTE4MDI1MzQ0WhgPMjEyNjA4MjUwMjUzNDRaMCUxIzAhBgNVBAMM\n"
    "GlRlc3QgRXhjbHVkZWQgSW50ZXJtZWRpYXRlMFkwEwYHKoZIzj0CAQYIKoZIzj0D\n"
    "AQcDQgAE3OeWDBuxIgkJ1yJiRNtspV3d1J8LtvyB/6uX61Xw203nlaY/tRtFJpms\n"
    "W5Qcq9v676bjgFGDuybbStf8+PMY3qOBgzCBgDAPBgNVHRMBAf8EBTADAQH/MA4G\n"
    "A1UdDwEB/wQEAwIBBjAdBgNVHQ4EFgQUud+UN27swGg6gVB/W4BaQPJ4YRIwHwYD\n"
    "VR0jBBgwFoAU5InBepxtbHhQp+BRI6Pj3RQ/8SswHQYDVR0eAQH/BBMwEaEPMA2C\n"
    "C2V4YW1wbGUub3JnMAoGCCqGSM49BAMCA0gAMEUCIQCbQPsHms8Hwei7PGtL6mpw\n"
    "5jH19pLpPzyY99Unanm9TgIgQsQKHB75RU+xBv5dLFSdW3UeiFSwQLtHF0k4o0pt\n"
    "nHI=\n"
    "-----END CERTIFICATE-----\n";

static const char dns_excl_leaf_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBrTCCAVOgAwIBAgIBBjAKBggqhkjOPQQDAjAlMSMwIQYDVQQDDBpUZXN0IEV4\n"
    "Y2x1ZGVkIEludGVybWVkaWF0ZTAgFw0yNjA5MTgwMjUzNDRaGA8yMTI2MDgyNTAy\n"
    "NTM0NFowGzEZMBcGA1UEAwwQbGVhZi5leGFtcGxlLm9yZzBZMBMGByqGSM49AgEG\n"
    "CCqGSM49AwEHA0IABDbzQfOvW110/r+jCHB2bAqKjsSDpEQHPCQQogEiCO2H46YS\n"
    "OAzX6gt8mW5Y8YFfgXaIYHDLhXlqpcFIy9KmxfujfDB6MAwGA1UdEwEB/wQCMAAw\n"
    "DgYDVR0PAQH/BAQDAgeAMB0GA1UdDgQWBBQ4Z0xqYT1cdKLs1+gbthQlSUEwEDAf\n"
    "BgNVHSMEGDAWgBS535Q3buzAaDqBUH9bgFpA8nhhEjAaBgNVHREEEzARgg93d3cu\n"
    "ZXhhbXBsZS5vcmcwCgYIKoZIzj0EAwIDSAAwRQIhAMH+klkOm7BHkul03p3GDVUk\n"
    "pPryO5u/0kU3Jl+d10OnAiBa0Y6nu6nfzj8jT59iJdfs5fwO8AE5kH3V57LkF3cd\n"
    "kA==\n"
    "-----END CERTIFICATE-----\n";

static const char email_ok_leaf_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBtDCCAVqgAwIBAgIBBDAKBggqhkjOPQQDAjAuMSwwKgYDVQQDDCNUZXN0IEVt\n"
    "YWlsIENvbnN0cmFpbmVkIEludGVybWVkaWF0ZTAgFw0yNjA5MTgwMjUzNDRaGA8y\n"
    "MTI2MDgyNTAyNTM0NFowNTESMBAGA1UEAwwJVGVzdCBVc2VyMR8wHQYJKoZIhvcN\n"
    "AQkBFhB1c2VyQGV4YW1wbGUuY29tMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE\n"
    "nn9I5/2RfTFiu7Vxemgews7Le/3v0OdDKpYN5KwYWudGBid9mddCLHXRBUVvA23r\n"
    "R4PTEsHB1/KaYzP5BWytOqNgMF4wDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMC\n"
    "B4AwHQYDVR0OBBYEFM/LNHcuOx+sE1EqG78wLXXuCTHoMB8GA1UdIwQYMBaAFCDI\n"
    "dWBN+tJaakZCscNnslxc1L/TMAoGCCqGSM49BAMCA0gAMEUCIE00zjYzEdP7dFfl\n"
    "s0V7iaIYNvZ41/t35oZ0bXHgUptRAiEAhUGI0T8jDqn7DuFiDQ1nHP00c01hs9Fy\n"
    "FIW+41F3IrU=\n"
    "-----END CERTIFICATE-----\n";

static const char email_excl_inter_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBtTCCAVqgAwIBAgIBBTAKBggqhkjOPQQDAjAUMRIwEAYDVQQDDAlUZXN0IFJv\n"
    "b3QwIBcNMjYwOTE4MDI1MzQ1WhgPMjEyNjA4MjUwMjUzNDVaMCsxKTAnBgNVBAMM\n"
    "IFRlc3QgRW1haWwgRXhjbHVkZWQgSW50ZXJtZWRpYXRlMFkwEwYHKoZIzj0CAQYI\n"
    "KoZIzj0DAQcDQgAEE1Rc0u+OMb8UbVRJDxoAeAWifYoq1oLwZ+4F8X47cRDL7lMC\n"
    "rZilhaSwGzjXFQJ5FxX/93SMxu+sjsswRsFbdqOBgzCBgDAPBgNVHRMBAf8EBTAD\n"
    "AQH/MA4GA1UdDwEB/wQEAwIBBjAdBgNVHQ4EFgQUqrFGC9sTUYjCT5pdFPOnAycf\n"
    "anowHwYDVR0jBBgwFoAUx2JSxgwWFJUfF8awK+A8osNTwggwHQYDVR0eAQH/BBMw\n"
    "EaEPMA2BC2V4YW1wbGUub3JnMAoGCCqGSM49BAMCA0kAMEYCIQD00PY0EoyrJS4/\n"
    "83kjFqb83lG9ZBWw0XGR8esr2SzMFAIhAO+G6nu6sT4u/k/AtrkCmln1/OLyGfxj\n"
    "Zj6ikR+6DvUy\n"
    "-----END CERTIFICATE-----\n";

static const char email_excl_leaf_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBsjCCAVegAwIBAgIBBjAKBggqhkjOPQQDAjArMSkwJwYDVQQDDCBUZXN0IEVt\n"
    "YWlsIEV4Y2x1ZGVkIEludGVybWVkaWF0ZTAgFw0yNjA5MTgwMjU2MzFaGA8yMTI2\n"
    "MDgyNTAyNTYzMVowNTESMBAGA1UEAwwJVGVzdCBVc2VyMR8wHQYJKoZIhvcNAQkB\n"
    "FhB1c2VyQGV4YW1wbGUub3JnMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEfVn0\n"
    "cdCdfkM1XAPHFJeK3zyfuLc/a7HkKOimAbhJSLxdgjVy98mzQHYmr1X26WW7+23K\n"
    "VdAT65wA7c3Ouw/ZFqNgMF4wDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMCB4Aw\n"
    "HQYDVR0OBBYEFNb22Ua2KfmULgfMXfv6jThOELGqMB8GA1UdIwQYMBaAFKqxRgvb\n"
    "E1GIwk+aXRTzpwMnH2p6MAoGCCqGSM49BAMCA0kAMEYCIQDruoAdKFOrJxUhVwcH\n"
    "/R9SjO65jv4fFdkMpSSJT6B+PwIhAKQ3v/k5lpSkSF6FHP7NhNMz6zkZM9W4AfyU\n"
    "QAspl5T3\n"
    "-----END CERTIFICATE-----\n";

static const char ta_root_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBmjCCAUCgAwIBAgIJAJR5kOBZRsJKMAoGCCqGSM49BAMCMCAxHjAcBgNVBAMM\n"
    "FVRlc3QgQ29uc3RyYWluZWQgUm9vdDAgFw0yNjA5MTgwMjUzNDVaGA8yMTI2MDgy\n"
    "NTAyNTM0NVowIDEeMBwGA1UEAwwVVGVzdCBDb25zdHJhaW5lZCBSb290MFkwEwYH\n"
    "KoZIzj0CAQYIKoZIzj0DAQcDQgAEwsk0AG3kxZUW0PTd0ACcdfehKqiahuz3KTL+\n"
    "PoguOZd2Yo9Q0kvpYbgqmVx1l0PNySOB/sbWfBvHZp6v/BWCKqNhMF8wDwYDVR0T\n"
    "AQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYEFP1xzpZ86Geu85q8\n"
    "ChAmleVSdvCjMB0GA1UdHgEB/wQTMBGgDzANggtleGFtcGxlLmNvbTAKBggqhkjO\n"
    "PQQDAgNIADBFAiBbvp2jH4jW7F0cTvbGON48L5L6t/6awyY6ZRHnrWIzWQIhAJ8z\n"
    "ny3xLcu6GdoovzkfIIYX53NhpESm8h/VveIwqrYE\n"
    "-----END CERTIFICATE-----\n";

static const char ta_leaf_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBqDCCAU6gAwIBAgIBAjAKBggqhkjOPQQDAjAgMR4wHAYDVQQDDBVUZXN0IENv\n"
    "bnN0cmFpbmVkIFJvb3QwIBcNMjYwOTE4MDI1MzQ1WhgPMjEyNjA4MjUwMjUzNDVa\n"
    "MBsxGTAXBgNVBAMMEGxlYWYuZXhhbXBsZS5vcmcwWTATBgcqhkjOPQIBBggqhkjO\n"
    "PQMBBwNCAARHYJTIBgDXb4MaYYuBFd8lh8k4Qa3+uubJie/1sxXR9vG3032OizO5\n"
    "55485s125WOkxKEOVqmFxBkRx2KfAwPZo3wwejAMBgNVHRMBAf8EAjAAMA4GA1Ud\n"
    "DwEB/wQEAwIHgDAdBgNVHQ4EFgQUIvzYrx2wGgl61G4j8faQvHMsbIgwHwYDVR0j\n"
    "BBgwFoAU/XHOlnzoZ67zmrwKECaV5VJ28KMwGgYDVR0RBBMwEYIPd3d3LmV4YW1w\n"
    "bGUub3JnMAoGCCqGSM49BAMCA0gAMEUCIQCWjCpNLtzVsg6XDfY23rk3KeJyijtQ\n"
    "/ib6CgFIASJOjQIgbgP7eHmzW/dWvW7XXYP3PulZitNVoEK47xP80CHpCIw=\n"
    "-----END CERTIFICATE-----\n";

static const time_t check_time = 1798761600;

struct chain_test {
	const char *root_pem;
	const char *inter_pem;
	const char *leaf_pem;
	int want_error;
};

static X509 *
cert_from_pem(struct test *t, const char *pem)
{
	BIO *bio;
	X509 *x = NULL;

	if ((bio = BIO_new_mem_buf(pem, -1)) == NULL) {
		test_errorf(t, "BIO_new_mem_buf");
		return NULL;
	}
	if ((x = PEM_read_bio_X509(bio, NULL, NULL, NULL)) == NULL)
		test_errorf(t, "PEM_read_bio_X509");
	BIO_free(bio);

	return x;
}

/*
 * Verify a chain of freshly parsed certificates, so that no extension state
 * is cached on any of them when verification starts.
 */
static void
test_chain(struct test *t, const void *arg)
{
	const struct chain_test *ct = arg;
	X509 *root = NULL, *intermediate = NULL, *leaf = NULL;
	X509_STORE *store = NULL;
	X509_STORE_CTX *ctx = NULL;
	STACK_OF(X509) *untrusted = NULL;
	int error, ret;

	if ((root = cert_from_pem(t, ct->root_pem)) == NULL)
		goto err;
	if (ct->inter_pem != NULL &&
	    (intermediate = cert_from_pem(t, ct->inter_pem)) == NULL)
		goto err;
	if ((leaf = cert_from_pem(t, ct->leaf_pem)) == NULL)
		goto err;

	if ((store = X509_STORE_new()) == NULL) {
		test_errorf(t, "X509_STORE_new");
		goto err;
	}
	if (!X509_STORE_add_cert(store, root)) {
		test_errorf(t, "X509_STORE_add_cert");
		goto err;
	}
	if ((untrusted = sk_X509_new_null()) == NULL) {
		test_errorf(t, "sk_X509_new_null");
		goto err;
	}
	if (intermediate != NULL && !sk_X509_push(untrusted, intermediate)) {
		test_errorf(t, "sk_X509_push");
		goto err;
	}
	if ((ctx = X509_STORE_CTX_new()) == NULL) {
		test_errorf(t, "X509_STORE_CTX_new");
		goto err;
	}
	if (!X509_STORE_CTX_init(ctx, store, leaf, untrusted)) {
		test_errorf(t, "X509_STORE_CTX_init");
		goto err;
	}
	X509_STORE_CTX_set_time(ctx, 0, check_time);

	ret = X509_verify_cert(ctx);
	error = X509_STORE_CTX_get_error(ctx);

	if (ct->want_error == X509_V_OK) {
		if (ret != 1)
			test_errorf(t, "X509_verify_cert: %s",
			    X509_verify_cert_error_string(error));
	} else {
		if (ret == 1)
			test_errorf(t, "X509_verify_cert succeeded, want %s",
			    X509_verify_cert_error_string(ct->want_error));
		else if (error != ct->want_error)
			test_errorf(t, "X509_verify_cert: %s, want %s",
			    X509_verify_cert_error_string(error),
			    X509_verify_cert_error_string(ct->want_error));
	}

 err:
	X509_STORE_CTX_free(ctx);
	X509_STORE_free(store);
	sk_X509_free(untrusted);
	X509_free(root);
	X509_free(intermediate);
	X509_free(leaf);
}

/*
 * The intermediate is constrained to DNS names under example.com. The leaf's
 * subject CN, leaf.example.org, lies outside that subtree while its SAN,
 * www.example.com, lies inside it, so the chain is only accepted if the
 * leaf's cached SAN is in place when its names are checked.
 */
static const struct chain_test dns_permitted_san = {
	.root_pem = dns_root_pem,
	.inter_pem = dns_inter_pem,
	.leaf_pem = dns_leaf_pem,
	.want_error = X509_V_OK,
};

/*
 * The intermediate is constrained to email addresses under example.com. The
 * leaf has no SAN and carries user@example.org as the second entry of its
 * subject, so the chain is only rejected if that entry is found.
 */
static const struct chain_test email_subject_not_permitted = {
	.root_pem = email_root_pem,
	.inter_pem = email_inter_pem,
	.leaf_pem = email_leaf_pem,
	.want_error = X509_V_ERR_PERMITTED_VIOLATION,
};

/* As above, but the subject email address user@example.com is permitted. */
static const struct chain_test email_subject_permitted = {
	.root_pem = email_root_pem,
	.inter_pem = email_inter_pem,
	.leaf_pem = email_ok_leaf_pem,
	.want_error = X509_V_OK,
};

/*
 * The intermediate is constrained to DNS names under example.com. The leaf's
 * SAN holds only an email address, so its subject CN, leaf.example.org, is
 * checked as a DNS name and violates the constraint.
 */
static const struct chain_test dns_cn_not_permitted = {
	.root_pem = dns_root_pem,
	.inter_pem = dns_inter_pem,
	.leaf_pem = dns_cn_leaf_pem,
	.want_error = X509_V_ERR_PERMITTED_VIOLATION,
};

/*
 * The intermediate excludes DNS names under example.org and the leaf's SAN
 * is www.example.org.
 */
static const struct chain_test dns_san_excluded = {
	.root_pem = dns_root_pem,
	.inter_pem = dns_excl_inter_pem,
	.leaf_pem = dns_excl_leaf_pem,
	.want_error = X509_V_ERR_EXCLUDED_VIOLATION,
};

/*
 * The intermediate excludes email addresses under example.org and the leaf
 * has no SAN and user@example.org in its subject.
 */
static const struct chain_test email_subject_excluded = {
	.root_pem = email_root_pem,
	.inter_pem = email_excl_inter_pem,
	.leaf_pem = email_excl_leaf_pem,
	.want_error = X509_V_ERR_EXCLUDED_VIOLATION,
};

/*
 * The trust anchor itself is constrained to DNS names under example.com and
 * directly issues a leaf with SAN www.example.org. Constraints on a trust
 * anchor are enforced.
 */
static const struct chain_test trust_anchor_constrained = {
	.root_pem = ta_root_pem,
	.inter_pem = NULL,
	.leaf_pem = ta_leaf_pem,
	.want_error = X509_V_ERR_PERMITTED_VIOLATION,
};

int
main(int argc, char **argv)
{
	struct test *t = test_init();

	test_run(t, "dns permitted san", test_chain, &dns_permitted_san);
	test_run(t, "email subject not permitted", test_chain,
	    &email_subject_not_permitted);
	test_run(t, "email subject permitted", test_chain,
	    &email_subject_permitted);
	test_run(t, "dns cn not permitted", test_chain, &dns_cn_not_permitted);
	test_run(t, "dns san excluded", test_chain, &dns_san_excluded);
	test_run(t, "email subject excluded", test_chain,
	    &email_subject_excluded);
	test_run(t, "trust anchor constrained", test_chain,
	    &trust_anchor_constrained);

	return test_result(t);
}
