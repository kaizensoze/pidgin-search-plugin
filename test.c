#include "internal.h"
#include "pidgin.h"

#include "conversation.h"

#include "debug.h"
#include "notify.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkprefs.h"
#include "gtkutils.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "search_xml_utils.c"


enum {
    LIST_ITEM = 0,
    /*
    ACTIVE_COLUMN,
	BAD_COLUMN,
	GOOD_COLUMN,
	WORD_ONLY_COLUMN,
	CASE_SENSITIVE_COLUMN,
    */
	N_COLUMNS
};

typedef struct {
	PurpleConversation *conv; // pointer to the conversation 
	GtkWidget *seperator; // seperator in the conversation 
        GtkWidget *combo_box;
	GtkWidget *button; // button in the conversation 
	GPid pid; // the pid of the score editor 
	
	gboolean started; // session has started and editor run 
	gboolean originator; // started the mm session 
	gboolean requested; // received a request to start a session 
	
} MMConversation;


static void add_widgets (MMConversation *mmconv);
static void remove_widget (GtkWidget *button);
static void init_conversation (PurpleConversation *conv);
static void conv_destroyed (PurpleConversation *conv);

static GtkWidget *active_list;

/* List of sessions */
static GList *conversations;

/* Map of search engines */
//static GHashTable *search_engines;
// moved to the utility include

PurplePlugin *test_plugin = NULL;


static void open_url(const gchar *search_term, const gchar *selected_search_engine)
{
    /*
     * TODO:
     * 
     * The query urls associated with search engines will contain {search_term}
     * or something like that. A replace_string utility method will be needed
     * to replace the substring "{search_term}" with the actual search_term.
     * NOTE: {search_term} could appear multiple times in the URL according to
     * the opensearch specs.
     */
	search_engine *se;
    se = g_hash_table_lookup(search_engines, selected_search_engine);
    gchar *search_url = g_strconcat(se->query_url, search_term, NULL);
    purple_debug_info(TEST_PLUGIN_ID, "search_term: %s\n", search_term);
    purple_notify_uri(NULL, search_url);
}

/*
static GtkWidget *bad_entry;
static GtkWidget *good_entry;
static GtkWidget *complete_toggle;
static GtkWidget *case_toggle;
static void save_list(void);
*/

static void load_search_engines() {
	load_all_from_opensearch_files_dir();
	
	/*	
	search_engine *site;
    const gchar *key;
	
    search_engines = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	site = g_malloc(sizeof(search_engine));
	site->name = "Answers";
	site->query_url = "http://www.answers.com/topic/";
	site->icon_url = "no_image";
    key = "Answers";

    g_hash_table_insert(search_engines, g_strdup(key), site);

	site = g_malloc(sizeof(search_engine));
	site->name = "Google";
	site->query_url = "http://www.google.com/#hl=en&q=";
	site->icon_url = "no_image";
    key = "Google";
    
    g_hash_table_insert(search_engines, g_strdup(key), site);

	site = g_malloc(sizeof(search_engine));
	site->name = "Wikipedia";
	site->query_url = "http://en.wikipedia.org/wiki/";
	site->icon_url = "no_image";
    key = "Wikipedia";

    g_hash_table_insert(search_engines, g_strdup(key), site);

    // view contents of search engine hashtable
    GList *keys = g_hash_table_get_keys(search_engines);
    purple_debug_info(TEST_PLUGIN_ID, "Search Engines:\n");
    while (keys) {
        search_engine *eng;
        gpointer key = keys->data;

        eng = g_hash_table_lookup(search_engines, key);
        purple_debug_info(TEST_PLUGIN_ID, "%s\n", eng->name);
        keys = keys->next;
    }
    */
}

static void active_toggled(GtkCellRendererToggle *cellrenderertoggle,
						gchar *path, gpointer data) {
    /*
	GtkTreeIter iter;
	gboolean enabled;

	g_return_if_fail(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &iter, path));
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
					   WORD_ONLY_COLUMN, &enabled,
					   -1);

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					   WORD_ONLY_COLUMN, !enabled,
					   -1);

	// I want to be sure that the above change has happened to the GtkTreeView first. 
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					   CASE_SENSITIVE_COLUMN, enabled,
					   -1);

	save_list();
    */
}

