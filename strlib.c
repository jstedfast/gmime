/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximain, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "strlib.h"
#include <stdio.h>
#include <ctype.h>


#ifndef HAVE_MEMCHR
/**
 * memchr: 
 * @s: memory area
 * @c: character
 * @n: memory area length
 *
 * Scans the first n bytes of the memory area pointed to by @s for the
 * character @c. The first byte to match @c (interpreted as an
 * unsigned character) stops the operation.
 *
 * Returns a pointer to the matching byte or NULL if the character
 * does not occur in the given memory area.
**/
void *
memchr (const void *s, int c, size_t n)
{
	unsigned char ch = c & 0xff;
	register unsigned char *p;
	unsigned char *q;
	
	for (p = (unsigned char *) s, q = p + n; p < q; p++)
		if (*p == ch)
			return (void *) p;
	
	return NULL;
}
#endif

#ifndef HAVE_MEMRCHR
/**
 * memrchr: 
 * @s: memory area
 * @c: character
 * @n: memory area length
 *
 * The memrchr() function scans in reverse the first n bytes of the
 * memory area pointed to by @s for the character @c. The first byte
 * to match @c (interpreted as an unsigned character) stops the
 * operation.
 *
 * Returns a pointer to the matching byte or NULL if the character
 * does not occur in the given memory area.
**/
void *
memrchr (const void *s, int c, size_t n)
{
	unsigned char ch = c & 0xff;
	register unsigned char *q;
	unsigned char *p;
	
	for (p = (unsigned char *) s, q = p + n - 1; q >= p; q--)
		if (*q == ch)
			return (void *) q;
	
	return NULL;
}
#endif

#define lowercase(c)            (isupper ((int) (c)) ? tolower ((int) (c)) : (int) (c))
#define bm_index(c, icase)      ((icase) ? lowercase (c) : (int) (c))
#define bm_equal(c1, c2, icase) ((icase) ? lowercase (c1) == lowercase (c2) : (c1) == (c2))

/* FIXME: this is just a guess... should really do some performace tests to get an accurate measure */
#define bm_optimal(hlen, nlen)  (((hlen) ? (hlen) > 20 : 1) && (nlen) > 10 ? 1 : 0)

static char *
__boyer_moore (const char *haystack, size_t haystacklen,
	       const char *needle, size_t needlelen, int icase)
{
	register unsigned char *hc_ptr, *nc_ptr;
	unsigned char *he_ptr, *ne_ptr, *h_ptr;
	size_t lookuptable[256], n;
	register int i;

#ifdef BOYER_MOORE_CHECKS
	/* we don't need to do these checks since memmem/strstr/etc do it already */
	
	/* if the haystack is shorter than the needle then we can't possibly match */
	if (haystacklen < needlelen)
		return NULL;
	
	/* instant match if the pattern buffer is 0-length */
	if (needlelen == 0)
		return (void *) haystack;
#endif /* BOYER_MOORE_CHECKS */
	
	/* set a pointer at the end of each string */
	ne_ptr = (unsigned char *) needle + needlelen - 1;
	he_ptr = (unsigned char *) haystack + haystacklen - 1;
	
	/* create our lookup table */
	for (i = 0; i < 256; i++)
		lookuptable[i] = needlelen;
	for (nc_ptr = (unsigned char *) needle; nc_ptr < ne_ptr; nc_ptr++)
		lookuptable[bm_index (*nc_ptr, icase)] = (size_t) (ne_ptr - nc_ptr);
	
	h_ptr = (unsigned char *) haystack;
	while (haystacklen >= needlelen) {
		hc_ptr = h_ptr + needlelen - 1;   /* set the haystack compare pointer */
		nc_ptr = ne_ptr;                  /* set the needle compare pointer */
		
		/* work our way backwards till they don't match */
		for (i = 0; nc_ptr > (unsigned char *) needle; nc_ptr--, hc_ptr--, i++)
			if (!bm_equal (*nc_ptr, *hc_ptr, icase))
				break;
		
		if (!bm_equal (*nc_ptr, *hc_ptr, icase)) {
			n = lookuptable[bm_index (*hc_ptr, icase)];
			if (n == needlelen && i)
				if (bm_equal (*ne_ptr, ((unsigned char *) needle)[0], icase))
					n--;
			h_ptr += n;
			haystacklen -= n;
		} else
			return (void *) h_ptr;
	}
	
	return NULL;
}

