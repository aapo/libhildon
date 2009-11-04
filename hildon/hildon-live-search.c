/*
 * This file is a part of hildon
 *
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Based in OssoABookLiveSearch, OSSO Address Book.
 * Author: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 * Hildon version: Claudio Saavedra <csaavedra@igalia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser Public License as published by
 * the Free Software Foundation; version 2 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser Public License for more details.
 *
 */

/**
 * SECTION:hildon-live-search
 * @short_description: A widget for manipulating contact filters.
 *
 * This widget provides a user interface for manipulating
 * #GtkTreeModelFilter instances.
 */

#include "config.h"
#include "hildon-live-search.h"

#include <hildon/hildon.h>
#include <string.h>

G_DEFINE_TYPE (HildonLiveSearch, hildon_live_search,
               GTK_TYPE_TOOLBAR);

#define GET_PRIVATE(o) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((o), HILDON_TYPE_LIVE_SEARCH, \
                                      HildonLiveSearchPrivate))

typedef struct _HildonLiveSearchPrivate HildonLiveSearchPrivate;

struct _HildonLiveSearchPrivate
{
        GtkTreeModelFilter *filter;

        GtkTreeView *treeview;

        GtkWidget *entry;
        GtkWidget *event_widget;
        GHashTable *selection_map;

        gulong key_press_id;
        gulong destroy_id;

        gchar *prefix;
        gint text_column;

        HildonLiveSearchFilterFunc visible_func;
        gpointer visible_data;
        GDestroyNotify visible_destroy;
};

enum
{
  PROP_0,

  PROP_FILTER,
  PROP_WIDGET,
  PROP_TEXT_COLUMN
};

static gboolean
visible_func (GtkTreeModel *model,
              GtkTreeIter *iter,
              gpointer data);

/* Private implementation */

static guint
hash_func (gconstpointer key)
{
        GtkTreePath *path;
        gchar *path_str;
        guint val;

        path = (GtkTreePath *) key;
        path_str = gtk_tree_path_to_string (path);
        val = g_str_hash (path_str);
        g_free (path_str);

        return val;
}

static gboolean
key_equal_func (gconstpointer v1,
                gconstpointer v2)
{
        return gtk_tree_path_compare ((GtkTreePath *)v1,
                                      (GtkTreePath *)v2) == 0;
}

/**
 * selection_map_create:
 * @priv: The private pimpl
 *
 * Adds a selection map which is useful when merging selected rows in
 * the treeview, when the live search widget is used.
 **/
static void
selection_map_create (HildonLiveSearchPrivate *priv)
{
        gboolean working;
        GtkTreeModel *base_model;
        GtkTreeIter iter;
        GtkTreePath *path;

        g_assert (priv->selection_map == NULL);

        base_model = gtk_tree_model_filter_get_model (priv->filter);

        priv->selection_map = g_hash_table_new_full
                (hash_func, key_equal_func, (GDestroyNotify) gtk_tree_path_free, NULL);

        for (working = gtk_tree_model_get_iter_first (base_model, &iter);
             working;
             working = gtk_tree_model_iter_next (base_model, &iter)) {
                path = gtk_tree_model_get_path (base_model, &iter);
                g_hash_table_insert (priv->selection_map,
                                     path, GINT_TO_POINTER (FALSE));
        }
}

/**
 * selection_map_destroy:
 * @priv: The private pimpl
 *
 * Destroys resources associated with the selection map.
 **/
static void
selection_map_destroy (HildonLiveSearchPrivate *priv)
{
        if (priv->selection_map != NULL) {
                g_hash_table_destroy (priv->selection_map);
                priv->selection_map = NULL;
        }
}

/**
 * selection_map_update_map_from_selection:
 * @priv: The private pimpl
 *
 * Find out which rows are visible in filter, and mark them as selected
 * or unselected from treeview to selection map.
 **/