/*
static void case_sensitive_toggled(GtkCellRendererToggle *cellrenderertoggle,
						gchar *path, gpointer data){
	GtkTreeIter iter;
	gboolean enabled;

	g_return_if_fail(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &iter, path));

	// Prevent the case sensitive column from changing on non-whole word replacements.
	 // Ideally, the column would be set insensitive in the word_only_toggled callback.
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
					   WORD_ONLY_COLUMN, &enabled,
					   -1);
	if (!enabled)
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
					   CASE_SENSITIVE_COLUMN, &enabled,
					   -1);

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
					   CASE_SENSITIVE_COLUMN, !enabled,
					   -1);

	save_list();
}

static void add_selected_row_to_list(GtkTreeModel *model, GtkTreePath *path,
	GtkTreeIter *iter, gpointer data)
{
	GtkTreeRowReference *row_reference;
	GSList **list = (GSList **)data;
	row_reference = gtk_tree_row_reference_new(model, path);
	*list = g_slist_prepend(*list, row_reference);
}

static void remove_row(void *data1, gpointer data2)
{
	GtkTreeRowReference *row_reference;
	GtkTreePath *path;
	GtkTreeIter iter;

	row_reference = (GtkTreeRowReference *)data1;
	path = gtk_tree_row_reference_get_path(row_reference);

	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
		gtk_list_store_remove(model, &iter);

	gtk_tree_path_free(path);
	gtk_tree_row_reference_free(row_reference);
}
*/

static void list_add(void)
{
 /*   
	GtkTreeIter iter;
	const char *word = gtk_entry_get_text(GTK_ENTRY(bad_entry));
	gboolean case_sensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(case_toggle));

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
		char *tmpword = g_utf8_casefold(word, -1);

		do {
			GValue bad_val;
			gboolean match;

			bad_val.g_type = 0;
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, BAD_COLUMN, &bad_val);

			if (case_sensitive)
			{
				GValue case_sensitive_val;
				case_sensitive_val.g_type = 0;
				gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, CASE_SENSITIVE_COLUMN, &case_sensitive_val);

				// If they're both case-sensitive, then compare directly.
				// * Otherwise, they overlap. 
				if (g_value_get_boolean(&case_sensitive_val))
				{
					match = !strcmp(g_value_get_string(&bad_val), word);
				}
				else
				{
					char *bad = g_utf8_casefold(g_value_get_string(&bad_val), -1);
					match = !strcmp(bad, tmpword);
					g_free(bad);
				}
				g_value_unset(&case_sensitive_val);
			}
			else
			{
				char *bad = g_utf8_casefold(g_value_get_string(&bad_val), -1);
				match = !strcmp(bad, tmpword);
				g_free(bad);
			}

			if (match) {
				g_value_unset(&bad_val);
				g_free(tmpword);

				purple_notify_error(NULL, _("Duplicate Correction"),
					_("The specified word already exists in the correction list."),
					gtk_entry_get_text(GTK_ENTRY(bad_entry)));
				return;
			}

			g_value_unset(&bad_val);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));

		g_free(tmpword);
	}

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
		BAD_COLUMN, word,
		GOOD_COLUMN, gtk_entry_get_text(GTK_ENTRY(good_entry)),
		WORD_ONLY_COLUMN, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(complete_toggle)),
		CASE_SENSITIVE_COLUMN, case_sensitive,
		-1);

	gtk_editable_delete_text(GTK_EDITABLE(bad_entry), 0, -1);
	gtk_editable_delete_text(GTK_EDITABLE(good_entry), 0, -1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(complete_toggle), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(case_toggle), FALSE);
	gtk_widget_grab_focus(bad_entry);

	save_list();
    */
}

static gboolean check_combo_entry(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    GtkListStore *store;
    gchar *name, *name_to_remove;

    gtk_tree_model_get(model, iter, 0, &name, -1);
    store = GTK_LIST_STORE(model);
    if (g_strncasecmp(name, name_to_remove, sizeof(name_to_remove))) {
        gtk_list_store_remove(store, iter);
        purple_debug_info(TEST_PLUGIN_ID, "removed %s from combo_box\n", name_to_remove);
        return TRUE;
    }

    return FALSE;
}

