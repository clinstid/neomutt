/**
 * @file
 * Mbox local mailbox type
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013 Michael R. Elkins <me@mutt.org>
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

/* This file contains code to parse ``mbox'' and ``mmdf'' style mailboxes */

#include "config.h"
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "lib/lib.h"
#include "mutt.h"
#include "address.h"
#include "body.h"
#include "buffy.h"
#include "context.h"
#include "copy.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "list.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mx.h"
#include "options.h"
#include "parameter.h"
#include "protos.h"
#include "rfc822.h"
#include "sort.h"
#include "thread.h"

/**
 * struct MUpdate - Store of new offsets, used by mutt_sync_mailbox()
 */
struct MUpdate
{
  short valid;
  LOFF_T hdr;
  LOFF_T body;
  long lines;
  LOFF_T length;
};

/**
 * mbox_lock_mailbox - Lock a mailbox
 * @param ctx   Context to lock
 * @param excl  Exclusive lock?
 * @param retry Should retry if unable to lock?
 */
static int mbox_lock_mailbox(struct Context *ctx, int excl, int retry)
{
  int r;

  r = mutt_lock_file(ctx->path, fileno(ctx->fp), excl, retry);
  if (r == 0)
    ctx->locked = true;
  else if (retry && !excl)
  {
    ctx->readonly = true;
    return 0;
  }

  return r;
}

static void mbox_unlock_mailbox(struct Context *ctx)
{
  if (ctx->locked)
  {
    fflush(ctx->fp);

    mutt_unlock_file(ctx->path, fileno(ctx->fp));
    ctx->locked = false;
  }
}

static int mmdf_parse_mailbox(struct Context *ctx)
{
  char buf[HUGE_STRING];
  char return_path[LONG_STRING];
  int count = 0, oldmsgcount = ctx->msgcount;
  int lines;
  time_t t;
  LOFF_T loc, tmploc;
  struct Header *hdr = NULL;
  struct stat sb;
  struct Progress progress;
  char msgbuf[STRING];

  if (stat(ctx->path, &sb) == -1)
  {
    mutt_perror(ctx->path);
    return -1;
  }
  ctx->atime = sb.st_atime;
  ctx->mtime = sb.st_mtime;
  ctx->size = sb.st_size;

  buf[sizeof(buf) - 1] = '\0';

  if (!ctx->quiet)
  {
    snprintf(msgbuf, sizeof(msgbuf), _("Reading %s..."), ctx->path);
    mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, ReadInc, 0);
  }

  while (true)
  {
    if (fgets(buf, sizeof(buf) - 1, ctx->fp) == NULL)
      break;

    if (SigInt == 1)
      break;

    if (mutt_strcmp(buf, MMDF_SEP) == 0)
    {
      loc = ftello(ctx->fp);
      if (loc < 0)
        return -1;

      count++;
      if (!ctx->quiet)
        mutt_progress_update(&progress, count, (int) (loc / (ctx->size / 100 + 1)));

      if (ctx->msgcount == ctx->hdrmax)
        mx_alloc_memory(ctx);
      ctx->hdrs[ctx->msgcount] = hdr = mutt_new_header();
      hdr->offset = loc;
      hdr->index = ctx->msgcount;

      if (fgets(buf, sizeof(buf) - 1, ctx->fp) == NULL)
      {
        /* TODO: memory leak??? */
        mutt_debug(1, "mmdf_parse_mailbox: unexpected EOF\n");
        break;
      }

      return_path[0] = '\0';

      if (!is_from(buf, return_path, sizeof(return_path), &t))
      {
        if (fseeko(ctx->fp, loc, SEEK_SET) != 0)
        {
          mutt_debug(1, "mmdf_parse_mailbox: fseek() failed\n");
          mutt_error(_("Mailbox is corrupt!"));
          return -1;
        }
      }
      else
        hdr->received = t - mutt_local_tz(t);

      hdr->env = mutt_read_rfc822_header(ctx->fp, hdr, 0, 0);

      loc = ftello(ctx->fp);
      if (loc < 0)
        return -1;

      if (hdr->content->length > 0 && hdr->lines > 0)
      {
        tmploc = loc + hdr->content->length;

        if (0 < tmploc && tmploc < ctx->size)
        {
          if (fseeko(ctx->fp, tmploc, SEEK_SET) != 0 ||
              fgets(buf, sizeof(buf) - 1, ctx->fp) == NULL ||
              (mutt_strcmp(MMDF_SEP, buf) != 0))
          {
            if (fseeko(ctx->fp, loc, SEEK_SET) != 0)
              mutt_debug(1, "mmdf_parse_mailbox: fseek() failed\n");
            hdr->content->length = -1;
          }
        }
        else
          hdr->content->length = -1;
      }
      else
        hdr->content->length = -1;

      if (hdr->content->length < 0)
      {
        lines = -1;
        do
        {
          loc = ftello(ctx->fp);
          if (loc < 0)
            return -1;
          if (fgets(buf, sizeof(buf) - 1, ctx->fp) == NULL)
            break;
          lines++;
        } while (mutt_strcmp(buf, MMDF_SEP) != 0);

        hdr->lines = lines;
        hdr->content->length = loc - hdr->content->offset;
      }

      if (!hdr->env->return_path && return_path[0])
        hdr->env->return_path = rfc822_parse_adrlist(hdr->env->return_path, return_path);

      if (!hdr->env->from)
        hdr->env->from = rfc822_cpy_adr(hdr->env->return_path, 0);

      ctx->msgcount++;
    }
    else
    {
      mutt_debug(1, "mmdf_parse_mailbox: corrupt mailbox!\n");
      mutt_error(_("Mailbox is corrupt!"));
      return -1;
    }
  }

  if (ctx->msgcount > oldmsgcount)
    mx_update_context(ctx, ctx->msgcount - oldmsgcount);

  if (SigInt == 1)
  {
    SigInt = 0;
    return -2; /* action aborted */
  }

  return 0;
}

