using System;
using System.Collections;
using System.Runtime.InteropServices;

namespace GMime {
	public class SignerCollection : IList {
		Signer[] signers;
		
		internal SignerCollection (SignatureValidity validity)
		{
			Signer signer;
			int count = 0;
			int i = 0;
			
			signer = GetFirstSigner (validity);
			while (signer != null) {
				signer = signer.Next ();
				count++;
			}
			
			signers = new Signer [count];
			signer = GetFirstSigner (validity);
			while (signer != null) {
				signers[i++] = signer;
				signer = signer.Next ();
			}
		}
		
		public int Count {
			get { return signers.Length; }
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
			if (signer == null)
				return false;
			
			for (int i = 0; i < Count; i++) {
				if (signers[i] == signer)
					return true;
			}
			
			return false;
		}
		
		bool IList.Contains (object value)
		{
			return Contains (value as Signer);
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
			return new SignerIterator (this);
		}
		
		public int IndexOf (Signer signer)
		{
			if (signer == null)
				return -1;
			
			for (int i = 0; i < Count; i++) {
				if (signers[i] == signer)
					return i;
			}
			
			return -1;
		}
		
		int IList.IndexOf (object value)
		{
			return IndexOf (value as Signer);
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
				if (index > Count)
					throw new ArgumentOutOfRangeException ("index");
				
				return signers[index];
			}
			
			set {
				throw new InvalidOperationException ("Cannot set signers in a SignerCollection.");
			}
		}
		
		object IList.this[int index] {
			get {
				return this[index];
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
		
		internal class SignerIterator : IEnumerator {
			SignerCollection signers;
			int index = -1;
			
			public SignerIterator (SignerCollection signers)
			{
				this.signers = signers;
			}
			
			public object Current {
				get { return index >= 0 ? signers[index] : null; }
			}
			
			public void Reset ()
			{
				index = -1;
			}
			
			public bool MoveNext ()
			{
				index++;
				
				return index < signers.Count;
			}
		}
	}
}
