/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef AUTHFILE_H
#define AUTHFILE_H

/*
 * Saves the authentication (private) key in a file, encrypting it with
 * passphrase.
 * For RSA keys: The identification of the file (lowest 64 bits of n)
 * will precede the key to provide identification of the key without
 * needing a passphrase.
 */
int
save_private_key(const char *filename, const char *passphrase,
    Key * private_key, const char *comment);

/*
 * Loads the public part of the key file (public key and comment). Returns 0
 * if an error occurred; zero if the public key was successfully read.  The
 * comment of the key is returned in comment_return if it is non-NULL; the
 * caller must free the value with xfree.
 */
int load_public_key(const char *filename, Key * pub, char **comment_return);
int try_load_public_key(const char *filename, Key * pub, char **comment_return);

/*
 * Loads the private key from the file.  Returns 0 if an error is encountered
 * (file does not exist or is not readable, or passphrase is bad). This
 * initializes the private key.  The comment of the key is returned in
 * comment_return if it is non-NULL; the caller must free the value with
 * xfree.
 */
int
load_private_key(const char *filename, const char *passphrase,
    Key * private_key, char **comment_return);

#endif