/**
 * mbox_parse_mailbox - Read a mailbox from disk
 *
 * Note that this function is also called when new mail is appended to the
 * currently open folder, and NOT just when the mailbox is initially read.
 *
 * NOTE: it is assumed that the mailbox being read has been locked before this
 * routine gets called.  Strange things could happen if it's not!
 */
static int mbox_parse_mailbox(struct Context *ctx)
{
  struct stat sb;
  char buf[HUGE_STRING], return_path[STRING];
  struct Header *curhdr = NULL;
  time_t t;
  int count = 0, lines = 0;
  LOFF_T loc;
  struct Progress progress;
  char msgbuf[STRING];

  /* Save information about the folder at the time we opened it. */
  if (stat(ctx->path, &sb) == -1)
  {
    mutt_perror(ctx->path);
    return -1;
  }

  ctx->size = sb.st_size;
  ctx->mtime = sb.st_mtime;
  ctx->atime = sb.st_atime;

  if (!ctx->readonly)
    ctx->readonly = access(ctx->path, W_OK) ? true : false;

  if (!ctx->quiet)
  {
    snprintf(msgbuf, sizeof(msgbuf), _("Reading %s..."), ctx->path);
    mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, ReadInc, 0);
  }

  loc = ftello(ctx->fp);
  while ((fgets(buf, sizeof(buf), ctx->fp) != NULL) && (SigInt != 1))
  {
    if (is_from(buf, return_path, sizeof(return_path), &t))
    {
      /* Save the Content-Length of the previous message */
      if (count > 0)
      {
        struct Header *h = ctx->hdrs[ctx->msgcount - 1];
        if (h->content->length < 0)
        {
          h->content->length = loc - h->content->offset - 1;
          if (h->content->length < 0)
            h->content->length = 0;
        }
        if (!h->lines)
          h->lines = lines ? lines - 1 : 0;
      }

      count++;

      if (!ctx->quiet)
        mutt_progress_update(&progress, count,
                             (int) (ftello(ctx->fp) / (ctx->size / 100 + 1)));

      if (ctx->msgcount == ctx->hdrmax)
        mx_alloc_memory(ctx);

      curhdr = ctx->hdrs[ctx->msgcount] = mutt_new_header();
      curhdr->received = t - mutt_local_tz(t);
      curhdr->offset = loc;
      curhdr->index = ctx->msgcount;

      curhdr->env = mutt_read_rfc822_header(ctx->fp, curhdr, 0, 0);

      /* if we know how long this message is, either just skip over the body,
       * or if we don't know how many lines there are, count them now (this will
       * save time by not having to search for the next message marker).
       */
      if (curhdr->content->length > 0)
      {
        LOFF_T tmploc;

        loc = ftello(ctx->fp);
        tmploc = loc + curhdr->content->length + 1;

        if (0 < tmploc && tmploc < ctx->size)
        {
          /*
           * check to see if the content-length looks valid.  we expect to
           * to see a valid message separator at this point in the stream
           */
          if (fseeko(ctx->fp, tmploc, SEEK_SET) != 0 ||
              fgets(buf, sizeof(buf), ctx->fp) == NULL ||
              (mutt_strncmp("From ", buf, 5) != 0))
          {
            mutt_debug(1, "mbox_parse_mailbox: bad content-length in message "
                          "%d (cl=" OFF_T_FMT ")\n",
                       curhdr->index, curhdr->content->length);
            mutt_debug(1, "\tLINE: %s", buf);
            /* nope, return the previous position */
            if ((loc < 0) || (fseeko(ctx->fp, loc, SEEK_SET) != 0))
            {
              mutt_debug(1, "mbox_parse_mailbox: fseek() failed\n");
            }
            curhdr->content->length = -1;
          }
        }
        else if (tmploc != ctx->size)
        {
          /* content-length would put us past the end of the file, so it
           * must be wrong
           */
          curhdr->content->length = -1;
        }

        if (curhdr->content->length != -1)
        {
          /* good content-length.  check to see if we know how many lines
           * are in this message.
           */
          if (curhdr->lines == 0)
          {
            int cl = curhdr->content->length;

            /* count the number of lines in this message */
            if ((loc < 0) || (fseeko(ctx->fp, loc, SEEK_SET) != 0))
              mutt_debug(1, "mbox_parse_mailbox: fseek() failed\n");
            while (cl-- > 0)
            {
              if (fgetc(ctx->fp) == '\n')
                curhdr->lines++;
            }
          }

          /* return to the offset of the next message separator */
          if (fseeko(ctx->fp, tmploc, SEEK_SET) != 0)
            mutt_debug(1, "mbox_parse_mailbox: fseek() failed\n");
        }
      }

      ctx->msgcount++;

      if (!curhdr->env->return_path && return_path[0])
        curhdr->env->return_path =
            rfc822_parse_adrlist(curhdr->env->return_path, return_path);

      if (!curhdr->env->from)
        curhdr->env->from = rfc822_cpy_adr(curhdr->env->return_path, 0);

      lines = 0;
    }
    else
      lines++;

    loc = ftello(ctx->fp);
  }

  /*
   * Only set the content-length of the previous message if we have read more
   * than one message during _this_ invocation.  If this routine is called
   * when new mail is received, we need to make sure not to clobber what
   * previously was the last message since the headers may be sorted.
   */
  if (count > 0)
  {
    struct Header *h = ctx->hdrs[ctx->msgcount - 1];
    if (h->content->length < 0)
    {
      h->content->length = ftello(ctx->fp) - h->content->offset - 1;
      if (h->content->length < 0)
        h->content->length = 0;
    }

    if (!h->lines)
      h->lines = lines ? lines - 1 : 0;

    mx_update_context(ctx, count);
  }

  if (SigInt == 1)
  {
    SigInt = 0;
    return -2; /* action aborted */
  }

  return 0;
}

