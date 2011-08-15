using System;

namespace GMime {
	public class Header {
		public Header () { }
		
		public Header (string name, string value)
		{
			Value = value;
			Name = name;
		}
		
		public string Name {
			get; set;
		}
		
		public string Value {
			get; set;
		}
	}
}