#ifndef HAVE_MEMMEM
/**
 * memmem:
 * @haysack: memory area to search
 * @haystacklen: memory area length
 * @needle: substring to search for
 * @needlelen: substring length
 *
 * Finds the start of the first occurence of the substring @needle of
 * length @needlelen within the memory area @haystack of length
 * @haystacklen.
 *
 * Returns a pointer to the beginning of the substring within the
 * memory area, or NULL if the substring is not found.
 **/
void *
memmem (const void *haystack, size_t haystacklen, const void *needle, size_t needlelen)
{
	register unsigned char *h, *n, *hc, *nc;
	unsigned char *he, *ne;
	
	if (haystacklen < needlelen) {
		return NULL;
	} else if (needlelen == 0) {
		return (void *) haystack;
	} else if (needlelen == 1) {
		return memchr (haystack, (int) ((unsigned char *) needle)[0], haystacklen);
	} else if (bm_optimal (haystacklen, needlelen)) {
		return (void *) __boyer_moore ((const char *) haystack, haystacklen,
					       (const char *) needle, needlelen, 0);
	}
	
	h = (unsigned char *) haystack;
	he = (unsigned char *) haystack + haystacklen - needlelen;
	n = (unsigned char *) needle;
	ne = (unsigned char *) needle + needlelen;
	
	while (h < he) {
		if (*h == *n) {
			for (hc = h + 1, nc = n + 1; nc < ne; hc++, nc++)
				if (*hc != *nc)
					break;
			
			if (nc == ne)
				return (void *) h;
		}
		
		h++;
	}
	
	return NULL;
}
#endif

#ifndef HAVE_STRLEN
/**
 * strlen:
 * @s: string
 *
 * Calculates the length of the string @s, not including the
 * terminating '\0' character.
 *
 * Returns the number of characters in @s.
 **/
size_t
strlen (const char *s)
{
	register const char *p;
	
	for (p = s; *p; p++);
	
	return p - s;
}
#endif

#ifndef HAVE_STRCPY
/**
 * strcpy:
 * @dest: destination string
 * @src: source string
 *
 * Copies the string pointed to by @src (including the terminating
 * '\0' character) to the character array pointed to by @dest. The
 * strings may not overlap and the destination string @dest must be
 * large enough to receive the copy.
 *
 * Returns a pointer to the resulting destination string @dest.
 **/
char *
strcpy (char *dest, const char *src)
{
	register const char *s = src;
	register char *d = dest;
	
	while (*s)
		*d++ = *s++;
	
	*d = '\0';
	
	return dest;
}
#endif

#ifndef HAVE_STRNCPY
/**
 * strncpy:
 * @dest: destination string
 * @src: source string
 * @n: number of bytes to copy
 *
 * Copies no more than the first @n characters of the string pointed
 * to by @src to the character array pointed to by @dest. Thus if
 * there is no null byte among the first @n bytes of @src, @dest will
 * not be null-terminated. The strings may not overlap and the
 * destination string @dest must be large enough to receive the copy.
 *
 * Returns a pointer to the resulting destination string @dest.
 **/
char *
strncpy (char *dest, const char *src, size_t n)
{
	register const char *s = src;
	register char *d = dest;
	
	while (*s && n)
		*d++ = *s++, n--;
	
	if (n)
		*d = '\0';
	
	return dest;
}
#endif

#ifndef HAVE_STRLCPY
/**
 * strlcpy:
 * @dest: destination string
 * @src: source string
 * @n: number of bytes to copy
 *
 * Copies the first @n characters of the string pointed to by @src to
 * the character array pointed to by @dest and null-terminates
 * @dest. The strings may not overlap and the destination string @dest
 * must be large enough to receive the copy.
 *
 * Returns the size of the resultant string, @dest.
 **/
size_t
strlcpy (char *dest, const char *src, size_t n)
{
	register const char *s = src;
	register char *d = dest;
	
	while (*s && n)
		*d++ = *s++, n--;
	
	*d = '\0';
	
	return d - dest;
}
#endif

#ifndef HAVE_STPCPY
/**
 * stpcpy:
 * @dest: destination string
 * @src: source string
 *
 * Copies the string pointed to by @src (including the terminating
 * '\0' character) to the character array pointed to by @dest. The
 * strings may not overlap and the destination string @dest must be
 * large enough to receive the copy.
 *
 * Returns a pointer to the end of the string @dest.
 **/
