/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) Jeffrey Stedfast
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

/* Based on MimeKit's RtfInterpreter and RtfKeywords.
 * Copyright (c) 2013-2026 .NET Foundation and Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <config.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "gmime-iconv.h"
#include "gmime-rtf-interpreter-private.h"
#include "gmime-rtf-tokenizer-private.h"

typedef enum {
	UNKNOWN, SKIP, RTF, ANSI, MAC, PC, PCA, ANSICPG, DEFF, FROMHTML, HTMLRTF, HTMLTAG,
	FONTTBL, COLORTBL, FIELD, FLDINST, FLDRSLT, UPR, UD, NESTTABLEPROPS, FONT, FCHARSET,
	CPG, RED, GREEN, BLUE, PLAIN, BOLD, ITALIC, UNDERLINE, ULNONE, STRIKE, HIDDEN,
	SUPER, SUB, NOSUPERSUB, FS, CF, CB, U, UC, PAR, PARD, LINE, TAB, QL, QC, QR, QJ,
	INTBL, CELL, ROW, OBJATTPH, CHARACTER
} Keyword;

typedef struct {
	const char *name;
	Keyword id;
	gunichar character;
} Word;

/* RTF 1.9.1 control words are case-sensitive. Keep sorted for binary search.
 * \object is intentionally absent: skip its data, but render its \result. */
#define W(name,id) { name, id, 0 }
#define C(name,c) { name, CHARACTER, c }
static const Word words[] = {
	W ("aftnsep", SKIP), W ("aftnsepc", SKIP), W ("annotation", SKIP),
	W ("ansi", ANSI), W ("ansicpg", ANSICPG), W ("atnauthor", SKIP), W ("atnid", SKIP),
	W ("b", BOLD), W ("background", SKIP), W ("bkmkend", SKIP), W ("bkmkstart", SKIP),
	W ("blue", BLUE), C ("bullet", 0x2022), W ("cb", CB), W ("cell", CELL),
	W ("cf", CF), W ("chcbpat", CB), W ("colorschememapping", SKIP), W ("colortbl", COLORTBL),
	W ("cpg", CPG), W ("datastore", SKIP), W ("deff", DEFF), W ("docvar", SKIP),
	C ("emdash", 0x2014), C ("emspace", 0x2003), C ("endash", 0x2013), C ("enspace", 0x2002),
	W ("f", FONT), W ("fcharset", FCHARSET), W ("field", FIELD), W ("filetbl", SKIP),
	W ("fldinst", FLDINST), W ("fldrslt", FLDRSLT), W ("fonttbl", FONTTBL),
	W ("footer", SKIP), W ("footerf", SKIP), W ("footerl", SKIP), W ("footerr", SKIP),
	W ("footnote", SKIP), W ("fromhtml", FROMHTML), W ("fs", FS), W ("ftnsep", SKIP),
	W ("ftnsepc", SKIP), W ("generator", SKIP), W ("green", GREEN), W ("header", SKIP),
	W ("headerf", SKIP), W ("headerl", SKIP), W ("headerr", SKIP), W ("highlight", CB),
	W ("htmlrtf", HTMLRTF), W ("htmltag", HTMLTAG), W ("i", ITALIC), W ("info", SKIP),
	W ("intbl", INTBL), W ("latentstyles", SKIP), C ("ldblquote", 0x201c), W ("line", LINE),
	W ("listoverridetable", SKIP), W ("listtable", SKIP), C ("lquote", 0x2018), C ("ltrmark", 0x200e),
	W ("mac", MAC), W ("mhtmltag", SKIP), W ("mmathPr", SKIP), W ("nestcell", CELL),
	W ("nestrow", ROW), W ("nesttableprops", NESTTABLEPROPS), W ("nonesttables", SKIP),
	W ("nonshppict", SKIP), W ("nosupersub", NOSUPERSUB), W ("objattph", OBJATTPH),
	W ("objdata", SKIP), W ("page", PAR), W ("par", PAR), W ("pard", PARD), W ("pc", PC),
	W ("pca", PCA), W ("pgdsctbl", SKIP), W ("pict", SKIP), W ("plain", PLAIN),
	W ("qc", QC), W ("qj", QJ), W ("ql", QL), C ("qmspace", 0x2005), W ("qr", QR),
	C ("rdblquote", 0x201d), W ("red", RED), W ("revtbl", SKIP), W ("row", ROW),
	C ("rquote", 0x2019), W ("rsidtbl", SKIP), W ("rtf", RTF), C ("rtlmark", 0x200f),
	W ("sect", PAR), W ("shpinst", SKIP), W ("strike", STRIKE), W ("striked", STRIKE),
	W ("stylesheet", SKIP), W ("sub", SUB), W ("super", SUPER), W ("tab", TAB),
	W ("tc", SKIP), W ("themedata", SKIP), W ("txe", SKIP), W ("u", U), W ("uc", UC),
	W ("ud", UD), W ("ul", UNDERLINE), W ("uld", UNDERLINE), W ("uldash", UNDERLINE),
	W ("uldashd", UNDERLINE), W ("uldashdd", UNDERLINE), W ("uldb", UNDERLINE),
	W ("ulhwave", UNDERLINE), W ("ulldash", UNDERLINE), W ("ulnone", ULNONE),
	W ("ulth", UNDERLINE), W ("ulthd", UNDERLINE), W ("ulthdash", UNDERLINE),
	W ("ulthdashd", UNDERLINE), W ("ulthdashdd", UNDERLINE), W ("ulthldash", UNDERLINE),
	W ("ululdbwave", UNDERLINE), W ("ulw", UNDERLINE), W ("ulwave", UNDERLINE),
	W ("upr", UPR), W ("userprops", SKIP), W ("v", HIDDEN), W ("xe", SKIP),
	W ("xmlnstbl", SKIP), C ("zwj", 0x200d), C ("zwnj", 0x200c)
};
#undef W
#undef C

