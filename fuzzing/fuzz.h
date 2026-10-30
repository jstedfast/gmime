/* Adapted from MimeKit.Fuzzing. Copyright (c) 2013-2026 .NET Foundation
 * and Contributors. Distributed under the MIT license; see LICENSE. */

#ifndef GMIME_FUZZ_H
#define GMIME_FUZZ_H

#include <gmime/gmime.h>

#define FUZZ_MAX_LENGTH (1024 * 1024)
#define FUZZ_MAX_MESSAGES 10000
#define FUZZ_HASH_INIT G_GUINT64_CONSTANT (14695981039346656037)
#define FUZZ_CHECK(condition, ...) G_STMT_START { if (!(condition)) g_error (__VA_ARGS__); } G_STMT_END

typedef struct {
	GString *trace;
	gsize length, operation_start;
	GMimeComplianceContext context;
	guint cap, counts[GMIME_COMPLIANCE_INCOMPLETE_UUENCODED_CONTENT + 1];
	gboolean reached_cap, truncated;
} FuzzCompliance;

typedef void (*FuzzRun) (const guint8 *data, gsize length);

void fuzz_init (void);
guint64 fuzz_hash (const guint8 *data, gsize length, guint64 hash);
guint32 fuzz_selector (const guint8 *data, gsize length);
void fuzz_equal (const guint8 *expected, gsize expected_length, const guint8 *actual, gsize actual_length, const char *what);
GMimeParserOptions *fuzz_parser_options (void);
GMimeStream *fuzz_stream_new (const guint8 *data, gsize length, gsize chunk);
void fuzz_check_error (GError *error);
void fuzz_compliance_init (FuzzCompliance *checker, gsize length, guint32 selector);
void fuzz_compliance_begin (FuzzCompliance *checker);
void fuzz_compliance_end (FuzzCompliance *checker);
GMimeComplianceOptions *fuzz_compliance_options (FuzzCompliance *checker);
void fuzz_compliance_attach (FuzzCompliance *checker, GMimeReader *reader);
void fuzz_compliance_report (const GMimeComplianceIssue *issue, gpointer user_data);
void fuzz_mime_reader (const guint8 *data, gsize length);
void fuzz_mime_parser (const guint8 *data, gsize length);

#endif
