/*
 * This file is part of hildon-libs
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Luc Pionchon <luc.pionchon@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/*
 * @file hildon-grid-item.c
 *
 * This file contains the implementation of HildonGridItem.
 * HildonGridItem is an item mainly used in HildonGrid. It has an icon,
 * emblem and a label. 
 *
 */
/*
 * TODO:
 * - play with libtool to get _-functions private but accesable from grid
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkenums.h>
#include <pango/pango.h>

#include "hildon-grid-item-private.h"
#include <hildon-widgets/hildon-grid-item.h>

#include <libintl.h>
#define _(String) dgettext(PACKAGE, String)

#define HILDON_GRID_ITEM_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_GRID_ITEM, \
                                      HildonGridItemPrivate))

typedef struct _HildonGridItemPrivate HildonGridItemPrivate;


/* Default icon. */
#define DEFAULT_ICON_BASENAME   "Unknown"
#define HILDON_GRID_ICON_SIZE         26
#define HILDON_GRID_EMBLEM_SIZE       16

/* Use somewhat dirty alpha-thing for emblems. */
#define USE_DIRTY_ALPHA

struct _HildonGridItemPrivate {
    gchar *icon_basename;
    gint icon_size;
    GtkWidget *icon;
    gint icon_width;
    gint icon_height;

    gchar *emblem_basename;
    gint emblem_size;

    GtkWidget *label;   /* TODO use pango! */
    HildonGridPositionType label_pos;

    gint label_hspacing;
    gint label_vspacing;

    gboolean selected;
};



/* Prototypes. */
static void hildon_grid_item_class_init(HildonGridItemClass * klass);
static void hildon_grid_item_init(HildonGridItem * item);
static gboolean hildon_grid_item_expose(GtkWidget * widget,
                                        GdkEventExpose * event);
static void hildon_grid_item_size_request(GtkWidget * widget,
                                          GtkRequisition * requisition);
static void hildon_grid_item_size_allocate(GtkWidget * widget,
                                           GtkAllocation * allocation);
static void hildon_grid_item_forall(GtkContainer * container,
                                    gboolean include_int,
                                    GtkCallback callback,
                                    gpointer callback_data);
static void hildon_grid_item_remove(GtkContainer * container,
                                    GtkWidget * child);

static void hildon_grid_item_finalize(GObject * object);

static void update_icon(HildonGridItem * item);
static void set_label_justify(HildonGridItem * item);


static GtkContainerClass *parent_class = NULL;


GType
hildon_grid_item_get_type(void)
{
    static GType grid_item_type = 0;

    if (!grid_item_type) {
        static const GTypeInfo grid_item_info = {
            sizeof(HildonGridItemClass),
            NULL,       /* base_init */
            NULL,       /* base_finalize */
            (GClassInitFunc) hildon_grid_item_class_init,
            NULL,       /* class_finalize */
            NULL,       /* class_data */
            sizeof(HildonGridItem),
            0,  /* n_preallocs */
            (GInstanceInitFunc) hildon_grid_item_init,
        };
        grid_item_type = g_type_register_static(GTK_TYPE_CONTAINER,
                                                "HildonGridItem",
                                                &grid_item_info, 0);
    }

    return grid_item_type;
}

static void
hildon_grid_item_class_init(HildonGridItemClass *klass)
{
    GtkWidgetClass *widget_class;
    GtkContainerClass *container_class;
    GObjectClass *gobject_class;

/* g_message ("hildon_grid_item_class_init (%d)\n", (int) klass); */

    widget_class = GTK_WIDGET_CLASS(klass);
    gobject_class = G_OBJECT_CLASS(klass);
    container_class = GTK_CONTAINER_CLASS(klass);

    parent_class = g_type_class_peek_parent(klass);

    g_type_class_add_private(klass, sizeof(HildonGridItemPrivate));

    gobject_class->finalize = hildon_grid_item_finalize;

    widget_class->expose_event = hildon_grid_item_expose;
    widget_class->size_request = hildon_grid_item_size_request;
    widget_class->size_allocate = hildon_grid_item_size_allocate;

    container_class->forall = hildon_grid_item_forall;
    container_class->remove = hildon_grid_item_remove;
}