static int
compare_word (const void *key, const void *entry)
{
	return strcmp (key, ((const Word *) entry)->name);
}

typedef enum {
	BODY,
	FONTS,
	COLORS,
	INSTRUCTION,
	HTML,
	ALTERNATIVE
} Destination;

enum {
	BOLD_BIT = 1, ITALIC_BIT = 2, UNDERLINE_BIT = 4, STRIKE_BIT = 8,
	HIDDEN_BIT = 16, TABLE_BIT = 32, HTMLRTF_BIT = 64
};

typedef struct {
	Destination dest;
	guint flags;
	GMimeRtfAlignment align;
	GMimeRtfVerticalAlignment vertical;
	int font, fs, fg, bg, uc;
	GBytes *link; /* immutable, NUL-terminated UTF-8 */
} State;

typedef struct {
	State state;
	guint64 count;
} Frame;

typedef struct {
	int page;
	iconv_t cd;
} Decoder;

struct _GMimeRtfInterpreter {
	GMimeRtfEventFunc callback;
	GMimeRtfTokenizer *tokenizer;
	gpointer user_data;
	GError **error;
	GMimeRtfDiagnostics *diagnostics;
	GMimeRtfToken token;
	State state;
	GArray *stack, *colors;
	GHashTable *fonts;
	GString *instruction;
	GBytes *field_link;
	guint max_depth, max_fonts, max_colors;
	guint64 depth, skip, font_depth;
	gboolean pop_skipped, root_closed, star, placeholder;
	gboolean extract, from_html, begun, failed, finished;
	guint prefix_tokens, header_state;
	guint32 reported;
	int doc_page, default_font, fallback, field_units;
	gboolean pending_font, pending_color;
	int font_number, font_charset, font_page, rgb;
	gunichar high;
	guint8 bytes[1024];
	gsize buffered;
	int byte_page;
	Decoder decoders[32];
	guint ndecoders;
};

typedef GMimeRtfInterpreter Interpreter;

#define VIOLATION(v) GMIME_RTF_COMPLIANCE_VIOLATION_##v
#define REPORT(interpreter,v) report (interpreter, VIOLATION (v), 0)
#define ONCE(interpreter,v,d) report_once (interpreter, VIOLATION (v), d)
#define BIT(v) (1u << VIOLATION (v))

static void
report (Interpreter *interpreter, GMimeRtfComplianceViolation violation, gsize delta)
{
	if (interpreter->diagnostics)
		_g_mime_rtf_report (interpreter->diagnostics, violation, interpreter->token.offset + delta, interpreter->token.line,
				    interpreter->token.column + delta, interpreter->token.depth);
}

static void
report_once (Interpreter *interpreter, GMimeRtfComplianceViolation violation, gsize delta)
{
	if (!(interpreter->reported & (1u << violation))) {
		interpreter->reported |= 1u << violation;
		report (interpreter, violation, delta);
	}
}

static int
color (Interpreter *interpreter, int index)
{
	return index > 0 && (guint) index < interpreter->colors->len ? g_array_index (interpreter->colors, int, index) : -1;
}

static void
dispatch (Interpreter *interpreter, GMimeRtfEvent event, const char *text, gsize length)
{
	GMimeRtfFormat format;

	if (interpreter->failed)
		return;

	format.from_html = interpreter->from_html;
	format.bold = (interpreter->state.flags & BOLD_BIT) != 0;
	format.italic = (interpreter->state.flags & ITALIC_BIT) != 0;
	format.underline = (interpreter->state.flags & UNDERLINE_BIT) != 0;
	format.strike = (interpreter->state.flags & STRIKE_BIT) != 0;
	format.in_table = (interpreter->state.flags & TABLE_BIT) != 0;
	format.alignment = interpreter->state.align;
	format.vertical_alignment = interpreter->state.vertical;
	format.font = interpreter->state.font;
	format.font_size = interpreter->state.fs;
	format.foreground = color (interpreter, interpreter->state.fg);
	format.background = color (interpreter, interpreter->state.bg);
	format.hyperlink = interpreter->state.link ? g_bytes_get_data (interpreter->state.link, NULL) : NULL;

	if (!interpreter->begun) {
		interpreter->begun = TRUE;

		if (!interpreter->callback (GMIME_RTF_EVENT_BEGIN, &format, NULL, 0, interpreter->user_data, interpreter->error))
			interpreter->failed = TRUE;
	}

	if (!interpreter->failed && event != GMIME_RTF_EVENT_BEGIN &&
	    !interpreter->callback (event, &format, text, length, interpreter->user_data, interpreter->error))
		interpreter->failed = TRUE;
}

static gboolean
visible (Interpreter *interpreter)
{
	return interpreter->state.dest == BODY && !(interpreter->state.flags & HIDDEN_BIT) &&
		!(interpreter->extract && interpreter->from_html && (interpreter->state.flags & HTMLRTF_BIT));
}

static int
charset_page (int charset)
{
	switch (charset) {
	case 77: return 10000;
	case 128: return 932;
	case 129: return 949;
	case 130: return 1361;
	case 134: return 936;
	case 136: return 950;
	case 161: return 1253;
	case 162: return 1254;
	case 163: return 1258;
	case 177: return 1255;
	case 178: return 1256;
	case 186: return 1257;
	case 204: return 1251;
	case 222: return 874;
	case 238: return 1250;
	case 254: return 437;
	case 255: return 850;
	default: return 0;
	}
}

