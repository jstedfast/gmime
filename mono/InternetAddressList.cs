using System;
using System.Collections;
using System.Runtime.InteropServices;

namespace GMime {
	public class InternetAddressList : ArrayList, IDisposable {
		bool needs_destroy;
		IntPtr list;

		[DllImport("gmime")]
		static extern IntPtr internet_address_list_next (IntPtr raw);

		[DllImport("gmime")]
		static extern IntPtr internet_address_list_get_address (IntPtr raw);

		[DllImport("gmime")]
		static extern void internet_address_list_destroy (IntPtr raw);

		public InternetAddressList (IntPtr list, bool needs_destroy)
		{
			this.needs_destroy = needs_destroy;
			this.list = list;
			
			while (list != IntPtr.Zero) {
				IntPtr ia = internet_address_list_get_address (list);
				
				InternetAddress addr = new InternetAddress (ia);
				Add (addr);
				
				list = internet_address_list_next (list);
			}
		}

		public InternetAddressList (IntPtr list) : this (list, false) { }

		~InternetAddressList ()
		{
			Dispose ();
		}

		public void Dispose ()
		{
			if (needs_destroy) {
				internet_address_list_destroy (list);
				needs_destroy = false;
				list = IntPtr.Zero;
			}
			GC.SuppressFinalize (this);
		}

		[DllImport("gmime")]
		static extern IntPtr internet_address_parse_string (string list);

		public static InternetAddressList ParseString (string list)
		{
			IntPtr ret = internet_address_parse_string (list);
			
			return new InternetAddressList (ret, true);
		}

		[DllImport("gmime")]
		static extern IntPtr internet_address_list_to_string (IntPtr list, bool encode);

		public string ToString (bool encode)
		{
			IntPtr str = internet_address_list_to_string (list, encode);
			
			return GLib.Marshaller.PtrToStringGFree (str);
		}

		public override string ToString ()
		{
			return ToString (false);
		}
	}
}
