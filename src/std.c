/*
 * Copyright 2019 The Hafnium Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hf/std.h"

#include "hf/panic.h"

/* Declare unsafe functions locally so they are not available globally. */
void *memset(void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);

/*
 * As per the C11 specification, mem*_s() operations fill the destination buffer
 * if runtime constraint validation fails, assuming that `dest` and `destsz`
 * are both valid.
 */
#define CHECK_OR_FILL(cond, dest, destsz, ch)                               \
	do {                                                                \
		if (!(cond)) {                                              \
			if ((dest) != NULL && (destsz) <= RSIZE_MAX) {      \
				memset_s((dest), (destsz), (ch), (destsz)); \
			}                                                   \
			panic("%s failed: " #cond, __func__);               \
		}                                                           \
	} while (0)

#define CHECK_OR_ZERO_FILL(cond, dest, destsz) \
	CHECK_OR_FILL(cond, dest, destsz, '\0')

void memset_s(void *dest, rsize_t destsz, int ch, rsize_t count)
{
	CHECK_OR_FILL(dest != NULL, dest, destsz, ch);

	/* Check count <= destsz <= RSIZE_MAX. */
	CHECK_OR_FILL(destsz <= RSIZE_MAX, dest, destsz, ch);
	CHECK_OR_FILL(count <= destsz, dest, destsz, ch);

	memset(dest, ch, count);
}

void memcpy_s(void *dest, rsize_t destsz, const void *src, rsize_t count)
{
	uintptr_t d = (uintptr_t)dest;
	uintptr_t s = (uintptr_t)src;

	CHECK_OR_ZERO_FILL(dest != NULL, dest, destsz);
	CHECK_OR_ZERO_FILL(src != NULL, dest, destsz);

	/* Check count <= destsz <= RSIZE_MAX. */
	CHECK_OR_ZERO_FILL(destsz <= RSIZE_MAX, dest, destsz);
	CHECK_OR_ZERO_FILL(count <= destsz, dest, destsz);

	/*
	 * Buffer overlap test.
	 * case a) `d < s` implies `s >= d+count`
	 * case b) `d > s` implies `d >= s+count`
	 */
	CHECK_OR_ZERO_FILL(d != s, dest, destsz);
	CHECK_OR_ZERO_FILL(d < s || d >= (s + count), dest, destsz);
	CHECK_OR_ZERO_FILL(d > s || s >= (d + count), dest, destsz);

	memcpy(dest, src, count);
}

void memmove_s(void *dest, rsize_t destsz, const void *src, rsize_t count)
{
	CHECK_OR_ZERO_FILL(dest != NULL, dest, destsz);
	CHECK_OR_ZERO_FILL(src != NULL, dest, destsz);

	/* Check count <= destsz <= RSIZE_MAX. */
	CHECK_OR_ZERO_FILL(destsz <= RSIZE_MAX, dest, destsz);
	CHECK_OR_ZERO_FILL(count <= destsz, dest, destsz);

	memmove(dest, src, count);
}

/**
 * Returns the length of the null-terminated byte string `str`, examining at
 * most `strsz` bytes.
 *
 * If `str` is a NULL pointer, it returns zero.
 * If a NULL character is not found, it returns `strsz`.
 */
size_t strnlen_s(const char *str, size_t strsz)
{
	if (str == NULL) {
		return 0;
	}

	for (size_t i = 0; i < strsz; ++i) {
		if (str[i] == '\0') {
			return i;
		}
	}

	/* NULL character not found. */
	return strsz;
}
