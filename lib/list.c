/**
 * @file
 * Singly-linked list type
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2017 Pietro Cerutti <gahr@gahr.ch>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page list Singly-linked list type
 *
 * | Function                 | Description
 * | :----------------------- | :-----------------------------------
 * | mutt_list_clear()        |
 * | mutt_list_find()         |
 * | mutt_list_free()         |
 * | mutt_list_insert_after() |
 * | mutt_list_insert_head()  |
 * | mutt_list_insert_tail()  |
 * | mutt_list_match()        |
 */

#include "config.h"
#include "list.h"
#include "memory.h"
#include "string2.h"

/**
 * mutt_list_insert_head -
 */
struct ListNode *mutt_list_insert_head(struct ListHead *h, char *s)
{
  struct ListNode *np = safe_calloc(1, sizeof(struct ListNode));
  np->data = s;
  STAILQ_INSERT_HEAD(h, np, entries);
  return np;
}

/**
 * mutt_list_insert_tail -
 */
struct ListNode *mutt_list_insert_tail(struct ListHead *h, char *s)
{
  struct ListNode *np = safe_calloc(1, sizeof(struct ListNode));
  np->data = s;
  STAILQ_INSERT_TAIL(h, np, entries);
  return np;
}

/**
 * mutt_list_insert_after -
 */
struct ListNode *mutt_list_insert_after(struct ListHead *h, struct ListNode *n, char *s)
{
  struct ListNode *np = safe_calloc(1, sizeof(struct ListNode));
  np->data = s;
  STAILQ_INSERT_AFTER(h, n, np, entries);
  return np;
}

/**
 * mutt_list_find -
 */
struct ListNode *mutt_list_find(struct ListHead *h, const char *data)
{
  struct ListNode *np;
  STAILQ_FOREACH(np, h, entries)
  {
    if (np->data == data || mutt_strcmp(np->data, data) == 0)
    {
      return np;
    }
  }
  return NULL;
}

/**
 * mutt_list_free -
 */
void mutt_list_free(struct ListHead *h)
{
  struct ListNode *np = STAILQ_FIRST(h), *next = NULL;
  while (np)
  {
    next = STAILQ_NEXT(np, entries);
    FREE(&np->data);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(h);
}

/**
 * mutt_list_clear -
 */
void mutt_list_clear(struct ListHead *h)
{
  struct ListNode *np = STAILQ_FIRST(h), *next = NULL;
  while (np)
  {
    next = STAILQ_NEXT(np, entries);
    FREE(&np);
    np = next;
  }
  STAILQ_INIT(h);
}

/**
 * mutt_list_match - Is the string in the list
 * @return true if the header contained in "s" is in list "h"
 */
bool mutt_list_match(const char *s, struct ListHead *h)
{
  struct ListNode *np;
  STAILQ_FOREACH(np, h, entries)
  {
    if (*np->data == '*' || mutt_strncasecmp(s, np->data, strlen(np->data)) == 0)
      return true;
  }
  return false;
}
