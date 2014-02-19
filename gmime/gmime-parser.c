/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "gmime-parser.h"

#include "gmime-table-private.h"
#include "gmime-message-part.h"
#include "gmime-parse-utils.h"
#include "gmime-stream-mem.h"
#include "gmime-multipart.h"
#include "gmime-common.h"
#include "gmime-part.h"

#if GLIB_MAJOR_VERSION > 2 || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 14)
#define HAVE_GLIB_REGEX
#elif defined (HAVE_REGEX_H)
#include <regex.h>
#endif

#ifdef ENABLE_WARNINGS
#define w(x) x
#else
#define w(x)
#endif /* ENABLE_WARNINGS */

#define d(x)


/**
 * SECTION: gmime-parser
 * @title: GMimeParser
 * @short_description: Message and MIME part parser
 * @see_also:
 *
 * A #GMimeParser parses a stream into a #GMimeMessage or other
 * #GMimeObject and can also handle parsing MBox formatted streams
 * into multiple #GMimeMessage objects.
 **/

typedef struct _boundary_stack {
	struct _boundary_stack *parent;
	char *boundary;
	size_t boundarylen;
	size_t boundarylenfinal;
	size_t boundarylenmax;
	gint64 content_end;
} BoundaryStack;

typedef struct _header_raw {
	struct _header_raw *next;
	char *name, *value;
	gint64 offset;
} HeaderRaw;

typedef struct _content_type {
	char *type, *subtype;
	gboolean exists;
} ContentType;

extern void _g_mime_object_set_content_type (GMimeObject *object, GMimeContentType *content_type);

static void g_mime_parser_class_init (GMimeParserClass *klass);
static void g_mime_parser_init (GMimeParser *parser, GMimeParserClass *klass);
static void g_mime_parser_finalize (GObject *object);

static void parser_init (GMimeParser *parser, GMimeStream *stream);
static void parser_close (GMimeParser *parser);

static GMimeObject *parser_construct_leaf_part (GMimeParser *parser, ContentType *content_type,
						gboolean toplevel, int *found);
static GMimeObject *parser_construct_multipart (GMimeParser *parser, ContentType *content_type,
						gboolean toplevel, int *found);

static GObjectClass *parent_class = NULL;

/* size of read buffer */
#define SCAN_BUF 4096

/* headroom guaranteed to be before each read buffer */
#define SCAN_HEAD 128

/* conservative growth sizes */
#define HEADER_INIT_SIZE 128
#define HEADER_RAW_INIT_SIZE 1024


enum {
	GMIME_PARSER_STATE_ERROR = -1,
	GMIME_PARSER_STATE_INIT,
	GMIME_PARSER_STATE_FROM,
	GMIME_PARSER_STATE_MESSAGE_HEADERS,
	GMIME_PARSER_STATE_HEADERS,
	GMIME_PARSER_STATE_HEADERS_END,
	GMIME_PARSER_STATE_CONTENT,
	GMIME_PARSER_STATE_COMPLETE,
};

struct _GMimeParserPrivate {
	GMimeStream *stream;
	
	gint64 offset;
	
	/* i/o buffers */
	char realbuf[SCAN_HEAD + SCAN_BUF + 4];
	char *inbuf;
	char *inptr;
	char *inend;
	
	gint64 from_offset;
	GByteArray *from_line;
	
	GMimeParserHeaderRegexFunc header_cb;
	gpointer user_data;
	
#if defined (HAVE_GLIB_REGEX)
	GRegex *regex;
#elif defined (HAVE_REGEX_H)
	regex_t regex;
#endif
	
	/* header buffer */
	char *headerbuf;
	char *headerptr;
	size_t headerleft;
	
	/* raw header buffer */
	char *rawbuf;
	char *rawptr;
	size_t rawleft;
	
	/* current message headerblock offsets */
	gint64 message_headers_begin;
	gint64 message_headers_end;
	
	/* current mime-part headerblock offsets */
	gint64 headers_begin;
	gint64 headers_end;
	
	/* current header field offset */
	gint64 header_offset;
	
	short int state;
	
	unsigned short int unused:10;
	unsigned short int midline:1;
	unsigned short int seekable:1;
	unsigned short int scan_from:1;
	unsigned short int have_regex:1;
	unsigned short int persist_stream:1;
	unsigned short int respect_content_length:1;
	
	HeaderRaw *headers;
	
	BoundaryStack *bounds;
};

static const char MBOX_BOUNDARY[6] = "From ";
#define MBOX_BOUNDARY_LEN 5

static void
parser_push_boundary (GMimeParser *parser, const char *boundary)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	BoundaryStack *s;
	size_t max;
	
	max = priv->bounds ? priv->bounds->boundarylenmax : 0;
	
	s = g_slice_new (BoundaryStack);
	s->parent = priv->bounds;
	priv->bounds = s;
	
	if (boundary == MBOX_BOUNDARY) {
		s->boundary = g_strdup (boundary);
		s->boundarylen = MBOX_BOUNDARY_LEN;
		s->boundarylenfinal = MBOX_BOUNDARY_LEN;
	} else {
		s->boundary = g_strdup_printf ("--%s--", boundary);
		s->boundarylen = strlen (boundary) + 2;
		s->boundarylenfinal = s->boundarylen + 2;
	}
	
	s->boundarylenmax = MAX (s->boundarylenfinal, max);
	
	s->content_end = -1;
}

static void
parser_pop_boundary (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	BoundaryStack *s;
	
	if (!priv->bounds) {
		d(g_warning ("boundary stack underflow"));
		return;
	}
	
	s = priv->bounds;
	priv->bounds = priv->bounds->parent;
	
	g_free (s->boundary);
	
	g_slice_free (BoundaryStack, s);
}

static const char *
header_raw_find (HeaderRaw *headers, const char *name, gint64 *offset)
{
	HeaderRaw *header = headers;
	
	while (header) {
		if (!g_ascii_strcasecmp (header->name, name)) {
			if (offset)
				*offset = header->offset;
			return header->value;
		}
		
		header = header->next;
	}
	
	return NULL;
}

static void
header_raw_clear (HeaderRaw **headers)
{
	HeaderRaw *header, *next;
	
	header = *headers;
	while (header) {
		next = header->next;
		g_free (header->name);
		g_free (header->value);
		
		g_slice_free (HeaderRaw, header);
		
		header = next;
	}
	
	*headers = NULL;
}

GType
g_mime_parser_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GMimeParserClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) g_mime_parser_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GMimeParser),
			0,    /* n_preallocs */
			(GInstanceInitFunc) g_mime_parser_init,
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GMimeParser", &info, 0);
	}
	
	return type;
}


static void
g_mime_parser_class_init (GMimeParserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (G_TYPE_OBJECT);
	
	object_class->finalize = g_mime_parser_finalize;
}

static void
g_mime_parser_init (GMimeParser *parser, GMimeParserClass *klass)
{
	parser->priv = g_new (struct _GMimeParserPrivate, 1);
	parser->priv->respect_content_length = FALSE;
	parser->priv->persist_stream = TRUE;
	parser->priv->have_regex = FALSE;
	parser->priv->scan_from = FALSE;
	
#if defined (HAVE_GLIB_REGEX)
	parser->priv->regex = NULL;
#endif
	
	parser_init (parser, NULL);
}