static void
selection_map_update_map_from_selection (HildonLiveSearchPrivate *priv)
{
        gboolean working;
        GtkTreeModel *base_model;
        GtkTreeSelection *selection;
        GtkTreeIter iter;

        base_model = gtk_tree_model_filter_get_model (priv->filter);
        selection = gtk_tree_view_get_selection (priv->treeview);

        for (working = gtk_tree_model_get_iter_first (base_model, &iter);
             working;
             working = gtk_tree_model_iter_next (base_model, &iter)) {
                if (visible_func (base_model, &iter, priv)) {
                                GtkTreePath *path, *filter_path;

                                path = gtk_tree_model_get_path (base_model, &iter);
                                filter_path = gtk_tree_model_filter_convert_child_path_to_path
                                        (priv->filter, path);

                                if (gtk_tree_selection_path_is_selected
                                    (selection, filter_path)) {
                                        g_hash_table_replace
                                                (priv->selection_map,
                                                 path,
                                                 GINT_TO_POINTER (TRUE));
                                } else {
                                        g_hash_table_replace
                                                (priv->selection_map,
                                                 path,
                                                 GINT_TO_POINTER (FALSE));
                                }
                                gtk_tree_path_free (filter_path);
                        }
        }
}

/**
 * selection_map_update_selection_from_map:
 * @priv: The private pimpl
 *
 * For currently visible rows in filter, set selection from selection
 * map to treeview.
 **/
static void
selection_map_update_selection_from_map (HildonLiveSearchPrivate *priv)
{
        gboolean working;
        GtkTreeModel *base_model;
        GtkTreeSelection *selection;
        GtkTreeIter iter;

        base_model = gtk_tree_model_filter_get_model (priv->filter);
        selection = gtk_tree_view_get_selection (priv->treeview);

        for (working = gtk_tree_model_get_iter_first (base_model, &iter);
             working;
             working = gtk_tree_model_iter_next (base_model, &iter)) {
                if (visible_func (base_model, &iter, priv)) {
                                GtkTreePath *path;
                                GtkTreePath *filter_path;
                                gboolean selected;

                                path = gtk_tree_model_get_path (base_model,
                                                                &iter);
                                selected = GPOINTER_TO_INT
                                        (g_hash_table_lookup
                                         (priv->selection_map, path));

                                filter_path = gtk_tree_model_filter_convert_child_path_to_path
                                        (priv->filter, path);

                                if (selected) {
                                        gtk_tree_selection_select_path
                                                (selection, filter_path);
                                } else {
                                        gtk_tree_selection_unselect_path
                                                (selection, filter_path);
                                }
                                gtk_tree_path_free (filter_path);
                        }
        }
}

static void
on_entry_changed (GtkEntry *entry,
                  gpointer user_data)
{
        HildonLiveSearchPrivate *priv;
        const char *text;
        glong len;
        char *old_prefix;

        g_return_if_fail (HILDON_IS_LIVE_SEARCH (user_data));
        priv = GET_PRIVATE (user_data);

        text = gtk_entry_get_text (GTK_ENTRY (entry));
        len = g_utf8_strlen (text, -1);

        old_prefix = priv->prefix;

        if (len < 1) {
                text = NULL;
        } else {
                if ((old_prefix == NULL) || (strlen (old_prefix) == 0)) {
                        selection_map_create (priv);
                }
        }

        selection_map_update_map_from_selection (priv);
        priv->prefix = g_strdup (text);
        gtk_tree_model_filter_refilter (priv->filter);
        selection_map_update_selection_from_map (priv);

        if (len < 1) {
                selection_map_destroy (priv);
		gtk_widget_hide (GTK_WIDGET (user_data));
        } else {
                gtk_widget_show (GTK_WIDGET (user_data));
        }

        g_free (old_prefix);
}

/**
 * hildon_live_search_append_text:
 * @livesearch: An #HildonLiveSearch widget
 * @utf8: The text to append. This text is copied internally, and @utf8 can be freed later by the caller.
 *
 * Appends a string to the entry text in the live search widget.
 **/
void
hildon_live_search_append_text (HildonLiveSearch *livesearch,
                                const char *utf8)
{
        HildonLiveSearchPrivate *priv;
        GtkEditable *editable;
        int pos, start, end;

        g_return_if_fail (HILDON_IS_LIVE_SEARCH (livesearch));
        g_return_if_fail (NULL != utf8);

        priv = GET_PRIVATE (livesearch);

        editable = GTK_EDITABLE (priv->entry);

        if (gtk_editable_get_selection_bounds (editable,
                                               &start,
                                               &end)) {
                gtk_editable_delete_text (editable, start, end);
        }
        pos = gtk_editable_get_position (editable);
        gtk_editable_insert_text (editable, utf8, strlen (utf8), &pos);
        gtk_editable_set_position (editable, pos + 1);
}