/**
 * mbox_open_mailbox - open a mbox or mmdf style mailbox
 */
static int mbox_open_mailbox(struct Context *ctx)
{
  int rc;

  ctx->fp = fopen(ctx->path, "r");
  if (!ctx->fp)
  {
    mutt_perror(ctx->path);
    return -1;
  }
  mutt_block_signals();
  if (mbox_lock_mailbox(ctx, 0, 1) == -1)
  {
    mutt_unblock_signals();
    return -1;
  }

  if (ctx->magic == MUTT_MBOX)
    rc = mbox_parse_mailbox(ctx);
  else if (ctx->magic == MUTT_MMDF)
    rc = mmdf_parse_mailbox(ctx);
  else
    rc = -1;
  mutt_touch_atime(fileno(ctx->fp));

  mbox_unlock_mailbox(ctx);
  mutt_unblock_signals();
  return rc;
}

static int mbox_open_mailbox_append(struct Context *ctx, int flags)
{
  ctx->fp = safe_fopen(ctx->path, flags & MUTT_NEWFOLDER ? "w" : "a");
  if (!ctx->fp)
  {
    mutt_perror(ctx->path);
    return -1;
  }

  if (mbox_lock_mailbox(ctx, 1, 1) != 0)
  {
    mutt_error(_("Couldn't lock %s\n"), ctx->path);
    safe_fclose(&ctx->fp);
    return -1;
  }

  fseek(ctx->fp, 0, SEEK_END);

  return 0;
}

static int mbox_close_mailbox(struct Context *ctx)
{
  if (!ctx->fp)
  {
    return 0;
  }

  if (ctx->append)
  {
    mutt_unlock_file(ctx->path, fileno(ctx->fp));
    mutt_unblock_signals();
  }

  safe_fclose(&ctx->fp);

  return 0;
}

static int mbox_open_message(struct Context *ctx, struct Message *msg, int msgno)
{
  msg->fp = ctx->fp;

  return 0;
}

static int mbox_close_message(struct Context *ctx, struct Message *msg)
{
  msg->fp = NULL;

  return 0;
}

static int mbox_commit_message(struct Context *ctx, struct Message *msg)
{
  if (fputc('\n', msg->fp) == EOF)
    return -1;

  if ((fflush(msg->fp) == EOF) || (fsync(fileno(msg->fp)) == -1))
  {
    mutt_perror(_("Can't write message"));
    return -1;
  }

  return 0;
}

static int mmdf_commit_message(struct Context *ctx, struct Message *msg)
{
  if (fputs(MMDF_SEP, msg->fp) == EOF)
    return -1;

  if ((fflush(msg->fp) == EOF) || (fsync(fileno(msg->fp)) == -1))
  {
    mutt_perror(_("Can't write message"));
    return -1;
  }

  return 0;
}

static int mbox_open_new_message(struct Message *msg, struct Context *dest, struct Header *hdr)
{
  msg->fp = dest->fp;
  return 0;
}

/**
 * strict_addrcmp - Strictly compare two address list
 * @retval 1 if address lists are strictly identical
 */
static int strict_addrcmp(const struct Address *a, const struct Address *b)
{
  while (a && b)
  {
    if ((mutt_strcmp(a->mailbox, b->mailbox) != 0) ||
        (mutt_strcmp(a->personal, b->personal) != 0))
      return 0;

    a = a->next;
    b = b->next;
  }
  if (a || b)
    return 0;

  return 1;
}

static int strict_cmp_stailq(const struct ListHead *ah, const struct ListHead *bh)
{
  struct ListNode *a = STAILQ_FIRST(ah);
  struct ListNode *b = STAILQ_FIRST(bh);

  while (a && b)
  {
    if (mutt_strcmp(a->data, b->data) != 0)
      return 0;

    a = STAILQ_NEXT(a, entries);
    b = STAILQ_NEXT(b, entries);
  }
  if (a || b)
    return 0;

  return 1;
}

