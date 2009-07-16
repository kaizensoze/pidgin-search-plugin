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

#define TEST_PLUGIN_ID "gtk-test"
#define TEST_OBJECT_KEY "test"

enum {
	BAD_COLUMN,
	GOOD_COLUMN,
	WORD_ONLY_COLUMN,
	CASE_SENSITIVE_COLUMN,
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

typedef struct {
    gchar *name;
    gchar *query_url;
    gchar *icon_url;
} search_engine;


static void add_widgets (MMConversation *mmconv);
static void remove_widget (GtkWidget *button);
static void init_conversation (PurpleConversation *conv);
static void conv_destroyed (PurpleConversation *conv);

/* List of sessions */
static GList *conversations;

/* Map of search engines */
static GHashTable *search_engines;


PurplePlugin *test_plugin = NULL;


static void open_url(const gchar *search_term, const gchar *search_engine)
{
    gchar *url = g_strconcat("http://www.google.com/#hl=en&q=", search_term, NULL);
    purple_debug_info(TEST_PLUGIN_ID, "search_term: %s\n", search_term);
    purple_notify_uri(NULL, url);
}

static GtkListStore *model;


static int buf_get_line(char *ibuf, char **buf, int *position, gsize len)
{
	int pos = *position;
	int spos = pos;

	if (pos == len)
		return 0;

	while (!(ibuf[pos] == '\n' ||
	         (ibuf[pos] == '\r' && ibuf[pos + 1] != '\n')))
	{
		pos++;
		if (pos == len)
			return 0;
	}

	if (pos != 0 && ibuf[pos] == '\n' && ibuf[pos - 1] == '\r')
		ibuf[pos - 1] = '\0';

	ibuf[pos] = '\0';
	*buf = &ibuf[spos];

	pos++;
	*position = pos;

	return 1;
}


static void load_conf(void)
{
	const char * const defaultconf =
			"BAD abbout\nGOOD about\n"
            "BAD wand\nGOOD wang\n";
	gchar *buf;
	gchar *ibuf;
	GHashTable *hashes;
	char bad[82] = "";
	char good[256] = "";
	int pnt = 0;
	gsize size;
	gboolean complete = TRUE;
	gboolean case_sensitive = FALSE;

	buf = g_build_filename(purple_user_dir(), "dict", NULL);
	g_file_get_contents(buf, &ibuf, &size, NULL);
	g_free(buf);
	if (!ibuf) {
		ibuf = g_strdup(defaultconf);
		size = strlen(defaultconf);
	}

	model = gtk_list_store_new((gint)N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	hashes = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	while (buf_get_line(ibuf, &buf, &pnt, size)) {
		if (*buf != '#') {
			if (!g_ascii_strncasecmp(buf, "BAD ", 4))
			{
				strncpy(bad, buf + 4, 81);
			}
			else if(!g_ascii_strncasecmp(buf, "CASE ", 5))
			{
				case_sensitive = *(buf+5) == '0' ? FALSE : TRUE;
			}
			else if(!g_ascii_strncasecmp(buf, "COMPLETE ", 9))
			{
				complete = *(buf+9) == '0' ? FALSE : TRUE;
			}
			else if (!g_ascii_strncasecmp(buf, "GOOD ", 5))
			{
				strncpy(good, buf + 5, 255);

				if (*bad && *good && g_hash_table_lookup(hashes, bad) == NULL) {
					GtkTreeIter iter;

					// We don't actually need to store the good string, since this
					 // hash is just being used to eliminate duplicate bad strings.
					// * The value has to be non-NULL so the lookup above will work.
					//
					g_hash_table_insert(hashes, g_strdup(bad), GINT_TO_POINTER(1));

					if (!complete)
						case_sensitive = TRUE;

					gtk_list_store_append(model, &iter);
					gtk_list_store_set(model, &iter,
						BAD_COLUMN, bad,
						GOOD_COLUMN, good,
						WORD_ONLY_COLUMN, complete,
						CASE_SENSITIVE_COLUMN, case_sensitive,
						-1);
				}
				bad[0] = '\0';
				complete = TRUE;
				case_sensitive = FALSE;
			}
		}
	}
	g_free(ibuf);
	g_hash_table_destroy(hashes);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
	                                     0, GTK_SORT_ASCENDING);
}

static GtkWidget *tree;
static GtkWidget *bad_entry;
static GtkWidget *good_entry;
static GtkWidget *complete_toggle;
static GtkWidget *case_toggle;
static void save_list(void);

static void on_edited(GtkCellRendererText *cellrenderertext,
					  gchar *path, gchar *arg2, gpointer data)
{
	GtkTreeIter iter;
	GValue val;

	if (arg2[0] == '\0') {
		gdk_beep();
		return;
	}

	g_return_if_fail(gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &iter, path));
	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, GPOINTER_TO_INT(data), &val);

	if (strcmp(arg2, g_value_get_string(&val))) {
		gtk_list_store_set(model, &iter, GPOINTER_TO_INT(data), arg2, -1);
		save_list();
	}
	g_value_unset(&val);
}