static void
commit_font (Interpreter *interpreter, gsize delta)
{
	gpointer key = GINT_TO_POINTER (interpreter->font_number);
	int page;

	if (!interpreter->pending_font)
		return;

	interpreter->pending_font = FALSE;
	if (g_hash_table_size (interpreter->fonts) >= interpreter->max_fonts && !g_hash_table_contains (interpreter->fonts, key)) {
		ONCE (interpreter, FONT_TABLE_LIMIT_EXCEEDED, delta);
		return;
	}

	page = interpreter->font_page > 0 ? interpreter->font_page : charset_page (interpreter->font_charset);
	g_hash_table_insert (interpreter->fonts, key, GINT_TO_POINTER (page));
}

static void
commit_color (Interpreter *interpreter, gsize delta)
{
	int rgb = interpreter->pending_color ? interpreter->rgb : -1;

	if (interpreter->colors->len < interpreter->max_colors)
		g_array_append_val (interpreter->colors, rgb);
	else
		ONCE (interpreter, COLOR_TABLE_LIMIT_EXCEEDED, delta);

	interpreter->rgb = 0;
	interpreter->pending_color = FALSE;
}

static void
append_instruction (Interpreter *interpreter, const char *text, gsize length)
{
	gsize n = 0;

	/* Match the reference limit of 2048 UTF-16 code units without splitting a scalar. */
	while (n < length) {
		int units = (guint8) text[n] >= 0xf0 ? 2 : 1;

		if (interpreter->field_units + units > 2048) {
			ONCE (interpreter, FIELD_INSTRUCTION_TOO_LONG, 0);
			interpreter->field_units = 2048;
			break;
		}

		interpreter->field_units += units;
		n += g_utf8_skip[(guint8) text[n]];
	}

	g_string_append_len (interpreter->instruction, text, n);
}

static void
deliver (Interpreter *interpreter, const char *text, gsize length)
{
	gsize i;

	switch (interpreter->state.dest) {
	case BODY:
		if (visible (interpreter))
			dispatch (interpreter, GMIME_RTF_EVENT_TEXT, text, length);
		break;
	case HTML:
		dispatch (interpreter, GMIME_RTF_EVENT_HTML_TAG, text, length);
		break;
	case INSTRUCTION:
		append_instruction (interpreter, text, length);
		break;
	case FONTS: case COLORS:
		for (i = 0; i < length; i++) {
			if (text[i] == ';') {
				if (interpreter->state.dest == FONTS)
					commit_font (interpreter, 0);
				else
					commit_color (interpreter, 0);
			}
		}
		break;
	case ALTERNATIVE:
		break;
	}
}

static void
flush_surrogate (Interpreter *interpreter)
{
	if (interpreter->high) {
		interpreter->high = 0;
		REPORT (interpreter, UNPAIRED_SURROGATE);
		deliver (interpreter, "\xef\xbf\xbd", 3);
	}
}

static void
emit_scalar (Interpreter *interpreter, gunichar c)
{
	char buf[6];
	int n;

	flush_surrogate (interpreter);
	n = g_unichar_to_utf8 (c, buf);
	deliver (interpreter, buf, n);
}

static void
emit_utf16 (Interpreter *interpreter, guint16 unit)
{
	if (unit >= 0xdc00 && unit <= 0xdfff && interpreter->high) {
		gunichar c = 0x10000 + ((interpreter->high - 0xd800) << 10) + (unit - 0xdc00);
		interpreter->high = 0;
		emit_scalar (interpreter, c);
	} else if (unit >= 0xd800 && unit <= 0xdbff) {
		flush_surrogate (interpreter);
		interpreter->high = unit;
	} else if (unit >= 0xdc00 && unit <= 0xdfff) {
		REPORT (interpreter, UNPAIRED_SURROGATE);
		emit_scalar (interpreter, 0xfffd);
	} else {
		emit_scalar (interpreter, unit);
	}
}

static int
current_page (Interpreter *interpreter)
{
	int page = 0;

	if (interpreter->state.dest != HTML)
		page = GPOINTER_TO_INT (g_hash_table_lookup (interpreter->fonts, GINT_TO_POINTER (interpreter->state.font)));

	return page > 0 ? page : interpreter->doc_page;
}

/* Numeric Microsoft code pages without a portable CPnnn iconv spelling. */
static iconv_t
open_decoder (int page)
{
	const char *name;
	char buf[32];
	iconv_t cd;

	switch (page) {
	case 10000: name = "MACINTOSH"; break;
	case 10001:
		/* Prefer the Apple mapping when available. Standard Japanese
		 * text in this code page uses Shift-JIS; Apple extensions differ. */
		cd = g_mime_iconv_open ("UTF-8", "X-MAC-JAPANESE");

		if (cd != (iconv_t) -1)
			return cd;

		name = "Shift_JIS";
		break;
	case 1200: name = "UTF-16LE"; break;
	case 1201: name = "UTF-16BE"; break;
	case 12000: name = "UTF-32LE"; break;
	case 12001: name = "UTF-32BE"; break;
	case 20127: name = "ASCII"; break;
	case 20866: name = "KOI8-R"; break;
	case 21866: name = "KOI8-U"; break;
	case 28591: case 28592: case 28593: case 28594: case 28595:
	case 28596: case 28597: case 28598: case 28599: case 28603: case 28605:
		g_snprintf (buf, sizeof buf, "ISO-8859-%d", page - 28590);
		name = buf;
		break;
	case 50220: case 50221: case 50222: name = "ISO-2022-JP"; break;
	case 51932: name = "EUC-JP"; break;
	case 51949: name = "EUC-KR"; break;
	case 54936: name = "GB18030"; break;
	case 65000: name = "UTF-7"; break;
	case 65001: name = "UTF-8"; break;
	default:
		/* GMime caches charset names process-wide. Do not intern an
		 * unbounded sequence of names supplied by malformed documents. */
		if (page <= 0 || page > 65535)
			return (iconv_t) -1;

		g_snprintf (buf, sizeof buf, "CP%d", page);
		name = buf;
		break;
	}

	return g_mime_iconv_open ("UTF-8", name);
}

