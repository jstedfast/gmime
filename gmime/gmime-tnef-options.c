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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gmime-tnef-private.h"

G_DEFINE_BOXED_TYPE (GMimeTnefOptions, g_mime_tnef_options, g_mime_tnef_options_ref, g_mime_tnef_options_unref)

/**
 * g_mime_tnef_options_new:
 *
 * Creates default TNEF options.
 *
 * Returns: (transfer full): new options
 **/
GMimeTnefOptions *
g_mime_tnef_options_new (void)
{
	GMimeTnefOptions *options = g_new0 (GMimeTnefOptions, 1);

	options->ref_count = 1;
	options->max_nesting_depth = 32;
	options->max_property_value_length = 32 * 1024 * 1024;
	options->max_total_data_bytes = 64 * 1024 * 1024;
	options->default_codepage = 0;

	return options;
}

/**
 * g_mime_tnef_options_ref:
 * @options: options to reference
 *
 * References the options.
 *
 * Returns: (transfer full): @options
 **/
GMimeTnefOptions *
g_mime_tnef_options_ref (GMimeTnefOptions *options)
{
	g_return_val_if_fail (options != NULL, NULL);

	g_atomic_int_inc (&options->ref_count);

	return options;
}

/**
 * g_mime_tnef_options_unref:
 * @options: options to release
 *
 * Releases a reference to the options.
 **/
void
g_mime_tnef_options_unref (GMimeTnefOptions *options)
{
	g_return_if_fail (options != NULL);

	if (g_atomic_int_dec_and_test (&options->ref_count))
		g_free (options);
}

/**
 * g_mime_tnef_options_clone:
 * @options: options to copy
 *
 * Copies the options.
 *
 * Returns: (transfer full): independent options
 **/
GMimeTnefOptions *
g_mime_tnef_options_clone (const GMimeTnefOptions *options)
{
	GMimeTnefOptions *clone;

	g_return_val_if_fail (options != NULL, NULL);

	clone = g_mime_tnef_options_new ();
	clone->max_nesting_depth = options->max_nesting_depth;
	clone->max_property_value_length = options->max_property_value_length;
	clone->max_total_data_bytes = options->max_total_data_bytes;
	clone->default_codepage = options->default_codepage;

	return clone;
}

/**
 * g_mime_tnef_options_get_max_nesting_depth:
 * @options: TNEF options
 *
 * Gets the maximum embedded-message depth; zero disallows nesting. Default: 32.
 *
 * Returns: the configured value
 **/
guint
g_mime_tnef_options_get_max_nesting_depth (const GMimeTnefOptions *options)
{
	g_return_val_if_fail (options != NULL, 0);

	return options->max_nesting_depth;
}

/**
 * g_mime_tnef_options_set_max_nesting_depth:
 * @options: TNEF options
 * @maximum: maximum embedded-message depth; zero disallows nesting
 *
 * Sets the maximum embedded-message depth; zero disallows nesting.
 **/
void
g_mime_tnef_options_set_max_nesting_depth (GMimeTnefOptions *options, guint maximum)
{
	g_return_if_fail (options != NULL);

	options->max_nesting_depth = maximum;
}

/**
 * g_mime_tnef_options_get_max_property_value_length:
 * @options: TNEF options
 *
 * Gets the maximum bytes materialized for one value. Default: 32 * 1024 * 1024.
 *
 * Returns: the configured value
 **/
guint
g_mime_tnef_options_get_max_property_value_length (const GMimeTnefOptions *options)
{
	g_return_val_if_fail (options != NULL, 0);

	return options->max_property_value_length;
}

/**
 * g_mime_tnef_options_set_max_property_value_length:
 * @options: TNEF options
 * @maximum: maximum bytes materialized for one value
 *
 * Sets the maximum bytes materialized for one value.
 **/
void
g_mime_tnef_options_set_max_property_value_length (GMimeTnefOptions *options, guint maximum)
{
	g_return_if_fail (options != NULL);

	options->max_property_value_length = maximum;
}

/**
 * g_mime_tnef_options_get_max_total_data_bytes:
 * @options: TNEF options
 *
 * Gets the cumulative materialization budget shared with embedded readers. Default: 64 * 1024 * 1024.
 *
 * Returns: the configured value
 **/