static void remove_search_engine(GtkWidget *widget, gpointer selection)
{
    GtkListStore *store;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter, first_iter;
    gboolean valid_iter;
    search_engine *search_engine_to_remove;
    const gchar *name;

    GList *engines;

    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(active_list)));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(active_list));

    if (gtk_tree_model_get_iter_first(model, &iter) == FALSE) {
        return;
    }

    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), &model, &iter)) {
        gtk_tree_model_get(model, &iter, LIST_ITEM, &name, -1);
        path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &iter);
        gtk_list_store_remove(store, &iter);
        valid_iter = gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
        if (!valid_iter) {
            gtk_tree_path_prev(path);
            valid_iter = gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
        }
        if (valid_iter) {
            gtk_tree_selection_select_iter(selection, &iter);
        }
        gtk_tree_path_free(path);
    }
	/*
    gpointer orig_key, orig_value;
    if (g_hash_table_lookup_extended(search_engines, name, &orig_key, &orig_value)) {
        search_engine_to_remove =  g_hash_table_lookup(search_engines, name); 
        g_hash_table_remove(search_engines, name);
        
        //g_free(orig_key); //segfault-a-licious!
        //g_free(orig_value); // segfault-a-licious!
    }*/
    
    // deletes the entry in the hash table with the key 'key'. Deletes the XML file
    // associated with the search. Frees the memory used by the search struct:
    destroy_search_engine(name);

    // sanity check that engine was removed from hashtable
    engines = g_hash_table_get_values(search_engines);
    purple_debug_info(TEST_PLUGIN_ID, "Current engines:\n");
    while (engines) {
        search_engine *eng;
        eng = engines->data;
        purple_debug_info(TEST_PLUGIN_ID, "%s\n", eng->name);
        engines = engines->next;
    }
    g_list_free(engines);

    /*
     * FIXME: broken
     */
    //while (conversations) {
    //    MMConversation *mmconv;
    //    GtkComboBox *combo_box;
    //    GtkTreeModel *model;
    //    mmconv = conversations->data;
    //    combo_box = (GtkComboBox *)mmconv->combo_box;
    //    model = gtk_combo_box_get_model(combo_box);
    //    gtk_tree_model_foreach(model, check_combo_entry, name);
    //    conversations = conversations->next;
    //}    

    g_free(name);
}

static void list_delete(void)
{
    /*
	GtkTreeSelection *sel;
	GSList *list = NULL;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_selected_foreach(sel, add_selected_row_to_list, &list);

	g_slist_foreach(list, remove_row, NULL);
	g_slist_free(list);

	save_list();
    */
}

static void save_list()
{
    /*
	GString *data;
	GtkTreeIter iter;

	data = g_string_new("");

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter)) {
		do {
			GValue val0;
			GValue val1;
			GValue val2;
			GValue val3;

			val0.g_type = 0;
			val1.g_type = 0;
			val2.g_type = 0;
			val3.g_type = 0;

			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, BAD_COLUMN, &val0);
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, GOOD_COLUMN, &val1);
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, WORD_ONLY_COLUMN, &val2);
			gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, CASE_SENSITIVE_COLUMN, &val3);

			g_string_append_printf(data, "COMPLETE %d\nCASE %d\nBAD %s\nGOOD %s\n\n",
								   g_value_get_boolean(&val2),
								   g_value_get_boolean(&val3),
								   g_value_get_string(&val0),
								   g_value_get_string(&val1));

			g_value_unset(&val0);
			g_value_unset(&val1);
			g_value_unset(&val2);
			g_value_unset(&val3);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
	}

	purple_util_write_data_to_file("dict", data->str, -1);

	g_string_free(data, TRUE);
    */
}

#if !GTK_CHECK_VERSION(2,2,0)
static void count_selected_helper(GtkTreeModel *model, GtkTreePath *path,
					GtkTreeIter *iter, gpointer user_data)
{
	(*(gint *)user_data)++;
}
#endif

