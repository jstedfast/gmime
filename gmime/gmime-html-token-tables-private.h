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

/* Tag, attribute and namespace tables ported from MimeKit/Text. */
static const char *const tag_names[] = {
	"Unknown",    "a",        "abbr",     "acronym",  "address",  "applet",    "area",     "article",  "aside",
	"audio",      "b",        "base",     "basefont", "bdi",      "bdo",       "bgsound",  "big",      "blink",
	"blockquote", "body",     "br",       "button",   "canvas",   "caption",   "center",   "cite",     "code",
	"col",        "colgroup", "command",  "!",        "datalist", "dd",        "del",      "details",  "dfn",
	"dialog",     "dir",      "div",      "dl",       "dt",       "em",        "embed",    "fieldset", "figcaption",
	"figure",     "font",     "footer",   "form",     "frame",    "frameset",  "h1",       "h2",       "h3",
	"h4",         "h5",       "h6",       "head",     "header",   "hr",        "html",     "i",        "iframe",
	"img",        "input",    "ins",      "isindex",  "kbd",      "keygen",    "label",    "legend",   "li",
	"link",       "listing",  "main",     "map",      "mark",     "marquee",   "menu",     "menuitem", "meta",
	"meter",      "nav",      "nextid",   "nobr",     "noembed",  "noframes",  "noscript", "object",   "ol",
	"optgroup",   "option",   "output",   "p",        "param",    "plaintext", "pre",      "progress", "q",
	"rp",         "rt",       "ruby",     "s",        "samp",     "script",    "section",  "select",   "small",
	"source",     "span",     "strike",   "strong",   "style",    "sub",       "summary",  "sup",      "table",
	"tbody",      "td",       "textarea", "tfoot",    "th",       "thead",     "time",     "title",    "tr",
	"track",      "tt",       "u",        "ul",       "var",      "video",     "wbr",      "xml",      "xmp"
};

static const char *const attribute_names[] = { "Unknown",      "abbr",        "accept",      "accept-charset",
					       "accesskey",    "action",      "align",       "alink",
					       "alt",          "archive",     "axis",        "background",
					       "bgcolor",      "border",      "cellpadding", "cellspacing",
					       "char",         "charoff",     "charset",     "checked",
					       "cite",         "class",       "classid",     "clear",
					       "code",         "codebase",    "codetype",    "color",
					       "cols",         "colspan",     "compact",     "content",
					       "coords",       "data",        "datetime",    "declare",
					       "defer",        "dir",         "disabled",    "dynsrc",
					       "enctype",      "face",        "for",         "frame",
					       "frameborder",  "headers",     "height",      "href",
					       "hreflang",     "hspace",      "http-equiv",  "id",
					       "ismap",        "label",       "lang",        "language",
					       "leftmargin",   "link",        "longdesc",    "lowsrc",
					       "marginheight", "marginwidth", "maxlength",   "media",
					       "method",       "multiple",    "name",        "nohref",
					       "noresize",     "noshade",     "nowrap",      "object",
					       "profile",      "prompt",      "readonly",    "rel",
					       "rev",          "rows",        "rowspan",     "rules",
					       "scheme",       "scope",       "scrolling",   "selected",
					       "shape",        "size",        "span",        "src",
					       "standby",      "start",       "style",       "summary",
					       "tabindex",     "target",      "text",        "title",
					       "topmargin",    "type",        "usemap",      "valign",
					       "value",        "valuetype",   "version",     "vlink",
					       "vspace",       "width",       "xmlns" };

static const char *const namespace_urls[] = {
	"http://www.w3.org/1999/xhtml", "http://www.w3.org/1998/Math/MathML",   "http://www.w3.org/2000/svg",
	"http://www.w3.org/1999/xlink", "http://www.w3.org/XML/1998/namespace", "http://www.w3.org/2000/xmlns/"
};