guint64
g_mime_tnef_options_get_max_total_data_bytes (const GMimeTnefOptions *options)
{
	g_return_val_if_fail (options != NULL, 0);

	return options->max_total_data_bytes;
}

/**
 * g_mime_tnef_options_set_max_total_data_bytes:
 * @options: TNEF options
 * @maximum: cumulative materialization budget shared with embedded readers
 *
 * Sets the cumulative materialization budget shared with embedded readers.
 **/
void
g_mime_tnef_options_set_max_total_data_bytes (GMimeTnefOptions *options, guint64 maximum)
{
	g_return_if_fail (options != NULL);

	options->max_total_data_bytes = maximum;
}

/**
 * g_mime_tnef_options_get_default_codepage:
 * @options: TNEF options
 *
 * Gets the default codepage; zero selects Windows-1252. Default: 0.
 *
 * Returns: the configured value
 **/
int
g_mime_tnef_options_get_default_codepage (const GMimeTnefOptions *options)
{
	g_return_val_if_fail (options != NULL, 0);

	return options->default_codepage;
}

/**
 * g_mime_tnef_options_set_default_codepage:
 * @options: TNEF options
 * @codepage: default codepage; zero selects Windows-1252
 *
 * Sets the default codepage; zero selects Windows-1252.
 **/
void
g_mime_tnef_options_set_default_codepage (GMimeTnefOptions *options, int codepage)
{
	g_return_if_fail (options != NULL);
	g_return_if_fail (codepage >= 0);

	options->default_codepage = codepage;
}

G_DEFINE_BOXED_TYPE (GMimeTnefComplianceOptions, g_mime_tnef_compliance_options, g_mime_tnef_compliance_options_ref,
		     g_mime_tnef_compliance_options_unref)

/**
 * g_mime_tnef_compliance_options_new:
 *
 * Creates default TNEF options.
 *
 * Returns: (transfer full): new options
 **/
GMimeTnefComplianceOptions *
g_mime_tnef_compliance_options_new (void)
{
	GMimeTnefComplianceOptions *options = g_new0 (GMimeTnefComplianceOptions, 1);

	options->ref_count = 1;
	options->max_issues_per_violation = 0;

	return options;
}

/**
 * g_mime_tnef_compliance_options_ref:
 * @options: options to reference
 *
 * References the options.
 *
 * Returns: (transfer full): @options
 **/
GMimeTnefComplianceOptions *
g_mime_tnef_compliance_options_ref (GMimeTnefComplianceOptions *options)
{
	g_return_val_if_fail (options != NULL, NULL);

	g_atomic_int_inc (&options->ref_count);

	return options;
}

/**
 * g_mime_tnef_compliance_options_unref:
 * @options: options to release
 *
 * Releases a reference to the options.
 **/
void
g_mime_tnef_compliance_options_unref (GMimeTnefComplianceOptions *options)
{
	g_return_if_fail (options != NULL);

	if (g_atomic_int_dec_and_test (&options->ref_count))
		g_free (options);
}

/**
 * g_mime_tnef_compliance_options_clone:
 * @options: options to copy
 *
 * Copies the options.
 *
 * Returns: (transfer full): independent options
 **/
GMimeTnefComplianceOptions *
g_mime_tnef_compliance_options_clone (const GMimeTnefComplianceOptions *options)
{
	GMimeTnefComplianceOptions *clone;

	g_return_val_if_fail (options != NULL, NULL);

	clone = g_mime_tnef_compliance_options_new ();
	clone->max_issues_per_violation = options->max_issues_per_violation;

	return clone;
}

/**
 * g_mime_tnef_compliance_options_get_max_issues_per_violation:
 * @options: TNEF options
 *
 * Gets the per-violation reporting cap; zero means unlimited. Default: 0.
 *
 * Returns: the configured value
 **/
guint
g_mime_tnef_compliance_options_get_max_issues_per_violation (const GMimeTnefComplianceOptions *options)
{
	g_return_val_if_fail (options != NULL, 0);

	return options->max_issues_per_violation;
}

/**
 * g_mime_tnef_compliance_options_set_max_issues_per_violation:
 * @options: TNEF options
 * @maximum: per-violation reporting cap; zero means unlimited
 *
 * Sets the per-violation reporting cap; zero means unlimited.
 **/