char *
stpcpy (char *dest, const char *src)
{
	register const char *s = src;
	register char *d = dest;
	
	while (*s)
		*d++ = *s++;
	
	*d = '\0';
	
	return d;
}
#endif

#ifndef HAVE_STRCAT
/**
 * strcat:
 * @dest: destination string
 * @src: source string
 *
 * Appends the @src string to the @dest string overwriting the '\0'
 * character at the end of @dest, and then adds a terminating '\0'
 * character. The strings may not overlap, and the destination string
 * dest must have enough space for the result.
 *
 * Returns a pointer to the resulting destination string @dest.
 **/
char *
strcat (char *dest, const char *src)
{
	register const char *s = src;
	register char *d = dest;
	
	while (*d)
		d++;
	
	while (*s)
		*d++ = *s++;
	
	*d = '\0';
	
	return dest;
}
#endif

#ifndef HAVE_STRNCAT
/**
 * strncat:
 * @dest: destination string
 * @src: source string
 * @n: number of bytes to append
 *
 * Appends at most the first @n characters of the @src string to the
 * @dest string overwriting the '\0' character at the end of
 * @dest. Thus if there is no null byte among the first @n bytes of
 * @src, @dest will not be null-terminated.  The strings may not
 * overlap, and the destination string dest must have enough space for
 * the result.
 *
 * Returns a pointer to the resulting destination string @dest.
 **/
char *
strncat (char *dest, const char *src, size_t n)
{
	register const char *s = src;
	register char *d = dest;
	
	while (*d)
		d++;
	
	while (*s && n)
		*d++ = *s++, n--;
	
	if (n)
		*d = '\0';
	
	return dest;
}
#endif

#ifndef HAVE_STRLCAT
/**
 * strlcat:
 * @dest: destination string
 * @src: source string
 * @n: number of bytes to append
 *
 * Appends at most the first @n characters of the @src string to the
 * @dest string overwriting the '\0' character at the end of @dest and
 * null-terminates the resulting @dest. The strings may not overlap,
 * and the destination string dest must have enough space for the
 * result.
 *
 * Returns the size of the resultant string, @dest.
 **/
size_t
strlcat (char *dest, const char *src, size_t n)
{
	register const char *s = src;
	register char *d = dest;
	
	while (*d)
		d++;
	
	while (*s && n)
		*d++ = *s++, n--;
	
	*d = '\0';
	
	return d - dest;
}
#endif

#ifndef HAVE_STRCHR
/**
 * strchr: 
 * @s: string
 * @c: character
 *
 * Scans the string pointed to by @s for the character @c. The first
 * byte to match @c (interpreted as an unsigned character) stops the
 * operation.
 *
 * Returns a pointer to the matching character or NULL if the
 * character does not occur in the given string.
**/
char *
strchr (const char *s, int c)
{
	unsigned char ch = c & 0xff;
	register unsigned char *p;
	
	for (p = (unsigned char *) s; *p; p++)
		if (*p == ch)
			return (char *) p;
	
	return NULL;
}
#endif

#ifndef HAVE_STRRCHR
/**
 * strrchr: 
 * @s: string
 * @c: character
 *
 * Scans the string pointed to by @s in reverse for the character
 * @c. The first byte to match @c (interpreted as an unsigned
 * character) stops the operation.
 *
 * Returns a pointer to the matching character or NULL if the
 * character does not occur in the given string.
**/
char *
strrchr (const char *s, int c)
{
	unsigned char ch = c & 0xff;
	register unsigned char *p;
	unsigned char *r = NULL;
	
	for (p = (unsigned char *) s; *p; p++)
		if (*p == ch)
			r = p;
	
	return (char *) r;
}
#endif

#ifndef HAVE_STRNSTR
/**
 * strnstr:
 * @haystack: string to search
 * @needle: substring to search for
 * @haystacklen: length of the haystack to search
 *
 * Finds the first occurence of the substring @needle within the
 * bounds of string @haystack.
 *
 * Returns a pointer to the beginning of the substring match within
 * @haystack, or NULL if the substring is not found.
 **/
