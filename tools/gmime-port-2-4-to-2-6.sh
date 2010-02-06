#!/bin/bash

for src in `find . -name "*.[c,h]"`
do
    echo "Auto-porting '$src' from GMime-2.4 to GMime-2.6..."
    sed -e "s/GMIME_BEST_ENCODING_7BIT/GMIME_ENCODING_CONSTRAINT_7BIT/g" \
	-e "s/GMIME_BEST_ENCODING_8BIT/GMIME_ENCODING_CONSTRAINT_8BIT/g" \
	-e "s/GMIME_BEST_ENCODING_BINARY/GMIME_ENCODING_CONSTRAINT_BINARY/g" \
	-e "s/GMimeBestEncoding/GMimeEncodingConstraint/g" \
	-e "s/g_mime_signer_get_created/g_mime_signer_get_sig_created/g" \
	-e "s/g_mime_signer_set_created/g_mime_signer_set_sig_created/g" \
	-e "s/g_mime_signer_get_expires/g_mime_signer_get_sig_expires/g" \
	-e "s/g_mime_signer_set_expires/g_mime_signer_set_sig_expires/g" \
	< "$src" > "$src.tmp"
    mv "$src.tmp" "$src"
done