void
g_mime_tnef_compliance_options_set_max_issues_per_violation (GMimeTnefComplianceOptions *options, guint maximum)
{
	g_return_if_fail (options != NULL);

	options->max_issues_per_violation = maximum;
}

/**
 * g_mime_tnef_compliance_violation_get_description:
 * @violation: a genuine TNEF violation, not NONE
 *
 * Gets the diagnostic description.
 *
 * Returns: (transfer none): the diagnostic description
 **/
const char *
g_mime_tnef_compliance_violation_get_description (GMimeTnefComplianceViolation violation)
{
	switch (violation) {
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_SIGNATURE:
		return "The stream did not begin with the TNEF signature.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNSUPPORTED_VERSION:
		return "The attTnefVersion attribute specified an unsupported version.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CODEPAGE:
		return "The attOemCodepage attribute specified a codepage that could not be resolved.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CLASS:
		return "The attMessageClass or attOriginalMessageClass attribute was not a valid message class.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_LEVEL:
		return "An attribute had a level that was neither the message level nor the attachment level.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_MESSAGE_ATTRIBUTE_AFTER_ATTACHMENT:
		return "A message-level attribute appeared after the first attachment-level attribute.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_LEVEL_MISMATCH:
		return "An attribute appeared at a level that it is not defined for.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNKNOWN_ATTRIBUTE:
		return "An attribute had an unrecognized attribute identifier.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_LENGTH:
		return "An attribute had a length that cannot be valid.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_CHECKSUM_MISMATCH:
		return "The checksum of an attribute did not match its value.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_VALUE:
		return "An attribute value was malformed for the type of the attribute.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_DATE:
		return "A date value was invalid.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM:
		return "The stream ended before the end of the current attribute.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_COUNT:
		return "A property list had a property count that cannot be valid.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ROW_COUNT:
		return "A property table had a row count that cannot be valid.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_VALUE_COUNT:
		return "A property had a value count that cannot be valid.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH:
		return "A property value had a length that cannot be valid.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNSUPPORTED_PROPERTY_TYPE:
		return "A property had an unsupported property type.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_NAMED_PROPERTY_KIND:
		return "A named property had an invalid kind.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_NESTING_TOO_DEEP:
		return "Embedded messages were nested more deeply than the configured limit.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_ATTACHMENTS:
		return "The stream contained more attachments than the configured limit.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_DATA_SIZE_LIMIT_EXCEEDED:
		return "The stream contained more data than the configured limit.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return "Too many compliance issues were detected and the remainder were suppressed.";
	default:
		g_return_val_if_reached (NULL);
	}
}

/**
 * g_mime_tnef_compliance_violation_get_remarks:
 * @violation: a genuine TNEF violation, not NONE
 *
 * Gets the diagnostic remarks.
 *
 * Returns: (transfer none): the diagnostic remarks
 **/
