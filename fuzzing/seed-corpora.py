#!/usr/bin/env python3
# Adapted from MimeKit's scripts/fuzz-seed-corpora.ps1.
# Copyright (c) 2013-2026 .NET Foundation and Contributors.
# Distributed under the MIT license; see LICENSE.

"""Merge byte-exact, content-addressed seeds into persistent working corpora."""

import argparse
import base64
import hashlib
from pathlib import Path
import re

MAX_LENGTH = 1024 * 1024
LIMITS = {"mime-reader": MAX_LENGTH, "mime-parser": MAX_LENGTH,
          "encoders": 256 * 1024, "decoders": MAX_LENGTH, "rfc2047": 64 * 1024}


def mime_inputs(root):
    inputs = []
    for directory in (root / "fuzzing" / "corpus", root / "tests" / "data" / "roundtrip"):
        for path in sorted(directory.rglob("*")):
            if path.is_file() and path.suffix.lower() in (".eml", ".msg", ".mbox", ".txt"):
                data = path.read_bytes()
                if len(data) <= MAX_LENGTH:
                    inputs.append(data)
    # Like MimeKit, retain small mailboxes whole and split at most 200 messages
    # from each larger mailbox, keeping each From-line in its seed.
    for path in sorted((root / "tests" / "data" / "mbox" / "input").glob("*.mbox")):
        data = path.read_bytes()
        if len(data) <= MAX_LENGTH:
            inputs.append(data)
        messages = re.split(rb"(?<=\n)(?=From )", data)
        for index, message in enumerate(item for item in messages if item.startswith(b"From ")):
            if index == 200:
                break
            if len(message) <= MAX_LENGTH:
                inputs.append(message)
    inputs.append(b"")
    return inputs


def regression_inputs():
    lengths = (0, 1, 2, 3, 15, 16, 17, 31, 32, 33, 45, 57, 64, 76, 77, 78,
               127, 128, 129, 255, 256, 257, 4095, 4096, 4097)
    payloads = [bytes((i * 31) & 255 for i in range(length)) for length in lengths]
    payloads += [b"\r\n\r\r\n\n \t\r\nbare\rtrail \t", b"\0=\xff_?%.From ",
                 b"A" * 998, b" " * 80, b"\r", b"\n", b"\t"]
    encoded = base64.b64encode(bytes(range(228)))
    malformed = [
        b"TQ==TWE=TWFu", b"!?T=W=F=u==\xff\0", b"=3D=0D=0A=\r\n= \t=\n=QZ=",
        b"begin 644 file\n#0V%T\n`\nend\n",
        b"=ybegin line=128 size=3 file=x\r\nklm\r\n=yend size=3 crc32=0\r\n",
        b"=ybegin part=1 line=128 size=3 name=x\n=ypart begin=1 end=3\n==\r\n",
    ]
    for newline in (b"\n", b"\r\n"):
        wrapped = newline.join(encoded[i:i + 76] for i in range(0, len(encoded), 76)) + newline
        malformed += [wrapped, wrapped[64:], wrapped[:77], wrapped[:78], wrapped[:-1]]
    # Keep all selector bits reachable, including split points at both ends.
    decoders = [bytes([selector]) + malformed[selector % len(malformed)] for selector in range(256)]
    decoders += [bytes([kind | 128]) + payload for kind in range(8) for payload in payloads + malformed]
    encoders = [bytes([kind, option]) + payload
                for kind in range(5) for option in (0, 1, 16, 127, 128, 255) for payload in payloads]
    encoders += [bytes([kind, 0]) + b"\xff" * (LIMITS["encoders"] - 2) for kind in range(5)]
    decoders += [b"\x80" + b"A" * (LIMITS["decoders"] - 1)]
    # MimeKit's five RFC 2047 seeds, plus C-string, whitespace and UTF-8 edges.
    rfc2047 = [
        b"=?utf-8?q?a?= =?utf-8?q?b?=  =?utf-8?b?w6k=?=",
        b"=?iso-2022-jp?b?GyRCJCIbKEI=?= plain =?ks_c_5601-1987?b?x9GxuQ==?=",
        b"=?iso-8859-1*en?q?caf=E9?= =?unknown-charset?b?AAAA?=",
        b"=?utf-8?x?abc?= =?utf-8?q?unterminated =?=?=?utf-8??= =??q?x?=",
        b"=?utf-8?b?4oI=?= =?utf-8?b?rA==?=",
        b"", b" ", b"  plain  text  ", b"=?utf-8?q?=00?=", b"\0after-nul",
        bytes(range(256)), b"caf\xc3\xa9 " * 40, b"\xf0\x9f\x98\x80" * 40,
        b"  literal =?utf-8?q?word?=  ", b"\r\n\tfolded\r\ntext",
        b"\xff\xfe\xc0\x80\xed\xa0\x80", b"\xff" * LIMITS["rfc2047"],
    ]
    return {"encoders": encoders, "decoders": decoders, "rfc2047": rfc2047}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--target", choices=("all", *LIMITS), default="all")
    parser.add_argument("--regressions-only", action="store_true",
                        help="generate deterministic codec/header seeds without scanning fixture directories")
    parser.add_argument("--stamp", type=Path, help="write a build stamp after successful seeding")
    args = parser.parse_args()
    if args.regressions_only and args.target.startswith("mime-"):
        parser.error("--regressions-only generates encoder, decoder and RFC 2047 seeds")
    root = Path(__file__).resolve().parent.parent
    targets = tuple(LIMITS) if args.target == "all" else (args.target,)
    inputs = regression_inputs()
    if not args.regressions_only:
        if any(target.startswith("mime-") for target in targets):
            inputs["mime-reader"] = inputs["mime-parser"] = mime_inputs(root)
        for path in sorted((root / "tests" / "data" / "encodings").iterdir()):
            if path.is_file() and path.stat().st_size < LIMITS["encoders"] - 2:
                data = path.read_bytes()
                inputs["encoders"] += [bytes([kind, 128]) + data for kind in range(5)]
                inputs["decoders"] += [bytes([kind | 128]) + data for kind in range(8)]
    for target in targets:
        if target not in inputs:
            continue
        corpus = args.output / target / "corpus"
        crashes = args.output / target / "crashes"
        corpus.mkdir(parents=True, exist_ok=True)
        crashes.mkdir(parents=True, exist_ok=True)
        added = 0
        for data in inputs[target]:
            if len(data) > LIMITS[target]:
                continue
            path = corpus / hashlib.sha256(data).hexdigest()
            if not path.exists():
                path.write_bytes(data)
                added += 1
        print(f"{target}: merged {added} new seed(s) into {corpus}")
    if args.stamp:
        args.stamp.write_text("seeded\n", encoding="ascii")


if __name__ == "__main__":
    main()
