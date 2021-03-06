/*
 * This file is a part of hildon
 *
 * Copyright (C) 2008 Nokia Corporation, all rights reserved.
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

#ifndef                                         __HILDON_WINDOW_STACK_PRIVATE_H__
#define                                         __HILDON_WINDOW_STACK_PRIVATE_H__

G_BEGIN_DECLS

void G_GNUC_INTERNAL
hildon_window_stack_remove                      (HildonStackableWindow *win);

gboolean G_GNUC_INTERNAL
_hildon_window_stack_do_push                    (HildonWindowStack     *stack,
                                                 HildonStackableWindow *win);

G_END_DECLS

#endif                                          /* __HILDON_WINDOW_STACK_PRIVATE_H__ */