const char *
g_mime_tnef_compliance_violation_get_remarks (GMimeTnefComplianceViolation violation)
{
	switch (violation) {
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_SIGNATURE:
		return "The TNEF specification requires that a TNEF stream begin with the 32-bit little-endian signature 0x223E9F78. A stream that does not is not TNEF at all, so nothing in it can be parsed.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNSUPPORTED_VERSION:
		return "The TNEF specification defines a single TNEF version, 0x00010000. Parsing continues, but a stream that claims a different version may use encodings that are not understood.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CODEPAGE:
		return "The codepage is used to decode every 8-bit string in the stream. When it cannot be resolved, the default codepage is used instead, which may decode those strings incorrectly.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CLASS:
		return "A message class is a string of at most 255 printable ASCII characters. The message class determines how a client interprets and renders the rest of the message, so a client that cannot interpret it may render something other than what a content scanner examined.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_LEVEL:
		return "The TNEF specification defines only two attribute levels: 0x01 for message attributes and 0x02 for attachment attributes. There is no way to know which object an attribute with any other level belongs to, so different implementations may apply it to different objects.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_MESSAGE_ATTRIBUTE_AFTER_ATTACHMENT:
		return "The TNEF specification requires that all message attributes precede all attachment attributes. Implementations disagree about whether a late message attribute applies to the message or is discarded, so the same stream can be presented differently by different implementations.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_LEVEL_MISMATCH:
		return "The TNEF specification defines each attribute as belonging to either the message or an attachment. An attachment attribute at the message level, or a message attribute at the attachment level, may be applied to the wrong object or discarded depending on the implementation.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNKNOWN_ATTRIBUTE:
		return "The attribute identifier is not one that the TNEF specification defines. The attribute is skipped, so whatever information it carried is not available.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_LENGTH:
		return "The length of an attribute is the only way to find where the next attribute begins. A length that is too large to represent cannot be followed, so the remainder of the stream cannot be parsed.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_CHECKSUM_MISMATCH:
		return "The TNEF specification defines the checksum of an attribute as the sum of the bytes in its value modulo 65536. A mismatch indicates that the value was altered or corrupted after it was written. The value is still used.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_VALUE:
		return "The attribute value was too short, or otherwise did not have the layout that the TNEF specification defines for the attribute, so it could not be interpreted.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_DATE:
		return "A date attribute or a PT_SYSTIME property described a date and time that does not exist or cannot be represented, so the value is not available.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM:
		return "The stream was most likely truncated in transit or in storage. Whatever followed the point of truncation is lost.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_COUNT:
		return "The property count was negative, or larger than the number of properties that could fit within the attribute that contains them. Only the properties that fit are read.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ROW_COUNT:
		return "The row count of the attRecipTable attribute was negative, or larger than the number of rows that could fit within the attribute. Only the rows that fit are read.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_VALUE_COUNT:
		return "The value count of a multi-valued or variable-length property was negative, larger than the number of values that could fit within the attribute that contains it, or, for a property that is not multi-valued, other than exactly one. A multi-valued Null property is not valid and is treated as having no values, so any non-zero value count is reported.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH:
		return "The length of a variable-length property value was negative, or extended beyond the end of the attribute that contains it, so the value cannot be read in full.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNSUPPORTED_PROPERTY_TYPE:
		return "The size of a property value is determined by its type, so a value of an unknown type cannot be skipped. The remaining properties in the same attribute cannot be read.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_NAMED_PROPERTY_KIND:
		return "The TNEF specification defines the kind of a named property as either 0x00000000 (a numeric identifier) or 0x00000001 (a string name). With any other kind, the size of the name cannot be determined, so the remaining properties in the same attribute cannot be read.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_NESTING_TOO_DEEP:
		return "This is not a defect in the stream, but the embedded messages nested beyond the configured limit were not processed. A content scanner that relies on the result has not examined them.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_ATTACHMENTS:
		return "This is not a defect in the stream, but the attachments beyond the configured limit were not processed. A content scanner that relies on the result has not examined them.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_DATA_SIZE_LIMIT_EXCEEDED:
		return "This is not a defect in the stream, but the data beyond the configured limit was not processed. A content scanner that relies on the result has not examined it.";
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return "A limit was placed on how many times each violation may be reported and some violation reached it, so the remaining occurrences of that violation were not reported. Other violations continue to be reported until they reach the limit themselves, but the report as a whole is no longer a complete account of what is wrong with the stream.";
	default:
		g_return_val_if_reached (NULL);
	}
}

/**
 * g_mime_tnef_compliance_violation_get_severity:
 * @violation: a genuine TNEF violation, not NONE
 *
 * Gets the diagnostic severity.
 *
 * Returns: the diagnostic severity
 **/