static void
g_mime_parser_finalize (GObject *object)
{
	GMimeParser *parser = (GMimeParser *) object;
	
	parser_close (parser);
	
#if defined (HAVE_GLIB_REGEX)
	if (parser->priv->regex)
		g_regex_unref (parser->priv->regex);
#elif defined (HAVE_REGEX_H)
	if (parser->priv->have_regex)
		regfree (&parser->priv->regex);
#endif
	
	g_free (parser->priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
parser_init (GMimeParser *parser, GMimeStream *stream)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	gint64 offset = -1;
	
	if (stream) {
		g_object_ref (stream);
		offset = g_mime_stream_tell (stream);
	}
	
	priv->state = GMIME_PARSER_STATE_INIT;
	
	priv->stream = stream;
	
	priv->offset = offset;
	
	priv->inbuf = priv->realbuf + SCAN_HEAD;
	priv->inptr = priv->inbuf;
	priv->inend = priv->inbuf;
	
	priv->from_offset = -1;
	priv->from_line = g_byte_array_new ();
	
	priv->headerbuf = g_malloc (HEADER_INIT_SIZE);
	priv->headerleft = HEADER_INIT_SIZE - 1;
	priv->headerptr = priv->headerbuf;
	
	if (offset == -1 || !priv->persist_stream) {
		priv->rawbuf = g_malloc (HEADER_RAW_INIT_SIZE);
		priv->rawleft = HEADER_RAW_INIT_SIZE - 1;
		priv->rawptr = priv->rawbuf;
	} else {
		priv->rawbuf = NULL;
		priv->rawptr = NULL;
		priv->rawleft = 0;
	}
	
	priv->message_headers_begin = -1;
	priv->message_headers_end = -1;
	
	priv->headers_begin = -1;
	priv->headers_end = -1;
	
	priv->header_offset = -1;
	
	priv->midline = FALSE;
	priv->seekable = offset != -1;
	
	priv->headers = NULL;
	
	priv->bounds = NULL;
}

static void
parser_close (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	
	if (priv->stream)
		g_object_unref (priv->stream);
	
	g_byte_array_free (priv->from_line, TRUE);
	
	g_free (priv->headerbuf);
	g_free (priv->rawbuf);
	
	header_raw_clear (&priv->headers);
	
	while (priv->bounds)
		parser_pop_boundary (parser);
}


/**
 * g_mime_parser_new:
 *
 * Creates a new parser object.
 *
 * Returns: a new parser object.
 **/
GMimeParser *
g_mime_parser_new (void)
{
	return g_object_newv (GMIME_TYPE_PARSER, 0, NULL);
}


/**
 * g_mime_parser_new_with_stream:
 * @stream: raw message or part stream
 *
 * Creates a new parser object preset to parse @stream.
 *
 * Returns: a new parser object.
 **/
GMimeParser *
g_mime_parser_new_with_stream (GMimeStream *stream)
{
	GMimeParser *parser;
	
	parser = g_mime_parser_new ();
	g_mime_parser_init_with_stream (parser, stream);
	
	return parser;
}


/**
 * g_mime_parser_init_with_stream:
 * @parser: a #GMimeParser context
 * @stream: raw message or part stream
 *
 * Initializes @parser to use @stream.
 *
 * WARNING: Initializing a parser with a stream is comparable to
 * selling your soul (@stream) to the devil (@parser). You are
 * basically giving the parser complete control of the stream, this
 * means that you had better not touch the stream so long as the
 * parser is still using it. This means no reading, writing, seeking,
 * or resetting of the stream. Anything that will/could change the
 * current stream's offset is PROHIBITED.
 *
 * It is also recommended that you not use g_mime_stream_tell()
 * because it will not necessarily give you the current @parser offset
 * since @parser handles its own internal read-ahead buffer. Instead,
 * it is recommended that you use g_mime_parser_tell() if you have a
 * reason to need the current offset of the @parser.
 **/
void
g_mime_parser_init_with_stream (GMimeParser *parser, GMimeStream *stream)
{
	g_return_if_fail (GMIME_IS_PARSER (parser));
	g_return_if_fail (GMIME_IS_STREAM (stream));
	
	parser_close (parser);
	parser_init (parser, stream);
}


/**
 * g_mime_parser_get_persist_stream:
 * @parser: a #GMimeParser context
 *
 * Gets whether or not the underlying stream is persistent.
 *
 * Returns: %TRUE if the @parser will leave the content on disk or
 * %FALSE if it will load the content into memory.
 **/
gboolean
g_mime_parser_get_persist_stream (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), FALSE);
	
	return (parser->priv->persist_stream && parser->priv->seekable);
}


/**
 * g_mime_parser_set_persist_stream:
 * @parser: a #GMimeParser context
 * @persist: persist attribute
 *
 * Sets whether or not the @parser's underlying stream is persistent.
 *
 * If @persist is %TRUE, the @parser will attempt to construct
 * messages/parts whose content will remain on disk rather than being
 * loaded into memory so as to reduce memory usage. This is the default.
 *
 * If @persist is %FALSE, the @parser will always load message content
 * into memory.
 *
 * Note: This attribute only serves as a hint to the @parser. If the
 * underlying stream does not support seeking, then this attribute
 * will be ignored.
 **/
void
g_mime_parser_set_persist_stream (GMimeParser *parser, gboolean persist)
{
	struct _GMimeParserPrivate *priv;
	
	g_return_if_fail (GMIME_IS_PARSER (parser));
	
	priv = parser->priv;
	
	if (priv->persist_stream == persist)
		return;
	
	if (persist) {
		priv->persist_stream = TRUE;
		
		if (priv->seekable && !priv->rawbuf) {
			priv->rawbuf = g_malloc (HEADER_RAW_INIT_SIZE);
			priv->rawleft = HEADER_RAW_INIT_SIZE - 1;
			priv->rawptr = priv->rawbuf;
		}
	} else {
		priv->persist_stream = FALSE;
		
		if (priv->rawbuf) {
			g_free (priv->rawbuf);
			priv->rawbuf = NULL;
			priv->rawptr = NULL;
			priv->rawleft = 0;
		}
	}
}


/**
 * g_mime_parser_get_scan_from:
 * @parser: a #GMimeParser context
 *
 * Gets whether or not @parser is set to scan mbox-style From-lines.
 *
 * Returns: whether or not @parser is set to scan mbox-style
 * From-lines.
 **/
gboolean
g_mime_parser_get_scan_from (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), FALSE);
	
	return parser->priv->scan_from;
}


/**
 * g_mime_parser_set_scan_from:
 * @parser: a #GMimeParser context
 * @scan_from: %TRUE to scan From-lines or %FALSE otherwise
 *
 * Sets whether or not @parser should scan mbox-style From-lines.
 **/
void
g_mime_parser_set_scan_from (GMimeParser *parser, gboolean scan_from)
{
	g_return_if_fail (GMIME_IS_PARSER (parser));
	
	parser->priv->scan_from = scan_from ? 1 : 0;
}


/**
 * g_mime_parser_get_respect_content_length:
 * @parser: a #GMimeParser context
 *
 * Gets whether or not @parser is set to use Content-Length for
 * determining the offset of the end of the message.
 *
 * Returns: whether or not @parser is set to use Content-Length for
 * determining the offset of the end of the message.
 **/