static void
hildon_grid_item_init(HildonGridItem *item)
{
    HildonGridItemPrivate *priv;

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);

    priv->icon_basename = NULL;
    priv->icon_size = HILDON_GRID_ICON_SIZE;
    priv->icon = NULL;

    priv->emblem_basename = NULL;
    priv->emblem_size = HILDON_GRID_EMBLEM_SIZE;

    priv->label = NULL;
    priv->label_pos = HILDON_GRID_ITEM_LABEL_POS_BOTTOM;
    priv->label_hspacing = 0;
    priv->label_vspacing = 0;

    priv->selected = FALSE;

}

/**
 * hildon_grid_item_new:
 * @icon_basename:  Icon base name
 *
 * Creates a new #HildonGridItem.
 *
 * Return value: A new #HildonGridItem
 */
GtkWidget *
hildon_grid_item_new(const gchar *icon_basename)
{
    HildonGridItem *item;
    HildonGridItemPrivate *priv;

    item = g_object_new(HILDON_TYPE_GRID_ITEM, NULL);
    GTK_WIDGET_SET_FLAGS(GTK_WIDGET(item), GTK_CAN_FOCUS);

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);
    priv->icon_basename = g_strdup(icon_basename);

    priv->label = gtk_label_new("");
    gtk_widget_set_parent(priv->label, GTK_WIDGET(item));
    gtk_widget_set_name(priv->label, "hildon-grid-item-label");

    update_icon(item);
    set_label_justify(item);

    gtk_widget_show(priv->label);

    return GTK_WIDGET(item);
}

/**
 * hildon_grid_item_new_with_label:
 * @icon_basename:  Icon base name
 * @label:          Text label for icon
 *
 * Creates a new #HildonGridItem.
 *
 * Return value: A new #HildonGridItem
 */
GtkWidget *
hildon_grid_item_new_with_label(const gchar *icon_basename,
                                const gchar *label)
{
    HildonGridItem *item;
    HildonGridItemPrivate *priv;


    item = g_object_new(HILDON_TYPE_GRID_ITEM, NULL);
    GTK_WIDGET_SET_FLAGS(GTK_WIDGET(item), GTK_CAN_FOCUS);

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);
    priv->icon_basename = g_strdup(icon_basename);

    priv->label = gtk_label_new(label != NULL ? label : "");
    gtk_widget_set_name(priv->label, "hildon-grid-item-label");
    gtk_widget_set_parent(priv->label, GTK_WIDGET(item));

    update_icon(item);
    set_label_justify(item);

    gtk_widget_show(priv->label);

    return GTK_WIDGET(item);
}


static void
update_icon(HildonGridItem *item)
{
    GtkIconTheme *icon_theme;
    GdkPixbuf *icon;
    GdkPixbuf *emblem_icon;
    HildonGridItemPrivate *priv;
    GError *error;

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);

    if (priv->icon != NULL) {
        if (GTK_WIDGET_VISIBLE(priv->icon))
            gtk_widget_hide(priv->icon);
        gtk_widget_unparent(priv->icon);
    }

    icon_theme = gtk_icon_theme_get_default();

    /* Load icon. Fall to default it loading fails. */
    error = NULL;
    icon = gtk_icon_theme_load_icon(icon_theme,
                                    priv->icon_basename,
                                    priv->icon_size, 0, &error);
    if (icon == NULL) {
        g_warning("Couldn't load icon \"%s\": %s", priv->icon_basename,
                  error->message);
        g_error_free(error);

        error = NULL;
        icon = gtk_icon_theme_load_icon(icon_theme,
                                        DEFAULT_ICON_BASENAME,
                                        priv->icon_size, 0, &error);
        if (icon == NULL) {
            g_warning("Couldn't load default icon: %s!\n", error->message);
            g_error_free(error);
        }
    }
    priv->icon_width = gdk_pixbuf_get_width(icon);
    priv->icon_height = gdk_pixbuf_get_height(icon);


    /* Load and merge emblem if one is specified. */
    if (priv->emblem_basename != NULL) {
        error = NULL;
        emblem_icon = gtk_icon_theme_load_icon(icon_theme,
                                               priv->emblem_basename,
                                               priv->emblem_size,
                                               0, &error);
        if (emblem_icon == NULL) {
            g_warning("Couldn't load emblem \"%s\": %s",
                      priv->emblem_basename, error->message);
            g_error_free(error);
        } else {
            gint icon_height;
            gint width, height, y;

#ifdef USE_DIRTY_ALPHA
            GdkPixbuf *tmp;
#endif

            icon_height = gdk_pixbuf_get_height(icon);
            width = MIN(gdk_pixbuf_get_width(emblem_icon),
                        gdk_pixbuf_get_width(icon));
            height = MIN(gdk_pixbuf_get_height(emblem_icon), icon_height);
            y = icon_height - height;
#ifndef USE_DIRTY_ALPHA
            gdk_pixbuf_copy_area(emblem_icon, 0, 0, width, height,
                                 icon, 0, y);
#else
            /* 
             * Using composite to copy emblem to lower left corner creates
             * some garbage on top of emblem. This way it can be avoided.
             */
            tmp = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE,
                                 8, width, icon_height);
            gdk_pixbuf_fill(tmp, 0x00000000);
            gdk_pixbuf_copy_area(emblem_icon, 0, 0, width, height,
                                 tmp, 0, y);
            gdk_pixbuf_composite(tmp, icon,
                                 0, 0, width, icon_height,
                                 0.0, 0.0, 1.0, 1.0,
                                 GDK_INTERP_NEAREST, 255);
            g_object_unref(tmp);
#endif /* ifndef else USE_DIRTY_ALPHA */
            g_object_unref(emblem_icon);
        }
    }

    priv->icon = gtk_image_new_from_pixbuf(icon);
    g_object_unref(icon);

    gtk_widget_set_parent(priv->icon, GTK_WIDGET(item));
    gtk_widget_show(priv->icon);

    gtk_widget_queue_draw(priv->icon);
}