static iconv_t
decoder (Interpreter *interpreter)
{
	Decoder *decoder;
	guint i;

	for (i = 0; i < interpreter->ndecoders; i++) {
		if (interpreter->decoders[i].page == interpreter->byte_page)
			return interpreter->decoders[i].cd;
	}

	if (interpreter->ndecoders == G_N_ELEMENTS (interpreter->decoders))
		return (iconv_t) -1;

	decoder = &interpreter->decoders[interpreter->ndecoders++];
	decoder->page = interpreter->byte_page;
	decoder->cd = open_decoder (decoder->page);

	if (decoder->cd == (iconv_t) -1 && decoder->page != 1252)
		REPORT (interpreter, UNSUPPORTED_CODE_PAGE);

	return decoder->cd;
}

static void
decode_1252 (Interpreter *interpreter)
{
	static const guint16 c1[] = {
		0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,
		0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
		0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,
		0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178
	};
	char out[sizeof (interpreter->bytes) * 3], *outptr = out;
	gsize i;

	for (i = 0; i < interpreter->buffered; i++) {
		guint8 b = interpreter->bytes[i];

		if (b < 128)
			*outptr++ = (char) b;
		else
			outptr += g_unichar_to_utf8 (b < 160 ? c1[b - 128] : b, outptr);
	}

	deliver (interpreter, out, outptr - out);
	interpreter->buffered = 0;
}

/* Some iconv backends accept pre-RFC 3629 sequences in UTF-8. Never pass an
 * invalid scalar to a renderer, but preserve legitimate embedded U+0000. */
static void
deliver_decoded (Interpreter *interpreter, const char *text, gsize length)
{
	const char *end = text + length, *bad;

	while (text < end && !g_utf8_validate (text, end - text, &bad)) {
		if (bad > text)
			deliver (interpreter, text, bad - text);

		if (*bad == '\0')
			deliver (interpreter, bad, 1);
		else
			deliver (interpreter, "\xef\xbf\xbd", 3);

		text = bad + 1;
	}

	if (text < end)
		deliver (interpreter, text, end - text);
}

static void
decode (Interpreter *interpreter, gboolean finish)
{
	gsize left = interpreter->buffered;
	char *in = (char *) interpreter->bytes;
	iconv_t cd;

	if (!left)
		return;

	flush_surrogate (interpreter);

	if (interpreter->byte_page == 1252 || (cd = decoder (interpreter)) == (iconv_t) -1) {
		decode_1252 (interpreter);
		return;
	}

	while (left && !interpreter->failed) {
		char buf[4096], *out = buf;
		gsize space = sizeof (buf);
		gsize status = g_mime_iconv (cd, &in, &left, &out, &space);
		int err = errno;

		if (out != buf)
			deliver_decoded (interpreter, buf, out - buf);

		if (status != (gsize) -1 || err == E2BIG)
			continue;

		/* Preserve a DBCS lead byte across internal buffer boundaries, but
		 * never carry an incomplete sequence into a different format/group. */
		if (err == EINVAL && !finish && left < sizeof interpreter->bytes)
			break;

		deliver (interpreter, "\xef\xbf\xbd", 3);

		if (err == EINVAL && finish) {
			left = 0;
		} else if (left) {
			/* An invalid DBCS lead byte must not swallow a following
			 * valid ASCII byte. Unlike .NET's CP950 "?" pair fallback,
			 * emit U+FFFD for the lead and retry the trail byte. */
			in++;
			left--;
		}

		g_mime_iconv (cd, NULL, NULL, NULL, NULL);
	}

	if (finish)
		g_mime_iconv (cd, NULL, NULL, NULL, NULL);

	if (left)
		memmove (interpreter->bytes, in, left);

	interpreter->buffered = left;
}

static void
append_byte (Interpreter *interpreter, guint8 b, int page)
{
	if (page != interpreter->byte_page) {
		decode (interpreter, TRUE);
		interpreter->byte_page = page;
	}

	if (interpreter->buffered == sizeof interpreter->bytes)
		decode (interpreter, FALSE);

	if (!interpreter->failed)
		interpreter->bytes[interpreter->buffered++] = b;
}

static const char *
skip_space (const char *inptr)
{
	while (*inptr && g_unichar_isspace (g_utf8_get_char (inptr)))
		inptr = g_utf8_next_char (inptr);

	return inptr;
}

static const char *
word_end (const char *inptr)
{
	while (*inptr && !g_unichar_isspace (g_utf8_get_char (inptr)))
		inptr = g_utf8_next_char (inptr);

	return inptr;
}

/* Return a validated external target, not escaped HTML. The renderer is
 * responsible for quoting attributes. The field buffer is bounded UTF-8. */