gboolean
g_mime_parser_get_respect_content_length (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), FALSE);
	
	return parser->priv->respect_content_length;
}


/**
 * g_mime_parser_set_respect_content_length:
 * @parser: a #GMimeParser context
 * @respect_content_length: %TRUE if the parser should use Content-Length headers or %FALSE otherwise.
 *
 * Sets whether or not @parser should respect Content-Length headers
 * when deciding where to look for the start of the next message. Only
 * used when the parser is also set to scan for From-lines.
 *
 * Most notably useful when parsing broken Solaris mbox files (See
 * http://www.jwz.org/doc/content-length.html for details).
 **/
void
g_mime_parser_set_respect_content_length (GMimeParser *parser, gboolean respect_content_length)
{
	g_return_if_fail (GMIME_IS_PARSER (parser));
	
	parser->priv->respect_content_length = respect_content_length ? 1 : 0;
}


/**
 * g_mime_parser_set_header_regex:
 * @parser: a #GMimeParser context
 * @regex: regular expression
 * @header_cb: callback function
 * @user_data: user data
 *
 * Sets the regular expression pattern @regex on @parser. Whenever a
 * header matching the pattern @regex is parsed, @header_cb is called
 * with @user_data as the user_data argument.
 *
 * If @regex is %NULL, then the previously registered regex callback
 * is unregistered and no new callback is set.
 **/
void
g_mime_parser_set_header_regex (GMimeParser *parser, const char *regex,
				GMimeParserHeaderRegexFunc header_cb, gpointer user_data)
{
	struct _GMimeParserPrivate *priv;
	
	g_return_if_fail (GMIME_IS_PARSER (parser));
	
	priv = parser->priv;
	
#if defined (HAVE_GLIB_REGEX)
	if (priv->regex) {
		g_regex_unref (priv->regex);
		priv->regex = NULL;
	}
#elif defined (HAVE_REGEX_H)
	if (priv->have_regex) {
		regfree (&priv->regex);
		priv->have_regex = FALSE;
	}
#endif
	
	if (!regex || !header_cb)
		return;
	
	priv->header_cb = header_cb;
	priv->user_data = user_data;
	
#if defined (HAVE_GLIB_REGEX)
	priv->regex = g_regex_new (regex, G_REGEX_RAW | G_REGEX_EXTENDED | G_REGEX_CASELESS, 0, NULL);
#elif defined (HAVE_REGEX_H)
	priv->have_regex = !regcomp (&priv->regex, regex, REG_EXTENDED | REG_ICASE | REG_NOSUB);
#endif
}


static ssize_t
parser_fill (GMimeParser *parser, size_t atleast)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	char *inbuf, *inptr, *inend;
	ssize_t nread;
	size_t inlen;
	
	inbuf = priv->inbuf;
	inptr = priv->inptr;
	inend = priv->inend;
	inlen = inend - inptr;
	
	g_assert (inptr <= inend);
	
	if (inlen > atleast)
		return inlen;
	
	/* attempt to align 'inend' with realbuf + SCAN_HEAD */
	if (inptr >= inbuf) {
		inbuf -= inlen < SCAN_HEAD ? inlen : SCAN_HEAD;
		memmove (inbuf, inptr, inlen);
		inptr = inbuf;
		inbuf += inlen;
	} else if (inptr > priv->realbuf) {
		size_t shift;
		
		shift = MIN (inptr - priv->realbuf, inend - inbuf);
		memmove (inptr - shift, inptr, inlen);
		inptr -= shift;
		inbuf = inptr + inlen;
	} else {
		/* we can't shift... */
		inbuf = inend;
	}
	
	priv->inptr = inptr;
	priv->inend = inbuf;
	inend = priv->realbuf + SCAN_HEAD + SCAN_BUF;
	
	if ((nread = g_mime_stream_read (priv->stream, inbuf, inend - inbuf)) > 0) {
		priv->offset += nread;
		priv->inend += nread;
	}
	
	return (ssize_t) (priv->inend - priv->inptr);
}


static gint64
parser_offset (struct _GMimeParserPrivate *priv, const char *inptr)
{
	if (priv->offset == -1)
		return -1;
	
	if (!inptr)
		inptr = priv->inptr;
	
	return (priv->offset - (priv->inend - inptr));
}


/**
 * g_mime_parser_tell:
 * @parser: a #GMimeParser context
 *
 * Gets the current stream offset from the parser's internal stream.
 *
 * Returns: the current stream offset from the parser's internal stream
 * or %-1 on error.
 **/
gint64
g_mime_parser_tell (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);
	g_return_val_if_fail (GMIME_IS_STREAM (parser->priv->stream), -1);
	
	return parser_offset (parser->priv, NULL);
}


/**
 * g_mime_parser_eos:
 * @parser: a #GMimeParser context
 *
 * Tests the end-of-stream indicator for @parser's internal stream.
 *
 * Returns: %TRUE on EOS or %FALSE otherwise.
 **/
gboolean
g_mime_parser_eos (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv;
	
	g_return_val_if_fail (GMIME_IS_STREAM (parser->priv->stream), TRUE);
	
	priv = parser->priv;
	return g_mime_stream_eos (priv->stream) && priv->inptr == priv->inend;
}

static int
parser_step_from (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	register char *inptr;
	char *start, *inend;
	ssize_t left = 0;
	size_t len;
	
	g_byte_array_set_size (priv->from_line, 0);
	
	inptr = priv->inptr;
	
	g_assert (inptr <= priv->inend);
	
	do {
	refill:
		if (parser_fill (parser, MAX (SCAN_HEAD, left)) <= left) {
			/* failed to find a From line; EOF reached */
			priv->state = GMIME_PARSER_STATE_ERROR;
			priv->inptr = priv->inend;
			return -1;
		}
		
		inptr = priv->inptr;
		inend = priv->inend;
		*inend = '\n';
		
		while (inptr < inend) {
			start = inptr;
			while (*inptr != '\n')
				inptr++;
			
			if (inptr + 1 >= inend) {
				/* we don't have enough data; if we can't get more we have to bail */
				left = (ssize_t) (inend - start);
				priv->inptr = start;
				goto refill;
			}
			
			len = (size_t) (inptr - start);
			inptr++;
			
			if (len >= 5 && !strncmp (start, "From ", 5)) {
				priv->from_offset = parser_offset (priv, start);
				g_byte_array_append (priv->from_line, (unsigned char *) start, len);
				goto got_from;
			}
		}
		
		priv->inptr = inptr;
		left = 0;
	} while (1);
	
 got_from:
	
	priv->state = GMIME_PARSER_STATE_MESSAGE_HEADERS;
	
	priv->inptr = inptr;
	
	return 0;
}

#ifdef ALLOC_NEAREST_POW2
static inline size_t
nearest_pow (size_t num)
{
	size_t n;
	
	if (num == 0)
		return 0;
	
	n = num - 1;
#if defined (__GNUC__) && defined (__i386__)
	__asm__("bsrl %1,%0\n\t"
		"jnz 1f\n\t"
		"movl $-1,%0\n"
		"1:" : "=r" (n) : "rm" (n));
	n = (1 << (n + 1));
#else
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
#endif
	
	return n;
}