static int strict_cmp_envelopes(const struct Envelope *e1, const struct Envelope *e2)
{
  if (e1 && e2)
  {
    if ((mutt_strcmp(e1->message_id, e2->message_id) != 0) ||
        (mutt_strcmp(e1->subject, e2->subject) != 0) ||
        !strict_cmp_stailq(&e1->references, &e2->references) ||
        !strict_addrcmp(e1->from, e2->from) || !strict_addrcmp(e1->sender, e2->sender) ||
        !strict_addrcmp(e1->reply_to, e2->reply_to) ||
        !strict_addrcmp(e1->to, e2->to) || !strict_addrcmp(e1->cc, e2->cc) ||
        !strict_addrcmp(e1->return_path, e2->return_path))
      return 0;
    else
      return 1;
  }
  else
  {
    if (!e1 && !e2)
      return 1;
    else
      return 0;
  }
}

static int strict_cmp_parameters(const struct Parameter *p1, const struct Parameter *p2)
{
  while (p1 && p2)
  {
    if ((mutt_strcmp(p1->attribute, p2->attribute) != 0) ||
        (mutt_strcmp(p1->value, p2->value) != 0))
      return 0;

    p1 = p1->next;
    p2 = p2->next;
  }
  if (p1 || p2)
    return 0;

  return 1;
}

static int strict_cmp_bodies(const struct Body *b1, const struct Body *b2)
{
  if (b1->type != b2->type || b1->encoding != b2->encoding ||
      (mutt_strcmp(b1->subtype, b2->subtype) != 0) ||
      (mutt_strcmp(b1->description, b2->description) != 0) ||
      !strict_cmp_parameters(b1->parameter, b2->parameter) || b1->length != b2->length)
    return 0;
  return 1;
}

/**
 * mbox_strict_cmp_headers - Strictly compare message headers
 * @retval 1 if headers are strictly identical
 */
int mbox_strict_cmp_headers(const struct Header *h1, const struct Header *h2)
{
  if (h1 && h2)
  {
    if (h1->received != h2->received || h1->date_sent != h2->date_sent ||
        h1->content->length != h2->content->length || h1->lines != h2->lines ||
        h1->zhours != h2->zhours || h1->zminutes != h2->zminutes ||
        h1->zoccident != h2->zoccident || h1->mime != h2->mime ||
        !strict_cmp_envelopes(h1->env, h2->env) ||
        !strict_cmp_bodies(h1->content, h2->content))
      return 0;
    else
      return 1;
  }
  else
  {
    if (!h1 && !h2)
      return 1;
    else
      return 0;
  }
}

static int reopen_mailbox(struct Context *ctx, int *index_hint)
{
  int (*cmp_headers)(const struct Header *, const struct Header *) = NULL;
  struct Header **old_hdrs = NULL;
  int old_msgcount;
  bool msg_mod = false;
  bool index_hint_set;
  int i, j;
  int rc = -1;

  /* silent operations */
  ctx->quiet = true;

  if (!ctx->quiet)
    mutt_message(_("Reopening mailbox..."));

  /* our heuristics require the old mailbox to be unsorted */
  if (Sort != SORT_ORDER)
  {
    short old_sort;

    old_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers(ctx, 1);
    Sort = old_sort;
  }

  old_hdrs = NULL;
  old_msgcount = 0;

  /* simulate a close */
  if (ctx->id_hash)
    hash_destroy(&ctx->id_hash, NULL);
  if (ctx->subj_hash)
    hash_destroy(&ctx->subj_hash, NULL);
  hash_destroy(&ctx->label_hash, NULL);
  mutt_clear_threads(ctx);
  FREE(&ctx->v2r);
  if (ctx->readonly)
  {
    for (i = 0; i < ctx->msgcount; i++)
      mutt_free_header(&(ctx->hdrs[i])); /* nothing to do! */
    FREE(&ctx->hdrs);
  }
  else
  {
    /* save the old headers */
    old_msgcount = ctx->msgcount;
    old_hdrs = ctx->hdrs;
    ctx->hdrs = NULL;
  }

  ctx->hdrmax = 0; /* force allocation of new headers */
  ctx->msgcount = 0;
  ctx->vcount = 0;
  ctx->tagged = 0;
  ctx->deleted = 0;
  ctx->new = 0;
  ctx->unread = 0;
  ctx->flagged = 0;
  ctx->changed = false;
  ctx->id_hash = NULL;
  ctx->subj_hash = NULL;
  mutt_make_label_hash(ctx);

  switch (ctx->magic)
  {
    case MUTT_MBOX:
    case MUTT_MMDF:
      cmp_headers = mbox_strict_cmp_headers;
      safe_fclose(&ctx->fp);
      ctx->fp = safe_fopen(ctx->path, "r");
      if (!ctx->fp)
        rc = -1;
      else
        rc = ((ctx->magic == MUTT_MBOX) ? mbox_parse_mailbox : mmdf_parse_mailbox)(ctx);
      break;

    default:
      rc = -1;
      break;
  }

  if (rc == -1)
  {
    /* free the old headers */
    for (j = 0; j < old_msgcount; j++)
      mutt_free_header(&(old_hdrs[j]));
    FREE(&old_hdrs);

    ctx->quiet = false;
    return -1;
  }

  mutt_touch_atime(fileno(ctx->fp));

  /* now try to recover the old flags */

  index_hint_set = (index_hint == NULL);

  if (!ctx->readonly)
  {
    for (i = 0; i < ctx->msgcount; i++)
    {
      bool found = false;

      /* some messages have been deleted, and new  messages have been
       * appended at the end; the heuristic is that old messages have then
       * "advanced" towards the beginning of the folder, so we begin the
       * search at index "i"
       */
      for (j = i; j < old_msgcount; j++)
      {
        if (!old_hdrs[j])
          continue;
        if (cmp_headers(ctx->hdrs[i], old_hdrs[j]))
        {
          found = true;
          break;
        }
      }
      if (!found)
      {
        for (j = 0; j < i && j < old_msgcount; j++)
        {
          if (!old_hdrs[j])
            continue;
          if (cmp_headers(ctx->hdrs[i], old_hdrs[j]))
          {
            found = true;
            break;
          }
        }
      }

      if (found)
      {
        /* this is best done here */
        if (!index_hint_set && *index_hint == j)
          *index_hint = i;

        if (old_hdrs[j]->changed)
        {
          /* Only update the flags if the old header was changed;
           * otherwise, the header may have been modified externally,
           * and we don't want to lose _those_ changes
           */
          mutt_set_flag(ctx, ctx->hdrs[i], MUTT_FLAG, old_hdrs[j]->flagged);
          mutt_set_flag(ctx, ctx->hdrs[i], MUTT_REPLIED, old_hdrs[j]->replied);
          mutt_set_flag(ctx, ctx->hdrs[i], MUTT_OLD, old_hdrs[j]->old);
          mutt_set_flag(ctx, ctx->hdrs[i], MUTT_READ, old_hdrs[j]->read);
        }
        mutt_set_flag(ctx, ctx->hdrs[i], MUTT_DELETE, old_hdrs[j]->deleted);
        mutt_set_flag(ctx, ctx->hdrs[i], MUTT_PURGE, old_hdrs[j]->purge);
        mutt_set_flag(ctx, ctx->hdrs[i], MUTT_TAG, old_hdrs[j]->tagged);

        /* we don't need this header any more */
        mutt_free_header(&(old_hdrs[j]));
      }
    }

    /* free the remaining old headers */
    for (j = 0; j < old_msgcount; j++)
    {
      if (old_hdrs[j])
      {
        mutt_free_header(&(old_hdrs[j]));
        msg_mod = true;
      }
    }
    FREE(&old_hdrs);
  }

  ctx->quiet = false;

  return ((ctx->changed || msg_mod) ? MUTT_REOPENED : MUTT_NEW_MAIL);
}

