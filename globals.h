/**
 * @file
 * Hundreds of global variables to back the user variables
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2016 Michael R. Elkins <me@mutt.org>
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

#ifndef _MUTT_GLOBALS_H
#define _MUTT_GLOBALS_H

#include <signal.h>
#include "lib/lib.h"
#include "list.h"
#include "where.h"
#include "mutt_regex.h"

#ifdef MAIN_C
/* so that global vars get included */
#include "git_ver.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "sort.h"
#endif /* MAIN_C */

WHERE struct Context *Context;

WHERE char ErrorBuf[STRING];
WHERE char AttachmentMarker[STRING];

WHERE struct Address *EnvelopeFromAddress;
WHERE struct Address *From;

WHERE char *AliasFile;
WHERE char *AliasFormat;
WHERE char *AssumedCharset;
WHERE char *AttachSep;
WHERE char *Attribution;
WHERE char *AttributionLocale;
WHERE char *AttachCharset;
WHERE char *AttachFormat;
WHERE struct Regex AttachKeyword;
WHERE char *Charset;
WHERE char *ComposeFormat;
WHERE char *ConfigCharset;
WHERE char *ContentType;
WHERE char *DefaultHook;
WHERE char *DateFormat;
WHERE char *DisplayFilter;
WHERE char *DsnNotify;
WHERE char *DsnReturn;
WHERE char *Editor;
WHERE char *EmptySubject;
WHERE char *Escape;
WHERE char *FolderFormat;
WHERE char *ForwardAttributionIntro;
WHERE char *ForwardAttributionTrailer;
WHERE char *ForwardFormat;
WHERE char *Hostname;
WHERE struct MbTable *FromChars;
WHERE char *IndexFormat;
WHERE char *HistoryFile;
WHERE char *HomeDir;
WHERE char *ShortHostname;
#ifdef USE_IMAP
WHERE char *ImapAuthenticators;
WHERE char *ImapDelimChars;
WHERE char *ImapHeaders;
WHERE char *ImapLogin;
WHERE char *ImapPass;
WHERE char *ImapUser;
#endif
WHERE char *Mbox;
WHERE char *Ispell;
WHERE char *MailcapPath;
WHERE char *Folder;
#if defined(USE_IMAP) || defined(USE_POP) || defined(USE_NNTP)
WHERE char *MessageCachedir;
#endif
#ifdef USE_HCACHE
WHERE char *HeaderCache;
WHERE char *HeaderCacheBackend;
#if defined(HAVE_GDBM) || defined(HAVE_BDB)
WHERE char *HeaderCachePageSize;
#endif /* HAVE_GDBM || HAVE_BDB */
#endif /* USE_HCACHE */
WHERE char *MarkMacroPrefix;
WHERE char *MhSeqFlagged;
WHERE char *MhSeqReplied;
WHERE char *MhSeqUnseen;
WHERE char *MimeTypeQueryCommand;
WHERE char *MessageFormat;

#ifdef USE_SOCKET
WHERE char *Preconnect;
WHERE char *Tunnel;
WHERE short NetInc;
#endif /* USE_SOCKET */

#ifdef MIXMASTER
WHERE char *Mixmaster;
WHERE char *MixEntryFormat;
#endif