static GBytes *
parse_link (GString *instruction)
{
	const char *inptr = skip_space (instruction->str), *start, *end, *q;

	if (memchr (instruction->str, 0, instruction->len) || g_ascii_strncasecmp (inptr, "HYPERLINK", 9))
		return NULL;

	inptr += 9;

	while (*(inptr = skip_space (inptr))) {
		gboolean arg = FALSE;

		if (*inptr == '\\') {
			if (inptr[1] == 'l')
				return NULL;

			arg = inptr[1] == 'o' || inptr[1] == 't';
			inptr = skip_space (word_end (inptr));

			if (!arg)
				continue;
		}

		if (*inptr == '"') {
			start = ++inptr;
			end = strchr (inptr, '"');

			if (!end)
				end = inptr + strlen (inptr);

			inptr = end + (*end != 0);
		} else {
			start = inptr;
			end = inptr = word_end (inptr);
		}

		if (arg)
			continue;

		start = skip_space (start);

		while (end > start) {
			q = g_utf8_prev_char (end);

			if (!g_unichar_isspace (g_utf8_get_char (q)))
				break;

			end = q;
		}

		for (q = start; q < end; q = g_utf8_next_char (q)) {
			if (g_unichar_iscntrl (g_utf8_get_char (q)))
				return NULL;
		}

		if (start >= end ||
		    (g_ascii_strncasecmp (start, "http:", 5) && g_ascii_strncasecmp (start, "https:", 6) &&
		     g_ascii_strncasecmp (start, "mailto:", 7) && g_ascii_strncasecmp (start, "ftp:", 4) &&
		     g_ascii_strncasecmp (start, "tel:", 4)))
			return NULL;

		return g_bytes_new_take (g_strndup (start, end - start), end - start + 1);
	}

	return NULL;
}

static gboolean
same_state (const State *a, const State *b)
{
	return a->dest == b->dest && a->flags == b->flags && a->align == b->align &&
		a->vertical == b->vertical && a->font == b->font && a->fs == b->fs &&
		a->fg == b->fg && a->bg == b->bg && a->uc == b->uc && a->link == b->link;
}

static void
push (Interpreter *interpreter)
{
	Frame frame, *top;

	if (interpreter->stack->len) {
		top = &g_array_index (interpreter->stack, Frame, interpreter->stack->len - 1);

		if (same_state (&top->state, &interpreter->state)) {
			top->count++;
			interpreter->depth++;
			return;
		}
	}

	if (interpreter->stack->len >= interpreter->max_depth) {
		interpreter->skip = 1;
		interpreter->pop_skipped = FALSE;
		ONCE (interpreter, GROUP_DEPTH_LIMIT_EXCEEDED, 0);
		return;
	}

	frame.state = interpreter->state;

	if (frame.state.link)
		g_bytes_ref (frame.state.link);

	frame.count = 1;
	g_array_append_val (interpreter->stack, frame);
	interpreter->depth++;
}

static void
pop (Interpreter *interpreter)
{
	Destination previous = interpreter->state.dest;
	Frame *top;

	if (!interpreter->stack->len) {
		REPORT (interpreter, UNBALANCED_GROUP_END);
		return;
	}

	top = &g_array_index (interpreter->stack, Frame, interpreter->stack->len - 1);
	g_clear_pointer (&interpreter->state.link, g_bytes_unref);
	interpreter->state = top->state;

	if (--top->count) {
		if (interpreter->state.link)
			g_bytes_ref (interpreter->state.link);
	} else {
		/* Transfer the frame's link reference to the current state. */
		g_array_set_size (interpreter->stack, interpreter->stack->len - 1);
	}

	if (!--interpreter->depth)
		interpreter->root_closed = TRUE;

	if (interpreter->pending_font && interpreter->depth < interpreter->font_depth)
		commit_font (interpreter, 0);

	if (previous == INSTRUCTION && interpreter->state.dest != INSTRUCTION) {
		g_clear_pointer (&interpreter->field_link, g_bytes_unref);
		interpreter->field_link = parse_link (interpreter->instruction);
	}
}

static void
skip_group (Interpreter *interpreter)
{
	if (interpreter->depth > 1 || interpreter->root_closed) {
		interpreter->skip = 1;
		interpreter->pop_skipped = TRUE;
	}
}

static void
structure (Interpreter *interpreter, GMimeRtfEvent event)
{
	if (visible (interpreter))
		dispatch (interpreter, event, NULL, 0);
	else if (interpreter->state.dest == HTML && (event == GMIME_RTF_EVENT_PARAGRAPH || event == GMIME_RTF_EVENT_LINE_BREAK))
		dispatch (interpreter, GMIME_RTF_EVENT_HTML_TAG, "\r\n", 2);
}

