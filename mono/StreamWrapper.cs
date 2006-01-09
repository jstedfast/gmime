using System;
using System.IO;

namespace GMime {

	public class StreamWrapper : System.IO.Stream, IDisposable {

		private GMime.Stream gmime_stream;

		public StreamWrapper (GMime.Stream gmime_stream)
		{
			if (gmime_stream == null)
				throw new ArgumentNullException ();

			this.gmime_stream = gmime_stream;
		}

		~StreamWrapper ()
		{
			Close ();
		}

		public void Dispose ()
		{
			Close ();
		}

		public GMime.Stream GMimeStream {
			get { return this.gmime_stream; }
			set { 
				if (value == null)
					throw new ArgumentNullException ();

				this.gmime_stream = value;
			}
		}

		public override bool CanRead {
			get { return this.gmime_stream == null ? false : true; }
		}

		public override bool CanSeek {
			get { return this.gmime_stream == null ? false : true; }
		}
		
		// FIXME: Support writing?
		public override bool CanWrite {
			get { return false; }
		}

		public override long Length {
			get { 
				if (this.gmime_stream == null)
					throw new ObjectDisposedException ("GMimeStream", "The stream has been closed");

				return this.gmime_stream.Length;
			}
		}

		public override long Position {
			get { 
				if (this.gmime_stream == null)
					throw new ObjectDisposedException ("GMimeStream", "The stream has been closed");

				return this.gmime_stream.Tell ();
			}

			set { Seek (value, SeekOrigin.Begin); }
		}

		public override void Close ()
		{
			if (this.gmime_stream == null)
				throw new ObjectDisposedException ("GMimeStream", "The stream has been closed");

			this.gmime_stream.Dispose ();
			this.gmime_stream = null;
		}

		public override void Flush ()
		{
		}

		public override int Read (byte[] buffer, int offset, int count)
		{
			if (this.gmime_stream == null)
				throw new ObjectDisposedException ("GMimeStream", "The stream has been closed");

			if (offset != 0)
				throw new ArgumentOutOfRangeException ("offset must be 0");

			int bytes_read = (int) this.gmime_stream.Read (buffer, (uint) count);

			if (bytes_read < 0)
				return 0;

			return bytes_read;
		}

		public override long Seek (long offset, SeekOrigin origin)
		{
			if (this.gmime_stream == null)
				throw new ObjectDisposedException ("GMimeStream", "The stream has been closed");

			switch (origin) {
			case SeekOrigin.Begin:
				if (offset < 0)
					throw new ArgumentOutOfRangeException ();
				
				return this.gmime_stream.Seek (offset, GMime.SeekWhence.Set);

			case SeekOrigin.Current:
				return this.gmime_stream.Seek (offset, GMime.SeekWhence.Cur);

			case SeekOrigin.End:
				if (offset > 0)
					throw new ArgumentOutOfRangeException ();

				return this.gmime_stream.Seek (offset, GMime.SeekWhence.End);
			}

			// Dunno why the compiler is insisting on this...
			throw new Exception ();
		}

 		public override void SetLength (long value)
		{
			throw new NotSupportedException ();
		}

		public override void Write (byte[] buffer, int offset, int count)
		{
			throw new NotSupportedException ();
		}
	}
}