char *
strnstr (const char *haystack, const char *needle, size_t haystacklen)
{
	register unsigned char *h, *n, *hc, *nc;
	size_t needlelen;
	
	needlelen = strlen (needle);
	
	if (haystacklen < needlelen) {
		return NULL;
	} else if (needlelen == 0) {
		return (char *) haystack;
	} else if (needlelen == 1) {
		return memchr (haystack, (int) ((unsigned char *) needle)[0], haystacklen);
	} else if (bm_optimal (haystacklen, needlelen)) {
		return (void *) __boyer_moore ((const char *) haystack, haystacklen,
					       (const char *) needle, needlelen, 0);
	}
	
	h = (unsigned char *) haystack;
	n = (unsigned char *) needle;
	
	while (haystacklen >= needlelen) {
		if (*h == *n) {
			for (hc = h + 1, nc = n + 1; *nc; hc++, nc++)
				if (*hc != *nc)
					break;
			
			if (!*nc)
				return (char *) h;
		}
		
		haystacklen--;
		h++;
	}
	
	return NULL;
}
#endif

#ifndef HAVE_STRSTR
/**
 * strstr:
 * @haystack: string to search
 * @needle: substring to search for
 *
 * Finds the first occurence of the substring @needle within the
 * string @haystack.
 *
 * Returns a pointer to the beginning of the substring match within
 * @haystack, or NULL if the substring is not found.
 **/
char *
strstr (const char *haystack, const char *needle)
{
	register unsigned char *h, *n, *hc, *nc;
	size_t needlelen;
	
	needlelen = strlen (needle);
	
	if (needlelen == 0) {
		return (char *) haystack;
	} else if (needlelen == 1) {
		return strchr (haystack, (int) ((unsigned char *) needle)[0]);
	} else if (bm_optimal (0, needlelen)) {
		return (void *) __boyer_moore ((const char *) haystack, strlen (haystack),
					       (const char *) needle, needlelen, 0);
	}
	
	h = (unsigned char *) haystack;
	n = (unsigned char *) needle;
	
	while (*(h + needlelen - 1)) {
		if (*h == *n) {
			for (hc = h + 1, nc = n + 1; *hc && *nc; hc++, nc++)
				if (*hc != *nc)
					break;
			
			if (!*nc)
				return (char *) h;
		}
		
		h++;
	}
	
	return NULL;
}
#endif

#ifndef HAVE_STRNCASESTR
/**
 * strncasestr:
 * @haystack: string to search
 * @needle: substring to search for
 * @haystacklen: length of the haystack to search
 *
 * Finds the first occurence of the substring @needle within the
 * bounds of string @haystack ignoring case.
 *
 * Returns a pointer to the beginning of the substring match within
 * @haystack, or NULL if the substring is not found.
 **/
char *
strncasestr (const char *haystack, const char *needle, size_t haystacklen)
{
	register unsigned char *h, *n, *hc, *nc;
	size_t needlelen;
	
	needlelen = strlen (needle);
	
	if (needlelen == 0) {
		return (char *) haystack;
	} else if (bm_optimal (haystacklen, needlelen)) {
		return (void *) __boyer_moore ((const char *) haystack, haystacklen,
					       (const char *) needle, needlelen, 1);
	}
	
	h = (unsigned char *) haystack;
	n = (unsigned char *) needle;
	
	while (haystacklen >= needlelen) {
		if (lowercase (*h) == lowercase (*n)) {
			for (hc = h + 1, nc = n + 1; *nc; hc++, nc++)
				if (lowercase (*hc) != lowercase (*nc))
					break;
			
			if (!*nc)
				return h;
		}
		
		haystacklen--;
		h++;
	}
	
	return NULL;
}
#endif

#ifndef HAVE_STRCASESTR
/**
 * strcasestr:
 * @haystack: string to search
 * @needle: substring to search for
 *
 * Finds the first occurence of the substring @needle within the
 * string @haystack ignoring case.
 *
 * Returns a pointer to the beginning of the substring match within
 * @haystack, or NULL if the substring is not found.
 **/
char *
strcasestr (const char *haystack, const char *needle)
{
	register unsigned char *h, *n, *hc, *nc;
	size_t needlelen;
	
	needlelen = strlen (needle);
	
	if (needlelen == 0) {
		return (char *) haystack;
	} else if (bm_optimal (0, needlelen)) {
		return (void *) __boyer_moore ((const char *) haystack, strlen (haystack),
					       (const char *) needle, needlelen, 1);
	}
	
	h = (unsigned char *) haystack;
	n = (unsigned char *) needle;
	
	while (*(h + needlelen - 1)) {
		if (lowercase (*h) == lowercase (*n)) {
			for (hc = h + 1, nc = n + 1; *hc && *nc; hc++, nc++)
				if (lowercase (*hc) != lowercase (*nc))
					break;
			
			if (!*nc)
				return (char *) h;
		}
		
		h++;
	}
	
	return NULL;
}
#endif