static void
control_word (Interpreter *interpreter, const Word *word)
{
	Keyword id = word ? word->id : UNKNOWN;
	int n = interpreter->token.parameter;
	gboolean star = interpreter->star;
	gboolean on = !interpreter->token.has_parameter || n != 0;
	guint flag = 0;

	interpreter->star = FALSE;

	if (interpreter->fallback) {
		interpreter->fallback--;
		return;
	}

	if (interpreter->state.dest == FONTS) {
		switch (id) {
		case FONT:
			commit_font (interpreter, 0);
			interpreter->pending_font = TRUE;
			interpreter->font_number = n;
			interpreter->font_depth = interpreter->depth;
			interpreter->font_page = interpreter->font_charset = 0;
			break;
		case FCHARSET: interpreter->font_charset = n; break;
		case CPG: interpreter->font_page = n; break;
		default:
			if (star || id == SKIP)
				skip_group (interpreter);
			break;
		}

		return;
	}

	if (interpreter->state.dest == COLORS) {
		if (id == RED || id == GREEN || id == BLUE) {
			int shift = id == RED ? 16 : id == GREEN ? 8 : 0;

			interpreter->rgb = (interpreter->rgb & ~(255 << shift)) | (CLAMP (n, 0, 255) << shift);
			interpreter->pending_color = TRUE;
		} else if (star) {
			skip_group (interpreter);
		}

		return;
	}

	switch (id) {
	case ANSI: interpreter->doc_page = 1252; break;
	case MAC: interpreter->doc_page = 10000; break;
	case PC: interpreter->doc_page = 437; break;
	case PCA: interpreter->doc_page = 850; break;
	case ANSICPG:
		if (n > 0)
			interpreter->doc_page = n;
		break;
	case DEFF: interpreter->state.font = interpreter->default_font = n; break;
	case FROMHTML:
		if (!interpreter->begun)
			interpreter->from_html = on;
		break;
	case HTMLRTF: flag = HTMLRTF_BIT; break;
	case HTMLTAG:
		if (interpreter->extract && interpreter->from_html)
			interpreter->state.dest = HTML;
		else
			skip_group (interpreter);
		break;
	case FONTTBL:
		interpreter->state.dest = FONTS;
		interpreter->pending_font = FALSE;
		break;
	case COLORTBL:
		interpreter->state.dest = COLORS;
		interpreter->pending_color = FALSE;
		interpreter->rgb = 0;
		break;
	case FIELD:
		g_clear_pointer (&interpreter->field_link, g_bytes_unref);
		break;
	case FLDINST:
		interpreter->state.dest = INSTRUCTION;
		interpreter->field_units = 0;
		interpreter->reported &= ~BIT (FIELD_INSTRUCTION_TOO_LONG);
		g_string_truncate (interpreter->instruction, 0);
		break;
	case FLDRSLT:
		interpreter->state.dest = BODY;
		g_clear_pointer (&interpreter->state.link, g_bytes_unref);
		interpreter->state.link = interpreter->field_link ? g_bytes_ref (interpreter->field_link) : NULL;
		break;
	case UPR: interpreter->state.dest = ALTERNATIVE; break;
	case UD:
		if (interpreter->state.dest == ALTERNATIVE)
			interpreter->state.dest = BODY;
		break;
	case NESTTABLEPROPS: break;
	case SKIP: skip_group (interpreter); break;
	case FONT:
		if (interpreter->diagnostics && !(interpreter->reported & BIT (FONT_TABLE_LIMIT_EXCEEDED)) &&
		    !g_hash_table_contains (interpreter->fonts, GINT_TO_POINTER (n)))
			REPORT (interpreter, UNDEFINED_FONT);

		interpreter->state.font = n;
		break;
	case PLAIN:
		interpreter->state.flags &= ~(BOLD_BIT | ITALIC_BIT | UNDERLINE_BIT | STRIKE_BIT | HIDDEN_BIT);
		interpreter->state.vertical = GMIME_RTF_BASELINE;
		interpreter->state.fs = 24;
		interpreter->state.font = interpreter->default_font;
		interpreter->state.fg = interpreter->state.bg = 0;
		break;
	case BOLD: flag = BOLD_BIT; break;
	case ITALIC: flag = ITALIC_BIT; break;
	case UNDERLINE: flag = UNDERLINE_BIT; break;
	case ULNONE: interpreter->state.flags &= ~UNDERLINE_BIT; break;
	case STRIKE: flag = STRIKE_BIT; break;
	case HIDDEN: flag = HIDDEN_BIT; break;
	case SUPER: interpreter->state.vertical = GMIME_RTF_SUPERSCRIPT; break;
	case SUB: interpreter->state.vertical = GMIME_RTF_SUBSCRIPT; break;
	case NOSUPERSUB: interpreter->state.vertical = GMIME_RTF_BASELINE; break;
	case FS:
		if (interpreter->token.has_parameter && (n <= 0 || n > 3276))
			REPORT (interpreter, INVALID_FONT_SIZE);

		interpreter->state.fs = interpreter->token.has_parameter ? CLAMP (n, 0, 3276) : 24;
		break;
	case CF: case CB:
		if (n && !(interpreter->reported & BIT (COLOR_TABLE_LIMIT_EXCEEDED)) && (n < 0 || (guint) n >= interpreter->colors->len))
			REPORT (interpreter, UNDEFINED_COLOR);

		if (id == CF)
			interpreter->state.fg = MAX (n, 0);
		else
			interpreter->state.bg = MAX (n, 0);

		break;
	case U:
		if (interpreter->state.dest != ALTERNATIVE) {
			if (n < 0)
				n += 65536;

			if (n < 0 || n > 65535) {
				if (!interpreter->token.parameter_invalid)
					REPORT (interpreter, INVALID_PARAMETER);

				n = 0xfffd;
			}

			emit_utf16 (interpreter, (guint16) n);
		}

		interpreter->fallback = interpreter->state.uc;
		break;
	case UC: interpreter->state.uc = MAX (0, n); break;
	case PAR: structure (interpreter, GMIME_RTF_EVENT_PARAGRAPH); break;
	case LINE: structure (interpreter, GMIME_RTF_EVENT_LINE_BREAK); break;
	case TAB: emit_scalar (interpreter, '\t'); break;
	case PARD:
		interpreter->state.align = GMIME_RTF_ALIGN_LEFT;
		interpreter->state.flags &= ~TABLE_BIT;
		break;
	case QL: interpreter->state.align = GMIME_RTF_ALIGN_LEFT; break;
	case QC: interpreter->state.align = GMIME_RTF_ALIGN_CENTER; break;
	case QR: interpreter->state.align = GMIME_RTF_ALIGN_RIGHT; break;
	case QJ: interpreter->state.align = GMIME_RTF_ALIGN_JUSTIFY; break;
	case INTBL: interpreter->state.flags |= TABLE_BIT; break;
	case CELL: structure (interpreter, GMIME_RTF_EVENT_CELL); break;
	case ROW: structure (interpreter, GMIME_RTF_EVENT_ROW); break;
	case OBJATTPH:
		if (visible (interpreter)) {
			structure (interpreter, GMIME_RTF_EVENT_OBJECT_PLACEHOLDER);
			interpreter->placeholder = TRUE;
		}
		break;
	case CHARACTER: emit_scalar (interpreter, word->character); break;
	default:
		if (star)
			skip_group (interpreter);
		break;
	}

	if (flag)
		interpreter->state.flags = on ? interpreter->state.flags | flag : interpreter->state.flags & ~flag;
}

