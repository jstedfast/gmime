/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GMime
 * Copyright (C) 2026 Jeffrey Stedfast
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>

#include "gmime-openpgp.h"
#include "gmime-internal.h"
#include "gmime-error.h"

#define _(x) x
#define MEMORY_LIMIT (1024 * 1024)

/**
 * SECTION: gmime-openpgp
 * @title: Inline OpenPGP
 * @short_description: Non-mutating discovery of independent inline-PGP blocks
 *
 * Discovery snapshots transfer-decoded bytes without charset conversion,
 * flowed-text processing, cryptography, or key import. A document retains
 * the entire source, including unprotected gaps and malformed candidates.
 * Blocks and opened read-only streams can outlive the document and part.
 *
 * Snapshots larger than 1 MiB spill to a private temporary file, removed
 * when the last document, block, or stream releases it. This bounds snapshot
 * memory, not disk usage or the per-block metadata. Temporary-file creation
 * and I/O failures are reported, never replaced with truncated results.
 *
 * Only exact, unquoted armor markers at line starts are recognized. LF,
 * CRLF, and a final marker at EOF are supported. A new beginning interrupts
 * an unfinished block, except for the signature within a clear-signed block.
 * Unexpected end markers terminate a candidate as malformed. Unmatched end
 * markers and unknown armor types remain ordinary source bytes.
 *
 * Native verification/decryption are declared in gmime-gpgme.h. Protection
 * applies only to each processed payload, not adjacent blocks, unprotected
 * gaps, MIME headers, or the document as a whole.
 **/

typedef struct {
	gatomicrefcount refs;
	GMutex mutex;
	GByteArray *memory;
	GFileIOStream *file;
	GFile *path;
	gint64 length;
} OpenPGPStorage;

struct _GMimeOpenPGPBlock {
	GObject parent_object;
	OpenPGPStorage *storage;
	GMimeOpenPGPBlockKind kind;
	GMimeOpenPGPBlockStatus status;
	gint64 offset;
	gint64 length;
};

struct _GMimeOpenPGPDocument {
	GObject parent_object;
	OpenPGPStorage *storage;
	GPtrArray *blocks;
};

typedef struct {
	GMimeStream parent_object;
	OpenPGPStorage *storage;
	gint64 origin;
	gint64 start;
	gint64 end;
} OpenPGPStream;

typedef GMimeStreamClass OpenPGPStreamClass;

static void list_model_init (GListModelInterface *iface);
static GType openpgp_stream_get_type (void);
static GMimeStream *openpgp_stream_new (OpenPGPStorage *storage, gint64 origin, gint64 start, gint64 end);

G_DEFINE_FINAL_TYPE (GMimeOpenPGPBlock, g_mime_openpgp_block, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE_WITH_CODE (GMimeOpenPGPDocument, g_mime_openpgp_document, G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_init))
G_DEFINE_FINAL_TYPE (OpenPGPStream, openpgp_stream, GMIME_TYPE_STREAM)

static OpenPGPStorage *
storage_new (void)
{
	OpenPGPStorage *storage = g_new0 (OpenPGPStorage, 1);

	g_atomic_ref_count_init (&storage->refs);
	g_mutex_init (&storage->mutex);
	storage->memory = g_byte_array_new ();

	return storage;
}

static OpenPGPStorage *
storage_ref (OpenPGPStorage *storage)
{
	g_atomic_ref_count_inc (&storage->refs);
	return storage;
}

static void
storage_unref (OpenPGPStorage *storage)
{
	GError *error = NULL;

	if (!storage || !g_atomic_ref_count_dec (&storage->refs))
		return;

	if (storage->file) {
		if (!g_io_stream_close (G_IO_STREAM (storage->file), NULL, &error)) {
			g_warning ("Cannot close OpenPGP snapshot: %s", error->message);
			g_clear_error (&error);
		}

		g_object_unref (storage->file);

		if (!g_file_delete (storage->path, NULL, &error)) {
			g_warning ("Cannot remove OpenPGP snapshot: %s", error->message);
			g_clear_error (&error);
		}

		g_object_unref (storage->path);
	}

	g_clear_pointer (&storage->memory, g_byte_array_unref);
	g_mutex_clear (&storage->mutex);
	g_free (storage);
}