/**
 * hildon_live_search_get_text:
 * @livesearch: An #HildonLiveSearch widget
 *
 * Retrieves the text contents of the #HildonLiveSearch widget.
 *
 * Returns: a pointer to the text contents of the widget as a
 * string. This string points to an internal widget buffer and must not
 * be freed, modified or stored.
 **/
const char *
hildon_live_search_get_text (HildonLiveSearch *livesearch)
{
        HildonLiveSearchPrivate *priv;

        g_return_val_if_fail (HILDON_IS_LIVE_SEARCH (livesearch), NULL);

        priv = GET_PRIVATE (livesearch);

        return gtk_entry_get_text (GTK_ENTRY (priv->entry));
}

/*
 * Key press handler. This takes key presses from the source widget and
 * manipulate the entry. This manipulation then calls #on_entry_changed.
 */
static gboolean
on_key_press_event (GtkWidget *widget,
                    GdkEventKey *event,
                    HildonLiveSearch *live_search)
{
        HildonLiveSearchPrivate *priv;

        g_return_val_if_fail (HILDON_IS_LIVE_SEARCH (live_search), FALSE);
        priv = GET_PRIVATE (live_search);

        /* If the entry is realized and has focus, it is enough to catch events.
         * This assume that the toolbar is a child of the hook widget. */
        gtk_widget_realize (priv->entry);
        gtk_widget_grab_focus (priv->entry);

        return FALSE;
}

static void
on_hide_cb (GtkWidget *widget,
            HildonLiveSearch *live_search)
{
        HildonLiveSearchPrivate *priv;

        priv = GET_PRIVATE (live_search);

        if (priv->prefix) {
                g_free (priv->prefix);
                priv->prefix = NULL;
        }

        /* Remove text from the entry without calling on_entry_changed */
        g_signal_handlers_block_by_func (priv->entry, on_entry_changed, live_search);
        gtk_entry_set_text (GTK_ENTRY (priv->entry), "");
        g_signal_handlers_unblock_by_func (priv->entry, on_entry_changed, live_search);
}

/* GObject methods */