#ifndef HAVE_STRNCASECMP
/**
 * strncasecmp:
 * @s1: string 1
 * @s2: string 2
 * @n:
 *
 * Compares the first @n characters of the 2 strings, @s1 and @s2,
 * ignoring the case of the characters.
 *
 * Returns an integer less than, equal to, or greater than zero if @s1
 * is found, respectively, to be less than, to match, or to be greater
 * than @s2.
 **/
int
strncasecmp (const char *s1, const char *s2, size_t n)
{
	register const unsigned char *p1 = s1, *p2 = s2;
	const unsigned char *q1 = s1 + n;
	
	for ( ; p1 < q1; p1++, p2++)
		if (lowercase (*p1) != lowercase (*p2))
			lowercase (*p1) - lowercase (*p2);
	
	return 0;
}
#endif

#ifndef HAVE_STRCASECMP
/**
 * strncasecmp:
 * @s1: string 1
 * @s2: string 2
 *
 * Compares the 2 strings, @s1 and @s2, ignoring the case of the
 * characters.
 *
 * Returns an integer less than, equal to, or greater than zero if @s1
 * is found, respectively, to be less than, to match, or to be greater
 * than @s2.
 **/
int
strcasecmp (const char *s1, const char *s2)
{
	register const unsigned char *p1 = s1, *p2 = s2;
	
	for ( ; *p1; p1++, p2++)
		if (lowercase (*p1) != lowercase (*p2))
			break;
	
	return lowercase (*p1) - lowercase (*p2);
}
#endif



#ifdef TEST_SUITE
#define check(expr)           {                                                  \
	if (!(expr))                                                             \
		fprintf (stderr, "file %s: line %d (%s): check failed: (%s)\n",  \
			 __FILE__, __LINE__, __FUNCTION__, #expr);               \
}

static void
test_strchr (void)
{
	char one[256];
	
	(void) strcpy (one, "abcd");
	check (strchr (one, 'a') == one);
	check (strchr (one, 'b') == one + 1);
	check (strchr (one, 'c') == one + 2);
	check (strchr (one, 'd') == one + 3);
	check (strchr (one, 'z') == NULL);
}

static void
test_strrchr (void)
{
	char one[256];
	
	(void) strcpy (one, "abcd");
	check (strrchr (one, 'a') == one);
	check (strrchr (one, 'b') == one + 1);
	check (strrchr (one, 'c') == one + 2);
	check (strrchr (one, 'd') == one + 3);
	check (strrchr (one, 'z') == NULL);
	(void) strcpy (one, "abcdabcabcabcac");
	check (strrchr (one, 'c') == one + strlen (one) - 1);
	check (strrchr (one, 'a') == one + strlen (one) - 2);
	check (strrchr (one, 'b') == one + strlen (one) - 4);
}

static void
test_strstr (void)
{
	char one[256];
	
	check (strstr ("abcd", "z") == NULL);        /* Not found. */
	check (strstr ("abcd", "abx") == NULL);      /* Dead end. */
	(void) strcpy (one, "abcd");
	check (strstr (one, "c") == one + 2);        /* Basic test. */
	check (strstr (one, "bc") == one + 1);       /* Multichar. */
	check (strstr (one, "d") == one + 3);        /* End of string. */
	check (strstr (one, "cd") == one + 2);       /* Tail of string. */
	check (strstr (one, "abc") == one);          /* Beginning. */
	check (strstr (one, "abcd") == one);         /* Exact match. */
	check (strstr (one, "abcde") == NULL);       /* Too long. */
	check (strstr (one, "de") == NULL);          /* Past end. */
	check (strstr (one, "") == one);             /* Finding empty. */
	(void) strcpy (one, "ababa");
	check (strstr (one, "ba") == one + 1);       /* Finding first. */
	(void) strcpy (one, "");
	check (strstr (one, "b") == NULL);           /* Empty string. */
	check (strstr (one, "") == one);             /* Empty in empty string. */
	(void) strcpy (one, "bcbca");
	check (strstr (one, "bca") == one + 2);      /* False start. */
	(void) strcpy (one, "bbbcabbca");
	check (strstr (one, "bbca") == one + 1);     /* With overlap. */
}