static gboolean
storage_append (OpenPGPStorage *storage, const char *buffer, gsize length, GError **error)
{
	GOutputStream *output;

	if (length > (guint64) (G_MAXINT64 - storage->length)) {
		g_set_error_literal (error, GMIME_ERROR, EOVERFLOW, _("OpenPGP snapshot is too large."));
		return FALSE;
	}

	if (!storage->file && storage->length + (gint64) length > MEMORY_LIMIT) {
		storage->path = g_file_new_tmp ("gmime-openpgp-XXXXXX", &storage->file, error);
		if (!storage->path)
			return FALSE;

		output = g_io_stream_get_output_stream (G_IO_STREAM (storage->file));

		if (!g_output_stream_write_all (output, storage->memory->data, storage->memory->len, NULL, NULL, error))
			return FALSE;

		g_clear_pointer (&storage->memory, g_byte_array_unref);
	}

	if (storage->file) {
		output = g_io_stream_get_output_stream (G_IO_STREAM (storage->file));

		if (!g_output_stream_write_all (output, buffer, length, NULL, NULL, error))
			return FALSE;
	} else {
		g_byte_array_append (storage->memory, (const guint8 *) buffer, length);
	}

	storage->length += length;

	return TRUE;
}

static gboolean
stream_bounds (GMimeStream *stream, gint64 *start, gint64 *end)
{
	OpenPGPStream *self = (OpenPGPStream *) stream;
	gint64 position = g_mime_stream_get_position (stream);

	if (!self->storage) {
		errno = EBADF;
		return FALSE;
	}

	g_mime_stream_get_bounds (stream, start, end);

	if (*end == -1)
		*end = self->end;

	if (*start < self->start || *end > self->end || *end < *start ||
	    position < *start || position > *end) {
		errno = EINVAL;
		return FALSE;
	}

	return TRUE;
}

static ssize_t
snapshot_read (GMimeStream *stream, char *buffer, size_t length)
{
	OpenPGPStream *self = (OpenPGPStream *) stream;
	OpenPGPStorage *storage = self->storage;
	gint64 start, end, position;
	gssize nread;

	if (!stream_bounds (stream, &start, &end))
		return -1;

	position = g_mime_stream_get_position (stream);
	length = MIN ((guint64) (end - position), MIN (length, (size_t) G_MAXSSIZE));

	if (!length)
		return 0;

	if (storage->file) {
		g_mutex_lock (&storage->mutex);

		if (!g_seekable_seek (G_SEEKABLE (storage->file), self->origin + position, G_SEEK_SET, NULL, NULL)) {
			nread = -1;
		} else {
			nread = g_input_stream_read (g_io_stream_get_input_stream (G_IO_STREAM (storage->file)),
						    buffer, length, NULL, NULL);
		}

		g_mutex_unlock (&storage->mutex);

		if (nread <= 0) {
			errno = EIO;
			return -1;
		}
	} else {
		memcpy (buffer, storage->memory->data + self->origin + position, length);
		nread = length;
	}

	g_mime_stream_set_position (stream, position + nread);

	return nread;
}

static ssize_t
snapshot_write (GMimeStream *stream, const char *buffer, size_t length)
{
	errno = EBADF;
	return -1;
}

static int
snapshot_close (GMimeStream *stream)
{
	OpenPGPStream *self = (OpenPGPStream *) stream;

	g_clear_pointer (&self->storage, storage_unref);

	return 0;
}

static int
snapshot_check (GMimeStream *stream)
{
	gint64 start, end;

	return stream_bounds (stream, &start, &end) ? 0 : -1;
}