static void
hildon_live_search_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
        HildonLiveSearch *livesearch = HILDON_LIVE_SEARCH (object);
        HildonLiveSearchPrivate *priv = GET_PRIVATE (livesearch);

        switch (property_id) {
        case PROP_FILTER:
                g_value_set_object (value, priv->filter);
                break;
        case PROP_TEXT_COLUMN:
                g_value_set_int (value, priv->text_column);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
hildon_live_search_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        HildonLiveSearch *livesearch = HILDON_LIVE_SEARCH (object);

        switch (property_id) {
        case PROP_FILTER:
                hildon_live_search_set_filter (livesearch,
                                               g_value_get_object (value));
                break;
        case PROP_TEXT_COLUMN:
                hildon_live_search_set_text_column (livesearch,
                                                    g_value_get_int (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
hildon_live_search_dispose (GObject *object)
{
        HildonLiveSearchPrivate *priv = GET_PRIVATE (object);

        hildon_live_search_widget_unhook (HILDON_LIVE_SEARCH (object));

        if (priv->filter) {
                selection_map_destroy (priv);
                g_object_unref (priv->filter);
                priv->filter = NULL;
        }

        if (priv->prefix) {
                g_free (priv->prefix);
                priv->prefix = NULL;
        }

        if (priv->visible_destroy) {
                priv->visible_destroy (priv->visible_data);
                priv->visible_destroy = NULL;
        }

        G_OBJECT_CLASS (hildon_live_search_parent_class)->dispose (object);
}

static void
hildon_live_search_class_init (HildonLiveSearchClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        g_type_class_add_private (klass, sizeof (HildonLiveSearchPrivate));

        object_class->get_property = hildon_live_search_get_property;
        object_class->set_property = hildon_live_search_set_property;
        object_class->dispose = hildon_live_search_dispose;

        g_object_class_install_property (object_class,
                                         PROP_FILTER,
                                         g_param_spec_object ("filter",
                                                              "Filter",
                                                              "Model filter",
                                                              GTK_TYPE_TREE_MODEL_FILTER,
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_NICK |
                                                              G_PARAM_STATIC_NAME |
                                                              G_PARAM_STATIC_BLURB));

        g_object_class_install_property (object_class,
                                         PROP_TEXT_COLUMN,
                                         g_param_spec_int ("text-column",
                                                           "Text column",
                                                           "Column to use to filter "
                                                           "elements from the #GtkTreeModelFilter",
                                                           -1, G_MAXINT, -1,
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_STATIC_STRINGS));
}

static void
close_button_clicked_cb (GtkWidget *button,
                         gpointer user_data)
{
        gtk_widget_hide (GTK_WIDGET (user_data));
}

static HildonGtkInputMode
filter_input_mode (HildonGtkInputMode imode)
{
        return imode & ~(HILDON_GTK_INPUT_MODE_AUTOCAP |
                         HILDON_GTK_INPUT_MODE_DICTIONARY);
}

static void
hildon_live_search_init (HildonLiveSearch *self)
{
        HildonLiveSearchPrivate *priv;
        GtkWidget *close_button_alignment;
        GtkToolItem *close_button_container;
        GtkWidget *close;
        GtkToolItem *close_button;
        GtkToolItem *entry_container;
        GtkWidget *entry_hbox;
        HildonGtkInputMode imode;

        priv = GET_PRIVATE (self);

        gtk_toolbar_set_style (GTK_TOOLBAR (self), GTK_TOOLBAR_ICONS);
        gtk_container_set_border_width (GTK_CONTAINER (self), 0);

        priv->treeview = NULL;
        priv->prefix = NULL;

        priv->visible_func = NULL;
        priv->visible_data = NULL;
        priv->visible_destroy = NULL;

        entry_container = gtk_tool_item_new ();
        gtk_tool_item_set_expand (entry_container, TRUE);

        entry_hbox = gtk_hbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (entry_container), entry_hbox);

        priv->entry = hildon_entry_new (HILDON_SIZE_FINGER_HEIGHT);

        /* Unset the autocap and dictionary input flags from the
           HildonEntry. */
        imode = hildon_gtk_entry_get_input_mode (GTK_ENTRY (priv->entry));
        hildon_gtk_entry_set_input_mode (GTK_ENTRY (priv->entry),
                                         filter_input_mode (imode));

        gtk_widget_set_name (GTK_WIDGET (priv->entry),
                             "HildonLiveSearchEntry");

        gtk_box_pack_start (GTK_BOX (entry_hbox), priv->entry, TRUE, TRUE,
                            HILDON_MARGIN_DEFAULT);

        gtk_toolbar_insert (GTK_TOOLBAR (self), entry_container, 0);

        close = gtk_image_new_from_icon_name ("general_close",
                                              HILDON_ICON_SIZE_FINGER);
        gtk_misc_set_padding (GTK_MISC (close), 0, 0);
        close_button = gtk_tool_button_new (close, NULL);
        GTK_WIDGET_UNSET_FLAGS (close_button, GTK_CAN_FOCUS);

        close_button_alignment = gtk_alignment_new (0.0f, 0.0f, 1.0f, 1.0f);
        gtk_alignment_set_padding (GTK_ALIGNMENT (close_button_alignment),
                                   0, 0,
                                   0, HILDON_MARGIN_DEFAULT);
        gtk_container_add (GTK_CONTAINER (close_button_alignment),
                           GTK_WIDGET (close_button));

        close_button_container = gtk_tool_item_new ();
        gtk_container_add (GTK_CONTAINER (close_button_container),
                           close_button_alignment);

        gtk_toolbar_insert (GTK_TOOLBAR (self), close_button_container, -1);

        g_signal_connect (G_OBJECT (close_button), "clicked",
                          G_CALLBACK (close_button_clicked_cb), self);

        g_signal_connect (G_OBJECT (priv->entry), "changed",
                          G_CALLBACK (on_entry_changed), self);

        g_signal_connect (self, "hide",
                          G_CALLBACK (on_hide_cb), self);

        gtk_widget_show_all (GTK_WIDGET (self));
        gtk_widget_set_no_show_all (GTK_WIDGET (self), TRUE);
}

/* Public interface */

/**
 * hildon_live_search_new:
 *
 * Creates and returns a new #HildonLiveSearch widget.
 *
 * Returns: The newly created live search widget.
 **/
GtkWidget *
hildon_live_search_new (void)
{
        return g_object_new (HILDON_TYPE_LIVE_SEARCH, NULL);
}

static gboolean
visible_func (GtkTreeModel *model,
              GtkTreeIter *iter,
              gpointer data)
{
        HildonLiveSearchPrivate *priv;
        gchar *string;
        gboolean visible = FALSE;

        priv = (HildonLiveSearchPrivate *) data;

        if (priv->prefix == NULL)
                return TRUE;

        if (priv->visible_func == NULL && priv->text_column == -1)
                return TRUE;

        if (priv->visible_func) {
                visible = (priv->visible_func) (model, iter,
                                                priv->prefix,
                                                priv->visible_data);
        } else {
                gtk_tree_model_get (model, iter, priv->text_column, &string, -1);
                visible = (string != NULL && g_str_has_prefix (string, priv->prefix));
                g_free (string);
        }

        return visible;
}

/**
 * hildon_live_search_set_filter:
 * @livesearch: An #HildonLiveSearch widget
 * @filter: a #GtkTreeModelFilter, or %NULL
 *
 * Sets the filter for @livesearch.
 */
void
hildon_live_search_set_filter (HildonLiveSearch  *livesearch,
                               GtkTreeModelFilter *filter)
{
        HildonLiveSearchPrivate *priv;

        g_return_if_fail (HILDON_IS_LIVE_SEARCH (livesearch));
        g_return_if_fail (filter == NULL || GTK_IS_TREE_MODEL_FILTER (filter));

        priv = GET_PRIVATE (livesearch);

        if (priv->filter) {
                g_object_unref (priv->filter);
                priv->filter = NULL;
        }

        if (filter) {
                priv->filter = g_object_ref (filter);
        }

        priv->text_column = -1;

        gtk_tree_model_filter_set_visible_func (filter,
                                                visible_func,
                                                priv,
                                                NULL);

        g_object_notify (G_OBJECT (livesearch), "filter");
}

/**
 * hildon_live_search_set_text_column:
 * @livesearch: a #HildonLiveSearch
 * @text_column: the column in the model of @livesearch to get the strings
 * to filter from
 *
 * Sets the column to be used by the default filtering method.
 * This column must be of type %G_TYPE_STRING.
 *
 * Calling this method will filtering of the model, so use with
 * moderation. Note that you can only use either
 * #HildonLiveSearch:text-column or
 * hildon_live_search_set_filter_func().
 *
 **/
void
hildon_live_search_set_text_column (HildonLiveSearch *livesearch,
                                    gint text_column)
{
        HildonLiveSearchPrivate *priv;
        priv = GET_PRIVATE (livesearch);

        g_return_if_fail (HILDON_IS_LIVE_SEARCH (livesearch));
        g_return_if_fail (-1 <= text_column);
        g_return_if_fail (text_column < gtk_tree_model_get_n_columns (gtk_tree_model_filter_get_model (priv->filter)));
        g_return_if_fail (priv->visible_func == NULL);

        if (priv->text_column == text_column)
                return;

        priv->text_column = text_column;

        gtk_tree_model_filter_refilter (priv->filter);
}

static void
on_hook_widget_destroy (GtkObject *object,
                        gpointer user_data)
{
        HildonLiveSearchPrivate *priv;

        priv = GET_PRIVATE (user_data);

        priv->destroy_id = 0;
}

/**
 * hildon_live_search_widget_hook:
 * @livesearch: An #HildonLiveSearch widget
 * @hook_widget: A widget on which we listen for key events
 * @kb_focus: The widget which we grab focus on
 *
 * This function must be called after an #HildonLiveSearch widget is
 * constructed to set the hook widget and the focus widget for
 * @livesearch. After that, the #HildonLiveSearch widget can be
 * packed into a container and used.
 **/
void
hildon_live_search_widget_hook (HildonLiveSearch *livesearch,
                                GtkWidget        *hook_widget,
                                GtkTreeView      *kb_focus)
{
        HildonLiveSearchPrivate *priv;

        g_return_if_fail (HILDON_IS_LIVE_SEARCH (livesearch));
        priv = GET_PRIVATE (livesearch);

        g_return_if_fail (priv->event_widget == NULL);

        priv->event_widget = hook_widget;
        priv->treeview = kb_focus;

        priv->key_press_id =
                g_signal_connect (hook_widget, "key-press-event",
                                  G_CALLBACK (on_key_press_event), livesearch);

        priv->destroy_id =
                g_signal_connect (G_OBJECT (hook_widget), "destroy",
                                  G_CALLBACK (on_hook_widget_destroy),
                                  livesearch);
}

/**
 * hildon_live_search_widget_unhook:
 * @livesearch: An #HildonLiveSearch widget
 *
 * This function unsets the hook and focus widgets which were set
 * earlier using hildon_live_search_widget_hook().
 **/
void
hildon_live_search_widget_unhook (HildonLiveSearch *livesearch)
{
        HildonLiveSearchPrivate *priv;

        g_return_if_fail (HILDON_IS_LIVE_SEARCH (livesearch));
        priv = GET_PRIVATE (livesearch);

        if (priv->event_widget == NULL)
                return;

        if (priv->key_press_id) {
                g_signal_handler_disconnect (priv->event_widget, priv->key_press_id);
                priv->key_press_id = 0;
        }
        if (priv->destroy_id) {
                g_signal_handler_disconnect (priv->event_widget, priv->destroy_id);
                priv->destroy_id = 0;
        }

        priv->event_widget = NULL;
        priv->treeview = NULL;
}

/**
 * hildon_live_search_save_state:
 * @livesearch: An #HildonLiveSearch widget
 * @key_file: The key file to save to
 *
 * Saves the live search text to a #GKeyFile.
 **/
void
hildon_live_search_save_state (HildonLiveSearch *livesearch,
                               GKeyFile *key_file)
{
        HildonLiveSearchPrivate *priv;
        const char *text;

        g_return_if_fail (HILDON_IS_LIVE_SEARCH (livesearch));
        priv = GET_PRIVATE (livesearch);

        text = gtk_entry_get_text (GTK_ENTRY (priv->entry));
        if (text) {
                g_key_file_set_string (key_file,
                                       "LiveSearch",
                                       "Text",
                                       text);
        }
}

/**
 * hildon_live_search_restore_state:
 * @livesearch: An #HildonLiveSearch widget
 * @key_file: The key file to read from
 *
 * Restores a live search widget's text from a #GKeyFile.
 **/
void
hildon_live_search_restore_state (HildonLiveSearch *livesearch,
                                  GKeyFile *key_file)
{
        HildonLiveSearchPrivate *priv;
        char *text;

        g_return_if_fail (HILDON_IS_LIVE_SEARCH (livesearch));
        priv = GET_PRIVATE (livesearch);

        text = g_key_file_get_string (key_file,
                                      "LiveSearch",
                                      "Text",
                                      NULL);
        if (text) {
                gtk_entry_set_text (GTK_ENTRY (priv->entry), text);
        }
}

/**
 * hildon_live_search_set_filter_func:
 * @livesearch: a HildonLiveSearch
 * @func: a #HildonLiveSearchFilterFunc
 * @data: user data to pass to @func or %NULL
 * @destroy: Destroy notifier of @data, or %NULL.
 *
 * Sets the function to use to determine whether a row should be
 * visible when the text in the entry changes. Internally,
 * gtk_tree_model_filter_set_visible_func() is used.
 *
 * If this function is unset, #HildonLiveSearch:text-column is used.
 **/
void
hildon_live_search_set_filter_func (HildonLiveSearch *livesearch,
                                    HildonLiveSearchFilterFunc func,
                                    gpointer data,
                                    GDestroyNotify destroy)
{
        HildonLiveSearchPrivate *priv;

        g_return_if_fail (HILDON_IS_LIVE_SEARCH (livesearch));
        g_return_if_fail (func != NULL);

        priv = GET_PRIVATE (livesearch);

        g_return_if_fail (priv->visible_func == NULL);
        g_return_if_fail (priv->text_column == -1);

        if (priv->visible_func) {
                GDestroyNotify d = priv->visible_destroy;

                priv->visible_destroy = NULL;
                d (priv->visible_data);
        }

        priv->visible_func = func;
        priv->visible_data = data;
        priv->visible_destroy = destroy;
}