#define next_alloc_size(n) nearest_pow (n)
#else
static inline size_t
next_alloc_size (size_t n)
{
	return (n + 63) & ~63;
}
#endif

#define header_append(priv, start, len) G_STMT_START {                    \
	if (priv->headerleft <= len) {                                    \
		size_t hlen, hoff;                                        \
		                                                          \
		hoff = priv->headerptr - priv->headerbuf;                 \
		hlen = next_alloc_size (hoff + len + 1);                  \
		                                                          \
		priv->headerbuf = g_realloc (priv->headerbuf, hlen);      \
		priv->headerptr = priv->headerbuf + hoff;                 \
		priv->headerleft = (hlen - 1) - hoff;                     \
	}                                                                 \
	                                                                  \
	memcpy (priv->headerptr, start, len);                             \
	priv->headerptr += len;                                           \
	priv->headerleft -= len;                                          \
} G_STMT_END

#define raw_header_append(priv, start, len) G_STMT_START {                \
	if (priv->rawbuf) {                                               \
		if (priv->rawleft <= len) {                               \
			size_t hlen, hoff;                                \
			                                                  \
			hoff = priv->rawptr - priv->rawbuf;               \
			hlen = next_alloc_size (hoff + len + 1);          \
			                                                  \
			priv->rawbuf = g_realloc (priv->rawbuf, hlen);    \
			priv->rawptr = priv->rawbuf + hoff;               \
			priv->rawleft = (hlen - 1) - hoff;                \
		}                                                         \
		                                                          \
		memcpy (priv->rawptr, start, len);                        \
		priv->rawptr += len;                                      \
		priv->rawleft -= len;                                     \
	}                                                                 \
} G_STMT_END

#define raw_header_reset(priv) G_STMT_START {                             \
	if (priv->rawbuf) {                                               \
		priv->rawleft += priv->rawptr - priv->rawbuf;             \
		priv->rawptr = priv->rawbuf;                              \
	}                                                                 \
} G_STMT_END

static void
header_parse (GMimeParser *parser, HeaderRaw **tail)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	register char *inptr;
	HeaderRaw *header;
	
	*priv->headerptr = '\0';
	inptr = priv->headerbuf;
	while (*inptr && *inptr != ':' && !is_type (*inptr, IS_SPACE | IS_CTRL))
		inptr++;
	
	if (*inptr != ':') {
		/* ignore invalid headers */
		w(g_warning ("Invalid header at %lld: '%s'",
			     (long long) priv->header_offset,
			     priv->headerbuf));
		
		priv->headerleft += priv->headerptr - priv->headerbuf;
		priv->headerptr = priv->headerbuf;
		
		return;
	}
	
	header = g_slice_new (HeaderRaw);
	header->next = NULL;
	
	header->name = g_strndup (priv->headerbuf, (size_t) (inptr - priv->headerbuf));
	header->value = g_mime_strdup_trim (inptr + 1);
	
	header->offset = priv->header_offset;
	
	(*tail)->next = header;
	*tail = header;
	
	priv->headerleft += priv->headerptr - priv->headerbuf;
	priv->headerptr = priv->headerbuf;
	
#if defined (HAVE_GLIB_REGEX)
	if (priv->regex && g_regex_match (priv->regex, header->name, 0, NULL))
		priv->header_cb (parser, header->name, header->value,
				 header->offset, priv->user_data);
#elif defined (HAVE_REGEX_H)
	if (priv->have_regex &&
	    !regexec (&priv->header_regex, header->name, 0, NULL, 0))
		priv->header_cb (parser, header->name, header->value,
				 header->offset, priv->user_data);
#endif
}

enum {
	SUBJECT = 1 << 0,
	FROM    = 1 << 1,
	DATE    = 1 << 2,
	TO      = 1 << 3,
	CC      = 1 << 4
};

static gboolean
has_message_headers (HeaderRaw *headers)
{
	unsigned int found = 0;
	HeaderRaw *header;
	
	header = headers;
	while (header != NULL) {
		if (!g_ascii_strcasecmp (header->name, "Subject"))
			found |= SUBJECT;
		else if (!g_ascii_strcasecmp (header->name, "From"))
			found |= FROM;
		else if (!g_ascii_strcasecmp (header->name, "Date"))
			found |= DATE;
		else if (!g_ascii_strcasecmp (header->name, "To"))
			found |= TO;
		else if (!g_ascii_strcasecmp (header->name, "Cc"))
			found |= CC;
		
		header = header->next;
	}
	
	return found != 0;
}

static gboolean
has_content_headers (HeaderRaw *headers)
{
	HeaderRaw *header;
	
	header = headers;
	while (header != NULL) {
		if (!g_ascii_strcasecmp (header->name, "Content-Type"))
			return TRUE;
		
		header = header->next;
	}
	
	return FALSE;
}

