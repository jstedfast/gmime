/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2014 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "list.h"

void
list_init (List *list)
{
	list->head = (ListNode *) &list->tail;
	list->tail = NULL;
	list->tailpred = (ListNode *) &list->head;
}

int
list_is_empty (List *list)
{
	return list->head == (ListNode *) &list->tail;
}

int
list_length (List *list)
{
	ListNode *node;
	int n = 0;
	
	node = list->head;
	while (node->next) {
		node = node->next;
		n++;
	}
	
	return n;
}

ListNode *
list_unlink_head (List *list)
{
	ListNode *n, *nn;
	
	n = list->head;
	nn = n->next;
	if (nn) {
		nn->prev = n->prev;
		list->head = nn;
		return n;
	}
	
	return NULL;
}

ListNode *
list_unlink_tail (List *list)
{
	ListNode *n, *np;
	
	n = list->tailpred;
	np = n->prev;
	if (np) {
		np->next = n->next;
		list->tailpred = np;
		return n;
	}
	
	return NULL;
}

ListNode *
list_prepend (List *list, ListNode *node)
{
	node->next = list->head;
	node->prev = (ListNode *) &list->head;
	list->head->prev = node;
	list->head = node;
	
	return node;
}

ListNode *
list_append (List *list, ListNode *node)
{
	node->next = (ListNode *) &list->tail;
	node->prev = list->tailpred;
	list->tailpred->next = node;
	list->tailpred = node;
	
	return node;
}

ListNode *
list_unlink (ListNode *node)
{
	node->next->prev = node->prev;
        node->prev->next = node->next;
	
	return node;
}
