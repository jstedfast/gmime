#!/usr/bin/env python3
"""Compile and run consumers using only an installed GMime prefix."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def run(command, env):
    print("+", shlex.join(str(arg) for arg in command), flush=True)
    subprocess.run(command, env=env, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path)
    parser.add_argument("--bindings", action="store_true")
    parser.add_argument("--static", action="store_true")
    args = parser.parse_args()
    prefix = args.prefix.resolve()
    env = os.environ.copy()
    env["PKG_CONFIG_PATH"] = str(prefix / "lib" / "pkgconfig")
    env["LD_LIBRARY_PATH"] = str(prefix / "lib")
    env["DYLD_LIBRARY_PATH"] = str(prefix / "lib")
    env["GI_TYPELIB_PATH"] = str(prefix / "lib" / "girepository-1.0")
    pkg_config = shlex.split(env.get("PKG_CONFIG", "pkg-config"))
    query = pkg_config + (["--static"] if args.static else [])
    version = subprocess.check_output(query + ["--modversion", "gmime-4.0"], env=env, text=True).strip()
    if not version.startswith("4."):
        raise RuntimeError(f"Expected installed GMime 4, got {version}")
    pc_dir = subprocess.check_output(query + ["--variable=pcfiledir", "gmime-4.0"], env=env, text=True).strip()
    if Path(pc_dir).resolve() != prefix / "lib" / "pkgconfig":
        raise RuntimeError(f"pkg-config selected an unexpected installation: {pc_dir}")
    flags = shlex.split(subprocess.check_output(query + ["--cflags", "--libs", "gmime-4.0"], env=env, text=True))

    with tempfile.TemporaryDirectory(prefix="gmime-installed-") as directory:
        work = Path(directory)
        source = work / "consumer.c"
        binary = work / "consumer"
        source.write_text(r'''
#include <gmime/gmime.h>
#include <string.h>

static guint diagnostics, logger_destroyed;

static void report_issue (const GMimeComplianceIssue *issue, gpointer user_data)
{
    g_assert_nonnull (issue);
    (*(guint *) user_data)++;
}

static void destroy_logger (gpointer user_data)
{
    g_assert_true (user_data == &diagnostics);
    logger_destroyed++;
}

int main (void)
{
    const char wire[] = "Subject: installed\nContent-Transfer-Encoding: base64\n\nSGVsbG8K\n";
    GMimeStream *input, *output;
    GMimeMessage *message;
    GMimeContent *content;
    GMimeYencOptions *options;
    GMimeYencHeader *header;
    GMimeYencWriter *writer;
    GMimeYencReader *reader;
    GMimeParser *parser;
    GMimeComplianceOptions *compliance;
    GByteArray *decoded;
    GError *error = NULL;
    iconv_t cd;
    char *in, *out, buffer[16];
    size_t inleft, outleft;

    g_mime_init ();
    /* This consumer deliberately uses only installed pkg-config flags.
     * Its iconv header need not belong to GMime's conversion backend. */
    cd = g_mime_iconv_open ("BIG5-HKSCS", "UTF-8");
    g_assert_true (cd != (iconv_t) -1);
    in = "\360\240\200\241";
    inleft = 4;
    out = buffer;
    outleft = sizeof buffer;
    g_assert_cmpuint (g_mime_iconv (cd, &in, &inleft, &out, &outleft), ==, 0);
    g_assert_cmpuint (inleft, ==, 0);
    g_assert_cmpmem (buffer, (out - buffer), "\234\161", 2);
    g_assert_cmpint (g_mime_iconv_close (cd), ==, 0);

    input = g_mime_stream_mem_new_with_buffer (wire, sizeof wire - 1);
    message = g_mime_message_load (NULL, input, &error);
    g_assert_no_error (error);
    g_assert_nonnull (message);
    g_assert_cmpstr (g_mime_message_get_subject (message), ==, "installed");
    content = g_mime_part_get_content (GMIME_PART (g_mime_message_get_mime_part (message)));
    output = g_mime_stream_mem_new ();
    g_assert_cmpint (g_mime_content_decode_to_stream (content, output), ==, 6);
    decoded = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (output));
    g_assert_cmpmem (decoded->data, decoded->len, "Hello\n", 6);
    g_object_unref (output);
    g_object_unref (message);
    g_assert_cmpint (g_mime_stream_reset (input), ==, 0);
    parser = g_mime_parser_new ();
    compliance = g_mime_compliance_options_new ();
    g_mime_compliance_options_set_max_issues_per_violation (compliance, 1);
    g_mime_parser_set_compliance_options (parser, compliance);
    g_mime_compliance_options_unref (compliance);
    g_assert_cmpuint (g_mime_compliance_options_get_max_issues_per_violation (
        g_mime_parser_get_compliance_options (parser)), ==, 1);
    g_mime_parser_set_compliance_logger (parser, report_issue, &diagnostics, destroy_logger);
    g_mime_parser_init_with_stream (parser, input);
    message = g_mime_parser_construct_message (parser, NULL, &error);
    g_assert_no_error (error);
    g_assert_nonnull (message);
    g_assert_cmpuint (diagnostics, >, 0);
    g_assert_cmpuint (logger_destroyed, ==, 0);
    g_mime_parser_set_compliance_logger (parser, NULL, NULL, NULL);
    g_assert_cmpuint (logger_destroyed, ==, 1);
    g_mime_parser_set_compliance_options (parser, NULL);
    g_object_unref (parser);
    g_assert_cmpuint (logger_destroyed, ==, 1);
    g_object_unref (message);
    g_object_unref (input);

    input = g_mime_stream_mem_new_with_buffer ("123456789", 9);
    output = g_mime_stream_mem_new ();
    options = g_mime_yenc_options_new ();
    header = g_mime_yenc_header_new ("installed.bin", 9, 128, 0, 0, 0, 0, &error);
    g_assert_no_error (error);
    writer = g_mime_yenc_writer_new (output, options);
    g_assert_true (g_mime_yenc_writer_write (writer, header, input, NULL, NULL, &error));
    g_assert_no_error (error);
    g_object_unref (writer);
    g_object_unref (header);
    g_object_unref (input);
    g_assert_cmpint (g_mime_stream_reset (output), ==, 0);
    reader = g_mime_yenc_reader_new (output, options);
    g_assert_true (g_mime_yenc_reader_read_next (reader, &error));
    g_assert_no_error (error);
    header = g_mime_yenc_reader_get_header (reader);
    g_assert_cmpstr (g_mime_yenc_header_get_filename (header), ==, "installed.bin");
    input = g_mime_stream_mem_new ();
    g_assert_true (g_mime_yenc_reader_decode_to (reader, input, &error));
    g_assert_no_error (error);
    decoded = g_mime_stream_mem_get_byte_array (GMIME_STREAM_MEM (input));
    g_assert_cmpmem (decoded->data, decoded->len, "123456789", 9);
    g_object_unref (input);
    g_object_unref (reader);
    g_object_unref (output);
    g_mime_yenc_options_free (options);
    g_mime_shutdown ();
    return 0;
}
''', encoding="ascii")
        run(shlex.split(env.get("CC", "cc")) + ["-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)] + flags, env)
        run([str(binary)], env)

        if args.bindings:
            for relative in ("share/gir-1.0/GMime-4.0.gir", "lib/girepository-1.0/GMime-4.0.typelib",
                             "share/vala/vapi/gmime-4.0.vapi"):
                if not (prefix / relative).is_file():
                    raise RuntimeError(f"Missing installed binding artifact: {relative}")
            run(["python3", "-c", r'''
import gi
gi.require_version('GMime', '4.0')
from gi.repository import GMime
GMime.init()
m = GMime.Message.new(False)
m.set_subject('installed', 'utf-8')
assert m.get_subject() == 'installed'
o = GMime.YencOptions.new()
assert o.get_max_line_length() == 16384
h = GMime.YencHeader.new('installed.bin', 0, 128, 0, 0, 0, 0)
assert h.get_filename() == 'installed.bin'
p = GMime.Parser.new()
o = GMime.ComplianceOptions.new()
o.set_max_issues_per_violation(1)
p.set_compliance_options(o)
assert p.get_compliance_options().get_max_issues_per_violation() == 1
issues = []
p.set_compliance_logger(lambda issue: issues.append(issue.copy()))
stream = GMime.StreamMem.new_with_buffer(b'Subject: installed\n\nbody\n')
p.init_with_stream(stream)
assert p.construct_message(None).get_subject() == 'installed'
assert len(issues) > 0
p.set_compliance_logger(None)
p.set_compliance_options(None)
assert p.get_compliance_options().get_max_issues_per_violation() == 0
GMime.shutdown()
'''], env)
            vala = work / "consumer.vala"
            vala.write_text('''
int main () {
    GMime.init ();
    var message = new GMime.Message (false);
    message.set_subject ("installed", "utf-8");
    assert (message.get_subject () == "installed");
    var options = new GMime.YencOptions ();
    assert (options.get_max_line_length () == 16384);
    var parser = new GMime.Parser ();
    var compliance = new GMime.ComplianceOptions ();
    compliance.set_max_issues_per_violation (1);
    parser.set_compliance_options (compliance);
    assert (parser.get_compliance_options ().get_max_issues_per_violation () == 1);
    uint issues = 0;
    parser.set_compliance_logger ((issue) => { issues++; });
    var input = new GMime.StreamMem.with_buffer ("Subject: installed\\n\\nbody\\n".data);
    parser.init_with_stream (input);
    try {
        var parsed = parser.construct_message (null);
        assert (parsed.get_subject () == "installed");
        assert (issues > 0);
        parser.set_compliance_logger (null);
        parser.set_compliance_options (null);
        assert (parser.get_compliance_options ().get_max_issues_per_violation () == 0);
        var header = new GMime.YencHeader ("installed.bin", 0, 128, 0, 0, 0, 0);
        assert (header.get_filename () == "installed.bin");
    } catch (GLib.Error error) {
        GLib.error ("%s", error.message);
    }
    GMime.shutdown ();
    return 0;
}
''', encoding="ascii")
            run(["valac", "--vapidir=" + str(prefix / "share" / "vala" / "vapi"),
                 "--pkg=gmime-4.0", "-o", str(binary), str(vala)], env)
            run([str(binary)], env)
    print("Installed C" + (", GIR and Vala" if args.bindings else "") + " consumers passed.")


if __name__ == "__main__":
    main()
