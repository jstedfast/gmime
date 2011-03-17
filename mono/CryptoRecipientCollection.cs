using System;
using System.Collections;
using System.Runtime.InteropServices;

namespace GMime {
	public class CryptoRecipientCollection : IList {
		ArrayList recipients;
		
		internal CryptoRecipientCollection (DecryptionResult result)
		{
			CryptoRecipient recipient;
			
			recipients = new ArrayList ();
			recipient = GetFirstRecipient (result);
			while (recipient != null) {
				recipients.Add (recipient);
				recipient = recipient.Next ();
			}
		}
		
		public int Count {
			get { return recipients.Count; }
		}
		
		public bool IsFixedSize {
			get { return true; }
		}
		
		public bool IsReadOnly {
			get { return true; }
		}
		
		public bool IsSynchronized {
			get { return false; }
		}
		
		public object SyncRoot {
			get { return this; }
		}
		
		int IList.Add (object value)
		{
			throw new InvalidOperationException ("Cannot add recipients to a CryptoRecipientCollection.");
		}
		
		public void Clear ()
		{
			throw new InvalidOperationException ("Cannot clear a CryptoRecipientCollection.");
		}
		
		public bool Contains (CryptoRecipient recipient)
		{
			return recipients.Contains (recipient);
		}
		
		bool IList.Contains (object value)
		{
			return recipients.Contains (value);
		}
		
		public void CopyTo (Array array, int index)
		{
			recipients.CopyTo (array, index);
		}
		
		public IEnumerator GetEnumerator ()
		{
			return recipients.GetEnumerator ();
		}
		
		public int IndexOf (CryptoRecipient recipient)
		{
			return recipients.IndexOf (recipient);
		}
		
		int IList.IndexOf (object value)
		{
			return recipients.IndexOf (value);
		}
		
		void IList.Insert (int index, object value)
		{
			throw new InvalidOperationException ("Cannot insert recipients into a CryptoRecipientCollection.");
		}
		
		void IList.Remove (object value)
		{
			throw new InvalidOperationException ("Cannot remove recipients from a CryptoRecipientCollection.");
		}
		
		void IList.RemoveAt (int index)
		{
			throw new InvalidOperationException ("Cannot remove recipients from a CryptoRecipientCollection.");
		}
		
		public CryptoRecipient this[int index] {
			get {
				return recipients[index] as CryptoRecipient;
			}
			
			set {
				throw new InvalidOperationException ("Cannot set recipients in a CryptoRecipientCollection.");
			}
		}
		
		object IList.this[int index] {
			get {
				return recipients[index];
			}
			
			set {
				throw new InvalidOperationException ("Cannot set recipients in a CryptoRecipientCollection.");
			}
		}
		
		[DllImport("gmime")]
		static extern IntPtr g_mime_decryption_result_get_recipients(IntPtr raw);
		
		static CryptoRecipient GetFirstRecipient (DecryptionResult valididty)
		{
			IntPtr rv = g_mime_decryption_result_get_recipients (valididty.Handle);
			
			if (rv == IntPtr.Zero)
				return null;
			
			return (CryptoRecipient) GLib.Opaque.GetOpaque (rv, typeof (CryptoRecipient), false);
		}
	}
}