static gboolean non_empty(const char *s)
{
	while (*s && g_ascii_isspace(*s))
		s++;
	return *s;
}

static void on_entry_changed(GtkEditable *editable, gpointer data)
{
    /*
	gtk_widget_set_sensitive((GtkWidget*)data,
		non_empty(gtk_entry_get_text(GTK_ENTRY(bad_entry))) &&
		non_empty(gtk_entry_get_text(GTK_ENTRY(good_entry))));
        */
}


static int mmconv_from_conv_loc(PurpleConversation *conv)
{
	GList *l;
	MMConversation *mmconv_current = NULL;
	guint i;
	
	i = 0;
	for (l = conversations; l != NULL; l = l->next)
	{
		mmconv_current = l->data;
		if (conv == mmconv_current->conv)
		{
			return i;
		}
		i++;
	}
	return -1;
}

static MMConversation* mmconv_from_conv(PurpleConversation *conv)
{
	return (MMConversation *)g_list_nth_data(conversations, mmconv_from_conv_loc(conv));
}

static void kill_editor (MMConversation *mmconv)
{
	if (mmconv->pid)
	{
		kill(mmconv->pid, SIGINT);
		mmconv->pid = 0;
	}
}

static void conv_destroyed (PurpleConversation *conv)
{
	MMConversation *mmconv = mmconv_from_conv(conv);
	
	remove_widget(mmconv->button);
    remove_widget(mmconv->combo_box);
	remove_widget(mmconv->seperator);
	if (mmconv->started)
	{
		kill_editor(mmconv);
	}
	conversations = g_list_remove(conversations, mmconv);
}

static void remove_widget (GtkWidget *widget)
{
	gtk_widget_hide(widget);
	gtk_widget_destroy(widget);
}

static void search_button_clicked (GtkWidget *widget, gpointer data)
{
	MMConversation *mmconv = mmconv_from_conv(((MMConversation *) data)->conv);

    PidginConversation *gtkconv = PIDGIN_CONVERSATION(mmconv->conv);

    GtkTextIter start;
    GtkTextIter end;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
    gchar *text, *search_engine;

    gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    search_engine = gtk_combo_box_get_active_text(mmconv->combo_box);
    
    purple_debug_info(TEST_PLUGIN_ID, "search engine: %s\n", search_engine);
    purple_debug_info(TEST_PLUGIN_ID, "selected text: %s\n", text);
   //purple_debug_info(TEST_PLUGIN_ID, "Combobox test: %d\n", ((GtkComboBox *)mmconv->combo_box)->active);
    open_url(text, search_engine);
}

// add widgets to convo ui
static void add_widgets (MMConversation *mmconv)
{
	PurpleConversation *conv = mmconv->conv;
    GList *engines;
    guint count = 0;
	
	GtkWidget *sep, *button, *button_image, *combo_box;
    GtkListStore *store;
    GtkTreeIter iter;
    GtkCellRendererText *cell;
	gchar *button_image_file_path;

	sep = gtk_vseparator_new();

    // setup button
	button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(search_button_clicked), mmconv);
	button_image_file_path = g_build_filename(DATADIR, "pixmaps", "purple", "buttons",
										"search.png", NULL);
	button_image = gtk_image_new_from_file(button_image_file_path);
	g_free(button_image_file_path);
	gtk_container_add((GtkContainer *)button, button_image);

    // setup combo box
    purple_debug_info(TEST_PLUGIN_ID, "setting up combo_box\n");
    store = gtk_list_store_new(1, G_TYPE_STRING);

    engines = g_hash_table_get_values(search_engines);
    while (engines) {
        search_engine *eng;
        eng = engines->data;
        gtk_list_store_append(store, &iter);
        //TODO display the shortname in the pulldown but get the value of the filename when
        // the shortname is selected
        gtk_list_store_set(store, &iter, 0, eng->filename, 1, eng->name, -1);
        engines = engines->next;
        count += 1;
    }
    g_list_free(engines);

    combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);

    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo_box), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), cell, "text", 0, NULL);

    if (count > 0) {
        gtk_combo_box_set_active(combo_box, 0);
    }
	
	mmconv->seperator = sep;
    mmconv->combo_box = combo_box;
	mmconv->button = button;
	
	gtk_widget_show(sep);
    gtk_widget_show(combo_box);
	gtk_widget_show(button_image);
	gtk_widget_show(button);
	
	gtk_box_pack_start(GTK_BOX(PIDGIN_CONVERSATION(conv)->toolbar), sep, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(PIDGIN_CONVERSATION(conv)->toolbar), combo_box, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(PIDGIN_CONVERSATION(conv)->toolbar), button, FALSE, FALSE, 0);
}

