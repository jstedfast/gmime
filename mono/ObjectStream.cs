using System;
using System.IO;
using System.Runtime.InteropServices;

namespace GMime {

	public class ObjectStream : System.IO.Stream {

		private GMime.Object obj;
		private IntPtr raw_data = IntPtr.Zero;
		private int raw_data_length;
		private int pos = 0;

		public ObjectStream (GMime.Object obj)
		{
			if (obj == null)
				throw new ArgumentNullException ();

			this.obj = obj;
		}

		public override bool CanRead {
			get { return true; }
		}

		public override bool CanSeek {
			get { return true; }
		}

		public override bool CanWrite {
			get { return false; }
		}

		public override long Length {
			get {
				if (this.raw_data == IntPtr.Zero)
					GetObjectData ();

				return this.raw_data_length;
			}
		}

		public override long Position {
			get { return this.pos; }
			set { throw new NotSupportedException (); }
		}

		public override void Close ()
		{
			g_free (this.raw_data);
			this.raw_data = IntPtr.Zero;
		}

		public override void Flush ()
		{
		}

		public override int Read (byte[] buffer, int offset, int count)
		{
			if (buffer == null)
				throw new ArgumentNullException ();

			if (offset < 0 || count < 0)
				throw new ArgumentOutOfRangeException ();

			if (buffer.Length - offset < count)
				throw new ArgumentOutOfRangeException ();

			if (this.raw_data == IntPtr.Zero)
				GetObjectData ();

			if (this.pos >= this.raw_data_length || count == 0)
				return 0;

			if (this.pos + count > this.raw_data_length)
				count = this.raw_data_length - this.pos;

			long ptr_int = this.raw_data.ToInt64 ();
			IntPtr data_with_offset = new IntPtr (ptr_int + this.pos);

			Marshal.Copy (data_with_offset, buffer, offset, count);

			this.pos += count;

			return count;
		}

		public override long Seek (long offset, SeekOrigin origin)
		{
			long absolute_pos = 0;

			switch (origin) {
			case SeekOrigin.Begin:
				if (offset < 0)
					throw new ArgumentOutOfRangeException ();
				
				absolute_pos = offset;
				break;

			case SeekOrigin.Current:
				absolute_pos = this.pos + offset;
				break;

			case SeekOrigin.End:
				if (offset > 0)
					throw new ArgumentOutOfRangeException ();

				absolute_pos = this.pos + offset;
				break;
			}

			this.pos = (int) absolute_pos;

			return absolute_pos;
		}

		public override void SetLength (long value)
		{
			throw new NotSupportedException ();
		}

		public override void Write (byte[] buffer, int offset, int count)
		{
			throw new NotSupportedException ();
		}

		[DllImport ("gmime")]
		static extern IntPtr g_mime_object_to_string (IntPtr raw);

		[DllImport ("libc")]
		static extern int strlen (IntPtr raw_string);

		[DllImport ("libglib-2.0.so.0")]
		static extern void g_free (IntPtr mem);

		private void GetObjectData ()
		{
			this.raw_data = g_mime_object_to_string (obj.Handle);
			this.raw_data_length = strlen (this.raw_data);
		}
	}
}