GMimeComplianceSeverity
g_mime_tnef_compliance_violation_get_severity (GMimeTnefComplianceViolation violation)
{
	switch (violation) {
	/* Note: Implementations are known to disagree about whether a late message attribute */
	/* overrides the earlier value or is discarded, so a message attribute that follows an */
	/* attachment can smuggle a value (e.g. a body or message class) past a content scanner */
	/* that resolves the conflict differently than the recipient's mail client. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_MESSAGE_ATTRIBUTE_AFTER_ATTACHMENT:
		return GMIME_COMPLIANCE_SEVERITY_CRITICAL;

	/* Note: Both of these are tolerated in practice. A version mismatch does not change how any */
	/* attribute is framed, and a checksum mismatch does not change which bytes are read, only */
	/* whether they are the bytes that the writer intended. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNSUPPORTED_VERSION:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_CHECKSUM_MISMATCH:
	/* Note: These lose a single attribute or value, and nothing else is affected. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNKNOWN_ATTRIBUTE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_DATE:
		return GMIME_COMPLIANCE_SEVERITY_MINOR;

	/* Note: Attributes that may be applied to a different object by different readers. A */
	/* content scanner and a mail client can be made to disagree about which attachment some */
	/* content belongs to, or whether it belongs to the message itself. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_LEVEL:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_LEVEL_MISMATCH:
	/* Note: The message class determines how a client renders the message, so a reader that */
	/* cannot interpret it may render something other than what a scanner examined. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CLASS:
		return GMIME_COMPLIANCE_SEVERITY_MAJOR;

	/* Note: Corruption that loses more than a single value. In each case, the remainder of the */
	/* attribute (or, for the framing violations, the remainder of the stream) cannot be parsed. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_SIGNATURE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CODEPAGE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_LENGTH:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_VALUE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_COUNT:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ROW_COUNT:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_VALUE_COUNT:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNSUPPORTED_PROPERTY_TYPE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_NAMED_PROPERTY_KIND:
		return GMIME_COMPLIANCE_SEVERITY_MAJOR;

	/* Note: These are not defects in the stream, but some of its content was not processed, */
	/* and a consumer that only looks at Major and above needs to know that. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_NESTING_TOO_DEEP:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_ATTACHMENTS:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_DATA_SIZE_LIMIT_EXCEEDED:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return GMIME_COMPLIANCE_SEVERITY_MAJOR;

	default:
		g_return_val_if_reached (GMIME_COMPLIANCE_SEVERITY_MAJOR);
	}
}

/**
 * g_mime_tnef_compliance_violation_get_categories:
 * @violation: a genuine TNEF violation, not NONE
 *
 * Gets the diagnostic categories.
 *
 * Returns: the diagnostic categories
 **/
GMimeComplianceCategories
g_mime_tnef_compliance_violation_get_categories (GMimeTnefComplianceViolation violation)
{
	switch (violation) {
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNSUPPORTED_VERSION:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_CHECKSUM_MISMATCH:
		return GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY;

	/* Note: The content of the stream, or of a single attribute or value, is not available. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_SIGNATURE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CODEPAGE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNKNOWN_ATTRIBUTE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_LENGTH:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_VALUE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_DATE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_COUNT:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ROW_COUNT:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_VALUE_COUNT:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_PROPERTY_LENGTH:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_UNSUPPORTED_PROPERTY_TYPE:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_NAMED_PROPERTY_KIND:
		return GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY | GMIME_COMPLIANCE_CATEGORY_DATA_LOSS;

	/* Note: Different readers may attach the content to different objects or render it */
	/* differently, so what a scanner examines may not be what the recipient is shown. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_ATTRIBUTE_LEVEL:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_MESSAGE_ATTRIBUTE_AFTER_ATTACHMENT:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_ATTRIBUTE_LEVEL_MISMATCH:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_INVALID_MESSAGE_CLASS:
		return GMIME_COMPLIANCE_CATEGORY_INTEROPERABILITY | GMIME_COMPLIANCE_CATEGORY_SECURITY;

	/* Note: A truncated stream is unlikely to be interpreted differently by different readers, */
	/* but whatever followed the point of truncation has been lost. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TRUNCATED_STREAM:
		return GMIME_COMPLIANCE_CATEGORY_DATA_LOSS;

	/* Note: The stream did not cause these, so they do not harm interoperability, but content */
	/* that went unprocessed is content that went unscanned. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_NESTING_TOO_DEEP:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_ATTACHMENTS:
	case GMIME_TNEF_COMPLIANCE_VIOLATION_DATA_SIZE_LIMIT_EXCEEDED:
		return GMIME_COMPLIANCE_CATEGORY_DATA_LOSS | GMIME_COMPLIANCE_CATEGORY_SECURITY;

	/* Note: A suppressed report is a blind spot, and hiding a violation from whatever is */
	/* inspecting the stream is exactly what an attacker would want out of a flood of cheap ones. */
	case GMIME_TNEF_COMPLIANCE_VIOLATION_TOO_MANY_COMPLIANCE_ISSUES:
		return GMIME_COMPLIANCE_CATEGORY_SECURITY;

	default:
		g_return_val_if_reached (GMIME_COMPLIANCE_CATEGORY_NONE);
	}
}
