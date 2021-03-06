/*-
 *   BSD LICENSE
 *
 *   Copyright (c) Intel Corporation.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file
 * String utility functions
 */

#ifndef SPDK_STRING_H
#define SPDK_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * sprintf with automatic buffer allocation.
 *
 * The return value is the formatted string,
 * which should be passed to free() when no longer needed,
 * or NULL on failure.
 */
char *spdk_sprintf_alloc(const char *format, ...) __attribute__((format(printf, 1, 2)));

/**
 * Convert string to lowercase in place.
 *
 * \param s String to convert to lowercase.
 */
char *spdk_strlwr(char *s);

/**
 * Parse a delimited string with quote handling.
 *
 * \param stringp Pointer to starting location in string. *stringp will be updated to point to the
 * start of the next field, or NULL if the end of the string has been reached.
 * \param delim Null-terminated string containing the list of accepted delimiters.
 *
 * \return Pointer to beginning of the current field.
 *
 * Note that the string will be modified in place to add the string terminator to each field.
 */
char *spdk_strsepq(char **stringp, const char *delim);

/**
 * Trim whitespace from a string in place.
 *
 * \param s String to trim.
 */
char *spdk_str_trim(char *s);

#ifdef __cplusplus
}
#endif

#endif
