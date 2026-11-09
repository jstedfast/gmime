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

#ifndef __GMIME_HTML_TOKEN_H__
#define __GMIME_HTML_TOKEN_H__

#include <glib-object.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

/**
 * GMimeHtmlTagId:
 * @GMIME_HTML_TAG_UNKNOWN: An unrecognized tag.
 * @GMIME_HTML_TAG_A: The a tag.
 * @GMIME_HTML_TAG_ABBR: The abbr tag.
 * @GMIME_HTML_TAG_ACRONYM: The acronym tag.
 * @GMIME_HTML_TAG_ADDRESS: The address tag.
 * @GMIME_HTML_TAG_APPLET: The applet tag.
 * @GMIME_HTML_TAG_AREA: The area tag.
 * @GMIME_HTML_TAG_ARTICLE: The article tag.
 * @GMIME_HTML_TAG_ASIDE: The aside tag.
 * @GMIME_HTML_TAG_AUDIO: The audio tag.
 * @GMIME_HTML_TAG_B: The b tag.
 * @GMIME_HTML_TAG_BASE: The base tag.
 * @GMIME_HTML_TAG_BASE_FONT: The basefont tag.
 * @GMIME_HTML_TAG_BDI: The bdi tag.
 * @GMIME_HTML_TAG_BDO: The bdo tag.
 * @GMIME_HTML_TAG_BG_SOUND: The bgsound tag.
 * @GMIME_HTML_TAG_BIG: The big tag.
 * @GMIME_HTML_TAG_BLINK: The blink tag.
 * @GMIME_HTML_TAG_BLOCK_QUOTE: The blockquote tag.
 * @GMIME_HTML_TAG_BODY: The body tag.
 * @GMIME_HTML_TAG_BR: The br tag.
 * @GMIME_HTML_TAG_BUTTON: The button tag.
 * @GMIME_HTML_TAG_CANVAS: The canvas tag.
 * @GMIME_HTML_TAG_CAPTION: The caption tag.
 * @GMIME_HTML_TAG_CENTER: The center tag.
 * @GMIME_HTML_TAG_CITE: The cite tag.
 * @GMIME_HTML_TAG_CODE: The code tag.
 * @GMIME_HTML_TAG_COL: The col tag.
 * @GMIME_HTML_TAG_COL_GROUP: The colgroup tag.
 * @GMIME_HTML_TAG_COMMAND: The command tag.
 * @GMIME_HTML_TAG_COMMENT: The comment marker, represented by an exclamation mark.
 * @GMIME_HTML_TAG_DATA_LIST: The datalist tag.
 * @GMIME_HTML_TAG_DD: The dd tag.
 * @GMIME_HTML_TAG_DEL: The del tag.
 * @GMIME_HTML_TAG_DETAILS: The details tag.
 * @GMIME_HTML_TAG_DFN: The dfn tag.
 * @GMIME_HTML_TAG_DIALOG: The dialog tag.
 * @GMIME_HTML_TAG_DIR: The dir tag.
 * @GMIME_HTML_TAG_DIV: The div tag.
 * @GMIME_HTML_TAG_DL: The dl tag.
 * @GMIME_HTML_TAG_DT: The dt tag.
 * @GMIME_HTML_TAG_EM: The em tag.
 * @GMIME_HTML_TAG_EMBED: The embed tag.
 * @GMIME_HTML_TAG_FIELD_SET: The fieldset tag.
 * @GMIME_HTML_TAG_FIG_CAPTION: The figcaption tag.
 * @GMIME_HTML_TAG_FIGURE: The figure tag.
 * @GMIME_HTML_TAG_FONT: The font tag.
 * @GMIME_HTML_TAG_FOOTER: The footer tag.
 * @GMIME_HTML_TAG_FORM: The form tag.
 * @GMIME_HTML_TAG_FRAME: The frame tag.
 * @GMIME_HTML_TAG_FRAME_SET: The frameset tag.
 * @GMIME_HTML_TAG_H1: The h1 tag.
 * @GMIME_HTML_TAG_H2: The h2 tag.
 * @GMIME_HTML_TAG_H3: The h3 tag.
 * @GMIME_HTML_TAG_H4: The h4 tag.
 * @GMIME_HTML_TAG_H5: The h5 tag.
 * @GMIME_HTML_TAG_H6: The h6 tag.
 * @GMIME_HTML_TAG_HEAD: The head tag.
 * @GMIME_HTML_TAG_HEADER: The header tag.
 * @GMIME_HTML_TAG_HR: The hr tag.
 * @GMIME_HTML_TAG_HTML: The html tag.
 * @GMIME_HTML_TAG_I: The i tag.
 * @GMIME_HTML_TAG_I_FRAME: The iframe tag.
 * @GMIME_HTML_TAG_IMAGE: The img tag.
 * @GMIME_HTML_TAG_INPUT: The input tag.
 * @GMIME_HTML_TAG_INS: The ins tag.
 * @GMIME_HTML_TAG_IS_INDEX: The isindex tag.
 * @GMIME_HTML_TAG_KBD: The kbd tag.
 * @GMIME_HTML_TAG_KEYGEN: The keygen tag.
 * @GMIME_HTML_TAG_LABEL: The label tag.
 * @GMIME_HTML_TAG_LEGEND: The legend tag.
 * @GMIME_HTML_TAG_LI: The li tag.
 * @GMIME_HTML_TAG_LINK: The link tag.
 * @GMIME_HTML_TAG_LISTING: The listing tag.
 * @GMIME_HTML_TAG_MAIN: The main tag.
 * @GMIME_HTML_TAG_MAP: The map tag.
 * @GMIME_HTML_TAG_MARK: The mark tag.
 * @GMIME_HTML_TAG_MARQUEE: The marquee tag.
 * @GMIME_HTML_TAG_MENU: The menu tag.
 * @GMIME_HTML_TAG_MENU_ITEM: The menuitem tag.
 * @GMIME_HTML_TAG_META: The meta tag.
 * @GMIME_HTML_TAG_METER: The meter tag.
 * @GMIME_HTML_TAG_NAV: The nav tag.
 * @GMIME_HTML_TAG_NEXT_ID: The nextid tag.
 * @GMIME_HTML_TAG_NO_BR: The nobr tag.
 * @GMIME_HTML_TAG_NO_EMBED: The noembed tag.
 * @GMIME_HTML_TAG_NO_FRAMES: The noframes tag.
 * @GMIME_HTML_TAG_NO_SCRIPT: The noscript tag.
 * @GMIME_HTML_TAG_OBJECT: The object tag.
 * @GMIME_HTML_TAG_OL: The ol tag.
 * @GMIME_HTML_TAG_OPT_GROUP: The optgroup tag.
 * @GMIME_HTML_TAG_OPTION: The option tag.
 * @GMIME_HTML_TAG_OUTPUT: The output tag.
 * @GMIME_HTML_TAG_P: The p tag.
 * @GMIME_HTML_TAG_PARAM: The param tag.
 * @GMIME_HTML_TAG_PLAIN_TEXT: The plaintext tag.
 * @GMIME_HTML_TAG_PRE: The pre tag.
 * @GMIME_HTML_TAG_PROGRESS: The progress tag.
 * @GMIME_HTML_TAG_Q: The q tag.
 * @GMIME_HTML_TAG_RP: The rp tag.
 * @GMIME_HTML_TAG_RT: The rt tag.
 * @GMIME_HTML_TAG_RUBY: The ruby tag.
 * @GMIME_HTML_TAG_S: The s tag.
 * @GMIME_HTML_TAG_SAMP: The samp tag.
 * @GMIME_HTML_TAG_SCRIPT: The script tag.
 * @GMIME_HTML_TAG_SECTION: The section tag.
 * @GMIME_HTML_TAG_SELECT: The select tag.
 * @GMIME_HTML_TAG_SMALL: The small tag.
 * @GMIME_HTML_TAG_SOURCE: The source tag.
 * @GMIME_HTML_TAG_SPAN: The span tag.
 * @GMIME_HTML_TAG_STRIKE: The strike tag.
 * @GMIME_HTML_TAG_STRONG: The strong tag.
 * @GMIME_HTML_TAG_STYLE: The style tag.
 * @GMIME_HTML_TAG_SUB: The sub tag.
 * @GMIME_HTML_TAG_SUMMARY: The summary tag.
 * @GMIME_HTML_TAG_SUP: The sup tag.
 * @GMIME_HTML_TAG_TABLE: The table tag.
 * @GMIME_HTML_TAG_T_BODY: The tbody tag.
 * @GMIME_HTML_TAG_TD: The td tag.
 * @GMIME_HTML_TAG_TEXT_AREA: The textarea tag.
 * @GMIME_HTML_TAG_TFOOT: The tfoot tag.
 * @GMIME_HTML_TAG_TH: The th tag.
 * @GMIME_HTML_TAG_T_HEAD: The thead tag.
 * @GMIME_HTML_TAG_TIME: The time tag.
 * @GMIME_HTML_TAG_TITLE: The title tag.
 * @GMIME_HTML_TAG_TR: The tr tag.
 * @GMIME_HTML_TAG_TRACK: The track tag.
 * @GMIME_HTML_TAG_TT: The tt tag.
 * @GMIME_HTML_TAG_U: The u tag.
 * @GMIME_HTML_TAG_UL: The ul tag.
 * @GMIME_HTML_TAG_VAR: The var tag.
 * @GMIME_HTML_TAG_VIDEO: The video tag.
 * @GMIME_HTML_TAG_WBR: The wbr tag.
 * @GMIME_HTML_TAG_XML: The xml tag.
 * @GMIME_HTML_TAG_XMP: The xmp tag.
 *
 * Known tag names, including legacy HTML elements. Names are matched without
 * regard to ASCII case.
 */
