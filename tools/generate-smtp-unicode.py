#!/usr/bin/env python3
"""Import MimeKit's pinned SMTPUTF8 Unicode tables without host Unicode APIs."""

import argparse
import ast
import hashlib
from pathlib import Path
import re
import struct
import subprocess

REFERENCE = "bbccea7630ba9475d03a2a09a1c5046924d8bf3f"


def test_vectors(reference, output, notice):
    literal = r'"(?:[^"\\]|\\.)*"'
    errors = ["None", "Syntax", "LocalPartTooLong", "LabelTooLong",
              "DomainTooLong", "AddressTooLong", "ALabel", "DisallowedCodePoint",
              "NotNormalized", "ContextRule", "MixedScripts"]
    rows = []
    skipped = 0
    for utf8 in (False, True):
        name = f"Smtp{'Utf8' if utf8 else ''}AddressValidatorTests.cs"
        source = subprocess.check_output(
            ["git", "-C", str(reference), "show", f"{REFERENCE}:UnitTests/Utils/{name}"]
        ).decode("utf-8-sig")
        constants = dict(re.findall(rf"const string (\w+) = ({literal});", source))
        pending = []
        for match in re.finditer(r"\[TestCase \((.*?)\)\]|public void (\w+) \([^)]*\)\s*\{([^}]+)", source, re.S):
            if match[1] is not None:
                pending.append(match[1])
                continue
            method, body = match[2], match[3]
            default = re.search(r"Smtp(?:Utf8)?AddressError\.(\w+)", body)
            options = {"TestIso2022Jp": 1, "TestDecodeIdn": 1,
                       "TestAsciiIsLatin": 2, "TestSpoofDetection": 3}.get(method, 0)
            for args in pending:
                token = re.match(literal + r"|\w+", args)[0]
                token = constants.get(token, token)
                if not token.startswith('"'):
                    raise ValueError(f"Unknown test argument {token}")
                text = ast.literal_eval(token)
                expected = re.search(r"Smtp(?:Utf8)?AddressError\.(\w+)", args) or default
                if method == "TestLabelLengthBoundary":
                    continue  # Covered by native byte/Punycode boundary tests.
                if expected is None:
                    raise ValueError(f"Missing expected result in {method}")
                try:
                    data = text.encode("utf-8")
                except UnicodeEncodeError:
                    skipped += 1  # Lone UTF-16 surrogates have no valid UTF-8 representation.
                    continue
                names = errors if utf8 else errors[:2] + ["NonAscii"] + errors[2:6]
                escaped = "".join(chr(b) if 32 <= b < 127 and b not in (34, 92)
                                  else f"\\{b:03o}" for b in data)
                rows.append(f'\t{{ "{escaped}", {options}, {names.index(expected[1])}, '
                            f'{"TRUE" if utf8 else "FALSE"} }},')
            pending.clear()
    output.write_text("\n".join([
        "/*", notice, " *",
        f" * Imported TestCase vectors from MimeKit {REFERENCE}.",
        " * Regenerate with tools/generate-smtp-unicode.py.",
        f" * {skipped} lone-surrogate cases omitted; native invalid UTF-8 is tested separately.",
        " */", "static const struct {", "\tconst char *address;",
        "\tguint options;", "\tint expected;", "\tgboolean utf8;",
        "} smtp_vectors[] = {", *rows, "};", ""
    ]), encoding="ascii", newline="\n")
    print(f"Generated {len(rows)} SMTP test vectors: {output}")