static gboolean
snapshot_eos (GMimeStream *stream)
{
	gint64 start, end;

	return !stream_bounds (stream, &start, &end) || g_mime_stream_get_position (stream) == end;
}

static gint64
snapshot_seek (GMimeStream *stream, gint64 offset, GMimeSeekWhence whence)
{
	gint64 start, end, origin;

	if (!stream_bounds (stream, &start, &end))
		return -1;

	switch (whence) {
	case GMIME_STREAM_SEEK_SET: origin = 0; break;
	case GMIME_STREAM_SEEK_CUR: origin = g_mime_stream_get_position (stream); break;
	case GMIME_STREAM_SEEK_END: origin = end; break;
	default:
		errno = EINVAL;
		return -1;
	}

	if (offset < start - origin || offset > end - origin) {
		errno = EINVAL;
		return -1;
	}

	g_mime_stream_set_position (stream, origin + offset);

	return origin + offset;
}

static gint64
snapshot_tell (GMimeStream *stream)
{
	return snapshot_check (stream) == 0 ? g_mime_stream_get_position (stream) : -1;
}

static gint64
snapshot_length (GMimeStream *stream)
{
	gint64 start, end;

	return stream_bounds (stream, &start, &end) ? end - start : -1;
}

static GMimeStream *
snapshot_substream (GMimeStream *stream, gint64 start, gint64 end)
{
	OpenPGPStream *self = (OpenPGPStream *) stream;
	gint64 lower, upper;

	if (!stream_bounds (stream, &lower, &upper))
		return NULL;

	if (end == -1)
		end = upper;

	if (start < lower || end > upper || start > end) {
		errno = EINVAL;
		return NULL;
	}

	return openpgp_stream_new (self->storage, self->origin, start, end);
}

static void
snapshot_finalize (GObject *object)
{
	snapshot_close (GMIME_STREAM (object));
	G_OBJECT_CLASS (openpgp_stream_parent_class)->finalize (object);
}

static void
openpgp_stream_class_init (OpenPGPStreamClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = snapshot_finalize;
	klass->read = snapshot_read;
	klass->write = snapshot_write;
	klass->flush = snapshot_check;
	klass->close = snapshot_close;
	klass->eos = snapshot_eos;
	klass->reset = snapshot_check;
	klass->seek = snapshot_seek;
	klass->tell = snapshot_tell;
	klass->length = snapshot_length;
	klass->substream = snapshot_substream;
}

static void
openpgp_stream_init (OpenPGPStream *self)
{
}

static GMimeStream *
openpgp_stream_new (OpenPGPStorage *storage, gint64 origin, gint64 start, gint64 end)
{
	OpenPGPStream *self = g_object_new (openpgp_stream_get_type (), NULL);

	self->storage = storage_ref (storage);
	self->origin = origin;
	self->start = start;
	self->end = end;

	g_mime_stream_construct (GMIME_STREAM (self), start, end);

	return GMIME_STREAM (self);
}

static void
block_finalize (GObject *object)
{
	storage_unref (GMIME_OPENPGP_BLOCK (object)->storage);
	G_OBJECT_CLASS (g_mime_openpgp_block_parent_class)->finalize (object);
}

static void
g_mime_openpgp_block_class_init (GMimeOpenPGPBlockClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = block_finalize;
}

static void
g_mime_openpgp_block_init (GMimeOpenPGPBlock *block)
{
	block->status = GMIME_OPENPGP_BLOCK_TRUNCATED;
}

static void
document_finalize (GObject *object)
{
	GMimeOpenPGPDocument *document = GMIME_OPENPGP_DOCUMENT (object);

	g_ptr_array_unref (document->blocks);
	storage_unref (document->storage);
	G_OBJECT_CLASS (g_mime_openpgp_document_parent_class)->finalize (object);
}

static void
g_mime_openpgp_document_class_init (GMimeOpenPGPDocumentClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = document_finalize;
}

static void
g_mime_openpgp_document_init (GMimeOpenPGPDocument *document)
{
	document->storage = storage_new ();
	document->blocks = g_ptr_array_new_with_free_func (g_object_unref);
}