static int
parser_step_headers (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	gboolean eoln, valid = TRUE, fieldname = TRUE;
	gboolean continuation = FALSE;
	register char *inptr;
	char *start, *inend;
	ssize_t left = 0;
	HeaderRaw *tail;
	size_t len;
	
	priv->midline = FALSE;
	raw_header_reset (priv);
	header_raw_clear (&priv->headers);
	tail = (HeaderRaw *) &priv->headers;
	priv->headers_begin = parser_offset (priv, NULL);
	priv->header_offset = priv->headers_begin;
	
	inptr = priv->inptr;
	inend = priv->inend;
	
	do {
	refill:
		if (parser_fill (parser, MAX (SCAN_HEAD, left)) <= left)
			break;
		
		inptr = priv->inptr;
		inend = priv->inend;
		/* Note: see optimization comment [1] */
		*inend = '\n';
		
		g_assert (inptr <= inend);
		
		while (inptr < inend) {
			start = inptr;
			
			/* if we are scanning a new line, check for a folded header */
			if (!priv->midline && continuation && (*inptr != ' ' && *inptr != '\t')) {
				header_parse (parser, &tail);
				priv->header_offset = parser_offset (priv, inptr);
				continuation = FALSE;
				fieldname = TRUE;
				valid = TRUE;
			}
			
			eoln = inptr[0] == '\n' || (inptr[0] == '\r' && inptr[1] == '\n');
			if (fieldname && !eoln) {
				/* scan and validate the field name */
				if (*inptr != ':') {
					/* Note: see optimization comment [1] */
					*inend = ':';
					
					while (*inptr != ':') {
						if (is_type (*inptr, IS_SPACE | IS_CTRL)) {
							valid = FALSE;
							break;
						}
						
						inptr++;
					}
					
					if (inptr == inend) {
						/* don't have the full field name */
						left = (ssize_t) (inend - start);
						priv->inptr = start;
						goto refill;
					}
					
					/* Note: see optimization comment [1] */
					*inend = '\n';
				} else {
					valid = FALSE;
				}
				
				if (!valid) {
					if (priv->scan_from && (inptr - start) == 4
					    && !strncmp (start, "From ", 5))
						goto next_message;
					
					if (priv->headers != NULL) {
						if (priv->state == GMIME_PARSER_STATE_MESSAGE_HEADERS) {
							if (has_message_headers (priv->headers)) {
								/* probably the start of the content,
								 * a broken mailer didn't terminate the
								 * headers with an empty line. *sigh* */
								goto content_start;
							}
						} else if (has_content_headers (priv->headers)) {
							/* probably the start of the content,
							 * a broken mailer didn't terminate the
							 * headers with an empty line. *sigh* */
							goto content_start;
						}
					} else if (priv->state == GMIME_PARSER_STATE_MESSAGE_HEADERS) {
						/* Be a little more strict when scanning toplevel message
						 * headers, but remain lenient with From-lines. */
						if ((inptr - start) != 4 || strncmp (start, "From ", 5) != 0) {
							priv->state = GMIME_PARSER_STATE_ERROR;
							return -1;
						}
					}
				}
			}
			
			fieldname = FALSE;
			
			/* Note: see optimization comment [1] */
			while (*inptr != '\n')
				inptr++;
			
			len = (size_t) (inptr - start);
			
			if (inptr == inend) {
				/* we don't have the full line, save
				 * what we have and refill our
				 * buffer... */
				if (inptr > start && inptr[-1] == '\r') {
					inptr--;
					len--;
				}
				
				raw_header_append (priv, start, len);
				header_append (priv, start, len);
				left = (ssize_t) (inend - inptr);
				priv->midline = TRUE;
				priv->inptr = inptr;
				goto refill;
			}
			
			raw_header_append (priv, start, len);
			
			if (inptr > start && inptr[-1] == '\r')
				len--;
			
			/* check to see if we've reached the end of the headers */
			if (!priv->midline && len == 0)
				goto headers_end;
			
			header_append (priv, start, len);
			
			/* inptr has to be less than inend - 1 */
			raw_header_append (priv, "\n", 1);
			priv->midline = FALSE;
			continuation = TRUE;
			inptr++;
		}
		
		left = (ssize_t) (inend - inptr);
		priv->inptr = inptr;
	} while (1);
	
	inptr = priv->inptr;
	inend = priv->inend;
	start = inptr;
	
	len = (size_t) (inend - inptr);
	header_append (priv, inptr, len);
	raw_header_append (priv, inptr, len);
	
 headers_end:
	
	if (priv->headerptr > priv->headerbuf)
		header_parse (parser, &tail);
	
	priv->headers_end = parser_offset (priv, start);
	priv->state = GMIME_PARSER_STATE_HEADERS_END;
	if (priv->rawbuf)
		*priv->rawptr = '\0';
	priv->inptr = inptr;
	
	return 0;
	
 next_message:
	
	priv->headers_end = parser_offset (priv, start);
	priv->state = GMIME_PARSER_STATE_COMPLETE;
	if (priv->rawbuf)
		*priv->rawptr = '\0';
	priv->inptr = start;
	
	return 0;
	
 content_start:
	
	priv->headers_end = parser_offset (priv, start);
	priv->state = GMIME_PARSER_STATE_CONTENT;
	if (priv->rawbuf)
		*priv->rawptr = '\0';
	priv->inptr = start;
	
	return 0;
}

static void
content_type_destroy (ContentType *content_type)
{
	g_free (content_type->subtype);
	g_free (content_type->type);
	
	g_slice_free (ContentType, content_type);
}

static gboolean
content_type_is_type (ContentType *content_type, const char *type, const char *subtype)
{
	if (!strcmp (type, "*") || !g_ascii_strcasecmp (content_type->type, type)) {
		if (!strcmp (subtype, "*")) {
			/* special case */
			return TRUE;
		}
		
		if (!g_ascii_strcasecmp (content_type->subtype, subtype))
			return TRUE;
	}
	
	return FALSE;
}

static ContentType *
parser_content_type (GMimeParser *parser, GMimeContentType *parent)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	ContentType *content_type;
	const char *value;
	
	content_type = g_slice_new (ContentType);
	
	if (!(value = header_raw_find (priv->headers, "Content-Type", NULL)) ||
	    !g_mime_parse_content_type (&value, &content_type->type, &content_type->subtype)) {
		if (parent != NULL && g_mime_content_type_is_type (parent, "multipart", "digest")) {
			content_type->type = g_strdup ("message");
			content_type->subtype = g_strdup ("rfc822");
		} else {
			content_type->type = g_strdup ("text");
			content_type->subtype = g_strdup ("plain");
		}
	}
	
	content_type->exists = value != NULL;
	
	return content_type;
}

static int
parser_skip_line (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	register char *inptr;
	char *inend;
	int rv = 0;
	
	do {
		inptr = priv->inptr;
		inend = priv->inend;
		*inend = '\n';
		
		while (*inptr != '\n')
			inptr++;
		
		if (inptr < inend)
			break;
		
		priv->inptr = inptr;
		
		if (parser_fill (parser, SCAN_HEAD) <= 0) {
			inptr = priv->inptr;
			rv = -1;
			break;
		}
	} while (1);
	
	priv->midline = FALSE;
	
	priv->inptr = MIN (inptr + 1, priv->inend);
	
	return rv;
}

static int
parser_step (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	
	switch (priv->state) {
	case GMIME_PARSER_STATE_ERROR:
		break;
	case GMIME_PARSER_STATE_INIT:
		priv->message_headers_begin = -1;
		priv->message_headers_end = -1;
		if (priv->scan_from)
			priv->state = GMIME_PARSER_STATE_FROM;
		else
			priv->state = GMIME_PARSER_STATE_MESSAGE_HEADERS;
		break;
	case GMIME_PARSER_STATE_FROM:
		priv->message_headers_begin = -1;
		priv->message_headers_end = -1;
		parser_step_from (parser);
		break;
	case GMIME_PARSER_STATE_MESSAGE_HEADERS:
	case GMIME_PARSER_STATE_HEADERS:
		parser_step_headers (parser);
		
		if (priv->message_headers_begin == -1) {
			priv->message_headers_begin = priv->headers_begin;
			priv->message_headers_end = priv->headers_end;
		}
		break;
	case GMIME_PARSER_STATE_HEADERS_END:
		if (parser_skip_line (parser) == -1)
			priv->state = GMIME_PARSER_STATE_ERROR;
		else
			priv->state = GMIME_PARSER_STATE_CONTENT;
		break;
	case GMIME_PARSER_STATE_CONTENT:
		break;
	case GMIME_PARSER_STATE_COMPLETE:
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	
	return priv->state;
}


enum {
	FOUND_NOTHING,
	FOUND_EOS,
	FOUND_BOUNDARY,
	FOUND_END_BOUNDARY
};

#define content_save(content, start, len) G_STMT_START {                     \
	if (content)                                                         \
		g_byte_array_append (content, (unsigned char *) start, len); \
} G_STMT_END

#define possible_boundary(scan_from, start, len)                                      \
                         ((scan_from && len >= 5 && !strncmp (start, "From ", 5)) ||  \
			  (len >= 2 && (start[0] == '-' && start[1] == '-')))

static gboolean
is_boundary (const char *text, size_t len, const char *boundary, size_t boundary_len)
{
	const char *inptr = text + boundary_len;
	const char *inend = text + len;
	
	if (boundary_len > len)
		return FALSE;
	
	/* make sure that the text matches the boundary */
	if (strncmp (text, boundary, boundary_len) != 0)
		return FALSE;
	
	if (!strncmp (text, "From ", 5))
		return TRUE;
	
	/* the boundary may be optionally followed by linear whitespace */
	while (inptr < inend) {
		if (!is_lwsp (*inptr))
			return FALSE;
		
		inptr++;
	}
	
	return TRUE;
}