def array(source, name, width):
    match = re.search(rf"{name} => new byte\[\] \{{(.*?)\}};", source, re.S)
    if match is None:
        raise ValueError(f"Missing table {name}")
    data = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", match[1]))
    return [item[0] for item in struct.iter_unpack("<I" if width == 4 else "<H", data)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="MimeKit checkout at the pinned revision")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] /
                        "gmime" / "gmime-smtp-unicode-private.h")
    parser.add_argument("--tests-output", type=Path, default=Path(__file__).resolve().parents[1] /
                        "tests" / "smtp-address-vectors.h")
    args = parser.parse_args()
    revision = subprocess.check_output(
        ["git", "-C", str(args.reference), "rev-parse", "HEAD"], text=True).strip()
    if revision != REFERENCE:
        raise ValueError(f"Expected MimeKit {REFERENCE}, got {revision}")

    sources = []
    provenance = []
    for filename in ("SmtpUtf8AddressValidator.g.cs", "UnicodeNormalization.g.cs"):
        path = args.reference / "MimeKit" / "Utils" / filename
        data = path.read_bytes()
        original = subprocess.check_output(
            ["git", "-C", str(args.reference), "show", f"{REFERENCE}:MimeKit/Utils/{filename}"])
        # Git may check these sources out with different line endings.
        if data.replace(b"\r\n", b"\n") != original.replace(b"\r\n", b"\n"):
            raise ValueError(f"Modified reference table: {filename}")
        sources.append(data.decode("utf-8-sig"))
        provenance.append(f" * {filename}: SHA256 {hashlib.sha256(original).hexdigest()}")
    versions = [re.search(r'UnicodeVersion = "([^"]+)"', source)[1] for source in sources]
    if versions != ["18.0.0", "18.0.0"]:
        raise ValueError(f"Unexpected Unicode versions: {versions}")

    scripts = re.search(r"enum UnicodeScript : ushort\s*\{([^}]+)\}", sources[0])[1]
    scripts = [name.strip() for name in scripts.split(",") if name.strip()]
    tables = [
        ("smtp_codepoint_data", array(sources[0], "CodePointData", 4), 4, 2152),
        ("smtp_joining_data", array(sources[0], "JoiningData", 4), 4, 981),
        ("smtp_normalization_data", array(sources[1], "NormalizationData", 4), 4, 842),
        ("smtp_composition_data", array(sources[1], "CompositionData", 4), 4, 961 * 3),
        ("smtp_composition_pairs", array(sources[1], "CompositionPairs", 2), 2, 961),
    ]
    for name, values, width, count in tables:
        if len(values) != count:
            raise ValueError(f"Unexpected {name} length: {len(values)}")
    for (_, values, _, _), shift in zip(tables[:3], (11, 4, 10)):
        starts = [value >> shift for value in values]
        if starts[0] != 0 or any(a >= b for a, b in zip(starts, starts[1:])):
            raise ValueError("Invalid range table")
    compositions = tables[3][1]
    pairs = tables[4][1]
    if sorted(pairs) != list(range(961)):
        raise ValueError("Invalid composition indexes")
    keys = [(compositions[i * 3 + 1], compositions[i * 3 + 2]) for i in pairs]
    if keys != sorted(set(keys)):
        raise ValueError("Unsorted composition pairs")

    # Preserve the upstream MIT notice with the converted data.
    notice = sources[0].split("// WARNING:", 1)[0]
    notice = "\n".join(" *" + line[2:] for line in notice.splitlines() if line.startswith("//"))
    output = ["/*", notice, " *",
              " * Generated by tools/generate-smtp-unicode.py; do not edit.",
              f" * MimeKit revision: {REFERENCE}", *provenance, " */",
              "#ifndef __GMIME_SMTP_UNICODE_PRIVATE_H__",
              "#define __GMIME_SMTP_UNICODE_PRIVATE_H__", "",
              '#define SMTP_UNICODE_VERSION "18.0.0"', "", "typedef enum {"]
    output.extend(f"\tSMTP_SCRIPT_{name.upper()} = {i}," for i, name in enumerate(scripts))
    output.extend(["} SmtpUnicodeScript;", ""])
    for name, values, width, _ in tables:
        output.append(f"static const guint{width * 8} {name}[] = {{")
        for start in range(0, len(values), 8):
            output.append("\t" + ", ".join(f"0x{value:0{width * 2}x}" for value in values[start:start + 8]) + ",")
        output.extend(["};", ""])
    output.extend(["#endif", ""])
    args.output.write_text("\n".join(output), encoding="ascii", newline="\n")
    print(f"Generated Unicode {versions[0]} tables: {args.output}")
    test_vectors(args.reference, args.tests_output, notice)


if __name__ == "__main__":
    main()
