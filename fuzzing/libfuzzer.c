/* Copyright (C) 2026 Jeffrey Stedfast.
 * Distributed under the MIT license; see LICENSE. */

#include "fuzz.h"
#include <stdint.h>

int
LLVMFuzzerInitialize (int *argc, char ***argv)
{
	fuzz_init ();
	return 0;
}

int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t length)
{
	if (length <= FUZZ_MAX_LENGTH)
		FUZZ_TARGET (data, length);
	return 0;
}