static int
check_boundary (struct _GMimeParserPrivate *priv, const char *start, size_t len)
{
	gint64 offset = parser_offset (priv, start);
	
	if (len > 0 && start[len - 1] == '\r')
		len--;
	
	if (possible_boundary (priv->scan_from, start, len)) {
		BoundaryStack *s;
		
		d(printf ("checking boundary '%.*s'\n", len, start));
		
		s = priv->bounds;
		while (s) {
			if (offset >= s->content_end &&
			    is_boundary (start, len, s->boundary, s->boundarylenfinal)) {
				d(printf ("found %s\n", s->content_end != -1 && offset >= s->content_end ?
					  "end of content" : "end boundary"));
				return FOUND_END_BOUNDARY;
			}
			
			if (is_boundary (start, len, s->boundary, s->boundarylen)) {
				d(printf ("found boundary\n"));
				return FOUND_BOUNDARY;
			}
			
			s = s->parent;
		}
		
		d(printf ("'%.*s' not a boundary\n", len, start));
	}
	
	return FOUND_NOTHING;
}

static gboolean
found_immediate_boundary (struct _GMimeParserPrivate *priv, gboolean end)
{
	BoundaryStack *s = priv->bounds;
	size_t boundary_len = end ? s->boundarylenfinal : s->boundarylen;
	register char *inptr = priv->inptr;
	char *inend = priv->inend;
	
	/* Note: see optimization comment [1] */
	*inend = '\n';
	while (*inptr != '\n')
		inptr++;
	
	return is_boundary (priv->inptr, inptr - priv->inptr, s->boundary, boundary_len);
}

/* Optimization Notes:
 *
 * 1. By making the priv->realbuf char array 1 extra char longer, we
 * can safely set '*inend' to '\n' and not fear an ABW. Setting *inend
 * to '\n' means that we can eliminate having to check that inptr <
 * inend every trip through our inner while-loop. This cuts the number
 * of instructions down from ~7 to ~4, assuming the compiler does its
 * job correctly ;-)
 **/


/* we add 2 for \r\n */
#define MAX_BOUNDARY_LEN(bounds) (bounds ? bounds->boundarylenmax + 2 : 0)

static int
parser_scan_content (GMimeParser *parser, GByteArray *content, guint *crlf)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	char *aligned, *start, *inend;
	register char *inptr;
	register int *dword;
	size_t nleft, len;
	size_t atleast;
	int found = 0;
	int mask;
	
	d(printf ("scan-content\n"));
	
	priv->midline = FALSE;
	
	g_assert (priv->inptr <= priv->inend);
	
	start = inptr = priv->inptr;
	
	/* figure out minimum amount of data we need */
	atleast = MAX (SCAN_HEAD, MAX_BOUNDARY_LEN (priv->bounds));
	
	do {
	refill:
		nleft = priv->inend - inptr;
		if (parser_fill (parser, atleast) <= 0) {
			start = priv->inptr;
			found = FOUND_EOS;
			break;
		}
		
		inptr = priv->inptr;
		inend = priv->inend;
		/* Note: see optimization comment [1] */
		*inend = '\n';
		
		len = (size_t) (inend - inptr);
		if (priv->midline && len == nleft)
			found = FOUND_EOS;
		
		priv->midline = FALSE;
		
		while (inptr < inend) {
			aligned = (char *) (((long) (inptr + 3)) & ~3);
			start = inptr;
			
			/* Note: see optimization comment [1] */
			while (inptr < aligned && *inptr != '\n')
				inptr++;
			
			if (inptr == aligned) {
				dword = (int *) inptr;
				
				do {
					mask = *dword++ ^ 0x0A0A0A0A;
					mask = ((mask - 0x01010101) & (~mask & 0x80808080));
				} while (mask == 0);
				
				inptr = (char *) (dword - 1);
				while (*inptr != '\n')
					inptr++;
			}
			
			len = (size_t) (inptr - start);
			
			if (inptr < inend) {
				if ((found = check_boundary (priv, start, len)))
					goto boundary;
				
				inptr++;
				len++;
			} else {
				/* didn't find an end-of-line */
				priv->midline = TRUE;
				
				if (!found) {
					/* not enough to tell if we found a boundary */
					priv->inptr = start;
					inptr = start;
					goto refill;
				}
				
				/* check for a boundary not ending in a \n (EOF) */
				if ((found = check_boundary (priv, start, len)))
					goto boundary;
			}
			
			content_save (content, start, len);
		}
		
		priv->inptr = inptr;
	} while (!found);
	
 boundary:
	
	/* don't chew up the boundary */
	priv->inptr = start;
	
	if (found != FOUND_EOS) {
		if (inptr[-1] == '\r')
			*crlf = 2;
		else
			*crlf = 1;
	} else {
		*crlf = 0;
	}
	
	return found;
}

static void
parser_scan_mime_part_content (GMimeParser *parser, GMimePart *mime_part, int *found)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	GMimeContentEncoding encoding;
	GByteArray *content = NULL;
	GMimeDataWrapper *wrapper;
	GMimeStream *stream;
	gint64 start, end;
	guint crlf;
	
	g_assert (priv->state >= GMIME_PARSER_STATE_HEADERS_END);
	
	if (priv->persist_stream && priv->seekable)
		start = parser_offset (priv, NULL);
	else
		content = g_byte_array_new ();
	
	*found = parser_scan_content (parser, content, &crlf);
	if (*found != FOUND_EOS) {
		/* last '\n' belongs to the boundary */
		if (priv->persist_stream && priv->seekable)
			end = parser_offset (priv, NULL) - crlf;
		else if (content->len > crlf)
			g_byte_array_set_size (content, content->len - crlf);
		else
			g_byte_array_set_size (content, 0);
	} else if (priv->persist_stream && priv->seekable) {
		end = parser_offset (priv, NULL);
	}
	
	encoding = g_mime_part_get_content_encoding (mime_part);
	
	if (priv->persist_stream && priv->seekable)
		stream = g_mime_stream_substream (priv->stream, start, end);
	else
		stream = g_mime_stream_mem_new_with_byte_array (content);
	
	wrapper = g_mime_data_wrapper_new_with_stream (stream, encoding);
	g_mime_part_set_content_object (mime_part, wrapper);
	g_object_unref (wrapper);
	g_object_unref (stream);
}

