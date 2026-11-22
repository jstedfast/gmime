"""Regenerate the final-RFC RSA verifier fixtures; not needed to run tests."""

import base64
import hashlib
import re
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

root = Path(__file__).parent
key = serialization.load_pem_private_key((root / "dummy.pem").read_bytes(), None)
original = (root / "c_simple_simple.eml").read_bytes()
names = [b"mime-version", b"date", b"from", b"to", b"subject"]


def sign(data):
    return base64.b64encode(key.sign(data, padding.PKCS1v15(), hashes.SHA256()))


def relaxed(field):
    name, value = field.split(b":", 1)
    return name.lower() + b":" + re.sub(rb"[ \t]+", b" ", value.replace(b"\r\n", b"")).strip()


def hop(number, message, previous=b"", ams_tags=b"x_test=ok;", as_tags=b"", bad_seal=False):
    body = message.split(b"\r\n\r\n", 1)[1]
    headers = b"".join(re.search(b"^" + n + rb":[^\r]*\r\n", message, re.I | re.M)[0] for n in names)
    bh = base64.b64encode(hashlib.sha256(body).digest())
    i = str(number).encode()
    cv = b"none" if number == 1 else b"pass"
    aar = b"ARC-Authentication-Results: i=" + i + b"; lists.example.org;\r\n\tarc=" + cv
    aar += b"; spf=pass smtp.mfrom=jqd@d1.example"
    ams = b"ARC-Message-Signature: i=" + i + b"; a=rsa-sha256; d=example.org; s=dummy;\r\n\t"
    ams += b"c=simple/simple; t=12345; h=" + b":".join(names) + b";\r\n\tbh=" + bh + b"; b=; " + ams_tags
    ams = ams.replace(b"b=;", b"b=\r\n\t" + sign(headers + ams) + b";", 1)
    seal = b"ARC-Seal: i=" + i + b"; a=rsa-sha256; cv=" + cv + b";\r\n\t"
    seal += b"d=example.org; s=dummy; t=12345; " + as_tags + b"b="
    previous += relaxed(aar) + b"\r\n" + relaxed(ams) + b"\r\n"
    sig = sign(previous + relaxed(seal))
    if bad_seal:
        sig = (b"A" if sig[:1] != b"A" else b"B") + sig[1:]
    seal += sig
    return b"\r\n".join([seal, ams, aar, message]), previous + relaxed(seal) + b"\r\n"


def save(name, result):
    (root / (name + ".eml")).write_bytes(result[0])


one, history = hop(1, original)
save("signed-1", (one, history))
save("signed-2", hop(2, one, history))
save("old-body-changed", hop(2, one.replace(b"Hey gang,", b"Hi all,"), history))
save("old-seal-invalid", hop(2, *hop(1, original, bad_seal=True)))

# Sign even the malformed tag lists: removing a parser check must change
# the outcome, not merely expose a signature mismatch later in verification.
save("ams-duplicate", hop(1, original, ams_tags=b"x_test=1; x_test=2;"))
save("ams-invalid-name", hop(1, original, ams_tags=b"bad-name=x;"))
save("seal-forbidden-h", hop(1, original, as_tags=b"h=from; "))
save("seal-duplicate", hop(1, original, as_tags=b"cv=none; "))
save("folded-tags", hop(1, original, ams_tags=b"x_test \t=\r\n  value ;", as_tags=b"x_test = test ; "))