typedef enum {
	GMIME_HTML_TAG_UNKNOWN,
	GMIME_HTML_TAG_A,
	GMIME_HTML_TAG_ABBR,
	GMIME_HTML_TAG_ACRONYM,
	GMIME_HTML_TAG_ADDRESS,
	GMIME_HTML_TAG_APPLET,
	GMIME_HTML_TAG_AREA,
	GMIME_HTML_TAG_ARTICLE,
	GMIME_HTML_TAG_ASIDE,
	GMIME_HTML_TAG_AUDIO,
	GMIME_HTML_TAG_B,
	GMIME_HTML_TAG_BASE,
	GMIME_HTML_TAG_BASE_FONT,
	GMIME_HTML_TAG_BDI,
	GMIME_HTML_TAG_BDO,
	GMIME_HTML_TAG_BG_SOUND,
	GMIME_HTML_TAG_BIG,
	GMIME_HTML_TAG_BLINK,
	GMIME_HTML_TAG_BLOCK_QUOTE,
	GMIME_HTML_TAG_BODY,
	GMIME_HTML_TAG_BR,
	GMIME_HTML_TAG_BUTTON,
	GMIME_HTML_TAG_CANVAS,
	GMIME_HTML_TAG_CAPTION,
	GMIME_HTML_TAG_CENTER,
	GMIME_HTML_TAG_CITE,
	GMIME_HTML_TAG_CODE,
	GMIME_HTML_TAG_COL,
	GMIME_HTML_TAG_COL_GROUP,
	GMIME_HTML_TAG_COMMAND,
	GMIME_HTML_TAG_COMMENT,
	GMIME_HTML_TAG_DATA_LIST,
	GMIME_HTML_TAG_DD,
	GMIME_HTML_TAG_DEL,
	GMIME_HTML_TAG_DETAILS,
	GMIME_HTML_TAG_DFN,
	GMIME_HTML_TAG_DIALOG,
	GMIME_HTML_TAG_DIR,
	GMIME_HTML_TAG_DIV,
	GMIME_HTML_TAG_DL,
	GMIME_HTML_TAG_DT,
	GMIME_HTML_TAG_EM,
	GMIME_HTML_TAG_EMBED,
	GMIME_HTML_TAG_FIELD_SET,
	GMIME_HTML_TAG_FIG_CAPTION,
	GMIME_HTML_TAG_FIGURE,
	GMIME_HTML_TAG_FONT,
	GMIME_HTML_TAG_FOOTER,
	GMIME_HTML_TAG_FORM,
	GMIME_HTML_TAG_FRAME,
	GMIME_HTML_TAG_FRAME_SET,
	GMIME_HTML_TAG_H1,
	GMIME_HTML_TAG_H2,
	GMIME_HTML_TAG_H3,
	GMIME_HTML_TAG_H4,
	GMIME_HTML_TAG_H5,
	GMIME_HTML_TAG_H6,
	GMIME_HTML_TAG_HEAD,
	GMIME_HTML_TAG_HEADER,
	GMIME_HTML_TAG_HR,
	GMIME_HTML_TAG_HTML,
	GMIME_HTML_TAG_I,
	GMIME_HTML_TAG_I_FRAME,
	GMIME_HTML_TAG_IMAGE,
	GMIME_HTML_TAG_INPUT,
	GMIME_HTML_TAG_INS,
	GMIME_HTML_TAG_IS_INDEX,
	GMIME_HTML_TAG_KBD,
	GMIME_HTML_TAG_KEYGEN,
	GMIME_HTML_TAG_LABEL,
	GMIME_HTML_TAG_LEGEND,
	GMIME_HTML_TAG_LI,
	GMIME_HTML_TAG_LINK,
	GMIME_HTML_TAG_LISTING,
	GMIME_HTML_TAG_MAIN,
	GMIME_HTML_TAG_MAP,
	GMIME_HTML_TAG_MARK,
	GMIME_HTML_TAG_MARQUEE,
	GMIME_HTML_TAG_MENU,
	GMIME_HTML_TAG_MENU_ITEM,
	GMIME_HTML_TAG_META,
	GMIME_HTML_TAG_METER,
	GMIME_HTML_TAG_NAV,
	GMIME_HTML_TAG_NEXT_ID,
	GMIME_HTML_TAG_NO_BR,
	GMIME_HTML_TAG_NO_EMBED,
	GMIME_HTML_TAG_NO_FRAMES,
	GMIME_HTML_TAG_NO_SCRIPT,
	GMIME_HTML_TAG_OBJECT,
	GMIME_HTML_TAG_OL,
	GMIME_HTML_TAG_OPT_GROUP,
	GMIME_HTML_TAG_OPTION,
	GMIME_HTML_TAG_OUTPUT,
	GMIME_HTML_TAG_P,
	GMIME_HTML_TAG_PARAM,
	GMIME_HTML_TAG_PLAIN_TEXT,
	GMIME_HTML_TAG_PRE,
	GMIME_HTML_TAG_PROGRESS,
	GMIME_HTML_TAG_Q,
	GMIME_HTML_TAG_RP,
	GMIME_HTML_TAG_RT,
	GMIME_HTML_TAG_RUBY,
	GMIME_HTML_TAG_S,
	GMIME_HTML_TAG_SAMP,
	GMIME_HTML_TAG_SCRIPT,
	GMIME_HTML_TAG_SECTION,
	GMIME_HTML_TAG_SELECT,
	GMIME_HTML_TAG_SMALL,
	GMIME_HTML_TAG_SOURCE,
	GMIME_HTML_TAG_SPAN,
	GMIME_HTML_TAG_STRIKE,
	GMIME_HTML_TAG_STRONG,
	GMIME_HTML_TAG_STYLE,
	GMIME_HTML_TAG_SUB,
	GMIME_HTML_TAG_SUMMARY,
	GMIME_HTML_TAG_SUP,
	GMIME_HTML_TAG_TABLE,
	GMIME_HTML_TAG_T_BODY,
	GMIME_HTML_TAG_TD,
	GMIME_HTML_TAG_TEXT_AREA,
	GMIME_HTML_TAG_TFOOT,
	GMIME_HTML_TAG_TH,
	GMIME_HTML_TAG_T_HEAD,
	GMIME_HTML_TAG_TIME,
	GMIME_HTML_TAG_TITLE,
	GMIME_HTML_TAG_TR,
	GMIME_HTML_TAG_TRACK,
	GMIME_HTML_TAG_TT,
	GMIME_HTML_TAG_U,
	GMIME_HTML_TAG_UL,
	GMIME_HTML_TAG_VAR,
	GMIME_HTML_TAG_VIDEO,
	GMIME_HTML_TAG_WBR,
	GMIME_HTML_TAG_XML,
	GMIME_HTML_TAG_XMP
} GMimeHtmlTagId;

