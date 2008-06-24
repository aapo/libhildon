/*
 * This file is a part of hildon
 *
 * Copyright (C) 2008 Nokia Corporation, all rights reserved.
 *
 * Contact: Karl Lattimer <karl.lattimer@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
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

/**
 * SECTION:hildon-stackable-window
 * @short_description: Widget representing a stackable, top-level window in the Hildon framework.
 *
 * The #HildonStackableWindow is a GTK+ widget which represents a
 * top-level window in the Hildon framework. It is derived from
 * #HildonWindow. Applications that use stackable windows are organized
 * in a hierarchical way so users can go from any window back to the
 * application's root window.
 */

#include                                        <X11/X.h>
#include                                        <X11/Xatom.h>
#include                                        "hildon-stackable-window.h"
#include                                        "hildon-program.h"
#include                                        "hildon-window-private.h"
#include                                        "hildon-program-private.h"

typedef struct                                  _HildonStackableWindowPrivate HildonStackableWindowPrivate;

struct                                          _HildonStackableWindowPrivate
{
    gboolean going_home;
};

#define                                         HILDON_STACKABLE_WINDOW_GET_PRIVATE(obj) \
                                                (G_TYPE_INSTANCE_GET_PRIVATE ((obj),\
                                                HILDON_TYPE_STACKABLE_WINDOW, HildonStackableWindowPrivate))

G_DEFINE_TYPE (HildonStackableWindow, hildon_stackable_window, HILDON_TYPE_WINDOW);

static void
hildon_stackable_window_set_going_home          (HildonStackableWindow *self,
                                                 gboolean going_home)
{
    HildonStackableWindowPrivate *priv = HILDON_STACKABLE_WINDOW_GET_PRIVATE (self);
    priv->going_home = going_home;
}

static gboolean
hildon_stackable_window_get_going_home          (HildonStackableWindow *self)
{
    HildonStackableWindowPrivate *priv = HILDON_STACKABLE_WINDOW_GET_PRIVATE (self);
    return priv->going_home;
}

static GSList*
get_window_list                                 (GtkWidget *widget)
{
    HildonWindowPrivate *wpriv;
    HildonProgramPrivate *ppriv;
    GSList *retval = NULL;
    g_return_val_if_fail (widget != NULL, NULL);

    wpriv = HILDON_WINDOW_GET_PRIVATE (widget);
    g_assert (wpriv != NULL);

    if (wpriv->program)
    {
        ppriv = HILDON_PROGRAM_GET_PRIVATE (wpriv->program);
        g_assert (ppriv != NULL);
        retval = ppriv->windows;
    }

    return retval;
}

static GtkWidget*
get_last_window                                 (GtkWidget *widget)
{
    GtkWidget *retval = NULL;
    GSList *windows = get_window_list (widget);
    GSList *last = NULL;

    g_return_val_if_fail (windows != NULL, NULL);

    /* Go to the end of the window list */
    while (windows->next != NULL)
    {
        last = windows;
        windows = windows->next;
    }

    if ((windows->data == widget) && (last != NULL))
    {
        retval = GTK_WIDGET (last->data);
    }

    return retval;
}

static void
hildon_stackable_window_map                     (GtkWidget *widget)
{
    GtkWidget *lastwin;

    if (GTK_WIDGET_CLASS (hildon_stackable_window_parent_class)->map)
        GTK_WIDGET_CLASS (hildon_stackable_window_parent_class)->map (widget);

    lastwin = get_last_window (widget);

    if (HILDON_IS_STACKABLE_WINDOW (lastwin) && GTK_WIDGET_VISIBLE (lastwin))
        gtk_widget_hide (lastwin);
}

static void
hildon_stackable_window_unmap                   (GtkWidget *widget)
{
    GtkWidget *lastwin;

    if (GTK_WIDGET_CLASS (hildon_stackable_window_parent_class)->unmap)
        GTK_WIDGET_CLASS (hildon_stackable_window_parent_class)->unmap (widget);

    lastwin = get_last_window (widget);

    if (HILDON_IS_STACKABLE_WINDOW (lastwin) && !GTK_WIDGET_VISIBLE (lastwin) &&
        !hildon_stackable_window_get_going_home (HILDON_STACKABLE_WINDOW (widget)))
    {
        gtk_widget_show (lastwin);
    }
}