void
_hildon_grid_item_set_label(HildonGridItem *item, const gchar *label)
{
    HildonGridItemPrivate *priv;

    g_return_if_fail(HILDON_IS_GRID_ITEM(item));

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);
    if ((priv->label == NULL && label == NULL) ||
        (priv->label != NULL && label != NULL &&
         strcmp((char *) priv->label, (char *) label) == 0)) {
        return;
    }
    gtk_label_set_label(GTK_LABEL(priv->label), label);
}

void
_hildon_grid_item_set_icon_size(HildonGridItem              *item,
                                HildonGridItemIconSizeType  icon_size)
{
    HildonGridItemPrivate *priv;

    g_return_if_fail(HILDON_IS_GRID_ITEM(item));

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);
    if (priv->icon_size == icon_size) {
        return;
    }
    HILDON_GRID_ITEM_GET_PRIVATE(item)->icon_size = icon_size;
    update_icon(HILDON_GRID_ITEM(item));
}


void
_hildon_grid_item_set_label_pos(HildonGridItem          *item,
                                HildonGridPositionType  label_pos)
{
    HildonGridItemPrivate *priv;

    g_return_if_fail(HILDON_IS_GRID_ITEM(item));

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);
    if (priv->label_pos == label_pos) {
        return;
    }
    priv->label_pos = label_pos;
    set_label_justify(item);
    /* No refresh here, grid will do it. */
}

void
_hildon_grid_item_set_label_spacing(HildonGridItem  *item,
                                    const gint      hspacing,
                                    const gint      vspacing)
{
    HildonGridItemPrivate *priv;

    g_return_if_fail(HILDON_IS_GRID_ITEM(item));

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);

    priv->label_hspacing = hspacing;
    priv->label_vspacing = vspacing;
}


void
_hildon_grid_item_set_emblem_size(HildonGridItem *item, gint emblem_size)
{
    HildonGridItemPrivate *priv;

    g_return_if_fail(HILDON_IS_GRID_ITEM(item));

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);
    if (priv->emblem_size == emblem_size) {
        return;
    }
    priv->emblem_size = emblem_size;
    update_icon(HILDON_GRID_ITEM(item));
}

static void
set_label_justify(HildonGridItem *item)
{
    HildonGridItemPrivate *priv;

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);

    if (priv->label != NULL) {
        switch (priv->label_pos) {
        case HILDON_GRID_ITEM_LABEL_POS_BOTTOM:
            gtk_misc_set_alignment(GTK_MISC(priv->label), 0.5, 0.5);
            break;

        case HILDON_GRID_ITEM_LABEL_POS_RIGHT:
            gtk_misc_set_alignment(GTK_MISC(priv->label), 0.0, 0.5);
            break;

        default:
            g_warning("Invalid position!");
            break;
        }
    }
}