/**
 * GMimeHtmlAttributeId:
 * @GMIME_HTML_ATTRIBUTE_UNKNOWN: An unrecognized attribute.
 * @GMIME_HTML_ATTRIBUTE_ABBR: The abbr attribute.
 * @GMIME_HTML_ATTRIBUTE_ACCEPT: The accept attribute.
 * @GMIME_HTML_ATTRIBUTE_ACCEPT_CHARSET: The accept-charset attribute.
 * @GMIME_HTML_ATTRIBUTE_ACCESS_KEY: The accesskey attribute.
 * @GMIME_HTML_ATTRIBUTE_ACTION: The action attribute.
 * @GMIME_HTML_ATTRIBUTE_ALIGN: The align attribute.
 * @GMIME_HTML_ATTRIBUTE_ALINK: The alink attribute.
 * @GMIME_HTML_ATTRIBUTE_ALT: The alt attribute.
 * @GMIME_HTML_ATTRIBUTE_ARCHIVE: The archive attribute.
 * @GMIME_HTML_ATTRIBUTE_AXIS: The axis attribute.
 * @GMIME_HTML_ATTRIBUTE_BACKGROUND: The background attribute.
 * @GMIME_HTML_ATTRIBUTE_BG_COLOR: The bgcolor attribute.
 * @GMIME_HTML_ATTRIBUTE_BORDER: The border attribute.
 * @GMIME_HTML_ATTRIBUTE_CELL_PADDING: The cellpadding attribute.
 * @GMIME_HTML_ATTRIBUTE_CELL_SPACING: The cellspacing attribute.
 * @GMIME_HTML_ATTRIBUTE_CHAR: The char attribute.
 * @GMIME_HTML_ATTRIBUTE_CHAR_OFF: The charoff attribute.
 * @GMIME_HTML_ATTRIBUTE_CHARSET: The charset attribute.
 * @GMIME_HTML_ATTRIBUTE_CHECKED: The checked attribute.
 * @GMIME_HTML_ATTRIBUTE_CITE: The cite attribute.
 * @GMIME_HTML_ATTRIBUTE_CLASS: The class attribute.
 * @GMIME_HTML_ATTRIBUTE_CLASS_ID: The classid attribute.
 * @GMIME_HTML_ATTRIBUTE_CLEAR: The clear attribute.
 * @GMIME_HTML_ATTRIBUTE_CODE: The code attribute.
 * @GMIME_HTML_ATTRIBUTE_CODE_BASE: The codebase attribute.
 * @GMIME_HTML_ATTRIBUTE_CODE_TYPE: The codetype attribute.
 * @GMIME_HTML_ATTRIBUTE_COLOR: The color attribute.
 * @GMIME_HTML_ATTRIBUTE_COLS: The cols attribute.
 * @GMIME_HTML_ATTRIBUTE_COL_SPAN: The colspan attribute.
 * @GMIME_HTML_ATTRIBUTE_COMPACT: The compact attribute.
 * @GMIME_HTML_ATTRIBUTE_CONTENT: The content attribute.
 * @GMIME_HTML_ATTRIBUTE_COORDS: The coords attribute.
 * @GMIME_HTML_ATTRIBUTE_DATA: The data attribute.
 * @GMIME_HTML_ATTRIBUTE_DATE_TIME: The datetime attribute.
 * @GMIME_HTML_ATTRIBUTE_DECLARE: The declare attribute.
 * @GMIME_HTML_ATTRIBUTE_DEFER: The defer attribute.
 * @GMIME_HTML_ATTRIBUTE_DIR: The dir attribute.
 * @GMIME_HTML_ATTRIBUTE_DISABLED: The disabled attribute.
 * @GMIME_HTML_ATTRIBUTE_DYN_SRC: The dynsrc attribute.
 * @GMIME_HTML_ATTRIBUTE_ENC_TYPE: The enctype attribute.
 * @GMIME_HTML_ATTRIBUTE_FACE: The face attribute.
 * @GMIME_HTML_ATTRIBUTE_FOR: The for attribute.
 * @GMIME_HTML_ATTRIBUTE_FRAME: The frame attribute.
 * @GMIME_HTML_ATTRIBUTE_FRAME_BORDER: The frameborder attribute.
 * @GMIME_HTML_ATTRIBUTE_HEADERS: The headers attribute.
 * @GMIME_HTML_ATTRIBUTE_HEIGHT: The height attribute.
 * @GMIME_HTML_ATTRIBUTE_HREF: The href attribute.
 * @GMIME_HTML_ATTRIBUTE_HREF_LANG: The hreflang attribute.
 * @GMIME_HTML_ATTRIBUTE_HSPACE: The hspace attribute.
 * @GMIME_HTML_ATTRIBUTE_HTTP_EQUIV: The http-equiv attribute.
 * @GMIME_HTML_ATTRIBUTE_ID: The id attribute.
 * @GMIME_HTML_ATTRIBUTE_IS_MAP: The ismap attribute.
 * @GMIME_HTML_ATTRIBUTE_LABEL: The label attribute.
 * @GMIME_HTML_ATTRIBUTE_LANG: The lang attribute.
 * @GMIME_HTML_ATTRIBUTE_LANGUAGE: The language attribute.
 * @GMIME_HTML_ATTRIBUTE_LEFT_MARGIN: The leftmargin attribute.
 * @GMIME_HTML_ATTRIBUTE_LINK: The link attribute.
 * @GMIME_HTML_ATTRIBUTE_LONG_DESC: The longdesc attribute.
 * @GMIME_HTML_ATTRIBUTE_LOW_SRC: The lowsrc attribute.
 * @GMIME_HTML_ATTRIBUTE_MARGIN_HEIGHT: The marginheight attribute.
 * @GMIME_HTML_ATTRIBUTE_MARGIN_WIDTH: The marginwidth attribute.
 * @GMIME_HTML_ATTRIBUTE_MAX_LENGTH: The maxlength attribute.
 * @GMIME_HTML_ATTRIBUTE_MEDIA: The media attribute.
 * @GMIME_HTML_ATTRIBUTE_METHOD: The method attribute.
 * @GMIME_HTML_ATTRIBUTE_MULTIPLE: The multiple attribute.
 * @GMIME_HTML_ATTRIBUTE_NAME: The name attribute.
 * @GMIME_HTML_ATTRIBUTE_NO_HREF: The nohref attribute.
 * @GMIME_HTML_ATTRIBUTE_NO_RESIZE: The noresize attribute.
 * @GMIME_HTML_ATTRIBUTE_NO_SHADE: The noshade attribute.
 * @GMIME_HTML_ATTRIBUTE_NO_WRAP: The nowrap attribute.
 * @GMIME_HTML_ATTRIBUTE_OBJECT: The object attribute.
 * @GMIME_HTML_ATTRIBUTE_PROFILE: The profile attribute.
 * @GMIME_HTML_ATTRIBUTE_PROMPT: The prompt attribute.
 * @GMIME_HTML_ATTRIBUTE_READ_ONLY: The readonly attribute.
 * @GMIME_HTML_ATTRIBUTE_REL: The rel attribute.
 * @GMIME_HTML_ATTRIBUTE_REV: The rev attribute.
 * @GMIME_HTML_ATTRIBUTE_ROWS: The rows attribute.
 * @GMIME_HTML_ATTRIBUTE_ROW_SPAN: The rowspan attribute.
 * @GMIME_HTML_ATTRIBUTE_RULES: The rules attribute.
 * @GMIME_HTML_ATTRIBUTE_SCHEME: The scheme attribute.
 * @GMIME_HTML_ATTRIBUTE_SCOPE: The scope attribute.
 * @GMIME_HTML_ATTRIBUTE_SCROLLING: The scrolling attribute.
 * @GMIME_HTML_ATTRIBUTE_SELECTED: The selected attribute.
 * @GMIME_HTML_ATTRIBUTE_SHAPE: The shape attribute.
 * @GMIME_HTML_ATTRIBUTE_SIZE: The size attribute.
 * @GMIME_HTML_ATTRIBUTE_SPAN: The span attribute.
 * @GMIME_HTML_ATTRIBUTE_SRC: The src attribute.
 * @GMIME_HTML_ATTRIBUTE_STAND_BY: The standby attribute.
 * @GMIME_HTML_ATTRIBUTE_START: The start attribute.
 * @GMIME_HTML_ATTRIBUTE_STYLE: The style attribute.
 * @GMIME_HTML_ATTRIBUTE_SUMMARY: The summary attribute.
 * @GMIME_HTML_ATTRIBUTE_TAB_INDEX: The tabindex attribute.
 * @GMIME_HTML_ATTRIBUTE_TARGET: The target attribute.
 * @GMIME_HTML_ATTRIBUTE_TEXT: The text attribute.
 * @GMIME_HTML_ATTRIBUTE_TITLE: The title attribute.
 * @GMIME_HTML_ATTRIBUTE_TOP_MARGIN: The topmargin attribute.
 * @GMIME_HTML_ATTRIBUTE_TYPE: The type attribute.
 * @GMIME_HTML_ATTRIBUTE_USE_MAP: The usemap attribute.
 * @GMIME_HTML_ATTRIBUTE_VALIGN: The valign attribute.
 * @GMIME_HTML_ATTRIBUTE_VALUE: The value attribute.
 * @GMIME_HTML_ATTRIBUTE_VALUE_TYPE: The valuetype attribute.
 * @GMIME_HTML_ATTRIBUTE_VERSION: The version attribute.
 * @GMIME_HTML_ATTRIBUTE_VLINK: The vlink attribute.
 * @GMIME_HTML_ATTRIBUTE_VSPACE: The vspace attribute.
 * @GMIME_HTML_ATTRIBUTE_WIDTH: The width attribute.
 * @GMIME_HTML_ATTRIBUTE_XML_NS: The xmlns attribute.
 *
 * Known attribute names, including legacy HTML attributes. Names are matched
 * without regard to ASCII case.
 */
