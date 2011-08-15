using System;
using System.Collections;

namespace GMime {
	public class HeaderEnumerator : IEnumerator, IDisposable {
		bool premove = true;
		HeaderList headers;
		HeaderIter iter;
		
		public HeaderEnumerator (HeaderList headers)
		{
			if (headers == null)
				throw new ArgumentNullException ("headers");
			
			iter = headers.GetIter ();
			this.headers = headers;
		}
		
		public Header Current {
			get {
				CheckDisposed ();
				return new Header (iter.Name, iter.Value);
			}
		}
		
		object IEnumerator.Current {
			get { return Current; }
		}
		
		public string Name {
			get {
				CheckDisposed ();
				return iter.Name;
			}
		}
		
		public string Value {
			set {
				CheckDisposed ();
				iter.Value = value;
			}
			
			get {
				CheckDisposed ();
				return iter.Value;
			}
		}
		
		public bool MoveFirst ()
		{
			CheckDisposed ();
			
			premove = false;
			return iter.MoveFirst ();
		}
		
		public bool MoveLast ()
		{
			CheckDisposed ();
			
			premove = false;
			return iter.MoveLast ();
		}
		
		public bool MoveNext ()
		{
			CheckDisposed ();
			
			if (premove) {
				premove = false;
				return iter.MoveFirst ();
			}
			
			return iter.MoveNext ();
		}
		
		public bool MovePrev ()
		{
			CheckDisposed ();
			
			if (premove)
				return false;
			
			premove = false;
			return iter.MovePrev ();
		}
		
		public bool Remove ()
		{
			CheckDisposed ();
			return iter.Remove ();
		}
		
		public void Reset ()
		{
			CheckDisposed ();
			iter.MoveFirst ();
			premove = true;
		}

		void CheckDisposed ()
		{
			if (headers == null)
				throw new ObjectDisposedException ("The HeaderEnumerator has been disposed.");
		}
		
		public void Dispose ()
		{
			headers = null;
			iter = null;
		}
	}
}