/**
 * mbox_check_mailbox - Has mailbox changed on disk
 * @param[in]  ctx        Context
 * @param[out] index_hint Keep track of current index selection
 * @retval #MUTT_REOPENED  Mailbox has been reopened
 * @retval #MUTT_NEW_MAIL  New mail has arrived
 * @retval #MUTT_LOCKED    Couldn't lock the file
 * @retval 0               No change
 * @retval -1              Error
 */
static int mbox_check_mailbox(struct Context *ctx, int *index_hint)
{
  struct stat st;
  char buffer[LONG_STRING];
  bool unlock = false;
  bool modified = false;

  if (stat(ctx->path, &st) == 0)
  {
    if (st.st_mtime == ctx->mtime && st.st_size == ctx->size)
      return 0;

    if (st.st_size == ctx->size)
    {
      /* the file was touched, but it is still the same length, so just exit */
      ctx->mtime = st.st_mtime;
      return 0;
    }

    if (st.st_size > ctx->size)
    {
      /* lock the file if it isn't already */
      if (!ctx->locked)
      {
        mutt_block_signals();
        if (mbox_lock_mailbox(ctx, 0, 0) == -1)
        {
          mutt_unblock_signals();
          /* we couldn't lock the mailbox, but nothing serious happened:
           * probably the new mail arrived: no reason to wait till we can
           * parse it: we'll get it on the next pass
           */
          return MUTT_LOCKED;
        }
        unlock = 1;
      }

      /*
       * Check to make sure that the only change to the mailbox is that
       * message(s) were appended to this file.  My heuristic is that we should
       * see the message separator at *exactly* what used to be the end of the
       * folder.
       */
      if (fseeko(ctx->fp, ctx->size, SEEK_SET) != 0)
        mutt_debug(1, "mbox_check_mailbox: fseek() failed\n");
      if (fgets(buffer, sizeof(buffer), ctx->fp) != NULL)
      {
        if ((ctx->magic == MUTT_MBOX && (mutt_strncmp("From ", buffer, 5) == 0)) ||
            (ctx->magic == MUTT_MMDF && (mutt_strcmp(MMDF_SEP, buffer) == 0)))
        {
          if (fseeko(ctx->fp, ctx->size, SEEK_SET) != 0)
            mutt_debug(1, "mbox_check_mailbox: fseek() failed\n");
          if (ctx->magic == MUTT_MBOX)
            mbox_parse_mailbox(ctx);
          else
            mmdf_parse_mailbox(ctx);

          /* Only unlock the folder if it was locked inside of this routine.
           * It may have been locked elsewhere, like in
           * mutt_checkpoint_mailbox().
           */

          if (unlock)
          {
            mbox_unlock_mailbox(ctx);
            mutt_unblock_signals();
          }

          return MUTT_NEW_MAIL; /* signal that new mail arrived */
        }
        else
          modified = true;
      }
      else
      {
        mutt_debug(1, "mbox_check_mailbox: fgets returned NULL.\n");
        modified = true;
      }
    }
    else
      modified = true;
  }

  if (modified)
  {
    if (reopen_mailbox(ctx, index_hint) != -1)
    {
      if (unlock)
      {
        mbox_unlock_mailbox(ctx);
        mutt_unblock_signals();
      }
      return MUTT_REOPENED;
    }
  }

  /* fatal error */

  mbox_unlock_mailbox(ctx);
  mx_fastclose_mailbox(ctx);
  mutt_unblock_signals();
  mutt_error(_("Mailbox was corrupted!"));
  return -1;
}