typedef enum {
	GMIME_HTML_ATTRIBUTE_UNKNOWN,
	GMIME_HTML_ATTRIBUTE_ABBR,
	GMIME_HTML_ATTRIBUTE_ACCEPT,
	GMIME_HTML_ATTRIBUTE_ACCEPT_CHARSET,
	GMIME_HTML_ATTRIBUTE_ACCESS_KEY,
	GMIME_HTML_ATTRIBUTE_ACTION,
	GMIME_HTML_ATTRIBUTE_ALIGN,
	GMIME_HTML_ATTRIBUTE_ALINK,
	GMIME_HTML_ATTRIBUTE_ALT,
	GMIME_HTML_ATTRIBUTE_ARCHIVE,
	GMIME_HTML_ATTRIBUTE_AXIS,
	GMIME_HTML_ATTRIBUTE_BACKGROUND,
	GMIME_HTML_ATTRIBUTE_BG_COLOR,
	GMIME_HTML_ATTRIBUTE_BORDER,
	GMIME_HTML_ATTRIBUTE_CELL_PADDING,
	GMIME_HTML_ATTRIBUTE_CELL_SPACING,
	GMIME_HTML_ATTRIBUTE_CHAR,
	GMIME_HTML_ATTRIBUTE_CHAR_OFF,
	GMIME_HTML_ATTRIBUTE_CHARSET,
	GMIME_HTML_ATTRIBUTE_CHECKED,
	GMIME_HTML_ATTRIBUTE_CITE,
	GMIME_HTML_ATTRIBUTE_CLASS,
	GMIME_HTML_ATTRIBUTE_CLASS_ID,
	GMIME_HTML_ATTRIBUTE_CLEAR,
	GMIME_HTML_ATTRIBUTE_CODE,
	GMIME_HTML_ATTRIBUTE_CODE_BASE,
	GMIME_HTML_ATTRIBUTE_CODE_TYPE,
	GMIME_HTML_ATTRIBUTE_COLOR,
	GMIME_HTML_ATTRIBUTE_COLS,
	GMIME_HTML_ATTRIBUTE_COL_SPAN,
	GMIME_HTML_ATTRIBUTE_COMPACT,
	GMIME_HTML_ATTRIBUTE_CONTENT,
	GMIME_HTML_ATTRIBUTE_COORDS,
	GMIME_HTML_ATTRIBUTE_DATA,
	GMIME_HTML_ATTRIBUTE_DATE_TIME,
	GMIME_HTML_ATTRIBUTE_DECLARE,
	GMIME_HTML_ATTRIBUTE_DEFER,
	GMIME_HTML_ATTRIBUTE_DIR,
	GMIME_HTML_ATTRIBUTE_DISABLED,
	GMIME_HTML_ATTRIBUTE_DYN_SRC,
	GMIME_HTML_ATTRIBUTE_ENC_TYPE,
	GMIME_HTML_ATTRIBUTE_FACE,
	GMIME_HTML_ATTRIBUTE_FOR,
	GMIME_HTML_ATTRIBUTE_FRAME,
	GMIME_HTML_ATTRIBUTE_FRAME_BORDER,
	GMIME_HTML_ATTRIBUTE_HEADERS,
	GMIME_HTML_ATTRIBUTE_HEIGHT,
	GMIME_HTML_ATTRIBUTE_HREF,
	GMIME_HTML_ATTRIBUTE_HREF_LANG,
	GMIME_HTML_ATTRIBUTE_HSPACE,
	GMIME_HTML_ATTRIBUTE_HTTP_EQUIV,
	GMIME_HTML_ATTRIBUTE_ID,
	GMIME_HTML_ATTRIBUTE_IS_MAP,
	GMIME_HTML_ATTRIBUTE_LABEL,
	GMIME_HTML_ATTRIBUTE_LANG,
	GMIME_HTML_ATTRIBUTE_LANGUAGE,
	GMIME_HTML_ATTRIBUTE_LEFT_MARGIN,
	GMIME_HTML_ATTRIBUTE_LINK,
	GMIME_HTML_ATTRIBUTE_LONG_DESC,
	GMIME_HTML_ATTRIBUTE_LOW_SRC,
	GMIME_HTML_ATTRIBUTE_MARGIN_HEIGHT,
	GMIME_HTML_ATTRIBUTE_MARGIN_WIDTH,
	GMIME_HTML_ATTRIBUTE_MAX_LENGTH,
	GMIME_HTML_ATTRIBUTE_MEDIA,
	GMIME_HTML_ATTRIBUTE_METHOD,
	GMIME_HTML_ATTRIBUTE_MULTIPLE,
	GMIME_HTML_ATTRIBUTE_NAME,
	GMIME_HTML_ATTRIBUTE_NO_HREF,
	GMIME_HTML_ATTRIBUTE_NO_RESIZE,
	GMIME_HTML_ATTRIBUTE_NO_SHADE,
	GMIME_HTML_ATTRIBUTE_NO_WRAP,
	GMIME_HTML_ATTRIBUTE_OBJECT,
	GMIME_HTML_ATTRIBUTE_PROFILE,
	GMIME_HTML_ATTRIBUTE_PROMPT,
	GMIME_HTML_ATTRIBUTE_READ_ONLY,
	GMIME_HTML_ATTRIBUTE_REL,
	GMIME_HTML_ATTRIBUTE_REV,
	GMIME_HTML_ATTRIBUTE_ROWS,
	GMIME_HTML_ATTRIBUTE_ROW_SPAN,
	GMIME_HTML_ATTRIBUTE_RULES,
	GMIME_HTML_ATTRIBUTE_SCHEME,
	GMIME_HTML_ATTRIBUTE_SCOPE,
	GMIME_HTML_ATTRIBUTE_SCROLLING,
	GMIME_HTML_ATTRIBUTE_SELECTED,
	GMIME_HTML_ATTRIBUTE_SHAPE,
	GMIME_HTML_ATTRIBUTE_SIZE,
	GMIME_HTML_ATTRIBUTE_SPAN,
	GMIME_HTML_ATTRIBUTE_SRC,
	GMIME_HTML_ATTRIBUTE_STAND_BY,
	GMIME_HTML_ATTRIBUTE_START,
	GMIME_HTML_ATTRIBUTE_STYLE,
	GMIME_HTML_ATTRIBUTE_SUMMARY,
	GMIME_HTML_ATTRIBUTE_TAB_INDEX,
	GMIME_HTML_ATTRIBUTE_TARGET,
	GMIME_HTML_ATTRIBUTE_TEXT,
	GMIME_HTML_ATTRIBUTE_TITLE,
	GMIME_HTML_ATTRIBUTE_TOP_MARGIN,
	GMIME_HTML_ATTRIBUTE_TYPE,
	GMIME_HTML_ATTRIBUTE_USE_MAP,
	GMIME_HTML_ATTRIBUTE_VALIGN,
	GMIME_HTML_ATTRIBUTE_VALUE,
	GMIME_HTML_ATTRIBUTE_VALUE_TYPE,
	GMIME_HTML_ATTRIBUTE_VERSION,
	GMIME_HTML_ATTRIBUTE_VLINK,
	GMIME_HTML_ATTRIBUTE_VSPACE,
	GMIME_HTML_ATTRIBUTE_WIDTH,
	GMIME_HTML_ATTRIBUTE_XML_NS
} GMimeHtmlAttributeId;