static void
hildon_grid_item_remove(GtkContainer *container, GtkWidget *child)
{
    HildonGridItem *item;
    HildonGridItemPrivate *priv;

    item = HILDON_GRID_ITEM(container);
    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);

    g_return_if_fail(GTK_IS_WIDGET(child));
    g_return_if_fail(child == priv->label || child == priv->icon);

    if (child == priv->label) {
        gtk_widget_unparent(child);
        priv->label = NULL;
    } else if (child == priv->icon) {
        gtk_widget_unparent(child);
        priv->icon = NULL;
    }
}

static gboolean
hildon_grid_item_expose(GtkWidget *widget, GdkEventExpose *event)
{
    HildonGridItem *item;
    HildonGridItemPrivate *priv;
    GdkRectangle clip;
    GtkWidget *focused;

    g_return_val_if_fail(widget, FALSE);
    g_return_val_if_fail(HILDON_IS_GRID_ITEM(widget), FALSE);
    g_return_val_if_fail(event, FALSE);

    item = HILDON_GRID_ITEM(widget);
    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);

    if (priv->label == NULL && priv->icon == NULL) {
        return FALSE;
    }
    if (GTK_WIDGET_HAS_FOCUS(GTK_WIDGET(item))) {
        if (priv->label != NULL) {
            focused = priv->label;
        } else {
            focused = priv->icon;
        }
        clip.x = focused->allocation.x;
        clip.y = focused->allocation.y;
        clip.width = focused->allocation.width;
        clip.height = focused->allocation.height;

        gtk_paint_box(focused->style,
                      gtk_widget_get_toplevel(focused)->window,
                      GTK_STATE_SELECTED,
                      GTK_SHADOW_NONE,
                      &clip, focused, "selected",
                      clip.x, clip.y, clip.width, clip.height);
    }

    /* 
     * Items are not exposed unless they are visible.
     * -> No need to "optimize" by checking if they need exposing.
     */
    gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                   priv->icon, event);
    gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                   priv->label, event);
    return TRUE;
}


static void
hildon_grid_item_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
    HildonGridItem *item;
    HildonGridItemPrivate *priv;
    GtkRequisition label_req;
    gint label_margin;

    item = HILDON_GRID_ITEM(widget);
    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);

    switch (priv->label_pos) {
    case HILDON_GRID_ITEM_LABEL_POS_BOTTOM:
        label_margin = priv->label_vspacing;
        break;

    case HILDON_GRID_ITEM_LABEL_POS_RIGHT:
        label_margin = priv->label_hspacing;
        break;
    default:
        label_margin = 0;
        break;
    }

    gtk_widget_size_request(priv->icon, requisition);
    gtk_widget_size_request(priv->label, &label_req);

    switch (priv->label_pos) {
    case HILDON_GRID_ITEM_LABEL_POS_BOTTOM:
        requisition->width = MAX(requisition->width, label_req.width);
        requisition->height += label_req.height + label_margin;
        break;

    case HILDON_GRID_ITEM_LABEL_POS_RIGHT:
        requisition->width += label_req.width + label_margin;
        requisition->height = MAX(requisition->height, label_req.height);
        break;
    default:
        g_warning("bad position");
        return;
        break;
    }
}