static void word_only_toggled(GtkCellRendererToggle *cellrenderertoggle,
						gchar *path, gpointer data){
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
}

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

static void list_add_new(void)
{
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

static void list_delete(void)
{
	GtkTreeSelection *sel;
	GSList *list = NULL;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_selected_foreach(sel, add_selected_row_to_list, &list);

	g_slist_foreach(list, remove_row, NULL);
	g_slist_free(list);

	save_list();
}

static void save_list()
{
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
}

#if !GTK_CHECK_VERSION(2,2,0)
static void count_selected_helper(GtkTreeModel *model, GtkTreePath *path,
					GtkTreeIter *iter, gpointer user_data)
{
	(*(gint *)user_data)++;
}
#endif

static void on_selection_changed(GtkTreeSelection *sel, gpointer data)
{
	gint num_selected;
#if GTK_CHECK_VERSION(2,2,0)
	num_selected = gtk_tree_selection_count_selected_rows(sel);
#else
	gtk_tree_selection_selected_foreach(sel, count_selected_helper, &num_selected);
#endif
	gtk_widget_set_sensitive((GtkWidget*)data, (num_selected > 0));
}

static gboolean non_empty(const char *s)
{
	while (*s && g_ascii_isspace(*s))
		s++;
	return *s;
}

static void on_entry_changed(GtkEditable *editable, gpointer data)
{
	gtk_widget_set_sensitive((GtkWidget*)data,
		non_empty(gtk_entry_get_text(GTK_ENTRY(bad_entry))) &&
		non_empty(gtk_entry_get_text(GTK_ENTRY(good_entry))));
}

static void test_xml()
{
	int i = 0, ii = 0;
	xmlnode *node = NULL, *temp = NULL;

	
	gchar *search_provider_path = "google_search-opensearch.xml";
	node = load_search_provider(search_provider_path);	
	if(node == NULL) {
		purple_debug_info(TEST_PLUGIN_ID, "the file %s could not be loaded", search_provider_path);
	} else {
		temp = node;
		
		while(node != NULL ){
			purple_debug_info(TEST_PLUGIN_ID, "i : %d: %s :%s (%d)\n", i++,
				 node->name, xmlnode_get_attrib(node,"type" ),  node->type );
			//purple_debug_info(TEST_PLUGIN_ID, "%s\n", g_hash_table_lookup(node->namespace_map, "type"));
			node = node->next;
		}
		node = temp->child;
		i = 0;
		while(node != NULL){					
			purple_debug_info(TEST_PLUGIN_ID, "i : %d: %s :%s (%d)\n", i++, 
				node->name, xmlnode_get_attrib(node,"type" ), node->type );		
			if(node->name != NULL && strcmp(node->name, "Url") == 0  ){
				temp = node->child;
				while(temp != NULL){
					purple_debug_info(TEST_PLUGIN_ID, "ii : %d: %s :%s (%d)\n", ii++, 
						temp->name, xmlnode_get_attrib(temp,"type" ), temp->type );
					temp = temp->next;
				}
				ii = 0;
			} 
				
			node = node->next;			
		}
	}
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

    open_url(text, search_engine);
}

// add widgets to convo ui
static void add_widgets (MMConversation *mmconv)
{
	PurpleConversation *conv = mmconv->conv;
	
	GtkWidget *sep, *button, *button_image, *combo_box;
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
    combo_box = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX (combo_box), "Google");
    gtk_combo_box_append_text(GTK_COMBO_BOX (combo_box), "Yahoo");
    gtk_combo_box_append_text(GTK_COMBO_BOX (combo_box), "Answers");
    gtk_combo_box_append_text (GTK_COMBO_BOX (combo_box), "Wikipedia");
    gtk_combo_box_set_active(combo_box, 0);
	
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

    // xml test
    test_xml();
	
    // TODO: parse search engine xml files
    search_engines = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL); // NOTE: may need custom function to destroy struct value

    test_plugin = plugin;

    purple_conversation_foreach(init_conversation);

	load_conf();

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
	return TRUE;
}

static void
whole_words_button_toggled(GtkToggleButton *complete_toggle, GtkToggleButton *case_toggle)
{
	gboolean enabled = gtk_toggle_button_get_active(complete_toggle);

	gtk_toggle_button_set_active(case_toggle, !enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(case_toggle), enabled);
}

