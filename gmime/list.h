/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __LIST_H__
#define __LIST_H__

#include <string.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _ListNode {
	struct _ListNode *next;
	struct _ListNode *prev;
} ListNode;

typedef struct {
	ListNode *head;
	ListNode *tail;
	ListNode *tailpred;
} List;

#define LIST_INITIALIZER(l) { (ListNode *) &l.tail, NULL, (ListNode *) &l.head }

void list_init (List *list);

int list_is_empty (List *list);

int list_length (List *list);

ListNode *list_unlink_head (List *list);
ListNode *list_unlink_tail (List *list);

ListNode *list_prepend_node (List *list, ListNode *node);
ListNode *list_append_node  (List *list, ListNode *node);

ListNode *list_node_unlink (ListNode *node);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LIST_H__ */