/**
 * GMimeHtmlNamespace:
 * @GMIME_HTML_NAMESPACE_HTML: The HTML namespace.
 * @GMIME_HTML_NAMESPACE_MATH_ML: The MathML namespace.
 * @GMIME_HTML_NAMESPACE_SVG: The SVG namespace.
 * @GMIME_HTML_NAMESPACE_X_LINK: The XLink namespace.
 * @GMIME_HTML_NAMESPACE_XML: The XML namespace.
 * @GMIME_HTML_NAMESPACE_XML_NS: The XML namespace-declaration namespace.
 *
 * Namespaces recognized in HTML and foreign content.
 */
typedef enum {
	GMIME_HTML_NAMESPACE_HTML,
	GMIME_HTML_NAMESPACE_MATH_ML,
	GMIME_HTML_NAMESPACE_SVG,
	GMIME_HTML_NAMESPACE_X_LINK,
	GMIME_HTML_NAMESPACE_XML,
	GMIME_HTML_NAMESPACE_XML_NS
} GMimeHtmlNamespace;

/**
 * GMimeHtmlTokenizerState:
 * @GMIME_HTML_TOKENIZER_STATE_DATA: Ordinary character data and markup.
 * @GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_DATA: A character reference in ordinary data.
 * @GMIME_HTML_TOKENIZER_STATE_RC_DATA: RCDATA, which recognizes character references and matching end tags.
 * @GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_RC_DATA: A character reference in RCDATA.
 * @GMIME_HTML_TOKENIZER_STATE_RAW_TEXT: Raw text, which recognizes matching end tags but not character references.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA: Script content.
 * @GMIME_HTML_TOKENIZER_STATE_PLAIN_TEXT: Plain text, without markup or character-reference recognition.
 * @GMIME_HTML_TOKENIZER_STATE_TAG_OPEN: Immediately after a tag-opening less-than sign.
 * @GMIME_HTML_TOKENIZER_STATE_END_TAG_OPEN: Immediately after a closing-tag opener.
 * @GMIME_HTML_TOKENIZER_STATE_TAG_NAME: Reading a tag name.
 * @GMIME_HTML_TOKENIZER_STATE_RC_DATA_LESS_THAN: After a less-than sign in RCDATA.
 * @GMIME_HTML_TOKENIZER_STATE_RC_DATA_END_TAG_OPEN: After an end-tag opener in RCDATA.
 * @GMIME_HTML_TOKENIZER_STATE_RC_DATA_END_TAG_NAME: Reading a possible RCDATA end-tag name.
 * @GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_LESS_THAN: After a less-than sign in raw text.
 * @GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_END_TAG_OPEN: After an end-tag opener in raw text.
 * @GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_END_TAG_NAME: Reading a possible raw-text end-tag name.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_LESS_THAN: After a less-than sign in script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_END_TAG_OPEN: After an end-tag opener in script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_END_TAG_NAME: Reading a possible script end-tag name.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPE_START: Beginning a possible comment-like script escape.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPE_START_DASH: After the first dash of a possible script escape.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED: Escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_DASH: After one dash in escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_DASH_DASH: After two dashes in escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_LESS_THAN: After a less-than sign in escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_END_TAG_OPEN: After an end-tag opener in escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_END_TAG_NAME: Reading a possible end-tag name in escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPE_START: Checking for entry into double-escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED: Double-escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_DASH: After one dash in double-escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH: After two dashes in double-escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN: After a less-than sign in double-escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPE_END: Checking for exit from double-escaped script data.
 * @GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME: Before an attribute name.
 * @GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_NAME: Reading an attribute name.
 * @GMIME_HTML_TOKENIZER_STATE_AFTER_ATTRIBUTE_NAME: After an attribute name.
 * @GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_VALUE: Before an attribute value.
 * @GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_DOUBLE_QUOTED: Reading a double-quoted attribute value.
 * @GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_SINGLE_QUOTED: Reading a single-quoted attribute value.
 * @GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_UNQUOTED: Reading an unquoted attribute value.
 * @GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE: A character reference in an attribute value.
 * @GMIME_HTML_TOKENIZER_STATE_AFTER_ATTRIBUTE_VALUE_QUOTED: After a quoted attribute value.
 * @GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG: After the slash of a possible self-closing tag.
 * @GMIME_HTML_TOKENIZER_STATE_BOGUS_COMMENT: Recovering malformed markup as a bogus comment.
 * @GMIME_HTML_TOKENIZER_STATE_MARKUP_DECLARATION_OPEN: Checking a markup-declaration opener.
 * @GMIME_HTML_TOKENIZER_STATE_COMMENT_START: Immediately after a comment opener.
 * @GMIME_HTML_TOKENIZER_STATE_COMMENT_START_DASH: After an initial dash in a comment.
 * @GMIME_HTML_TOKENIZER_STATE_COMMENT: Reading comment text.
 * @GMIME_HTML_TOKENIZER_STATE_COMMENT_END_DASH: After a possible comment-ending dash.
 * @GMIME_HTML_TOKENIZER_STATE_COMMENT_END: After a possible pair of comment-ending dashes.
 * @GMIME_HTML_TOKENIZER_STATE_COMMENT_END_BANG: After an exclamation mark following comment-ending dashes.
 * @GMIME_HTML_TOKENIZER_STATE_DOC_TYPE: Immediately after the DOCTYPE keyword.
 * @GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_NAME: Before a DOCTYPE name.
 * @GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_NAME: Reading a DOCTYPE name.
 * @GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_NAME: After a DOCTYPE name.
 * @GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_PUBLIC_KEYWORD: After the DOCTYPE PUBLIC keyword.
 * @GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER: Before a DOCTYPE public identifier.
 * @GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_PUBLIC_IDENTIFIER_QUOTED: Reading a quoted DOCTYPE public identifier.
 * @GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_PUBLIC_IDENTIFIER: After a DOCTYPE public identifier.
 * @GMIME_HTML_TOKENIZER_STATE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS: Between DOCTYPE public and system identifiers.
 * @GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_SYSTEM_KEYWORD: After the DOCTYPE SYSTEM keyword.
 * @GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER: Before a DOCTYPE system identifier.
 * @GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_SYSTEM_IDENTIFIER_QUOTED: Reading a quoted DOCTYPE system identifier.
 * @GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER: After a DOCTYPE system identifier.
 * @GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE: Recovering a malformed DOCTYPE.
 * @GMIME_HTML_TOKENIZER_STATE_C_DATA_SECTION: Reading a CDATA section.
 * @GMIME_HTML_TOKENIZER_STATE_END_OF_FILE: End of input.
 *
 * Lexical states of the streaming HTML tokenizer.
 */