static GType
model_item_type (GListModel *model)
{
	return GMIME_TYPE_OPENPGP_BLOCK;
}

static guint
model_count (GListModel *model)
{
	return GMIME_OPENPGP_DOCUMENT (model)->blocks->len;
}

static gpointer
model_item (GListModel *model, guint index)
{
	GMimeOpenPGPDocument *document = GMIME_OPENPGP_DOCUMENT (model);

	return index < document->blocks->len ? g_object_ref (document->blocks->pdata[index]) : NULL;
}

static void
list_model_init (GListModelInterface *iface)
{
	iface->get_item_type = model_item_type;
	iface->get_n_items = model_count;
	iface->get_item = model_item;
}

typedef struct {
	GMimeOpenPGPDocument *document;
	GMimeOpenPGPBlock *current;
	GMimeOpenPGPState expected;
	gint64 line_start;
	char line[38];
	guint length;
	gboolean overflow;
} OpenPGPScanner;

static void
finish_block (OpenPGPScanner *scanner, gint64 end, GMimeOpenPGPBlockStatus status)
{
	scanner->current->length = end - scanner->current->offset;
	scanner->current->status = status;
	scanner->current = NULL;
}

static void
scan_line (OpenPGPScanner *scanner, gint64 end)
{
	const GMimeOpenPGPMarker *marker;
	GMimeOpenPGPBlockKind kind;
	GMimeOpenPGPState expected;
	guint length = scanner->length;

	if (scanner->overflow)
		return;

	if (length && scanner->line[length - 1] == '\r' && end - scanner->line_start > length)
		length--;

	marker = _g_mime_openpgp_match_marker (scanner->line, length);
	if (!marker)
		return;

	if (scanner->current && marker->after == scanner->expected) {
		if (marker->is_end_marker)
			finish_block (scanner, end, GMIME_OPENPGP_BLOCK_COMPLETE);
		else
			scanner->expected = GMIME_OPENPGP_END_PGP_SIGNATURE;
		return;
	}

	if (marker->is_end_marker) {
		if (scanner->current)
			finish_block (scanner, end, GMIME_OPENPGP_BLOCK_MALFORMED);
		return;
	}

	if (scanner->current)
		finish_block (scanner, scanner->line_start, GMIME_OPENPGP_BLOCK_INTERRUPTED);

	switch (marker->after) {
	case GMIME_OPENPGP_BEGIN_PGP_MESSAGE:
		kind = GMIME_OPENPGP_BLOCK_MESSAGE;
		expected = GMIME_OPENPGP_END_PGP_MESSAGE;
		break;
	case GMIME_OPENPGP_BEGIN_PGP_SIGNED_MESSAGE:
		kind = GMIME_OPENPGP_BLOCK_SIGNED_MESSAGE;
		expected = GMIME_OPENPGP_BEGIN_PGP_SIGNATURE;
		break;
	case GMIME_OPENPGP_BEGIN_PGP_PUBLIC_KEY_BLOCK:
		kind = GMIME_OPENPGP_BLOCK_PUBLIC_KEY;
		expected = GMIME_OPENPGP_END_PGP_PUBLIC_KEY_BLOCK;
		break;
	case GMIME_OPENPGP_BEGIN_PGP_PRIVATE_KEY_BLOCK:
		kind = GMIME_OPENPGP_BLOCK_PRIVATE_KEY;
		expected = GMIME_OPENPGP_END_PGP_PRIVATE_KEY_BLOCK;
		break;
	default:
		kind = GMIME_OPENPGP_BLOCK_SIGNATURE;
		expected = GMIME_OPENPGP_END_PGP_SIGNATURE;
		break;
	}

	scanner->current = g_object_new (GMIME_TYPE_OPENPGP_BLOCK, NULL);
	scanner->current->storage = storage_ref (scanner->document->storage);
	scanner->current->kind = kind;
	scanner->current->offset = scanner->line_start;
	scanner->expected = expected;

	g_ptr_array_add (scanner->document->blocks, scanner->current);
}

