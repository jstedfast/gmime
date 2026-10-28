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
    GByteArray *decoded;
    GError *error = NULL;

    g_mime_init ();
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
            run(["python3", "-c",
                 "import gi; gi.require_version('GMime', '4.0'); "
                 "from gi.repository import GMime; GMime.init(); "
                 "m = GMime.Message.new(False); m.set_subject('installed', 'utf-8'); "
                 "assert m.get_subject() == 'installed'; "
                 "o = GMime.YencOptions.new(); assert o.get_max_line_length() == 16384; "
                 "h = GMime.YencHeader.new('installed.bin', 0, 128, 0, 0, 0, 0); "
                 "assert h.get_filename() == 'installed.bin'; GMime.shutdown()"], env)
            vala = work / "consumer.vala"
            vala.write_text('''
int main () {
    GMime.init ();
    var message = new GMime.Message (false);
    message.set_subject ("installed", "utf-8");
    assert (message.get_subject () == "installed");
    var options = new GMime.YencOptions ();
    assert (options.get_max_line_length () == 16384);
    try {
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