static void
hildon_grid_item_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    HildonGridItem *item;
    HildonGridItemPrivate *priv;
    GtkRequisition l_req;
    GtkAllocation i_alloc, l_alloc;
    gint label_margin;
    gint label_height;

    g_return_if_fail(widget);
    g_return_if_fail(allocation);

    item = HILDON_GRID_ITEM(widget);
    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);
    widget->allocation = *allocation;

    /* If creating label and icon failed, don't show a thing... */
    if (priv->label == NULL && priv->icon == NULL) {
        return;
    }
    if (priv->label != NULL) {
        gtk_widget_get_child_requisition(priv->label, &l_req);
    } else {
        l_req.width = l_req.height = 0;
    }

    label_height = l_req.height;
    switch (priv->label_pos) {
    case HILDON_GRID_ITEM_LABEL_POS_BOTTOM:
        label_margin = priv->label_vspacing;

        if (priv->icon_width > allocation->width) {
            priv->icon_width = allocation->width;
        }
        if (priv->icon_height > allocation->height) {
            priv->icon_height = allocation->height;
        }
        i_alloc.x = (allocation->width - priv->icon_width) /
            2 + allocation->x;
        i_alloc.y = (allocation->height - priv->icon_height -
                     label_height - label_margin) / 2 + allocation->y;
        i_alloc.width = priv->icon_width;
        i_alloc.height = priv->icon_height;

        if (label_height > 0) {
            if (l_req.height > allocation->height) {
                l_req.height = allocation->height;
            }
            l_alloc.x = allocation->x;
            l_alloc.y = i_alloc.y + priv->icon_height + label_margin;
            l_alloc.width = allocation->width;
            l_alloc.height = l_req.height;
        }
        break;

    case HILDON_GRID_ITEM_LABEL_POS_RIGHT:
        label_margin = priv->label_hspacing;

        if (priv->icon_width > allocation->width) {
            priv->icon_width = allocation->width;
        }
        if (priv->icon_height > allocation->height) {
            priv->icon_height = allocation->height;
        }

        i_alloc.x = allocation->x;
        i_alloc.y = (allocation->height - priv->icon_height) /
            2 + allocation->y;
        i_alloc.width = priv->icon_width;
        i_alloc.height = priv->icon_height;

        if (label_height > 0) {
            l_alloc.x = priv->icon_width + label_margin + allocation->x;
            l_alloc.y = (allocation->height - label_height) / 2
                + allocation->y;

            l_alloc.width = allocation->width - priv->icon_width
                - label_margin;
            l_alloc.height = MIN(l_req.height, allocation->height);
        }
        break;
    default:
        g_warning("bad label position");
        return;
        break;
    }

    if (priv->label != NULL) {
        gtk_widget_size_allocate(priv->label, &l_alloc);
    }
    if (priv->icon != NULL) {
        gtk_widget_size_allocate(priv->icon, &i_alloc);
    }
}

static void
hildon_grid_item_forall(GtkContainer    *container,
                        gboolean        include_int,
                        GtkCallback     callback,
                        gpointer        callback_data)
{
    HildonGridItem *item;
    HildonGridItemPrivate *priv;

    g_return_if_fail(container);
    g_return_if_fail(callback);

    item = HILDON_GRID_ITEM(container);
    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);

    if (priv->icon != NULL) {
        (*callback) (priv->icon, callback_data);
    }
    if (priv->label != NULL) {
        (*callback) (priv->label, callback_data);
    }
}

static void
hildon_grid_item_finalize(GObject *object)
{
    HildonGridItem *item;
    HildonGridItemPrivate *priv;

    item = HILDON_GRID_ITEM(object);
    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);

    g_free(priv->icon_basename);
    if (priv->emblem_basename != NULL) {
        g_free(priv->emblem_basename);
    }

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

#if 0
static int hildon_time_get_font_width(GtkWidget * widget)
{
    PangoContext *context;
    PangoFontMetrics *metrics;
    gint digit_width;

    context = gtk_widget_get_pango_context(widget);
    metrics = pango_context_get_metrics(context,
                                        widget->style->font_desc,
                                        pango_context_get_language
                                        (context));

    digit_width = pango_font_metrics_get_approximate_digit_width(metrics);
    digit_width = PANGO_PIXELS(digit_width);

    pango_font_metrics_unref(metrics);

    return digit_width;
}
#endif


/**
 * hildon_grid_item_set_emblem_type:
 * @item:               #HildonGridItem
 * @emblem_basename:    Emblem's basename
 *
 * Sets item emblem type.
 */
void
hildon_grid_item_set_emblem_type(HildonGridItem *item,
                                 const gchar *emblem_basename)
{
    HildonGridItemPrivate *priv;

    g_return_if_fail(HILDON_IS_GRID_ITEM(item));

    priv = HILDON_GRID_ITEM_GET_PRIVATE(item);

    if (priv->emblem_basename != NULL) {
        g_free(priv->emblem_basename);
    }

    priv->emblem_basename = g_strdup(emblem_basename);

    update_icon(item);
}

/**
 * hildon_grid_item_get_emblem_type:
 * @item:   #HildonGridItem
 *
 * Returns emblem's basename. Must not be changed or freed.
 *
 * Return value: Emblem's basename
 */
const gchar *
hildon_grid_item_get_emblem_type(HildonGridItem *item)
{
    g_return_val_if_fail(HILDON_IS_GRID_ITEM(item), NULL);

    return HILDON_GRID_ITEM_GET_PRIVATE(item)->emblem_basename;
}