typedef enum {
	GMIME_HTML_TOKENIZER_STATE_DATA,
	GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_DATA,
	GMIME_HTML_TOKENIZER_STATE_RC_DATA,
	GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_RC_DATA,
	GMIME_HTML_TOKENIZER_STATE_RAW_TEXT,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA,
	GMIME_HTML_TOKENIZER_STATE_PLAIN_TEXT,
	GMIME_HTML_TOKENIZER_STATE_TAG_OPEN,
	GMIME_HTML_TOKENIZER_STATE_END_TAG_OPEN,
	GMIME_HTML_TOKENIZER_STATE_TAG_NAME,
	GMIME_HTML_TOKENIZER_STATE_RC_DATA_LESS_THAN,
	GMIME_HTML_TOKENIZER_STATE_RC_DATA_END_TAG_OPEN,
	GMIME_HTML_TOKENIZER_STATE_RC_DATA_END_TAG_NAME,
	GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_LESS_THAN,
	GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_END_TAG_OPEN,
	GMIME_HTML_TOKENIZER_STATE_RAW_TEXT_END_TAG_NAME,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_LESS_THAN,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_END_TAG_OPEN,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_END_TAG_NAME,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPE_START,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPE_START_DASH,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_DASH,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_DASH_DASH,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_LESS_THAN,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_END_TAG_OPEN,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_ESCAPED_END_TAG_NAME,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPE_START,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_DASH,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN,
	GMIME_HTML_TOKENIZER_STATE_SCRIPT_DATA_DOUBLE_ESCAPE_END,
	GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_NAME,
	GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_NAME,
	GMIME_HTML_TOKENIZER_STATE_AFTER_ATTRIBUTE_NAME,
	GMIME_HTML_TOKENIZER_STATE_BEFORE_ATTRIBUTE_VALUE,
	GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_DOUBLE_QUOTED,
	GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_SINGLE_QUOTED,
	GMIME_HTML_TOKENIZER_STATE_ATTRIBUTE_VALUE_UNQUOTED,
	GMIME_HTML_TOKENIZER_STATE_CHARACTER_REFERENCE_IN_ATTRIBUTE_VALUE,
	GMIME_HTML_TOKENIZER_STATE_AFTER_ATTRIBUTE_VALUE_QUOTED,
	GMIME_HTML_TOKENIZER_STATE_SELF_CLOSING_START_TAG,
	GMIME_HTML_TOKENIZER_STATE_BOGUS_COMMENT,
	GMIME_HTML_TOKENIZER_STATE_MARKUP_DECLARATION_OPEN,
	GMIME_HTML_TOKENIZER_STATE_COMMENT_START,
	GMIME_HTML_TOKENIZER_STATE_COMMENT_START_DASH,
	GMIME_HTML_TOKENIZER_STATE_COMMENT,
	GMIME_HTML_TOKENIZER_STATE_COMMENT_END_DASH,
	GMIME_HTML_TOKENIZER_STATE_COMMENT_END,
	GMIME_HTML_TOKENIZER_STATE_COMMENT_END_BANG,
	GMIME_HTML_TOKENIZER_STATE_DOC_TYPE,
	GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_NAME,
	GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_NAME,
	GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_NAME,
	GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_PUBLIC_KEYWORD,
	GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_PUBLIC_IDENTIFIER,
	GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_PUBLIC_IDENTIFIER_QUOTED,
	GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_PUBLIC_IDENTIFIER,
	GMIME_HTML_TOKENIZER_STATE_BETWEEN_DOC_TYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS,
	GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_SYSTEM_KEYWORD,
	GMIME_HTML_TOKENIZER_STATE_BEFORE_DOC_TYPE_SYSTEM_IDENTIFIER,
	GMIME_HTML_TOKENIZER_STATE_DOC_TYPE_SYSTEM_IDENTIFIER_QUOTED,
	GMIME_HTML_TOKENIZER_STATE_AFTER_DOC_TYPE_SYSTEM_IDENTIFIER,
	GMIME_HTML_TOKENIZER_STATE_BOGUS_DOC_TYPE,
	GMIME_HTML_TOKENIZER_STATE_C_DATA_SECTION,
	GMIME_HTML_TOKENIZER_STATE_END_OF_FILE
} GMimeHtmlTokenizerState;

