using System;
using System.Collections;
using System.Runtime.InteropServices;

namespace GMime {
	public class SignerCollection : IList {
		ArrayList signers;
		
		internal SignerCollection (SignatureValidity validity)
		{
			Signer signer;
			
			signers = new ArrayList ();
			
			signer = GetFirstSigner (validity);
			while (signer != null) {
				signers.Add (signer);
				signer = signer.Next ();
			}
		}
		
		public int Count {
			get { return signers.Count; }
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
			throw new InvalidOperationException ("Cannot add signers to a SignerCollection.");
		}
		
		public void Clear ()
		{
			throw new InvalidOperationException ("Cannot clear a SignerCollection.");
		}
		
		public bool Contains (Signer signer)
		{
			return signers.Contains (signer);
		}
		
		bool IList.Contains (object value)
		{
			return signers.Contains (value);
		}
		
		public void CopyTo (Array array, int index)
		{
			signers.CopyTo (array, index);
		}
		
		public IEnumerator GetEnumerator ()
		{
			return signers.GetEnumerator ();
		}
		
		public int IndexOf (Signer signer)
		{
			return signers.IndexOf (signer);
		}
		
		int IList.IndexOf (object value)
		{
			return signers.IndexOf (value);
		}
		
		void IList.Insert (int index, object value)
		{
			throw new InvalidOperationException ("Cannot insert signers into a SignerCollection.");
		}
		
		void IList.Remove (object value)
		{
			throw new InvalidOperationException ("Cannot remove signers from a SignerCollection.");
		}
		
		void IList.RemoveAt (int index)
		{
			throw new InvalidOperationException ("Cannot remove signers from a SignerCollection.");
		}
		
		public Signer this[int index] {
			get {
				return signers[index] as Signer;
			}
			
			set {
				throw new InvalidOperationException ("Cannot set signers in a SignerCollection.");
			}
		}
		
		object IList.this[int index] {
			get {
				return signers[index];
			}
			
			set {
				throw new InvalidOperationException ("Cannot set signers in a SignerCollection.");
			}
		}
		
		[DllImport("gmime")]
		static extern IntPtr g_mime_signature_validity_get_signers(IntPtr raw);
		
		static Signer GetFirstSigner (SignatureValidity valididty)
		{
			IntPtr rv = g_mime_signature_validity_get_signers (valididty.Handle);
			
			if (rv == IntPtr.Zero)
				return null;
			
			return (Signer) GLib.Opaque.GetOpaque (rv, typeof (Signer), false);
		}
	}
}