static void
test_strnstr (void)
{
	char one[256];
	
	check (strnstr ("abcdefg", "g", 5) == NULL); /* Not found. */
}

static void
test_strcasestr (void)
{
	char one[256];
	
	check (strcasestr ("aBcd", "z") == NULL);        /* Not found. */
	check (strcasestr ("AbCd", "abx") == NULL);      /* Dead end. */
	(void) strcpy (one, "aBcD");
	check (strcasestr (one, "c") == one + 2);        /* Basic test. */
	check (strcasestr (one, "bc") == one + 1);       /* Multichar. */
	check (strcasestr (one, "d") == one + 3);        /* End of string. */
	check (strcasestr (one, "cd") == one + 2);       /* Tail of string. */
	check (strcasestr (one, "abc") == one);          /* Beginning. */
	check (strcasestr (one, "abcd") == one);         /* Exact match. */
	check (strcasestr (one, "abcde") == NULL);       /* Too long. */
	check (strcasestr (one, "de") == NULL);          /* Past end. */
	check (strcasestr (one, "") == one);             /* Finding empty. */
	(void) strcpy (one, "abABa");
	check (strcasestr (one, "ba") == one + 1);       /* Finding first. */
	(void) strcpy (one, "");
	check (strcasestr (one, "b") == NULL);           /* Empty string. */
	check (strcasestr (one, "") == one);             /* Empty in empty string. */
	(void) strcpy (one, "bcbca");
	check (strcasestr (one, "bca") == one + 2);      /* False start. */
	(void) strcpy (one, "bBbCabBcA");
	check (strcasestr (one, "bbca") == one + 1);     /* With overlap. */
}

static void
test_strcasecmp (void)
{
	check (strcasecmp ("", "") == 0);             /* Trivial case. */
	check (strcasecmp ("a", "a") == 0);           /* Identity. */
	check (strcasecmp ("aBc", "abc") == 0);       /* Multicharacter. */
	check (strcasecmp ("aBc", "abcd") < 0);       /* Length mismatches. */
	check (strcasecmp ("AbcD", "abc") > 0);
	check (strcasecmp ("aBcD", "abce") < 0);      /* Honest miscompares. */
	check (strcasecmp ("Abce", "abcd") > 0);
	check (strcasecmp ("A\203", "a") > 0);        /* Tricky if char signed. */
	check (strcasecmp ("A\203", "a\003") > 0);
	
	if (0) {
		char buf1[0x40], buf2[0x40];
		int i, j;
		for (i = 0; i < 0x10; i++) {
			for (j = 0; j < 0x10; j++) {
				int k;
				for (k = 0; k < 0x3f; k++) {
					buf1[j] = '0' ^ (k & 4);
					buf2[j] = '4' ^ (k & 4);
				}
				buf1[i] = buf1[0x3f] = 0;
				buf2[j] = buf2[0x3f] = 0;
				for (k = 0; k < 0xf; k++) {
					int cnum = 0x10+0x10*k+0x100*j+0x1000*i;
					
					check (strcasecmp (buf1+i,buf2+j) == 0);
					buf1[i+k] = 'A' + i + k;
					buf1[i+k+1] = 0;
					check (strcasecmp (buf1+i,buf2+j) > 0);
					check (strcasecmp (buf2+j,buf1+i) < 0);
					buf2[j+k] = 'B' + i + k;
					buf2[j+k+1] = 0;
					check (strcasecmp (buf1+i,buf2+j) < 0);
					check (strcasecmp (buf2+j,buf1+i) > 0);
					buf2[j+k] = 'A' + i + k;
					buf1[i] = 'A' + i + 0x80;
					check (strcasecmp (buf1+i,buf2+j) > 0);
					check (strcasecmp (buf2+j,buf1+i) < 0);
					buf1[i] = 'A' + i;
				}
			}
		}
	}
}

int main (int argc, char **argv)
{
	test_strchr ();
	test_strrchr ();
	test_strstr ();
	test_strnstr ();
	test_strcasestr ();
	test_strcasecmp ();
	
	return 0;
}
#endif
