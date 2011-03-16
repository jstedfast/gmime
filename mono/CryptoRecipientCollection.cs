using System;
using System.Collections;
using System.Runtime.InteropServices;

namespace GMime {
	public class CryptoRecipientCollection : IList {
		CryptoRecipient[] recipients;
		
		internal CryptoRecipientCollection (DecryptionResult result)
		{
			CryptoRecipient recipient;
			int count = 0;
			int i = 0;
			
			recipient = GetFirstRecipient (result);
			while (recipient != null) {
				recipient = recipient.Next ();
				count++;
			}
			
			recipients = new CryptoRecipient [count];
			recipient = GetFirstRecipient (result);
			while (recipient != null) {
				recipients[i++] = recipient;
				recipient = recipient.Next ();
			}
		}
		
		public int Count {
			get { return recipients.Length; }
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
			if (recipient == null)
				return false;
			
			for (int i = 0; i < Count; i++) {
				if (recipients[i] == recipient)
					return true;
			}
			
			return false;
		}
		
		bool IList.Contains (object value)
		{
			return Contains (value as CryptoRecipient);
		}
		
		public void CopyTo (Array array, int index)
		{
			if (array == null)
				throw new ArgumentNullException ("array");
			
			if (index < 0)
				throw new ArgumentOutOfRangeException ("index");
			
			for (int i = 0; i < Count; i++)
				array.SetValue (((IList) this)[i], index + i);
		}
		
		public IEnumerator GetEnumerator ()
		{
			return new CryptoRecipientIterator (this);
		}
		
		public int IndexOf (CryptoRecipient recipient)
		{
			if (recipient == null)
				return -1;
			
			for (int i = 0; i < Count; i++) {
				if (recipients[i] == recipient)
					return i;
			}
			
			return -1;
		}
		
		int IList.IndexOf (object value)
		{
			return IndexOf (value as CryptoRecipient);
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
				if (index > Count)
					throw new ArgumentOutOfRangeException ("index");
				
				return recipients[index];
			}
			
			set {
				throw new InvalidOperationException ("Cannot set recipients in a CryptoRecipientCollection.");
			}
		}
		
		object IList.this[int index] {
			get {
				return this[index];
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
		
		internal class CryptoRecipientIterator : IEnumerator {
			CryptoRecipientCollection recipients;
			int index = -1;
			
			public CryptoRecipientIterator (CryptoRecipientCollection recipients)
			{
				this.recipients = recipients;
			}
			
			public object Current {
				get { return index >= 0 ? recipients[index] : null; }
			}
			
			public void Reset ()
			{
				index = -1;
			}
			
			public bool MoveNext ()
			{
				index++;
				
				return index < recipients.Count;
			}
		}
	}
}