static void init_conversation (PurpleConversation *conv)
{
	MMConversation *mmconv;
	mmconv = g_malloc(sizeof(MMConversation));
	
	mmconv->conv = conv;
	mmconv->started = FALSE;
	mmconv->originator = FALSE;
	mmconv->requested = FALSE;
	
	add_widgets(mmconv);
	
	conversations = g_list_append(conversations, mmconv);
}

static gboolean plugin_load(PurplePlugin *plugin)
{
	void *conv_list_handle;

    // xml parsing test
    //test_xml();
	
    // load search engines
    load_search_engines();

    test_plugin = plugin;

    purple_conversation_foreach(init_conversation);

	/* Listen for any new conversations */
	conv_list_handle = purple_conversations_get_handle();

	purple_signal_connect(conv_list_handle, "conversation-created", 
					plugin, PURPLE_CALLBACK(init_conversation), NULL);
	
	/* Listen for conversations that are ending */
	purple_signal_connect(conv_list_handle, "deleting-conversation",
					plugin, PURPLE_CALLBACK(conv_destroyed), NULL);

	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin)
{
	/*GList *convs;

	// Detach from existing conversations 
	for (convs = purple_get_conversations(); convs != NULL; convs = convs->next)
	{
		PidginConversation *gtkconv = PIDGIN_CONVERSATION((PurpleConversation *)convs->data);
		spellchk *spell = g_object_get_data(G_OBJECT(gtkconv->entry), TEST_OBJECT_KEY);

		g_signal_handlers_disconnect_by_func(gtkconv->entry, message_send_cb, spell);
		g_object_set_data(G_OBJECT(gtkconv->entry), TEST_OBJECT_KEY, NULL);
	}
	*/
	/*
    GHashTable *test;
	search_engine *site;
    const gchar *key;

    test = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	site = g_malloc(sizeof(search_engine));
	site->name = "Answers";
	site->query_url = "test_url";
	site->icon_url = "no_image";
    key = "Answers";

    g_hash_table_insert(test, g_strdup(key), site);

	site = g_malloc(sizeof(search_engine));
	site->name = "Google";
	site->query_url = "test_url";
	site->icon_url = "no_image";
    key = "Google";

    g_hash_table_insert(test, g_strdup(key), site);

    gpointer orig_key, orig_value;
    if (g_hash_table_lookup_extended(test, key, &orig_key, &orig_value)) {
        g_hash_table_remove(test, key);
        //g_free(orig_key);
        //g_free(orig_value);
    }

    //search_engine *se = g_hash_table_lookup(test, key);
    //purple_debug_info(TEST_PLUGIN_ID, "before remove: %s\n", se->name);
    //g_hash_table_remove(test, key);
    search_engine *se2 = g_hash_table_lookup(test, key);
    if (se2 == NULL) {
        purple_debug_info(TEST_PLUGIN_ID, "remove successful \n");
    }
    g_hash_table_destroy(test);

    // g_hash_table_destroy(search_engines);
*/
	return TRUE;
}

/*
static void
whole_words_button_toggled(GtkToggleButton *complete_toggle, GtkToggleButton *case_toggle)
{
	gboolean enabled = gtk_toggle_button_get_active(complete_toggle);

	gtk_toggle_button_set_active(case_toggle, !enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(case_toggle), enabled);
}
*/