static void
hildon_stackable_window_unset_program           (HildonWindow *hwin)
{
    GSList *windows = get_window_list (GTK_WIDGET (hwin));
    gint l = g_slist_length (windows);
    GtkWidget *nextwin = GTK_WIDGET (g_slist_nth_data (windows, l - 2));

    if (HILDON_WINDOW_CLASS (hildon_stackable_window_parent_class)->unset_program)
        HILDON_WINDOW_CLASS (hildon_stackable_window_parent_class)->unset_program (hwin);

    if (HILDON_IS_STACKABLE_WINDOW (nextwin) && !GTK_WIDGET_VISIBLE (nextwin) &&
        !hildon_stackable_window_get_going_home (HILDON_STACKABLE_WINDOW (hwin)))
    {
        gtk_widget_show (nextwin);
    }
}

static void
hildon_stackable_window_realize                 (GtkWidget *widget)
{
    GdkDisplay *display;
    Atom atom;

    GTK_WIDGET_CLASS (hildon_stackable_window_parent_class)->realize (widget);

    /* Set the window type to "_HILDON_WM_WINDOW_TYPE_STACKABLE", to allow the WM to manage
       it properly.  */
    display = gdk_drawable_get_display (widget->window);
    atom = gdk_x11_get_xatom_by_name_for_display (display, "_HILDON_WM_WINDOW_TYPE_STACKABLE");
    XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (widget->window),
		     gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE"),
		     XA_ATOM, 32, PropModeAppend,
		     (guchar *)&atom, 1);
}

static void
hildon_stackable_window_class_init              (HildonStackableWindowClass *klass)
{
    GtkWidgetClass    *widget_class = GTK_WIDGET_CLASS (klass);
    HildonWindowClass *window_class = HILDON_WINDOW_CLASS (klass);

    widget_class->map               = hildon_stackable_window_map;
    widget_class->unmap             = hildon_stackable_window_unmap;
    widget_class->realize           = hildon_stackable_window_realize;

    window_class->unset_program     = hildon_stackable_window_unset_program;

    g_type_class_add_private (klass, sizeof (HildonStackableWindowPrivate));
}

static void
hildon_stackable_window_init                    (HildonStackableWindow *self)
{
    hildon_stackable_window_set_going_home (self, FALSE);
}

/**
 * hildon_stackable_window_new:
 *
 * Creates a new #HildonStackableWindow.
 *
 * Return value: A #HildonStackableWindow
 **/
GtkWidget*
hildon_stackable_window_new                     (void)
{
    HildonStackableWindow *newwindow = g_object_new (HILDON_TYPE_STACKABLE_WINDOW, NULL);

    return GTK_WIDGET (newwindow);
}

/**
 * hildon_stackable_window_go_to_root_window:
 * @self: A #HildonStackableWindow
 *
 * Will pop out all the stackable windows in the @HildonProgram but the
 * first one, which can be considered as the "home" window.
 */
void
hildon_stackable_window_go_to_root_window       (HildonStackableWindow *self)
{
    GSList *windows, *iter;
    gboolean windows_left;

    g_return_if_fail (HILDON_IS_STACKABLE_WINDOW (self));

    /* List of windows in reverse order (starting from the topmost one) */
    windows = g_slist_reverse (g_slist_copy (get_window_list (GTK_WIDGET (self))));
    iter = windows;

    /* Destroy all the windows but the last one (which is the root
     * window, as the list is reversed) */
    windows_left = (iter != NULL && iter->next != NULL);
    while (windows_left)
    {
        if (HILDON_IS_STACKABLE_WINDOW (iter->data))
        {
            GdkEvent *event;
            HildonStackableWindow *win;

            /* Mark the window as "going home" */
            win = HILDON_STACKABLE_WINDOW (iter->data);
            hildon_stackable_window_set_going_home (win, TRUE);

            /* Set win pointer to NULL if the window is destroyed */
            g_object_add_weak_pointer (G_OBJECT (win), (gpointer) &win);

            /* Send a delete event */
            event = gdk_event_new (GDK_DELETE);
            event->any.window = g_object_ref (GTK_WIDGET (win)->window);
            gtk_main_do_event (event);
            gdk_event_free (event);

            /* Continue sending delete events if the window has been destroyed */
            if (win == NULL)
            {
                iter = iter->next;
                windows_left = (iter != NULL && iter->next != NULL);
            }
            else
            {
                g_object_remove_weak_pointer (G_OBJECT (win), (gpointer) &win);
                hildon_stackable_window_set_going_home (win, FALSE);
                windows_left = FALSE;
            }
        }
        else
        {
            g_warning ("Window list contains a non-stackable window");
            windows_left = FALSE;
        }
    }

    /* Show the last window that hasn't been destroyed */
    if (iter != NULL && GTK_IS_WIDGET (iter->data))
    {
        gtk_widget_show (GTK_WIDGET (iter->data));
    }

    g_slist_free (windows);
}
