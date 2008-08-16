using System;
using System.Collections;
using System.Runtime.InteropServices;

namespace GMime {
	public class InternetAddressList : IDisposable, IList {
		IntPtr list;
		bool owner;
		
#region Native Methods
		[DllImport("gmime")]
		static extern IntPtr internet_address_list_new ();
		
		[DllImport("gmime")]
		static extern void internet_address_list_destroy (IntPtr list);
		
		[DllImport("gmime")]
		static extern int internet_address_list_length (IntPtr list);
		
		[DllImport("gmime")]
		static extern void internet_address_list_clear (IntPtr list);
		
		[DllImport("gmime")]
		static extern int internet_address_list_add (IntPtr list, IntPtr ia);
		
		//[DllImport("gmime")]
		//static extern void internet_address_list_concat (IntPtr list, IntPtr concat);
		
		[DllImport("gmime")]
		static extern void internet_address_list_insert (IntPtr list, int index, IntPtr concat);
		
		[DllImport("gmime")]
		static extern bool internet_address_list_remove (IntPtr list, IntPtr ia);
		
		[DllImport("gmime")]
		static extern bool internet_address_list_remove_at (IntPtr list, int index);
		
		[DllImport("gmime")]
		static extern bool internet_address_list_contains (IntPtr list, IntPtr ia);
		
		[DllImport("gmime")]
		static extern int internet_address_list_index_of (IntPtr list, IntPtr ia);
		
		[DllImport("gmime")]
		static extern IntPtr internet_address_list_get_address (IntPtr list, int index);
		
		[DllImport("gmime")]
		static extern void internet_address_list_set_address (IntPtr list, int index, IntPtr ia);
		
		[DllImport("gmime")]
		static extern IntPtr internet_address_list_to_string (IntPtr list, bool encode);
		
		[DllImport("gmime")]
		static extern IntPtr internet_address_list_parse_string (IntPtr str);
		
		[DllImport("gmime")]
		static extern void internet_address_ref (IntPtr raw);
#endregion
		
		public InternetAddressList () : this (internet_address_list_new (), true)
		{
			
		}
		
		internal InternetAddressList (IntPtr raw, bool owner)
		{
			this.owner = owner;
			list = raw;
		}
		
		public void Dispose ()
		{
			if (owner && list != IntPtr.Zero)
				internet_address_list_destroy (list);
			
			list = IntPtr.Zero;
		}
		
		Exception CannotAdd (object value)
		{
			if (value == null)
				return new ArgumentNullException ("value");
			
			string message = String.Format ("Cannot add objects of type '{0}' to an InternetAddressList.",
							value.GetType ().ToString ());
			
			return new InvalidOperationException (message);
		}
		
		Exception CannotInsert (object value)
		{
			if (value == null)
				return new ArgumentNullException ("value");
			
			string message = String.Format ("Cannot insert objects of type '{0}' into an InternetAddressList.",
							value.GetType ().ToString ());
			
			return new InvalidOperationException (message);
		}
		
		Exception CannotRemove (object value)
		{
			if (value == null)
				return new ArgumentNullException ("value");
			
			string  message = String.Format ("Cannot remove objects of type '{0}' from an InternetAddressList.",
							 value.GetType ().ToString ());
			
			return new InvalidOperationException (message);
		}
		
		Exception CannotSet (object value)
		{
			if (value == null)
				return new ArgumentNullException ("value");
			
			string message = String.Format ("Cannot set objects of type '{0}' on an InternetAddressList.",
							value.GetType ().ToString ());
			
			return new InvalidOperationException (message);
		}
		
#region IList
		public int Count { 
			get { return internet_address_list_length (list); }
		}
		
		public bool IsFixedSize {
			get { return false; }
		}
		
		public bool IsReadOnly {
			get { return false; }
		}
		
		public bool IsSynchronized {
			get { return false; }
		}
		
		public object SyncRoot {
			get { return this; }
		}
		
		public int Add (object value)
		{
			InternetAddress addr = value as InternetAddress;
			
			if (addr == null)
				throw CannotAdd (value);
			
			return internet_address_list_add (list, addr.Handle);
		}
		
		public void Clear ()
		{
			internet_address_list_clear (list);
		}
		
		public bool Contains (object value)
		{
			InternetAddress addr = value as InternetAddress;
			
			if (addr == null)
				return false;
			
			return internet_address_list_contains (list, addr.Handle);
		}
		
		public void CopyTo (Array array, int index)
		{
			if (array == null)
				throw new ArgumentNullException ("array");
			
			if (index < 0)
				throw new ArgumentOutOfRangeException ("index");
			
			int n = Count;
			
			for (int i = 0; i < n; i++)
				array.SetValue (((IList) this)[i], index + i);
		}
		
		public IEnumerator GetEnumerator ()
		{
			return new InternetAddressListIterator (this);
		}
		
		public int IndexOf (object value)
		{
			InternetAddress addr = value as InternetAddress;
			
			if (addr == null)
				return -1;
			
			return internet_address_list_index_of (list, addr.Handle);
		}
		
		public void Insert (int index, object value)
		{
			InternetAddress addr = value as InternetAddress;
			
			if (addr == null)
				throw CannotInsert (value);
			
			if (index < 0)
				throw new ArgumentOutOfRangeException ("index");
			
			internet_address_list_insert (list, index, addr.Handle);
		}
		
		public void Remove (object value)
		{
			InternetAddress addr = value as InternetAddress;
			
			if (addr == null)
				throw CannotRemove (value);
			
			internet_address_list_remove (list, addr.Handle);
		}
		
		public void RemoveAt (int index)
		{
			if (index < 0 || index >= Count)
				throw new ArgumentOutOfRangeException ("index");
			
			internet_address_list_remove_at (list, index);
		}
		
		public object this[int index] {
			get {
				IntPtr raw = internet_address_list_get_address (list, index);
				InternetAddress addr;
				
				if (raw == IntPtr.Zero)
					return null;
				
				internet_address_ref (raw);
				
				addr = new InternetAddress (raw);
				addr.Owned = true;
				
				return addr;
			}
			
			set {
				InternetAddress addr = value as InternetAddress;
				
				if (addr == null)
					throw CannotSet (value);
				
				internet_address_list_set_address (list, index, addr.Handle);
			}
		}
#endregion
		
		public static InternetAddressList Parse (string str)
		{
			IntPtr native_str = GLib.Marshaller.StringToPtrGStrdup (str);
			IntPtr raw = internet_address_list_parse_string (native_str);
			InternetAddressList list = null;
			
			if (raw != IntPtr.Zero)
				list = new InternetAddressList (raw, true);
			
			GLib.Marshaller.Free (native_str);
			
			return list;
		}
		
		public string ToString (bool encode)
		{
			IntPtr raw = internet_address_list_to_string (list, encode);
			
			return GLib.Marshaller.PtrToStringGFree (raw);
		}
		
		public override string ToString ()
		{
			return ToString (false);
		}
		
		internal class InternetAddressListIterator : IEnumerator {
			InternetAddressList list;
			int index = -1;
			
			public InternetAddressListIterator (InternetAddressList list)
			{
				this.list = list;
			}
			
			public object Current {
				get { return list[index]; }
			}
			
			public void Reset ()
			{
				index = -1;
			}
			
			public bool MoveNext ()
			{
				index++;
				
				return index < list.Count;
			}
		}
	}
}