/**
 * mbox_has_new - Does the mailbox have new mail
 * @param ctx Context
 * @retval true if the mailbox has at least 1 new messages (not old)
 * @retval false otherwise
 */
static bool mbox_has_new(struct Context *ctx)
{
  for (int i = 0; i < ctx->msgcount; i++)
    if (!ctx->hdrs[i]->deleted && !ctx->hdrs[i]->read && !ctx->hdrs[i]->old)
      return true;
  return false;
}

/**
 * mbox_reset_atime - Reset the access time on the mailbox file
 *
 * if mailbox has at least 1 new message, sets mtime > atime of mailbox so
 * buffy check reports new mail
 */
void mbox_reset_atime(struct Context *ctx, struct stat *st)
{
  struct utimbuf utimebuf;
  struct stat _st;

  if (!st)
  {
    if (stat(ctx->path, &_st) < 0)
      return;
    st = &_st;
  }

  utimebuf.actime = st->st_atime;
  utimebuf.modtime = st->st_mtime;

  /*
   * When $mbox_check_recent is set, existing new mail is ignored, so do not
   * reset the atime to mtime-1 to signal new mail.
   */
  if (!option(OPT_MAIL_CHECK_RECENT) && utimebuf.actime >= utimebuf.modtime &&
      mbox_has_new(ctx))
    utimebuf.actime = utimebuf.modtime - 1;

  utime(ctx->path, &utimebuf);
}

/**
 * mbox_sync_mailbox - Sync a mailbox to disk
 * @retval  0 Success
 * @retval -1 Failure
 */