static void load_config_active_list() {
    GtkListStore *store;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreeIter iter, first_iter;
    GList *engines;

    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(active_list)));

    engines = g_hash_table_get_values(search_engines);
    while (engines) {
        search_engine *eng;
        eng = engines->data;
        gtk_list_store_append(store, &iter);
       // gtk_list_store_set(store, &iter, LIST_ITEM, eng->name, -1);
        // TODO again we want to display the shortname and use the filename as the data
        // like in an html select tag <option value="$filename">$name</option>
        gtk_list_store_set(store, &iter, LIST_ITEM, eng->filename, 1, eng->name, -1);
        engines = engines->next;
    }

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(active_list));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(active_list));
    if (gtk_tree_model_get_iter_first(model, &first_iter) == TRUE) {
            gtk_tree_selection_select_iter(GTK_TREE_SELECTION(selection), &first_iter);
    }

    g_list_free(engines);
}

static GtkWidget * get_config_frame(PurplePlugin *plugin)
{
	GtkWidget *ret, *vbox, *win;
	GtkWidget *hbox;

    GtkTreeSelection *tree;

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkListStore *store;

	GtkWidget *add;
    GtkWidget *delete;

	GtkSizeGroup *sg;
	GtkSizeGroup *sg2;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame(ret, _("Search engines"));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
	gtk_widget_show(vbox);

	win = gtk_scrolled_window_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), win, TRUE, TRUE, 0);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(win),
										GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(win),
			GTK_POLICY_NEVER,
			GTK_POLICY_ALWAYS);
    gtk_widget_set_size_request(win, 200, 150);
	gtk_widget_show(win);

    active_list = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(active_list), FALSE);
    gtk_container_add(GTK_CONTAINER(win), active_list);
	gtk_widget_show(active_list);

    // initialize model/store stuff associated with tree view
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("List Item",
                    renderer, "text", LIST_ITEM, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(active_list), column);
    store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(active_list), GTK_TREE_MODEL(store));
    g_object_unref(store);

    // load the list in the config with current active search engines
    load_config_active_list();

    tree = gtk_tree_view_get_selection(GTK_TREE_VIEW(active_list));
	//gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(active_list)),
    //		 GTK_SELECTION_MULTIPLE);
	//gtk_container_add(GTK_CONTAINER(win), tree);

	hbox = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

    // add button
    add = gtk_button_new_from_stock(GTK_STOCK_ADD);
    g_signal_connect(G_OBJECT(add), "clicked",
               G_CALLBACK(list_add), NULL); // TODO: make callback function
    gtk_box_pack_start(GTK_BOX(hbox), add, FALSE, FALSE, 0);
    //gtk_widget_set_sensitive(add, FALSE);

    gtk_widget_show(add);

    // delete button
	delete = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	g_signal_connect(G_OBJECT(delete), "clicked",
			   G_CALLBACK(remove_search_engine), tree); // list_delete
	gtk_box_pack_start(GTK_BOX(hbox), delete, FALSE, FALSE, 0);
	//gtk_widget_set_sensitive(delete, FALSE);

	gtk_widget_show(delete);

	gtk_widget_show_all(ret);
	g_object_unref(sg);
	g_object_unref(sg2);

	return ret;
}

static PidginPluginUiInfo ui_info =
{
	get_config_frame,
	0, /* page_num (Reserved) */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

/* For specific notes on the meanings of each of these members, consult the C Plugin Howto
 * on the website. */
static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
    TEST_PLUGIN_ID,
	N_("A Test"),
	DISPLAY_VERSION, /* This constant is defined in config.h, but you shouldn't use it for
		                your own plugins. We use it here because it's our plugin and we too are lazy. */
	N_("This is a test plugin."),
	N_("This is a test plugin."),
	"Joeff Gallotkin",
    PURPLE_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	&ui_info,
	NULL,
	NULL,
    NULL,

	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
#if 0
	purple_prefs_add_none("/plugins");
	purple_prefs_add_none("/plugins/gtk");
	purple_prefs_add_none("/plugins/gtk/spellchk");
	purple_prefs_add_bool("/plugins/gtk/spellchk/last_word_replace", TRUE);
#endif
}

PURPLE_INIT_PLUGIN (test, init_plugin, info)
