/*
 * Copyright 2005, Ingo Weinhold <bonefish@cs.tu-berlin.de>.
 * Copyright 2010, Oliver Tappe, zooey@hirschkaefer.de.
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#define B_USE_POSITIVE_POSIX_ERRORS
#include <errno.h>
#include <string.h>

#include "LocaleBackend.h"


using BPrivate::gLocaleBackend;


extern "C" size_t
strxfrm(char *out, const char *in, size_t size)
{
	if (gLocaleBackend != NULL) {
		size_t outSize = 0;
		status_t status =  gLocaleBackend->Strxfrm(out, in, size, outSize);

		if (status != B_OK)
			errno = EINVAL;

		return outSize;
	}

	return strlcpy(out, in, size);
}