static void
parser_scan_message_part (GMimeParser *parser, GMimeMessagePart *mpart, int *found)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	ContentType *content_type;
	GMimeMessage *message;
	GMimeObject *object;
	GMimeStream *stream;
	HeaderRaw *header;
	
	g_assert (priv->state == GMIME_PARSER_STATE_CONTENT);
	
	if (priv->bounds != NULL) {
		/* Check for the possibility of an empty message/rfc822 part. */
		register char *inptr;
		size_t atleast;
		char *inend;
		
		/* figure out minimum amount of data we need */
		atleast = MAX (SCAN_HEAD, MAX_BOUNDARY_LEN (priv->bounds));
		
		if (parser_fill (parser, atleast) <= 0) {
			*found = FOUND_EOS;
			return;
		}
		
		inptr = priv->inptr;
		inend = priv->inend;
		/* Note: see optimization comment [1] */
		*inend = '\n';
		
		while (*inptr != '\n')
			inptr++;
		
		*found = check_boundary (priv, priv->inptr, inptr - priv->inptr);
		switch (*found) {
		case FOUND_END_BOUNDARY:
			/* ignore "From " boundaries, boken mailers tend to include these lines... */
			if (strncmp (priv->inptr, "From ", 5) != 0)
				return;
			break;
		case FOUND_BOUNDARY:
			return;
		}
	}
	
	/* get the headers */
	priv->state = GMIME_PARSER_STATE_HEADERS;
	if (parser_step (parser) == GMIME_PARSER_STATE_ERROR) {
		/* Note: currently cannot happen because
		 * parser_step_headers() never returns error */
		*found = FOUND_EOS;
		return;
	}
	
	message = g_mime_message_new (FALSE);
	header = priv->headers;
	while (header) {
		if (g_ascii_strncasecmp (header->name, "Content-", 8) != 0)
			g_mime_object_append_header ((GMimeObject *) message, header->name, header->value);
		header = header->next;
	}
	
	content_type = parser_content_type (parser, NULL);
	if (content_type_is_type (content_type, "multipart", "*"))
		object = parser_construct_multipart (parser, content_type, TRUE, found);
	else
		object = parser_construct_leaf_part (parser, content_type, TRUE, found);
	
	content_type_destroy (content_type);
	message->mime_part = object;
	
	/* set the same raw header stream on the message's header-list */
	if ((stream = g_mime_header_list_get_stream (object->headers)))
		g_mime_header_list_set_stream (((GMimeObject *) message)->headers, stream);
	
	g_mime_message_part_set_message (mpart, message);
	g_object_unref (message);
}

static GMimeObject *
parser_construct_leaf_part (GMimeParser *parser, ContentType *content_type, gboolean toplevel, int *found)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	GMimeObject *object;
	GMimeStream *stream;
	HeaderRaw *header;
	
	g_assert (priv->state >= GMIME_PARSER_STATE_HEADERS_END);
	
	object = g_mime_object_new_type (content_type->type, content_type->subtype);
	
	if (!content_type->exists) {
		GMimeContentType *mime_type;
		
		mime_type = g_mime_content_type_new (content_type->type, content_type->subtype);
		_g_mime_object_set_content_type (object, mime_type);
		g_object_unref (mime_type);
	}
	
	header = priv->headers;
	while (header) {
		if (!toplevel || !g_ascii_strncasecmp (header->name, "Content-", 8))
			g_mime_object_append_header (object, header->name, header->value);
		header = header->next;
	}
	
	header_raw_clear (&priv->headers);
	
	/* set the raw header stream on the header-list */
	if (priv->persist_stream && priv->seekable)
		stream = g_mime_stream_substream (priv->stream, priv->headers_begin, priv->headers_end);
	else
		stream = g_mime_stream_mem_new_with_buffer (priv->rawbuf, priv->rawptr - priv->rawbuf);
	
	g_mime_header_list_set_stream (object->headers, stream);
	g_object_unref (stream);
	
	raw_header_reset (priv);
	
	if (priv->state == GMIME_PARSER_STATE_HEADERS_END) {
		/* skip empty line after headers */
		if (parser_step (parser) == GMIME_PARSER_STATE_ERROR) {
			*found = FOUND_EOS;
			return object;
		}
	}
	
	if (GMIME_IS_MESSAGE_PART (object))
		parser_scan_message_part (parser, (GMimeMessagePart *) object, found);
	else
		parser_scan_mime_part_content (parser, (GMimePart *) object, found);
	
	return object;
}

static void
crlf2lf (char *in)
{
	register char *inptr = in;
	register char *outptr;
	
	while (*inptr != '\0' && !(inptr[0] == '\r' && inptr[1] == '\n'))
		inptr++;
	
	if (*inptr == '\0')
		return;
	
	outptr = inptr++;
	
	while (*inptr != '\0') {
		while (*inptr != '\0' && !(inptr[0] == '\r' && inptr[1] == '\n'))
			*outptr++ = *inptr++;
		
		if (*inptr == '\r')
			inptr++;
	}
	
	*outptr = '\0';
}

static int
parser_scan_multipart_face (GMimeParser *parser, GMimeMultipart *multipart, gboolean preface)
{
	GByteArray *buffer;
	char *face;
	guint crlf;
	int found;
	
	buffer = g_byte_array_new ();
	found = parser_scan_content (parser, buffer, &crlf);
	
	if (buffer->len >= crlf) {
		/* last '\n' belongs to the boundary */
		g_byte_array_set_size (buffer, buffer->len + 1);
		buffer->data[buffer->len - crlf - 1] = '\0';
		face = (char *) buffer->data;
		crlf2lf (face);
		
		if (preface)
			g_mime_multipart_set_preface (multipart, face);
		else
			g_mime_multipart_set_postface (multipart, face);
	}
	
	g_byte_array_free (buffer, TRUE);
	
	return found;
}

#define parser_scan_multipart_preface(parser, multipart) parser_scan_multipart_face (parser, multipart, TRUE)
#define parser_scan_multipart_postface(parser, multipart) parser_scan_multipart_face (parser, multipart, FALSE)

static int
parser_scan_multipart_subparts (GMimeParser *parser, GMimeMultipart *multipart)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	ContentType *content_type;
	GMimeObject *subpart;
	int found;
	
	do {
		/* skip over the boundary marker */
		if (parser_skip_line (parser) == -1) {
			found = FOUND_EOS;
			break;
		}
		
		/* get the headers */
		priv->state = GMIME_PARSER_STATE_HEADERS;
		if (parser_step (parser) == GMIME_PARSER_STATE_ERROR) {
			found = FOUND_EOS;
			break;
		}
		
		if (priv->state == GMIME_PARSER_STATE_COMPLETE && priv->headers == NULL) {
			found = FOUND_END_BOUNDARY;
			break;
		}
		
		content_type = parser_content_type (parser, ((GMimeObject *) multipart)->content_type);
		if (content_type_is_type (content_type, "multipart", "*"))
			subpart = parser_construct_multipart (parser, content_type, FALSE, &found);
		else
			subpart = parser_construct_leaf_part (parser, content_type, FALSE, &found);
		
		g_mime_multipart_add (multipart, subpart);
		content_type_destroy (content_type);
		g_object_unref (subpart);
	} while (found == FOUND_BOUNDARY && found_immediate_boundary (priv, FALSE));
	
	return found;
}

