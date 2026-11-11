namespace GMime {

    public abstract class Entity : GLib.Object {
        [CCode (cname = "g_mime_entity_new")]
        public static Entity new_for_type(GMime.ParserOptions? options,
                                          GMime.ContentType content_type);
        [CCode (cname = "g_mime_entity_new_type")]
        public static Entity new_for_type_str(GMime.ParserOptions? options,
                                              string type,
                                              string subtype);
    }

}