static void
scan_bytes (OpenPGPScanner *scanner, const char *buffer, gsize length, gint64 offset)
{
	gsize i;

	for (i = 0; i < length; i++) {
		if (buffer[i] == '\n') {
			scan_line (scanner, offset + i + 1);
			scanner->line_start = offset + i + 1;
			scanner->length = 0;
			scanner->overflow = FALSE;
		} else if (scanner->length < sizeof (scanner->line)) {
			scanner->line[scanner->length++] = buffer[i];
		} else {
			scanner->overflow = TRUE;
		}
	}
}

/**
 * g_mime_part_scan_openpgp:
 * @part: the part to inspect
 * @error: return location for an error
 *
 * Snapshots all transfer-decoded bytes and discovers independent blocks.
 * Does not modify content or headers, but reading may reposition the backing
 * content stream. No cryptographic operations or key imports are performed.
 * Missing content and I/O failures are errors; no candidates is a successful,
 * empty document whose source remains accessible.
 *
 * Returns: (transfer full) (nullable): a decoded document, or %NULL on error.
 **/
GMimeOpenPGPDocument *
g_mime_part_scan_openpgp (GMimePart *part, GError **error)
{
	GMimeOpenPGPDocument *document;
	OpenPGPScanner scanner = { 0 };
	GMimeContent *content;
	GMimeStream *input;
	char buffer[4096];
	ssize_t nread;
	int saved_errno;

	g_return_val_if_fail (GMIME_IS_PART (part), NULL);

	content = g_mime_part_get_content (part);
	if (!content || !g_mime_content_get_stream (content)) {
		g_set_error_literal (error, GMIME_ERROR, GMIME_ERROR_PARSE_ERROR, _("The MIME part has no content."));
		return NULL;
	}

	errno = 0;
	if (!(input = g_mime_content_open (content))) {
		saved_errno = errno ? errno : EIO;
		g_set_error (error, GMIME_ERROR, saved_errno, _("Cannot open OpenPGP content: %s"), g_strerror (saved_errno));
		return NULL;
	}

	document = g_object_new (GMIME_TYPE_OPENPGP_DOCUMENT, NULL);
	scanner.document = document;

	while (TRUE) {
		errno = 0;
		nread = g_mime_stream_read (input, buffer, sizeof (buffer));
		if (nread < 0 || (nread == 0 && !g_mime_stream_eos (input))) {
			saved_errno = errno ? errno : EIO;
			g_set_error (error, GMIME_ERROR, saved_errno, _("Cannot read OpenPGP content: %s"), g_strerror (saved_errno));
			goto fail;
		}

		if (nread == 0)
			break;

		if (!storage_append (document->storage, buffer, nread, error))
			goto fail;

		scan_bytes (&scanner, buffer, nread, document->storage->length - nread);
	}

	if (scanner.length || scanner.overflow)
		scan_line (&scanner, document->storage->length);

	if (scanner.current)
		finish_block (&scanner, document->storage->length, GMIME_OPENPGP_BLOCK_TRUNCATED);

	if (document->storage->file &&
	    !g_output_stream_flush (g_io_stream_get_output_stream (G_IO_STREAM (document->storage->file)), NULL, error))
		goto fail;

	g_object_unref (input);

	return document;

fail:
	g_object_unref (input);
	g_object_unref (document);

	return NULL;
}

/**
 * g_mime_openpgp_document_get_length:
 * @document: a decoded snapshot
 *
 * Returns: the complete decoded source length in bytes.
 **/
gint64
g_mime_openpgp_document_get_length (GMimeOpenPGPDocument *document)
{
	g_return_val_if_fail (GMIME_IS_OPENPGP_DOCUMENT (document), -1);

	return document->storage->length;
}

/**
 * g_mime_openpgp_document_get_count:
 * @document: a decoded snapshot
 *
 * Returns: the number of candidate blocks, including incomplete ones.
 **/
