using System;
using System.Collections;
using System.Runtime.InteropServices;

namespace GMime {
	public class InternetAddressList : ArrayList, IDisposable {
		private IntPtr raw_list;
		private bool needs_destroy;

		[DllImport("gmime")]
		static extern IntPtr internet_address_list_next (IntPtr raw);

		[DllImport("gmime")]
		static extern IntPtr internet_address_list_get_address (IntPtr raw);

		[DllImport("gmime")]
		static extern void internet_address_list_destroy (IntPtr raw);

		public InternetAddressList (IntPtr raw_list, bool needs_destroy)
		{
			this.raw_list = raw_list;
			this.needs_destroy = needs_destroy;
			
			while (raw_list != IntPtr.Zero) {
				IntPtr raw_ia = internet_address_list_get_address (raw_list);
				
				InternetAddress addr = new InternetAddress (raw_ia);
				this.Add (addr);
				
				raw_list = internet_address_list_next (raw_list);
			}
		}

		public InternetAddressList (IntPtr raw_list) : this (raw_list, false) { }

		~InternetAddressList ()
		{
			this.Dispose ();
		}
		
		public void Dispose ()
		{
			if (this.needs_destroy) {
				internet_address_list_destroy (this.raw_list);
				this.raw_list = IntPtr.Zero;
				this.needs_destroy = false;
			}
			GC.SuppressFinalize (this);
		}

		[DllImport("gmime")]
		static extern IntPtr internet_address_parse_string (string list);

		public static InternetAddressList ParseString (string list)
		{
			IntPtr raw_ret = internet_address_parse_string (list);
			
			return new InternetAddressList (raw_ret, true);
		}
	}
}
