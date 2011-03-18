#!/bin/bash

for src in `find . -name "*.[c,h]"`
do
    echo "Auto-porting '$src' from GMime-2.4 to GMime-2.6..."
    sed -e "s/GMIME_BEST_ENCODING_7BIT/GMIME_ENCODING_CONSTRAINT_7BIT/g" \
	-e "s/GMIME_BEST_ENCODING_8BIT/GMIME_ENCODING_CONSTRAINT_8BIT/g" \
	-e "s/GMIME_BEST_ENCODING_BINARY/GMIME_ENCODING_CONSTRAINT_BINARY/g" \
	-e "s/GMimeBestEncoding/GMimeEncodingConstraint/g" \
	-e "s/GMimeCryptoHash/GMimeDigestAlgo/g" \
	-e "s/GMIME_CRYPTO_HASH_/GMIME_DIGEST_ALGO_/g" \
	-e "s/GMimeCipherContext/GMimeCryptoContext/g" \
	-e "s/GMIME_CIPHER_CONTEXT/GMIME_CRYPTO_CONTEXT/g" \
	-e "s/GMIME_IS_CIPHER_CONTEXT/GMIME_IS_CRYPTO_CONTEXT/g" \
	-e "s/g_mime_cipher_context/g_mime_crypto_context/g" \
	-e "s/gmime-cipher-context.h/gmime-crypto-context.h/g" \
	-e "s/GMIME_TYPE_CIPHER_CONTEXT/GMIME_TYPE_CRYPTO_CONTEXT/g" \
	-e "s/GMimeSignatureValidity/GMimeSignatureList/g" \
	-e "s/g_mime_signature_validity_free/g_object_unref/g" \
	-e "s/GMimeSignerTrust/GMimeCertificateTrust/g" \
	-e "s/GMIME_SIGNER_TRUST_/GMIME_CERTIFICATE_TRUST_/g" \
	-e "s/GMimeSignerStatus/GMimeSignatureStatus/g" \
	-e "s/GMIME_SIGNER_STATUS_/GMIME_SIGNATURE_STATUS_/g" \
	-e "s/GMimeSignerError/GMimeSignatureError/g" \
	-e "s/GMIME_SIGNER_ERROR_/GMIME_SIGNATURE_ERROR_/g" \
	-e "s/GMimeSigner/GMimeSignature/g" \
	-e "s/g_mime_signer_get_fingerprint/g_mime_certificate_get_fingerprint/g" \
	-e "s/g_mime_signer_set_fingerprint/g_mime_certificate_set_fingerprint/g" \
	-e "s/g_mime_signer_get_created/g_mime_signature_get_created/g" \
	-e "s/g_mime_signer_set_created/g_mime_signature_set_created/g" \
	-e "s/g_mime_signer_get_expires/g_mime_signature_get_expires/g" \
	-e "s/g_mime_signer_set_expires/g_mime_signature_set_expires/g" \
	-e "s/g_mime_signer_get_key_id/g_mime_certificate_get_key_id/g" \
	-e "s/g_mime_signer_set_key_id/g_mime_certificate_set_key_id/g" \
	-e "s/g_mime_signer_get_status/g_mime_signature_get_status/g" \
	-e "s/g_mime_signer_set_status/g_mime_signature_set_status/g" \
	-e "s/g_mime_signer_get_errors/g_mime_signature_get_errors/g" \
	-e "s/g_mime_signer_set_errors/g_mime_signature_set_errors/g" \
	-e "s/g_mime_signer_get_name/g_mime_certificate_get_name/g" \
	-e "s/g_mime_signer_set_name/g_mime_certificate_set_name/g" \
	< "$src" > "$src.tmp"
    mv "$src.tmp" "$src"
done