WHERE struct ListHead Muttrc INITVAL(STAILQ_HEAD_INITIALIZER(Muttrc));
#ifdef USE_NNTP
WHERE char *GroupIndexFormat;
WHERE char *Inews;
WHERE char *NewsCacheDir;
WHERE char *NewsServer;
WHERE char *NewsgroupsCharset;
WHERE char *NewsRc;
WHERE char *NntpAuthenticators;
WHERE char *NntpUser;
WHERE char *NntpPass;
#endif
WHERE char *Record;
WHERE char *Pager;
WHERE char *PagerFormat;
WHERE char *PipeSep;
#ifdef USE_POP
WHERE char *PopAuthenticators;
WHERE short PopCheckinterval;
WHERE char *PopHost;
WHERE char *PopPass;
WHERE char *PopUser;
#endif
WHERE char *PostIndentString;
WHERE char *Postponed;
WHERE char *PostponeEncryptAs;
WHERE char *IndentString;
WHERE char *PrintCommand;
WHERE char *NewMailCommand;
WHERE char *QueryCommand;
WHERE char *QueryFormat;
WHERE char *RealName;
WHERE short SearchContext;
WHERE char *SendCharset;
WHERE char *Sendmail;
WHERE char *Shell;
WHERE char *ShowMultipartAlternative;
#ifdef USE_SIDEBAR
WHERE char *SidebarDelimChars;
WHERE char *SidebarDividerChar;
WHERE char *SidebarFormat;
WHERE char *SidebarIndentString;
#endif
WHERE char *Signature;
WHERE char *SimpleSearch;
#ifdef USE_SMTP
WHERE char *SmtpAuthenticators;
WHERE char *SmtpPass;
WHERE char *SmtpUrl;
#endif /* USE_SMTP */
WHERE char *SpoolFile;
WHERE char *SpamSeparator;
#ifdef USE_SSL
WHERE char *CertificateFile;
WHERE char *SslClientCert;
WHERE char *EntropyFile;
WHERE char *SslCiphers;
#ifdef USE_SSL_GNUTLS
WHERE short SslMinDhPrimeBits;
WHERE char *SslCaCertificatesFile;
#endif
#endif
WHERE struct MbTable *StatusChars;
WHERE char *StatusFormat;
WHERE char *Tmpdir;
WHERE struct MbTable *ToChars;
WHERE struct MbTable *FlagChars;
WHERE char *Trash;
WHERE char *TSStatusFormat;
WHERE char *TSIconFormat;
WHERE short TSSupported;
WHERE char *Username;
WHERE char *Visual;

WHERE char *CurrentFolder;
WHERE char *LastFolder;

WHERE const char *GitVer;

WHERE struct Hash *Groups;
WHERE struct Hash *ReverseAliases;
WHERE char *HiddenTags;
WHERE struct Hash *TagTransforms;
WHERE struct Hash *TagFormats;

WHERE struct ListHead AutoViewList INITVAL(STAILQ_HEAD_INITIALIZER(AutoViewList));
WHERE struct ListHead AlternativeOrderList INITVAL(STAILQ_HEAD_INITIALIZER(AlternativeOrderList));
WHERE struct ListHead AttachAllow INITVAL(STAILQ_HEAD_INITIALIZER(AttachAllow));
WHERE struct ListHead AttachExclude INITVAL(STAILQ_HEAD_INITIALIZER(AttachExclude));
WHERE struct ListHead InlineAllow INITVAL(STAILQ_HEAD_INITIALIZER(InlineAllow));
WHERE struct ListHead InlineExclude INITVAL(STAILQ_HEAD_INITIALIZER(InlineExclude));
WHERE struct ListHead HeaderOrderList INITVAL(STAILQ_HEAD_INITIALIZER(HeaderOrderList));
WHERE struct ListHead Ignore INITVAL(STAILQ_HEAD_INITIALIZER(Ignore));
WHERE struct ListHead MailToAllow INITVAL(STAILQ_HEAD_INITIALIZER(MailToAllow));
WHERE struct ListHead MimeLookupList INITVAL(STAILQ_HEAD_INITIALIZER(MimeLookupList));
WHERE struct ListHead UnIgnore INITVAL(STAILQ_HEAD_INITIALIZER(UnIgnore));

WHERE struct RegexList *Alternates;
WHERE struct RegexList *UnAlternates;
WHERE struct RegexList *MailLists;
WHERE struct RegexList *UnMailLists;
WHERE struct RegexList *SubscribedLists;
WHERE struct RegexList *UnSubscribedLists;
WHERE struct ReplaceList *SpamList;
WHERE struct RegexList *NoSpamList;
WHERE struct ReplaceList *SubjectRegexList;

/* bit vector for the yes/no/ask variable type */
#ifdef MAIN_C
unsigned char QuadOptions[(OPT_QUAD_MAX * 2 + 7) / 8];
#else
extern unsigned char QuadOptions[];
#endif

WHERE unsigned short Counter;

#ifdef USE_NNTP
WHERE short NntpPoll;
WHERE short NntpContext;
#endif

#ifdef DEBUG
WHERE short DebugLevel;
WHERE char *DebugFile;
#endif

