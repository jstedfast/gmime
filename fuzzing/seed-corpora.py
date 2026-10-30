#!/usr/bin/env python3
# Adapted from MimeKit's scripts/fuzz-seed-corpora.ps1.
# Copyright (c) 2013-2026 .NET Foundation and Contributors.
# Distributed under the MIT license; see LICENSE.

"""Merge byte-exact, content-addressed seeds into persistent working corpora."""

import argparse
import hashlib
from pathlib import Path
import re

MAX_LENGTH = 1024 * 1024


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--target", choices=("all", "mime-reader", "mime-parser"), default="all")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
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
    targets = ("mime-reader", "mime-parser") if args.target == "all" else (args.target,)
    for target in targets:
        corpus = args.output / target / "corpus"
        crashes = args.output / target / "crashes"
        corpus.mkdir(parents=True, exist_ok=True)
        crashes.mkdir(parents=True, exist_ok=True)
        added = 0
        for data in inputs:
            path = corpus / hashlib.sha256(data).hexdigest()
            if not path.exists():
                path.write_bytes(data)
                added += 1
        print(f"{target}: merged {added} new seed(s) into {corpus}")


if __name__ == "__main__":
    main()
