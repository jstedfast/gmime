#!/bin/bash

for src in `find . -name "*.[c,h]"`
do
    echo "Auto-porting '$src' from GMime-2.6 to GMime-3.0..."
    sed -e "s/g_mime_message_get_sender/g_mime_messaget_get_from/g" \
	-e "s/GMIME_RECIPIENT_TYPE_/GMIME_ADDRESS_TYPE_/g" \
	-e "s/g_mime_message_get_recipients/g_mime_message_get_addresses/g" \
	-e "s/g_mime_message_add_recipient/g_mime_message_add_mailbox/g" \
	-e "s/g_mime_param_new_from_string/g_mime_param_list_parse/g" \
	-e "s/g_mime_content_type_new_from_string/g_mime_content_type_parse/g" \
	-e "s/g_mime_content_type_to_string/g_mime_content_type_get_mime_type/g" \
	-e "s/g_mime_content_type_get_params/g_mime_content_type_get_parameters/g" \
	-e "s/g_mime_content_disposition_new_from_string/g_mime_content_disposition_parse/g" \
	-e "s/g_mime_content_disposition_to_string/g_mime_content_disposition_encode/g" \
	-e "s/g_mime_content_disposition_get_params/g_mime_content_disposition_get_parameters/g" \
	-e "s/internet_address_list_parse_string/internet_address_list_parse/g" \
	-e "s/GMimeCertificateTrust/GMimeTrust/g" \
	-e "s/GMIME_CERTIIFCATE_TRUST_NONE/GMIME_TRUST_UNKNOWN/g" \
	-e "s/GMIME_CERTIFICATE_TRUST_FULLY/GMIME_TRUST_FULL/g" \
	-e "s/GMIME_CERTIFICATE_TRUST_/GMIME_TRUST_/g" \
	-e "s/g_mime_stream_file_new_for_path/g_mime_stream_file_open/g" \
	-e "s/g_mime_stream_fs_new_for_path/g_mime_stream_fs_open/g" \
	-e "s/g_mime_part_set_content_object/g_mime_part_set_content/g" \
	-e "s/g_mime_part_get_content_object/g_mime_part_get_content/g" \
	-e "s/g_mime_parser_set_scan_from/g_mime_parser_set_format/g" \
	-e "s/g_mime_parser_get_scan_from/g_mime_parser_get_format/g" \
	-e "s/g_mime_parser_get_from/g_mime_parser_get_mbox_marker/g" \
	-e "s/g_mime_parser_get_from_offset/g_mime_parser_get_mbox_marker_offset/g" \
	< "$src" > "$src.tmp"
    mv "$src.tmp" "$src"
done