/**
 * GMimeHtmlTokenKind:
 * @GMIME_HTML_TOKEN_DATA: Character data.
 * @GMIME_HTML_TOKEN_TAG: A start tag or end tag.
 * @GMIME_HTML_TOKEN_COMMENT: A normal or bogus comment.
 * @GMIME_HTML_TOKEN_DOCTYPE: A document-type declaration.
 * @GMIME_HTML_TOKEN_CDATA: A CDATA section.
 * @GMIME_HTML_TOKEN_SCRIPT_DATA: Script character data.
 *
 * Kinds of tokens emitted by #GMimeHtmlTokenizer.
 */
typedef enum {
	GMIME_HTML_TOKEN_DATA,
	GMIME_HTML_TOKEN_TAG,
	GMIME_HTML_TOKEN_COMMENT,
	GMIME_HTML_TOKEN_DOCTYPE,
	GMIME_HTML_TOKEN_CDATA,
	GMIME_HTML_TOKEN_SCRIPT_DATA
} GMimeHtmlTokenKind;

/**
 * GMimeHtmlToken:
 *
 * An immutable, reference-counted HTML lexical token.
 **/
typedef struct _GMimeHtmlToken GMimeHtmlToken;

/**
 * GMimeHtmlAttribute:
 *
 * An immutable, reference-counted HTML attribute with an optional value.
 **/