static int mbox_sync_mailbox(struct Context *ctx, int *index_hint)
{
  char tempfile[_POSIX_PATH_MAX];
  char buf[32];
  int i, j, save_sort = SORT_ORDER;
  int rc = -1;
  int need_sort = 0; /* flag to resort mailbox if new mail arrives */
  int first = -1;    /* first message to be written */
  LOFF_T offset;     /* location in mailbox to write changed messages */
  struct stat statbuf;
  struct MUpdate *newOffset = NULL;
  struct MUpdate *oldOffset = NULL;
  FILE *fp = NULL;
  struct Progress progress;
  char msgbuf[STRING];
  struct Buffy *tmp = NULL;

  /* sort message by their position in the mailbox on disk */
  if (Sort != SORT_ORDER)
  {
    save_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers(ctx, 0);
    Sort = save_sort;
    need_sort = 1;
  }

  /* need to open the file for writing in such a way that it does not truncate
   * the file, so use read-write mode.
   */
  ctx->fp = freopen(ctx->path, "r+", ctx->fp);
  if (!ctx->fp)
  {
    mx_fastclose_mailbox(ctx);
    mutt_error(_("Fatal error!  Could not reopen mailbox!"));
    return -1;
  }

  mutt_block_signals();

  if (mbox_lock_mailbox(ctx, 1, 1) == -1)
  {
    mutt_unblock_signals();
    mutt_error(_("Unable to lock mailbox!"));
    goto bail;
  }

  /* Check to make sure that the file hasn't changed on disk */
  if ((i = mbox_check_mailbox(ctx, index_hint)) == MUTT_NEW_MAIL || i == MUTT_REOPENED)
  {
    /* new mail arrived, or mailbox reopened */
    need_sort = i;
    rc = i;
    goto bail;
  }
  else if (i < 0)
    /* fatal error */
    return -1;

  /* Create a temporary file to write the new version of the mailbox in. */
  mutt_mktemp(tempfile, sizeof(tempfile));
  if ((i = open(tempfile, O_WRONLY | O_EXCL | O_CREAT, 0600)) == -1 ||
      (fp = fdopen(i, "w")) == NULL)
  {
    if (-1 != i)
    {
      close(i);
      unlink(tempfile);
    }
    mutt_error(_("Could not create temporary file!"));
    mutt_sleep(5);
    goto bail;
  }

  /* find the first deleted/changed message.  we save a lot of time by only
   * rewriting the mailbox from the point where it has actually changed.
   */
  for (i = 0; i < ctx->msgcount && !ctx->hdrs[i]->deleted &&
              !ctx->hdrs[i]->changed && !ctx->hdrs[i]->attach_del;
       i++)
    ;
  if (i == ctx->msgcount)
  {
    /* this means ctx->changed or ctx->deleted was set, but no
     * messages were found to be changed or deleted.  This should
     * never happen, is we presume it is a bug in neomutt.
     */
    mutt_error(
        _("sync: mbox modified, but no modified messages! (report this bug)"));
    mutt_sleep(5); /* the mutt_error /will/ get cleared! */
    mutt_debug(1, "mbox_sync_mailbox(): no modified messages.\n");
    unlink(tempfile);
    goto bail;
  }

  /* save the index of the first changed/deleted message */
  first = i;
  /* where to start overwriting */
  offset = ctx->hdrs[i]->offset;

  /* the offset stored in the header does not include the MMDF_SEP, so make
   * sure we seek to the correct location
   */
  if (ctx->magic == MUTT_MMDF)
    offset -= (sizeof(MMDF_SEP) - 1);

  /* allocate space for the new offsets */
  newOffset = safe_calloc(ctx->msgcount - first, sizeof(struct MUpdate));
  oldOffset = safe_calloc(ctx->msgcount - first, sizeof(struct MUpdate));

  if (!ctx->quiet)
  {
    snprintf(msgbuf, sizeof(msgbuf), _("Writing %s..."), ctx->path);
    mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, WriteInc, ctx->msgcount);
  }

  for (i = first, j = 0; i < ctx->msgcount; i++)
  {
    if (!ctx->quiet)
      mutt_progress_update(&progress, i, (int) (ftello(ctx->fp) / (ctx->size / 100 + 1)));
    /*
     * back up some information which is needed to restore offsets when
     * something fails.
     */

    oldOffset[i - first].valid = 1;
    oldOffset[i - first].hdr = ctx->hdrs[i]->offset;
    oldOffset[i - first].body = ctx->hdrs[i]->content->offset;
    oldOffset[i - first].lines = ctx->hdrs[i]->lines;
    oldOffset[i - first].length = ctx->hdrs[i]->content->length;

    if (!ctx->hdrs[i]->deleted)
    {
      j++;

      if (ctx->magic == MUTT_MMDF)
      {
        if (fputs(MMDF_SEP, fp) == EOF)
        {
          mutt_perror(tempfile);
          mutt_sleep(5);
          unlink(tempfile);
          goto bail;
        }
      }

      /* save the new offset for this message.  we add `offset' because the
       * temporary file only contains saved message which are located after
       * `offset' in the real mailbox
       */
      newOffset[i - first].hdr = ftello(fp) + offset;

      if (mutt_copy_message(fp, ctx, ctx->hdrs[i], MUTT_CM_UPDATE,
                            CH_FROM | CH_UPDATE | CH_UPDATE_LEN) != 0)
      {
        mutt_perror(tempfile);
        mutt_sleep(5);
        unlink(tempfile);
        goto bail;
      }

      /* Since messages could have been deleted, the offsets stored in memory
       * will be wrong, so update what we can, which is the offset of this
       * message, and the offset of the body.  If this is a multipart message,
       * we just flush the in memory cache so that the message will be reparsed
       * if the user accesses it later.
       */
      newOffset[i - first].body = ftello(fp) - ctx->hdrs[i]->content->length + offset;
      mutt_free_body(&ctx->hdrs[i]->content->parts);

      switch (ctx->magic)
      {
        case MUTT_MMDF:
          if (fputs(MMDF_SEP, fp) == EOF)
          {
            mutt_perror(tempfile);
            mutt_sleep(5);
            unlink(tempfile);
            goto bail;
          }
          break;
        default:
          if (fputs("\n", fp) == EOF)
          {
            mutt_perror(tempfile);
            mutt_sleep(5);
            unlink(tempfile);
            goto bail;
          }
      }
    }
  }

  if (fclose(fp) != 0)
  {
    fp = NULL;
    mutt_debug(1, "mbox_sync_mailbox: safe_fclose (&) returned non-zero.\n");
    unlink(tempfile);
    mutt_perror(tempfile);
    mutt_sleep(5);
    goto bail;
  }
  fp = NULL;

  /* Save the state of this folder. */
  if (stat(ctx->path, &statbuf) == -1)
  {
    mutt_perror(ctx->path);
    mutt_sleep(5);
    unlink(tempfile);
    goto bail;
  }

  fp = fopen(tempfile, "r");
  if (!fp)
  {
    mutt_unblock_signals();
    mx_fastclose_mailbox(ctx);
    mutt_debug(1,
               "mbox_sync_mailbox: unable to reopen temp copy of mailbox!\n");
    mutt_perror(tempfile);
    mutt_sleep(5);
    FREE(&newOffset);
    FREE(&oldOffset);
    return -1;
  }

  if (fseeko(ctx->fp, offset, SEEK_SET) != 0 || /* seek the append location */
      /* do a sanity check to make sure the mailbox looks ok */
      fgets(buf, sizeof(buf), ctx->fp) == NULL ||
      (ctx->magic == MUTT_MBOX && (mutt_strncmp("From ", buf, 5) != 0)) ||
      (ctx->magic == MUTT_MMDF && (mutt_strcmp(MMDF_SEP, buf) != 0)))
  {
    mutt_debug(1, "mbox_sync_mailbox: message not in expected position.\n");
    mutt_debug(1, "\tLINE: %s\n", buf);
    i = -1;
  }
  else
  {
    if (fseeko(ctx->fp, offset, SEEK_SET) != 0) /* return to proper offset */
    {
      i = -1;
      mutt_debug(1, "mbox_sync_mailbox: fseek() failed\n");
    }
    else
    {
      /* copy the temp mailbox back into place starting at the first
       * change/deleted message
       */
      if (!ctx->quiet)
        mutt_message(_("Committing changes..."));
      i = mutt_copy_stream(fp, ctx->fp);

      if (ferror(ctx->fp))
        i = -1;
    }
    if (i == 0)
    {
      ctx->size = ftello(ctx->fp); /* update the size of the mailbox */
      if ((ctx->size < 0) || (ftruncate(fileno(ctx->fp), ctx->size) != 0))
      {
        i = -1;
        mutt_debug(1, "mbox_sync_mailbox: ftruncate() failed\n");
      }
    }
  }

  safe_fclose(&fp);
  fp = NULL;
  mbox_unlock_mailbox(ctx);

  if (safe_fclose(&ctx->fp) != 0 || i == -1)
  {
    /* error occurred while writing the mailbox back, so keep the temp copy
     * around
     */

    char savefile[_POSIX_PATH_MAX];

    snprintf(savefile, sizeof(savefile), "%s/mutt.%s-%s-%u", NONULL(Tmpdir),
             NONULL(Username), NONULL(ShortHostname), (unsigned int) getpid());
    rename(tempfile, savefile);
    mutt_unblock_signals();
    mx_fastclose_mailbox(ctx);
    mutt_pretty_mailbox(savefile, sizeof(savefile));
    mutt_error(_("Write failed!  Saved partial mailbox to %s"), savefile);
    mutt_sleep(5);
    FREE(&newOffset);
    FREE(&oldOffset);
    return -1;
  }

  /* Restore the previous access/modification times */
  mbox_reset_atime(ctx, &statbuf);

  /* reopen the mailbox in read-only mode */
  ctx->fp = fopen(ctx->path, "r");
  if (!ctx->fp)
  {
    unlink(tempfile);
    mutt_unblock_signals();
    mx_fastclose_mailbox(ctx);
    mutt_error(_("Fatal error!  Could not reopen mailbox!"));
    FREE(&newOffset);
    FREE(&oldOffset);
    return -1;
  }

  /* update the offsets of the rewritten messages */
  for (i = first, j = first; i < ctx->msgcount; i++)
  {
    if (!ctx->hdrs[i]->deleted)
    {
      ctx->hdrs[i]->offset = newOffset[i - first].hdr;
      ctx->hdrs[i]->content->hdr_offset = newOffset[i - first].hdr;
      ctx->hdrs[i]->content->offset = newOffset[i - first].body;
      ctx->hdrs[i]->index = j++;
    }
  }
  FREE(&newOffset);
  FREE(&oldOffset);
  unlink(tempfile); /* remove partial copy of the mailbox */
  mutt_unblock_signals();

  if (option(OPT_CHECK_MBOX_SIZE))
  {
    tmp = mutt_find_mailbox(ctx->path);
    if (tmp && tmp->new == false)
      mutt_update_mailbox(tmp);
  }

  return 0; /* signal success */

