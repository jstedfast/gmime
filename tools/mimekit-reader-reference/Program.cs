using System.Diagnostics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using MimeKit;

sealed class Input(byte[] data, int chunk, long origin, bool nonseekable) : Stream
{
    readonly MemoryStream source = new(data);
    public override bool CanRead => true;
    public override bool CanSeek => !nonseekable;
    public override bool CanWrite => false;
    public override long Length => source.Length;
    public override long Position { get => origin + source.Position; set => throw new NotSupportedException(); }
    public override int Read(byte[] buffer, int offset, int count) => source.Read(buffer, offset, Math.Min(count, chunk));
    public override void Flush() {}
    public override long Seek(long offset, SeekOrigin whence) => throw new NotSupportedException();
    public override void SetLength(long length) => throw new NotSupportedException();
    public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
    protected override void Dispose(bool disposing) { if (disposing) source.Dispose(); base.Dispose(disposing); }
}

sealed class Reader(ParserOptions options, Stream stream) : MimeReader(options, stream)
{
    static readonly PropertyInfo Invalid = typeof(Header).GetProperty("IsInvalid", BindingFlags.Instance | BindingFlags.NonPublic)!;
    public readonly StringBuilder Trace = new();
    readonly MemoryStream content = new(), boundary = new();

    protected override void OnHeadersBegin(long offset, int line, CancellationToken token) => Trace.Append($"B:{offset}:{line};");
    protected override void OnHeaderRead(Header header, int line, CancellationToken token)
    {
        bool invalid = (bool)Invalid.GetValue(header)!;
        var bytes = invalid ? header.RawField : header.RawField.Concat(new byte[] { 58 }).Concat(header.RawValue).ToArray();
        Trace.Append($"H:{header.Offset}:{line}:{header.Field.Length}:{header.RawField.Length}:{(invalid ? 1 : 0)}:{Convert.ToBase64String(bytes)};");
    }
    protected override void OnBodySeparator(long offset, int line, long end, CancellationToken token) => Trace.Append($"S:{offset}:{line}:{end};");
    protected override void OnHeadersEnd(long offset, int line, long end, int endLine, CancellationToken token) => Trace.Append($"E:{offset}:{line}:{end}:{endLine};");
    void EntityBegin(int kind, ContentType type, long offset, int line) => Trace.Append($"I:{kind}:{type.MimeType}:{offset}:{line};");
    void EntityEnd(int kind, long offset, int line, long headersEnd, long end, int lines) => Trace.Append($"O:{kind}:{offset}:{line}:{headersEnd}:{end}:{lines};");
    void ContentBegin(int kind, long offset, int line) { content.SetLength(0); Trace.Append($"C:{kind}:{offset}:{line};"); }
    void ContentEnd(int kind, long offset, int line, long end, int lines, NewLineFormat? format)
    {
        Trace.Append($"D:{kind}:{offset}:{line}:{end}:{lines}:{Convert.ToBase64String(content.ToArray())};");
        Trace.Append($"F:{kind}:{(format.HasValue ? (int)format.Value + 1 : 0)};");
    }
    void BoundaryBegin(bool final, long offset, int line) { boundary.SetLength(0); Trace.Append($"J:{(final ? 1 : 0)}:{offset}:{line};"); }
    void BoundaryEnd(bool final, long offset, int line, long end) => Trace.Append($"K:{(final ? 1 : 0)}:{offset}:{line}:{end}:{Convert.ToBase64String(boundary.ToArray())};");
    protected override void OnMimePartBegin(ContentType type, long offset, int line, CancellationToken token) => EntityBegin(0, type, offset, line);
    protected override void OnMultipartBegin(ContentType type, long offset, int line, CancellationToken token) => EntityBegin(1, type, offset, line);
    protected override void OnMessagePartBegin(ContentType type, long offset, int line, CancellationToken token) => EntityBegin(2, type, offset, line);
    protected override void OnMimePartEnd(ContentType type, long offset, int line, long headersEnd, long end, int lines, CancellationToken token) => EntityEnd(0, offset, line, headersEnd, end, lines);
    protected override void OnMultipartEnd(ContentType type, long offset, int line, long headersEnd, long end, int lines, CancellationToken token) => EntityEnd(1, offset, line, headersEnd, end, lines);
    protected override void OnMessagePartEnd(ContentType type, long offset, int line, long headersEnd, long end, int lines, CancellationToken token) => EntityEnd(2, offset, line, headersEnd, end, lines);
    protected override void OnMimePartContentBegin(long offset, int line, CancellationToken token) => ContentBegin(0, offset, line);
    protected override void OnMultipartPreambleBegin(long offset, int line, CancellationToken token) => ContentBegin(1, offset, line);
    protected override void OnMultipartEpilogueBegin(long offset, int line, CancellationToken token) => ContentBegin(2, offset, line);
    protected override void OnMimePartContentRead(byte[] buffer, int index, int count, CancellationToken token) => content.Write(buffer, index, count);
    protected override void OnMultipartPreambleRead(byte[] buffer, int index, int count, CancellationToken token) => content.Write(buffer, index, count);
    protected override void OnMultipartEpilogueRead(byte[] buffer, int index, int count, CancellationToken token) => content.Write(buffer, index, count);
    protected override void OnMimePartContentEnd(long offset, int line, long end, int lines, NewLineFormat? format, CancellationToken token) => ContentEnd(0, offset, line, end, lines, format);
    protected override void OnMultipartPreambleEnd(long offset, int line, long end, int lines, CancellationToken token) => ContentEnd(1, offset, line, end, lines, null);
    protected override void OnMultipartEpilogueEnd(long offset, int line, long end, int lines, CancellationToken token) => ContentEnd(2, offset, line, end, lines, null);
    protected override void OnMultipartBoundaryBegin(long offset, int line, CancellationToken token) => BoundaryBegin(false, offset, line);
    protected override void OnMultipartEndBoundaryBegin(long offset, int line, CancellationToken token) => BoundaryBegin(true, offset, line);
    protected override void OnMultipartBoundaryRead(byte[] buffer, int index, int count, long offset, int line, CancellationToken token) => boundary.Write(buffer, index, count);
    protected override void OnMultipartEndBoundaryRead(byte[] buffer, int index, int count, long offset, int line, CancellationToken token) => boundary.Write(buffer, index, count);
    protected override void OnMultipartBoundaryEnd(long offset, int line, long end, CancellationToken token) => BoundaryEnd(false, offset, line, end);
    protected override void OnMultipartEndBoundaryEnd(long offset, int line, long end, CancellationToken token) => BoundaryEnd(true, offset, line, end);
    protected override void OnMimeMessageBegin(long offset, int line, CancellationToken token) => Trace.Append($"M:{offset}:{line};");
    protected override void OnMimeMessageEnd(long offset, int line, long headersEnd, long end, int lines, CancellationToken token) => Trace.Append($"N:{offset}:{line}:{headersEnd}:{end}:{lines};");
}