static void
text_token (Interpreter *interpreter, gboolean placeholder)
{
	const GMimeRtfToken *token = &interpreter->token;
	gsize i = 0, count = token->literal ? 1 : token->length;
	int page;

	interpreter->star = FALSE;

	if (interpreter->diagnostics && !(interpreter->reported & BIT (UNESCAPED_EIGHT_BIT_CHARACTER))) {
		for (i = 0; i < token->length; i++) {
			if (token->text[i] >= 128) {
				ONCE (interpreter, UNESCAPED_EIGHT_BIT_CHARACTER, i);
				break;
			}
		}
	}

	if (interpreter->state.dest == ALTERNATIVE)
		return;

	i = MIN (count, (gsize) interpreter->fallback);
	interpreter->fallback -= (int) i;

	if (!i && placeholder && count && token->text[0] == ' ')
		i++;

	if (i == count)
		return;

	if (token->literal) {
		decode (interpreter, TRUE);
		emit_scalar (interpreter, token->literal);
		return;
	}

	if (interpreter->state.dest == COLORS || interpreter->state.dest == FONTS) {
		decode (interpreter, TRUE);

		for (; i < count; i++) {
			if (token->text[i] != ';')
				continue;

			if (interpreter->state.dest == FONTS)
				commit_font (interpreter, i);
			else
				commit_color (interpreter, i);
		}

		return;
	}

	page = current_page (interpreter);

	for (; i < count && !interpreter->failed; i++) {
		if (token->text[i] >= 0x20 || token->text[i] == '\t')
			append_byte (interpreter, token->text[i], page);
	}
}

static void
control_symbol (Interpreter *interpreter)
{
	guint8 c = interpreter->token.value;

	if (interpreter->fallback) {
		interpreter->fallback--;
		return;
	}

	interpreter->star = c == '*';

	if (c == '\\' || c == '{' || c == '}') {
		/* Escaped syntax bytes may be DBCS trail bytes. */
		if (interpreter->state.dest != ALTERNATIVE)
			append_byte (interpreter, c, current_page (interpreter));
		return;
	}

	decode (interpreter, TRUE);

	if (interpreter->state.dest == ALTERNATIVE)
		return;

	switch (c) {
	case '~': emit_scalar (interpreter, 0xa0); break;
	case '_': emit_scalar (interpreter, interpreter->state.dest == HTML ? 0xad : 0x2011); break;
	case '\n': structure (interpreter, GMIME_RTF_EVENT_PARAGRAPH); break;
	case '\t': emit_scalar (interpreter, '\t'); break;
	}
}

static gboolean
trailing_padding (const GMimeRtfToken *token)
{
	gsize i;

	if (token->kind != GMIME_RTF_TEXT)
		return FALSE;

	for (i = 0; i < token->length; i++) {
		if (token->text[i] && token->text[i] != ' ' && token->text[i] != '\t')
			return FALSE;
	}

	return TRUE;
}

static void
process (Interpreter *interpreter)
{
	GMimeRtfToken *token = &interpreter->token;
	gboolean placeholder = interpreter->placeholder;
	const Word *word = NULL;

	if (token->kind == GMIME_RTF_CONTROL_WORD && !token->keyword_truncated)
		word = bsearch (token->keyword, words, G_N_ELEMENTS (words), sizeof words[0], compare_word);

	if (interpreter->diagnostics) {
		if (token->kind != GMIME_RTF_TEXT)
			interpreter->reported &= ~BIT (UNESCAPED_EIGHT_BIT_CHARACTER);

		if (interpreter->header_state < 2) {
			if (!interpreter->header_state && token->kind == GMIME_RTF_GROUP_START) {
				interpreter->header_state++;
			} else {
				if (interpreter->header_state != 1 || !word || word->id != RTF)
					REPORT (interpreter, MISSING_RTF_HEADER);

				interpreter->header_state = 2;
			}
		} else if (interpreter->root_closed && !interpreter->depth && !interpreter->skip && token->kind != GMIME_RTF_GROUP_END &&
			   token->kind != GMIME_RTF_EOF && !(interpreter->reported & BIT (CONTENT_AFTER_DOCUMENT)) && !trailing_padding (token)) {
			ONCE (interpreter, CONTENT_AFTER_DOCUMENT, 0);
		}
	}

	if (token->kind == GMIME_RTF_EOF) {
		decode (interpreter, TRUE);
		flush_surrogate (interpreter);
		dispatch (interpreter, GMIME_RTF_EVENT_BEGIN, NULL, 0);
		interpreter->finished = TRUE;
		return;
	}

	if (!interpreter->begun && ((token->kind != GMIME_RTF_GROUP_START && token->kind != GMIME_RTF_CONTROL_WORD) || ++interpreter->prefix_tokens > 10))
		dispatch (interpreter, GMIME_RTF_EVENT_BEGIN, NULL, 0);

	interpreter->placeholder = FALSE;

	if (interpreter->skip) {
		if (token->kind == GMIME_RTF_GROUP_START)
			interpreter->skip++;
		else if (token->kind == GMIME_RTF_GROUP_END && !--interpreter->skip && interpreter->pop_skipped)
			pop (interpreter);

		return;
	}

	switch (token->kind) {
	case GMIME_RTF_TEXT:
		text_token (interpreter, placeholder);
		return;
	case GMIME_RTF_HEX_CHAR:
		interpreter->star = FALSE;

		if (placeholder && token->value == 0x20)
			return;

		if (interpreter->fallback)
			interpreter->fallback--;
		else if (interpreter->state.dest != ALTERNATIVE)
			append_byte (interpreter, token->value, current_page (interpreter));

		return;
	case GMIME_RTF_CONTROL_SYMBOL:
		control_symbol (interpreter);
		return;
	default: break;
	}

	decode (interpreter, TRUE);

	if (token->kind == GMIME_RTF_CONTROL_WORD) {
		if (!word || word->id != U)
			flush_surrogate (interpreter);

		control_word (interpreter, word);
	} else {
		flush_surrogate (interpreter);
		interpreter->star = FALSE;
		interpreter->fallback = 0;

		if (token->kind == GMIME_RTF_GROUP_START)
			push (interpreter);
		else
			pop (interpreter);
	}
}