WHERE short ConnectTimeout;
WHERE short History;
WHERE short MenuContext;
WHERE short PagerContext;
WHERE short PagerIndexLines;
WHERE short ReadInc;
WHERE short ReflowWrap;
WHERE short SaveHistory;
WHERE short SendmailWait;
WHERE short SleepTime;
WHERE short SkipQuotedOffset;
WHERE short TimeInc;
WHERE short Timeout;
WHERE short Wrap;
WHERE short WrapHeaders;
WHERE short WriteInc;

WHERE short ScoreThresholdDelete;
WHERE short ScoreThresholdRead;
WHERE short ScoreThresholdFlag;

#ifdef USE_SIDEBAR
WHERE short SidebarWidth;
WHERE struct ListHead SidebarWhitelist INITVAL(STAILQ_HEAD_INITIALIZER(SidebarWhitelist));
#endif

#ifdef USE_IMAP
WHERE short ImapKeepalive;
WHERE short ImapPipelineDepth;
WHERE short ImapPollTimeout;
#endif

/* flags for received signals */
WHERE SIG_ATOMIC_VOLATILE_T SigAlrm;
WHERE SIG_ATOMIC_VOLATILE_T SigInt;
WHERE SIG_ATOMIC_VOLATILE_T SigWinch;

WHERE int CurrentMenu;

WHERE struct Alias *Aliases;
WHERE struct ListHead UserHeader INITVAL(STAILQ_HEAD_INITIALIZER(UserHeader));

/* -- formerly in pgp.h -- */
WHERE struct Regex PgpGoodSign;
WHERE struct Regex PgpDecryptionOkay;
WHERE char *PgpSignAs;
WHERE short PgpTimeout;
WHERE char *PgpEntryFormat;
WHERE char *PgpClearSignCommand;
WHERE char *PgpDecodeCommand;
WHERE char *PgpVerifyCommand;
WHERE char *PgpDecryptCommand;
WHERE char *PgpSignCommand;
WHERE char *PgpEncryptSignCommand;
WHERE char *PgpEncryptOnlyCommand;
WHERE char *PgpImportCommand;
WHERE char *PgpExportCommand;
WHERE char *PgpVerifyKeyCommand;
WHERE char *PgpListSecringCommand;
WHERE char *PgpListPubringCommand;
WHERE char *PgpGetkeysCommand;
WHERE char *PgpSelfEncryptAs;

/* -- formerly in smime.h -- */
WHERE char *SmimeDefaultKey;
WHERE short SmimeTimeout;
WHERE char *SmimeCertificates;
WHERE char *SmimeKeys;
WHERE char *SmimeEncryptWith;
WHERE char *SmimeCALocation;
WHERE char *SmimeVerifyCommand;
WHERE char *SmimeVerifyOpaqueCommand;
WHERE char *SmimeDecryptCommand;
WHERE char *SmimeSignCommand;
WHERE char *SmimeSignDigestAlg;
WHERE char *SmimeEncryptCommand;
WHERE char *SmimeGetSignerCertCommand;
WHERE char *SmimePk7outCommand;
WHERE char *SmimeGetCertCommand;
WHERE char *SmimeImportCertCommand;
WHERE char *SmimeGetCertEmailCommand;
WHERE char *SmimeSelfEncryptAs;

#ifdef USE_NOTMUCH
WHERE int NmOpenTimeout;
WHERE char *NmDefaultUri;
WHERE char *NmExcludeTags;
WHERE char *NmUnreadTag;
WHERE char *VfolderFormat;
WHERE int NmDbLimit;
WHERE char *NmQueryType;
WHERE char *NmRecordTags;
WHERE int NmQueryWindowDuration;
WHERE char *NmQueryWindowTimebase;
WHERE int NmQueryWindowCurrentPosition;
WHERE char *NmQueryWindowCurrentSearch;
#endif

#ifdef MAIN_C
const char *const BodyTypes[] = {
    "x-unknown", "audio",     "application", "image", "message",
    "model",     "multipart", "text",        "video", "*",
};
const char *const BodyEncodings[] = {
    "x-unknown", "7bit",   "8bit",        "quoted-printable",
    "base64",    "binary", "x-uuencoded",
};
#endif

#endif /* _MUTT_GLOBALS_H */