static class Program
{
    static string SourceHash(string root)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        foreach (var file in Directory.EnumerateFiles(Path.Combine(root, "MimeKit"), "*.cs", SearchOption.AllDirectories)
                     .Where(path => !path.Replace('\\', '/').Contains("/bin/") && !path.Replace('\\', '/').Contains("/obj/"))
                     .Where(path => Path.GetFileName(path) != "EventHandler.cs" && Path.GetFileName(path) != "OptimizedOrdinalComparer.cs")
                     .OrderBy(path => Path.GetRelativePath(root, path).Replace('\\', '/'), StringComparer.Ordinal))
        {
            hash.AppendData(Encoding.UTF8.GetBytes(Path.GetRelativePath(root, file).Replace('\\', '/') + "\0"));
            hash.AppendData(File.ReadAllBytes(file));
        }
        return Convert.ToHexString(hash.GetHashAndReset()).ToLowerInvariant();
    }

    static void Main(string[] args)
    {
        if (args.Length != 2) throw new ArgumentException("Expected MimeKit source directory and output fixture path.");
        var start = new ProcessStartInfo("git") { RedirectStandardOutput = true };
        foreach (var arg in new[] { "-C", args[0], "rev-parse", "HEAD" }) start.ArgumentList.Add(arg);
        using var git = Process.Start(start)!;
        string revision = git.StandardOutput.ReadToEnd().Trim();
        git.WaitForExit();
        if (git.ExitCode != 0) throw new InvalidOperationException("Cannot identify the MimeKit reference revision.");

        string[] messages = [
            "", "\n", "\r\n", "Subject: no separator", "not a header\n\nbody",
            "\uFEFFSubject: BOM\n\nbody\n",
            "X \t: a\r\n\tb\nBad header\n\nbody\r",
            "Content-Type: text/plain\r\n\r\none\r\ntwo\r\n",
            "Content-Type: application/octet-stream\n\n\0\u00ff\nbinary\r\nmixed\n",
            "Content-Type: message/rfc822\n\nSubject: inner\r\n\r\nembedded\r\n",
            "Content-Type: message/rfc822\nContent-Transfer-Encoding: base64\n\nU3ViamVjdDogaW5uZXIKCmJvZHkK",
            "Content-Type: multipart/mixed; boundary=b\r\n\r\npreamble\r\n--b\r\nContent-Type: text/plain\r\n\r\nbody\r\n--b--\r\nepilogue",
            "Content-Type: multipart/mixed; boundary=b\n\n--b\nContent-Type: multipart/alternative; boundary=c\n\n--c\n\none\n--c--\n--b--\n",
            "Content-Type: multipart/mixed; boundary=b\n\n--b\n\ntruncated",
            "Content-Type: multipart/mixed; boundary=b\n\n" + new string('x', 4095) + "\r\n--b\n\nvector edge\n--b--\n"
        ];
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[1]))!);
        using var output = new StreamWriter(args[1], false, new UTF8Encoding(false)) { NewLine = "\n" };
        output.WriteLine("# MimeKit HEAD " + revision);
        output.WriteLine("# Compiled MimeKit source SHA256 " + SourceHash(args[0]));
        output.WriteLine("# base64-input|read-chunk|origin|nonseekable|success|position|eos|callback-trace");
        foreach (string message in messages)
        foreach (int chunk in new[] { 1, 128, 4096 })
        foreach (bool nonseekable in new[] { false, true })
        {
            var data = Encoding.Latin1.GetBytes(message);
            if (message.StartsWith('\uFEFF')) data = Encoding.UTF8.GetBytes(message);
            long origin = nonseekable ? 17 : 0;
            using var input = new Input(data, chunk, origin, nonseekable);
            var reader = new Reader(new ParserOptions { MaxMimeDepth = 512 }, input);
            bool success = true;
            try { reader.ReadMessage(); }
            catch (FormatException) { success = false; }
            output.WriteLine($"{Convert.ToBase64String(data)}|{chunk}|{origin}|{(nonseekable ? 1 : 0)}|{(success ? 1 : 0)}|{reader.Position}|{(reader.IsEndOfStream ? 1 : 0)}|{reader.Trace}");
        }
    }
}