static GMimeObject *
parser_construct_multipart (GMimeParser *parser, ContentType *content_type, gboolean toplevel, int *found)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	GMimeMultipart *multipart;
	const char *boundary;
	GMimeObject *object;
	GMimeStream *stream;
	HeaderRaw *header;
	
	g_assert (priv->state >= GMIME_PARSER_STATE_HEADERS_END);
	
	object = g_mime_object_new_type (content_type->type, content_type->subtype);
	
	header = priv->headers;
	while (header) {
		if (!toplevel || !g_ascii_strncasecmp (header->name, "Content-", 8))
			g_mime_object_append_header (object, header->name, header->value);
		header = header->next;
	}
	
	header_raw_clear (&priv->headers);
	
	/* set the raw header stream on the header-list */
	if (priv->persist_stream && priv->seekable)
		stream = g_mime_stream_substream (priv->stream, priv->headers_begin, priv->headers_end);
	else
		stream = g_mime_stream_mem_new_with_buffer (priv->rawbuf, priv->rawptr - priv->rawbuf);
	
	g_mime_header_list_set_stream (object->headers, stream);
	g_object_unref (stream);
	
	raw_header_reset (priv);
	
	multipart = (GMimeMultipart *) object;
	
	if (priv->state == GMIME_PARSER_STATE_HEADERS_END) {
		/* skip empty line after headers */
		if (parser_step (parser) == GMIME_PARSER_STATE_ERROR) {
			*found = FOUND_EOS;
			return object;
		}
	}
	
	boundary = g_mime_object_get_content_type_parameter (object, "boundary");
	if (boundary) {
		parser_push_boundary (parser, boundary);
		
		*found = parser_scan_multipart_preface (parser, multipart);
		
		if (*found == FOUND_BOUNDARY)
			*found = parser_scan_multipart_subparts (parser, multipart);
		
		if (*found == FOUND_END_BOUNDARY && found_immediate_boundary (priv, TRUE)) {
			/* eat end boundary */
			parser_skip_line (parser);
			parser_pop_boundary (parser);
			*found = parser_scan_multipart_postface (parser, multipart);
		} else {
			parser_pop_boundary (parser);
		}
	} else {
		w(g_warning ("multipart without boundary encountered"));
		/* this will scan everything into the preface */
		*found = parser_scan_multipart_preface (parser, multipart);
	}
	
	return object;
}

static GMimeObject *
parser_construct_part (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	ContentType *content_type;
	GMimeObject *object;
	int found;
	
	/* get the headers */
	priv->state = GMIME_PARSER_STATE_HEADERS;
	while (priv->state < GMIME_PARSER_STATE_HEADERS_END) {
		if (parser_step (parser) == GMIME_PARSER_STATE_ERROR)
			return NULL;
	}
	
	content_type = parser_content_type (parser, NULL);
	if (content_type_is_type (content_type, "multipart", "*"))
		object = parser_construct_multipart (parser, content_type, TRUE, &found);
	else
		object = parser_construct_leaf_part (parser, content_type, TRUE, &found);
	
	content_type_destroy (content_type);
	
	return object;
}


/**
 * g_mime_parser_construct_part:
 * @parser: a #GMimeParser context
 *
 * Constructs a MIME part from @parser.
 *
 * Returns: (transfer full): a MIME part based on @parser or %NULL on
 * fail.
 **/
GMimeObject *
g_mime_parser_construct_part (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);
	
	return parser_construct_part (parser);
}


static GMimeMessage *
parser_construct_message (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv = parser->priv;
	unsigned long content_length = ULONG_MAX;
	ContentType *content_type;
	GMimeMessage *message;
	GMimeObject *object;
	GMimeStream *stream;
	HeaderRaw *header;
	char *endptr;
	int found;
	
	/* scan the from-line if we are parsing an mbox */
	while (priv->state != GMIME_PARSER_STATE_MESSAGE_HEADERS) {
		if (parser_step (parser) == GMIME_PARSER_STATE_ERROR)
			return NULL;
	}
	
	/* parse the headers */
	while (priv->state < GMIME_PARSER_STATE_HEADERS_END) {
		if (parser_step (parser) == GMIME_PARSER_STATE_ERROR)
			return NULL;
	}
	
	message = g_mime_message_new (FALSE);
	header = priv->headers;
	while (header) {
		if (priv->respect_content_length && !g_ascii_strcasecmp (header->name, "Content-Length")) {
			content_length = strtoul (header->value, &endptr, 10);
			if (endptr == header->value)
				content_length = ULONG_MAX;
		}
		
		if (g_ascii_strncasecmp (header->name, "Content-", 8) != 0)
			g_mime_object_append_header ((GMimeObject *) message, header->name, header->value);
		header = header->next;
	}
	
	if (priv->scan_from) {
		parser_push_boundary (parser, MBOX_BOUNDARY);
		if (priv->respect_content_length && content_length < ULONG_MAX)
			priv->bounds->content_end = parser_offset (priv, NULL) + content_length;
	}
	
	content_type = parser_content_type (parser, NULL);
	if (content_type_is_type (content_type, "multipart", "*"))
		object = parser_construct_multipart (parser, content_type, TRUE, &found);
	else
		object = parser_construct_leaf_part (parser, content_type, TRUE, &found);
	
	content_type_destroy (content_type);
	message->mime_part = object;
	
	/* set the same raw header stream on the message's header-list */
	if ((stream = g_mime_header_list_get_stream (object->headers)))
		g_mime_header_list_set_stream (((GMimeObject *) message)->headers, stream);
	
	if (priv->scan_from) {
		priv->state = GMIME_PARSER_STATE_FROM;
		parser_pop_boundary (parser);
	}
	
	return message;
}


/**
 * g_mime_parser_construct_message:
 * @parser: a #GMimeParser context
 *
 * Constructs a MIME message from @parser.
 *
 * Returns: (transfer full): a MIME message or %NULL on fail.
 **/
GMimeMessage *
g_mime_parser_construct_message (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);
	
	return parser_construct_message (parser);
}


/**
 * g_mime_parser_get_from:
 * @parser: a #GMimeParser context
 *
 * Gets the mbox-style From-line of the most recently parsed message
 * (gotten from g_mime_parser_construct_message()).
 *
 * Returns: the mbox-style From-line of the most recently parsed
 * message or %NULL on error.
 **/
char *
g_mime_parser_get_from (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv;
	
	g_return_val_if_fail (GMIME_IS_PARSER (parser), NULL);
	
	priv = parser->priv;
	if (!priv->scan_from)
		return NULL;
	
	if (priv->from_line->len)
		return g_strndup ((char *) priv->from_line->data, priv->from_line->len);
	
	return NULL;
}


/**
 * g_mime_parser_get_from_offset:
 * @parser: a #GMimeParser context
 *
 * Gets the offset of the most recently parsed mbox-style From-line
 * (gotten from g_mime_parser_construct_message()).
 *
 * Returns: the offset of the most recently parsed mbox-style From-line
 * or %-1 on error.
 **/
gint64
g_mime_parser_get_from_offset (GMimeParser *parser)
{
	struct _GMimeParserPrivate *priv;
	
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);
	
	priv = parser->priv;
	if (!priv->scan_from)
		return -1;
	
	return priv->from_offset;
}


/**
 * g_mime_parser_get_headers_begin:
 * @parser: a #GMimeParser context
 *
 * Gets the stream offset of the beginning of the headers of the most
 * recently parsed message.
 *
 * Returns: the offset of the beginning of the headers of the most
 * recently parsed message or %-1 on error.
 **/
gint64
g_mime_parser_get_headers_begin (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);
	
	return parser->priv->message_headers_begin;
}


/**
 * g_mime_parser_get_headers_end:
 * @parser: a #GMimeParser context
 *
 * Gets the stream offset of the end of the headers of the most
 * recently parsed message.
 *
 * Returns: the offset of the end of the headers of the most recently
 * parsed message or %-1 on error.
 **/
gint64
g_mime_parser_get_headers_end (GMimeParser *parser)
{
	g_return_val_if_fail (GMIME_IS_PARSER (parser), -1);
	
	return parser->priv->message_headers_end;
}