typedef struct _GMimeHtmlAttribute GMimeHtmlAttribute;

#define GMIME_TYPE_HTML_TOKEN (g_mime_html_token_get_type ())
#define GMIME_TYPE_HTML_ATTRIBUTE (g_mime_html_attribute_get_type ())

GType g_mime_html_token_get_type (void);
GType g_mime_html_attribute_get_type (void);

GMimeHtmlToken *g_mime_html_token_ref (GMimeHtmlToken *token);

void g_mime_html_token_unref (GMimeHtmlToken *token);

GMimeHtmlAttribute *g_mime_html_attribute_ref (GMimeHtmlAttribute *attribute);

void g_mime_html_attribute_unref (GMimeHtmlAttribute *attribute);

GMimeHtmlAttribute *g_mime_html_attribute_new (const char *name, const char *value);

const char *g_mime_html_attribute_get_name (const GMimeHtmlAttribute *attribute);

const char *g_mime_html_attribute_get_value (const GMimeHtmlAttribute *attribute);
GMimeHtmlAttributeId g_mime_html_attribute_get_id (const GMimeHtmlAttribute *attribute);

GMimeHtmlToken *g_mime_html_token_new_tag (const char *name, gboolean is_end_tag, gboolean is_empty_element,
					   GMimeHtmlAttribute *const *attributes, gsize n_attributes);

GMimeHtmlToken *g_mime_html_token_new_data (const char *data);

GMimeHtmlToken *g_mime_html_token_new_comment (const char *data, gboolean bogus);

GMimeHtmlToken *g_mime_html_token_new_cdata (const char *data);

GMimeHtmlToken *g_mime_html_token_new_script_data (const char *data);

GMimeHtmlToken *g_mime_html_token_new_doctype (const char *name, const char *public_id, const char *system_id);
GMimeHtmlTokenKind g_mime_html_token_get_kind (const GMimeHtmlToken *token);

const char *g_mime_html_token_get_data (const GMimeHtmlToken *token);
gsize g_mime_html_token_get_data_length (const GMimeHtmlToken *token);

const char *g_mime_html_token_get_name (const GMimeHtmlToken *token);
GMimeHtmlTagId g_mime_html_token_get_tag_id (const GMimeHtmlToken *token);
gboolean g_mime_html_token_is_end_tag (const GMimeHtmlToken *token);
gboolean g_mime_html_token_is_empty_element (const GMimeHtmlToken *token);
gboolean g_mime_html_token_get_encode_entities (const GMimeHtmlToken *token);
gboolean g_mime_html_token_get_is_data_state (const GMimeHtmlToken *token);
gboolean g_mime_html_token_get_is_bogus_comment (const GMimeHtmlToken *token);
gboolean g_mime_html_token_get_force_quirks_mode (const GMimeHtmlToken *token);

const char *g_mime_html_token_get_public_identifier (const GMimeHtmlToken *token);

const char *g_mime_html_token_get_system_identifier (const GMimeHtmlToken *token);
gsize g_mime_html_token_get_n_attributes (const GMimeHtmlToken *token);

const GMimeHtmlAttribute *g_mime_html_token_get_attribute (const GMimeHtmlToken *token, gsize index);

const GMimeHtmlAttribute *g_mime_html_token_lookup_attribute (const GMimeHtmlToken *token, const char *name);

char *g_mime_html_token_to_string (const GMimeHtmlToken *token);

gboolean g_mime_html_token_write_to (const GMimeHtmlToken *token, GMimeStream *stream, GError **error);

const char *g_mime_html_tag_id_to_name (GMimeHtmlTagId id);

GMimeHtmlTagId g_mime_html_tag_id_from_name (const char *name);
gboolean g_mime_html_tag_id_is_empty_element (GMimeHtmlTagId id);
gboolean g_mime_html_tag_id_is_formatting_element (GMimeHtmlTagId id);

const char *g_mime_html_namespace_to_url (GMimeHtmlNamespace ns);

GMimeHtmlNamespace g_mime_html_namespace_from_url (const char *url);

G_END_DECLS

#endif
