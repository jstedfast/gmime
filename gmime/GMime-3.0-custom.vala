namespace GMime {

    public abstract class Object : GLib.Object {
        [CCode (cname = "g_mime_object_new")]
        public static Object new_for_type(GMime.ParserOptions? options,
                                          GMime.ContentType content_type);
        [CCode (cname = "g_mime_object_new_type")]
        public static Object new_for_type_str(GMime.ParserOptions? options,
                                              string type,
                                              string subtype);
    }

}