bail: /* Come here in case of disaster */

  safe_fclose(&fp);

  /* restore offsets, as far as they are valid */
  if (first >= 0 && oldOffset)
  {
    for (i = first; i < ctx->msgcount && oldOffset[i - first].valid; i++)
    {
      ctx->hdrs[i]->offset = oldOffset[i - first].hdr;
      ctx->hdrs[i]->content->hdr_offset = oldOffset[i - first].hdr;
      ctx->hdrs[i]->content->offset = oldOffset[i - first].body;
      ctx->hdrs[i]->lines = oldOffset[i - first].lines;
      ctx->hdrs[i]->content->length = oldOffset[i - first].length;
    }
  }

  /* this is ok to call even if we haven't locked anything */
  mbox_unlock_mailbox(ctx);

  mutt_unblock_signals();
  FREE(&newOffset);
  FREE(&oldOffset);

  ctx->fp = freopen(ctx->path, "r", ctx->fp);
  if (!ctx->fp)
  {
    mutt_error(_("Could not reopen mailbox!"));
    mx_fastclose_mailbox(ctx);
    return -1;
  }

  if (need_sort)
    /* if the mailbox was reopened, the thread tree will be invalid so make
     * sure to start threading from scratch.  */
    mutt_sort_headers(ctx, (need_sort == MUTT_REOPENED));

  return rc;
}

/**
 * mbox_check_empty - Is the mailbox empty
 * @param path Path to mailbox
 * @retval 1 mailbox is not empty
 * @retval 0 mailbox is empty
 * @retval -1 on error
 */
int mbox_check_empty(const char *path)
{
  struct stat st;

  if (stat(path, &st) == -1)
    return -1;

  return ((st.st_size == 0));
}

struct MxOps mx_mbox_ops = {
  .open = mbox_open_mailbox,
  .open_append = mbox_open_mailbox_append,
  .close = mbox_close_mailbox,
  .open_msg = mbox_open_message,
  .close_msg = mbox_close_message,
  .commit_msg = mbox_commit_message,
  .open_new_msg = mbox_open_new_message,
  .check = mbox_check_mailbox,
  .sync = mbox_sync_mailbox,
  .edit_msg_tags = NULL,
  .commit_msg_tags = NULL,
};

struct MxOps mx_mmdf_ops = {
  .open = mbox_open_mailbox,
  .open_append = mbox_open_mailbox_append,
  .close = mbox_close_mailbox,
  .open_msg = mbox_open_message,
  .close_msg = mbox_close_message,
  .commit_msg = mmdf_commit_message,
  .open_new_msg = mbox_open_new_message,
  .check = mbox_check_mailbox,
  .sync = mbox_sync_mailbox,
  .edit_msg_tags = NULL,
  .commit_msg_tags = NULL,
};