GMimeRtfInterpreter *
_g_mime_rtf_interpreter_new (GMimeStream *input, gboolean extract_html, guint max_group_depth,
			     guint max_fonts, guint max_colors, GMimeRtfDiagnostics *diagnostics,
			     GMimeRtfEventFunc callback, gpointer user_data)
{
	Interpreter *interpreter;

	g_return_val_if_fail (GMIME_IS_STREAM (input), NULL);
	g_return_val_if_fail (callback != NULL, NULL);

	interpreter = g_new0 (Interpreter, 1);
	interpreter->tokenizer = _g_mime_rtf_tokenizer_new (input);
	_g_mime_rtf_tokenizer_set_diagnostics (interpreter->tokenizer, diagnostics);
	interpreter->diagnostics = diagnostics;
	interpreter->callback = callback;
	interpreter->user_data = user_data;
	interpreter->extract = extract_html;
	interpreter->max_depth = max_group_depth;
	interpreter->max_fonts = max_fonts;
	interpreter->max_colors = max_colors;
	interpreter->state.fs = 24;
	interpreter->state.uc = 1;
	interpreter->doc_page = 1252;
	interpreter->stack = g_array_new (FALSE, FALSE, sizeof (Frame));
	interpreter->colors = g_array_new (FALSE, FALSE, sizeof (int));
	interpreter->fonts = g_hash_table_new (g_direct_hash, g_direct_equal);
	interpreter->instruction = g_string_new (NULL);

	return interpreter;
}

gboolean
_g_mime_rtf_interpreter_step (GMimeRtfInterpreter *interpreter, gboolean *eof, GError **error)
{
	g_return_val_if_fail (interpreter != NULL, FALSE);
	g_return_val_if_fail (eof != NULL, FALSE);

	*eof = FALSE;

	if (!interpreter->finished && !interpreter->failed) {
		if (!_g_mime_rtf_tokenizer_next (interpreter->tokenizer, &interpreter->token, error)) {
			interpreter->failed = TRUE;
			return FALSE;
		}

		interpreter->error = error;
		process (interpreter);
		interpreter->error = NULL;
	}

	*eof = interpreter->finished;

	return !interpreter->failed;
}

void
_g_mime_rtf_interpreter_free (GMimeRtfInterpreter *interpreter)
{
	guint k;

	if (!interpreter)
		return;

	for (k = 0; k < interpreter->ndecoders; k++) {
		if (interpreter->decoders[k].cd != (iconv_t) -1)
			g_mime_iconv_close (interpreter->decoders[k].cd);
	}

	for (k = 0; k < interpreter->stack->len; k++) {
		Frame *frame = &g_array_index (interpreter->stack, Frame, k);
		g_clear_pointer (&frame->state.link, g_bytes_unref);
	}

	g_clear_pointer (&interpreter->state.link, g_bytes_unref);
	g_clear_pointer (&interpreter->field_link, g_bytes_unref);
	g_array_unref (interpreter->stack);
	g_array_unref (interpreter->colors);
	g_hash_table_unref (interpreter->fonts);
	g_string_free (interpreter->instruction, TRUE);
	_g_mime_rtf_tokenizer_free (interpreter->tokenizer);
	g_free (interpreter);
}

gboolean
_g_mime_rtf_interpret (GMimeStream *input, gboolean extract_html, guint max_group_depth,
		       guint max_fonts, guint max_colors, GMimeRtfDiagnostics *diagnostics,
		       GMimeRtfEventFunc callback, gpointer user_data, GError **error)
{
	GMimeRtfInterpreter *interpreter;
	gboolean ok, eof;

	interpreter = _g_mime_rtf_interpreter_new (input, extract_html, max_group_depth, max_fonts, max_colors,
					 diagnostics, callback, user_data);

	if (!interpreter)
		return FALSE;

	do {
		ok = _g_mime_rtf_interpreter_step (interpreter, &eof, error);
	} while (ok && !eof);

	_g_mime_rtf_interpreter_free (interpreter);

	return ok;
}