guint
g_mime_openpgp_document_get_count (GMimeOpenPGPDocument *document)
{
	g_return_val_if_fail (GMIME_IS_OPENPGP_DOCUMENT (document), 0);

	return document->blocks->len;
}

/**
 * g_mime_openpgp_document_get_block:
 * @document: a decoded snapshot
 * @index: a zero-based block index
 *
 * Returns: (transfer none) (nullable): the block, or %NULL if out of range.
 * Reference it to retain it independently of @document.
 **/
GMimeOpenPGPBlock *
g_mime_openpgp_document_get_block (GMimeOpenPGPDocument *document, guint index)
{
	g_return_val_if_fail (GMIME_IS_OPENPGP_DOCUMENT (document), NULL);

	return index < document->blocks->len ? document->blocks->pdata[index] : NULL;
}

/**
 * g_mime_openpgp_document_open_stream:
 * @document: a decoded snapshot
 *
 * Opens an independently positioned, read-only view of the entire source.
 * Use substreams to read unprotected gaps. The stream retains its storage;
 * read failures are reported through errno, as for other GMime streams.
 *
 * Returns: (transfer full): a stream positioned at zero.
 **/
GMimeStream *
g_mime_openpgp_document_open_stream (GMimeOpenPGPDocument *document)
{
	g_return_val_if_fail (GMIME_IS_OPENPGP_DOCUMENT (document), NULL);

	return openpgp_stream_new (document->storage, 0, 0, document->storage->length);
}

/**
 * g_mime_openpgp_block_get_kind:
 * @block: a candidate block
 *
 * Returns: the observed armor kind.
 **/
GMimeOpenPGPBlockKind
g_mime_openpgp_block_get_kind (GMimeOpenPGPBlock *block)
{
	g_return_val_if_fail (GMIME_IS_OPENPGP_BLOCK (block), GMIME_OPENPGP_BLOCK_MESSAGE);

	return block->kind;
}

/**
 * g_mime_openpgp_block_get_status:
 * @block: a candidate block
 *
 * Returns: its structural discovery status, not cryptographic validity.
 **/
GMimeOpenPGPBlockStatus
g_mime_openpgp_block_get_status (GMimeOpenPGPBlock *block)
{
	g_return_val_if_fail (GMIME_IS_OPENPGP_BLOCK (block), GMIME_OPENPGP_BLOCK_MALFORMED);

	return block->status;
}

/**
 * g_mime_openpgp_block_get_offset:
 * @block: a candidate block
 *
 * Returns: the byte offset in the complete transfer-decoded snapshot.
 **/
gint64
g_mime_openpgp_block_get_offset (GMimeOpenPGPBlock *block)
{
	g_return_val_if_fail (GMIME_IS_OPENPGP_BLOCK (block), -1);

	return block->offset;
}

/**
 * g_mime_openpgp_block_get_length:
 * @block: a candidate block
 *
 * Returns: the byte length, including the end marker's newline, if present.
 **/
gint64
g_mime_openpgp_block_get_length (GMimeOpenPGPBlock *block)
{
	g_return_val_if_fail (GMIME_IS_OPENPGP_BLOCK (block), -1);

	return block->length;
}

/**
 * g_mime_openpgp_block_open_stream:
 * @block: a candidate block
 *
 * Opens an independent read-only view, including original malformed bytes.
 * Coordinates start at zero for this block, not at its document offset.
 * Substreams use absolute coordinates within this view, as usual for GMime.
 * The stream retains its storage independently of @block.
 *
 * Returns: (transfer full): a stream positioned at zero.
 **/
GMimeStream *
g_mime_openpgp_block_open_stream (GMimeOpenPGPBlock *block)
{
	g_return_val_if_fail (GMIME_IS_OPENPGP_BLOCK (block), NULL);
	g_return_val_if_fail (block->storage != NULL, NULL);

	return openpgp_stream_new (block->storage, block->offset, 0, block->length);
}