static GtkWidget * get_config_frame(PurplePlugin *plugin)
{
	GtkWidget *ret, *vbox, *win;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkSizeGroup *sg;
	GtkSizeGroup *sg2;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkWidget *vbox2;
	GtkWidget *vbox3;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame(ret, _("Text Replacements"));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
	gtk_widget_show(vbox);

	win = gtk_scrolled_window_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), win, TRUE, TRUE, 0);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(win),
										GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(win),
			GTK_POLICY_NEVER,
			GTK_POLICY_ALWAYS);
	gtk_widget_show(win);

	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_widget_set_size_request(tree, -1, 200);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer),
		"editable", TRUE,
		NULL);
	g_signal_connect(G_OBJECT(renderer), "edited",
		G_CALLBACK(on_edited), GINT_TO_POINTER(0));
	column = gtk_tree_view_column_new_with_attributes(_("You type"), renderer,
													  "text", BAD_COLUMN,
													  NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 150);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer),
		"editable", TRUE,
		NULL);
	g_signal_connect(G_OBJECT(renderer), "edited",
		G_CALLBACK(on_edited), GINT_TO_POINTER(1));
	column = gtk_tree_view_column_new_with_attributes(_("You send"), renderer,
													  "text", GOOD_COLUMN,
													  NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 150);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_toggle_new();
	g_object_set(G_OBJECT(renderer),
		"activatable", TRUE,
		NULL);
	g_signal_connect(G_OBJECT(renderer), "toggled",
		G_CALLBACK(word_only_toggled), NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Whole words only"), renderer,
													  "active", WORD_ONLY_COLUMN,
													  NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_toggle_new();
	g_object_set(G_OBJECT(renderer),
		"activatable", TRUE,
		NULL);
	g_signal_connect(G_OBJECT(renderer), "toggled",
		G_CALLBACK(case_sensitive_toggled), NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Case sensitive"), renderer,
													  "active", CASE_SENSITIVE_COLUMN,
													  NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)),
		 GTK_SELECTION_MULTIPLE);
	gtk_container_add(GTK_CONTAINER(win), tree);
	gtk_widget_show(tree);

	hbox = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	g_signal_connect(G_OBJECT(button), "clicked",
			   G_CALLBACK(list_delete), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);

	g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(tree))),
		"changed", G_CALLBACK(on_selection_changed), button);

	gtk_widget_show(button);

	vbox = pidgin_make_frame(ret, _("Add a new text replacement"));

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);
	vbox2 = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show(vbox2);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	sg2 = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	bad_entry = gtk_entry_new();
	// Set a minimum size. Since they're in a size group, the other entry will match up. 
	gtk_widget_set_size_request(bad_entry, 350, -1);
	gtk_size_group_add_widget(sg2, bad_entry);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox2), _("You _type:"), sg, bad_entry, FALSE, NULL);

	good_entry = gtk_entry_new();
	gtk_size_group_add_widget(sg2, good_entry);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox2), _("You _send:"), sg, good_entry, FALSE, NULL);

	// Created here so it can be passed to whole_words_button_toggled. 
	case_toggle = gtk_check_button_new_with_mnemonic(_("_Exact case match (uncheck for automatic case handling)"));

	complete_toggle = gtk_check_button_new_with_mnemonic(_("Only replace _whole words"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(complete_toggle), TRUE);
	g_signal_connect(G_OBJECT(complete_toggle), "clicked",
                         G_CALLBACK(whole_words_button_toggled), case_toggle);
	gtk_widget_show(complete_toggle);
	gtk_box_pack_start(GTK_BOX(vbox2), complete_toggle, FALSE, FALSE, 0);

	// The button is created above so it can be passed to whole_words_button_toggled. 
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(case_toggle), FALSE);
	gtk_widget_show(case_toggle);
	gtk_box_pack_start(GTK_BOX(vbox2), case_toggle, FALSE, FALSE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(button), "clicked",
			   G_CALLBACK(list_add_new), NULL);
	vbox3 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox3, TRUE, FALSE, 0);
	gtk_widget_show(vbox3);
	gtk_box_pack_end(GTK_BOX(vbox3), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(bad_entry), "changed", G_CALLBACK(on_entry_changed), button);
	g_signal_connect(G_OBJECT(good_entry), "changed", G_CALLBACK(on_entry_changed), button);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

#if 0
	vbox = pidgin_make_frame(ret, _("General Text Replacement Options"));
	pidgin_prefs_checkbox(_("Enable replacement of last word on send"),
	                        "/plugins/gtk/spellchk/last_word_replace", vbox);
#endif

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
	N_("Test"),
	DISPLAY_VERSION, /* This constant is defined in config.h, but you shouldn't use it for
		                your own plugins.  We use it here because it's our plugin. And we're lazy. */
	N_("This is a test plugin."),
	N_("This is a test plugin."),
	"Joe Gallo",
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
