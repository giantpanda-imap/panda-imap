/*
 * Program:	MMDF mail routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	20 December 1989
 * Last Edited:	17 November 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
extern int errno;		/* just in case */
#include <signal.h>
#include "mail.h"
#include "osdep.h"
#include <time.h>
#include <sys/stat.h>
#include "mmdf.h"
#include "pseudo.h"
#include "fdstring.h"
#include "misc.h"
#include "dummy.h"

/* MMDF mail routines */


/* Driver dispatch used by MAIL */

DRIVER mmdfdriver = {
  "mmdf",			/* driver name */
  DR_LOCAL|DR_MAIL,		/* driver flags */
  (DRIVER *) NIL,		/* next driver */
  mmdf_valid,			/* mailbox is valid for us */
  mmdf_parameters,		/* manipulate parameters */
  mmdf_scan,			/* scan mailboxes */
  mmdf_list,			/* list mailboxes */
  mmdf_lsub,			/* list subscribed mailboxes */
  NIL,				/* subscribe to mailbox */
  NIL,				/* unsubscribe from mailbox */
  mmdf_create,			/* create mailbox */
  mmdf_delete,			/* delete mailbox */
  mmdf_rename,			/* rename mailbox */
  NIL,				/* status of mailbox */
  mmdf_open,			/* open mailbox */
  mmdf_close,			/* close mailbox */
  NIL,				/* fetch message "fast" attributes */
  NIL,				/* fetch message flags */
  NIL,				/* fetch overview */
  NIL,				/* fetch message envelopes */
  mmdf_header,			/* fetch message header */
  mmdf_text,			/* fetch message text */
  NIL,				/* fetch partial message text */
  NIL,				/* unique identifier */
  NIL,				/* message number */
  NIL,				/* modify flags */
  mmdf_flagmsg,			/* per-message modify flags */
  NIL,				/* search for message based on criteria */
  NIL,				/* sort messages */
  NIL,				/* thread messages */
  mmdf_ping,			/* ping mailbox to see if still alive */
  mmdf_check,			/* check for new messages */
  mmdf_expunge,			/* expunge deleted messages */
  mmdf_copy,			/* copy messages to another mailbox */
  mmdf_append,			/* append string message to mailbox */
  NIL				/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM mmdfproto = {&mmdfdriver};

char *mmdfhdr = MMDFHDRTXT;	/* MMDF header */

/* MMDF mail validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *mmdf_valid (char *name)
{
  char tmp[MAILTMPLEN];
  return mmdf_isvalid (name,tmp) ? &mmdfdriver : NIL;
}


/* MMDF mail test for valid mailbox name
 * Accepts: mailbox name
 *	    scratch buffer
 * Returns: T if valid, NIL otherwise
 */

long mmdf_isvalid (char *name,char *tmp)
{
  int fd;
  int ret = NIL;
  char *t,file[MAILTMPLEN];
  struct stat sbuf;
  time_t tp[2];
  errno = EINVAL;		/* assume invalid argument */
				/* must be non-empty file */
  if ((t = dummy_file (file,name)) && !stat (t,&sbuf)) {
    if (!sbuf.st_size)errno = 0;/* empty file */
    else if ((fd = open (file,O_RDONLY,NIL)) >= 0) {
				/* error -1 for invalid format */
      if (!(ret = mmdf_isvalid_fd (fd,tmp))) errno = -1;
      close (fd);		/* close the file */
      tp[0] = sbuf.st_atime;	/* preserve atime and mtime */
      tp[1] = sbuf.st_mtime;
      utime (file,tp);		/* set the times */
    }
  }
				/* in case INBOX but not MMDF format */
  else if ((errno == ENOENT) && ((name[0] == 'I') || (name[0] == 'i')) &&
	   ((name[1] == 'N') || (name[1] == 'n')) &&
	   ((name[2] == 'B') || (name[2] == 'b')) &&
	   ((name[3] == 'O') || (name[3] == 'o')) &&
	   ((name[4] == 'X') || (name[4] == 'x')) && !name[5]) errno = -1;
  return ret;			/* return what we should */
}

/* MMDF mail test for valid mailbox
 * Accepts: file descriptor
 *	    scratch buffer
 * Returns: T if valid, NIL otherwise
 */

long mmdf_isvalid_fd (int fd,char *tmp)
{
  int ret = NIL;
  memset (tmp,'\0',MAILTMPLEN);
  if (read (fd,tmp,MAILTMPLEN-1) >= 0) ret = ISMMDF (tmp) ? T : NIL;
  return ret;			/* return what we should */
}


/* MMDF manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *mmdf_parameters (long function,void *value)
{
  return NIL;
}

/* MMDF mail scan mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void mmdf_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  if (stream) dummy_scan (NIL,ref,pat,contents);
}


/* MMDF mail list mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void mmdf_list (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_list (NIL,ref,pat);
}


/* MMDF mail list subscribed mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void mmdf_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_lsub (NIL,ref,pat);
}

/* MMDF mail create mailbox
 * Accepts: MAIL stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long mmdf_create (MAILSTREAM *stream,char *mailbox)
{
  char *s,mbx[MAILTMPLEN],tmp[MAILTMPLEN];
  long ret = NIL;
  int i,fd;
  time_t ti = time (0);
  if (!(s = dummy_file (mbx,mailbox))) {
    sprintf (tmp,"Can't create %.80s: invalid name",mailbox);
    mm_log (tmp,ERROR);
  }
				/* create underlying file */
  else if (dummy_create_path (stream,s)) {
				/* done if made directory */
    if ((s = strrchr (s,'/')) && !s[1]) return T;
    if ((fd = open (mbx,O_WRONLY,
		    (int) mail_parameters (NIL,GET_MBXPROTECTION,NIL))) < 0) {
      sprintf (tmp,"Can't reopen mailbox node %.80s: %s",mbx,strerror (errno));
      mm_log (tmp,ERROR);
      unlink (mbx);		/* delete the file */
    }
				/* in case a whiner with no life */
    else if (mail_parameters (NIL,GET_USERHASNOLIFE,NIL)) ret = T;
    else {			/* initialize header */
      memset (tmp,'\0',MAILTMPLEN);
      sprintf (tmp,"%sFrom %s %sDate: ",mmdfhdr,pseudo_from,ctime (&ti));
      rfc822_date (s = tmp + strlen (tmp));
      sprintf (s += strlen (s),	/* write the pseudo-header */
	       "\nFrom: %s <%s@%s>\nSubject: %s\nX-IMAP: %010lu 0000000000",
	       pseudo_name,pseudo_from,mylocalhost (),pseudo_subject,
	       (unsigned long) ti);
      for (i = 0; i < NUSERFLAGS; ++i) if (default_user_flag (i))
	sprintf (s += strlen (s)," %s",default_user_flag (i));
      sprintf (s += strlen (s),"\nStatus: RO\n\n%s\n%s",pseudo_msg,mmdfhdr);
      if ((write (fd,tmp,strlen (tmp)) < 0) || close (fd)) {
	sprintf (tmp,"Can't initialize mailbox node %.80s: %s",mbx,
		 strerror (errno));
	mm_log (tmp,ERROR);
	unlink (mbx);		/* delete the file */
      }
      else ret = T;		/* success */
    }
    close (fd);			/* close file, set proper protections */
  }
  return ret ? set_mbx_protections (mailbox,mbx) : NIL;
}


/* MMDF mail delete mailbox
 * Accepts: MAIL stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long mmdf_delete (MAILSTREAM *stream,char *mailbox)
{
  return mmdf_rename (stream,mailbox,NIL);
}

/* MMDF mail rename mailbox
 * Accepts: MAIL stream
 *	    old mailbox name
 *	    new mailbox name (or NIL for delete)
 * Returns: T on success, NIL on failure
 */

long mmdf_rename (MAILSTREAM *stream,char *old,char *newname)
{
  long ret = NIL;
  char c,*s = NIL;
  char tmp[MAILTMPLEN],file[MAILTMPLEN],lock[MAILTMPLEN];
  DOTLOCK lockx;
  int fd,ld;
  long i;
  struct stat sbuf;
  mm_critical (stream);		/* get the c-client lock */
  if (newname && !((s = dummy_file (tmp,newname)) && *s))
    sprintf (tmp,"Can't rename mailbox %.80s to %.80s: invalid name",
	     old,newname);
				/* lock out other c-clients */
  else if ((ld = lockname (lock,dummy_file (file,old),LOCK_EX|LOCK_NB,&i)) < 0)
    sprintf (tmp,"Mailbox %.80s is in use by another process",old);
  else {
    if ((fd = mmdf_lock (file,O_RDWR,S_IREAD|S_IWRITE,&lockx,LOCK_EX)) < 0)
      sprintf (tmp,"Can't lock mailbox %.80s: %s",old,strerror (errno));
    else {
      if (newname) {		/* want rename? */
				/* found superior to destination name? */
	if (s = strrchr (s,'/')) {
	  c = *++s;		/* remember first character of inferior */
	  *s = '\0';		/* tie off to get just superior */
				/* name doesn't exist, create it */
	  if ((stat (tmp,&sbuf) || ((sbuf.st_mode & S_IFMT) != S_IFDIR)) &&
	      !dummy_create (stream,tmp)) {
	    mmdf_unlock (fd,NIL,&lockx);
	    mmdf_unlock (ld,NIL,NIL);
	    unlink (lock);
	    mm_nocritical (stream);
	    return ret;		/* return success or failure */
	  }
	  *s = c;		/* restore full name */
	}
	if (rename (file,tmp))
	  sprintf (tmp,"Can't rename mailbox %.80s to %.80s: %s",old,newname,
		   strerror (errno));
	else ret = T;		/* set success */
      }
      else if (unlink (file))
	sprintf (tmp,"Can't delete mailbox %.80s: %s",old,strerror (errno));
      else ret = T;		/* set success */
      mmdf_unlock (fd,NIL,&lockx);
    }
    mmdf_unlock (ld,NIL,NIL);	/* flush the lock */
    unlink (lock);
  }
  mm_nocritical (stream);	/* no longer critical */
  if (!ret) mm_log (tmp,ERROR);	/* log error */
  return ret;			/* return success or failure */
}

/* MMDF mail open
 * Accepts: Stream to open
 * Returns: Stream on success, NIL on failure
 */

MAILSTREAM *mmdf_open (MAILSTREAM *stream)
{
  long i;
  int fd;
  char tmp[MAILTMPLEN];
  DOTLOCK lock;
  long retry;
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return user_flags (&mmdfproto);
  retry = stream->silent ? 1 : KODRETRY;
  if (stream->local) fatal ("mmdf recycle stream");
  stream->local = memset (fs_get (sizeof (MMDFLOCAL)),0,sizeof (MMDFLOCAL));
				/* note if an INBOX or not */
  stream->inbox = !strcmp (ucase (strcpy (tmp,stream->mailbox)),"INBOX");
				/* canonicalize the stream mailbox name */
  dummy_file (tmp,stream->mailbox);
				/* flush old name */
  fs_give ((void **) &stream->mailbox);
				/* save canonical name */
  stream->mailbox = cpystr (tmp);
  LOCAL->fd = LOCAL->ld = -1;	/* no file or state locking yet */
  LOCAL->buf = (char *) fs_get ((LOCAL->buflen = CHUNK) + 1);
  stream->sequence++;		/* bump sequence number */

				/* make lock for read/write access */
  if (!stream->rdonly) while (retry) {
				/* try to lock file */
    if ((fd = lockname (tmp,stream->mailbox,LOCK_EX|LOCK_NB,&i)) < 0) {
      if (retry-- == KODRETRY) {/* no, first time through? */
	if (i) {		/* learned the other guy's PID? */
	  kill ((int) i,SIGUSR2);
	  sprintf (tmp,"Trying to get mailbox lock from process %ld",i);
	  mm_log (tmp,WARN);
	}
	else retry = 0;		/* give up */
      }
      if (!stream->silent) {	/* nothing if silent stream */
	if (retry) sleep (1);	/* wait a second before trying again */
	else mm_log ("Mailbox is open by another process, access is readonly",
		     WARN);
      }
    }
    else {			/* got the lock, nobody else can alter state */
      LOCAL->ld = fd;		/* note lock's fd and name */
      LOCAL->lname = cpystr (tmp);
				/* make sure mode OK (don't use fchmod()) */
      chmod (LOCAL->lname,(int) mail_parameters (NIL,GET_LOCKPROTECTION,NIL));
      if (stream->silent) i = 0;/* silent streams won't accept KOD */
      else {			/* note our PID in the lock */
	sprintf (tmp,"%d",getpid ());
	write (fd,tmp,(i = strlen (tmp))+1);
      }
      ftruncate (fd,i);		/* make sure tied off */
      fsync (fd);		/* make sure it's available */
      retry = 0;		/* no more need to try */
    }
  }

				/* parse mailbox */
  stream->nmsgs = stream->recent = 0;
				/* will we be able to get write access? */
  if ((LOCAL->ld >= 0) && access (stream->mailbox,W_OK) && (errno == EACCES)) {
    mm_log ("Can't get write access to mailbox, access is readonly",WARN);
    flock (LOCAL->ld,LOCK_UN);	/* release the lock */
    close (LOCAL->ld);		/* close the lock file */
    LOCAL->ld = -1;		/* no more lock fd */
    unlink (LOCAL->lname);	/* delete it */
  }
				/* reset UID validity */
  stream->uid_validity = stream->uid_last = 0;
  if (stream->silent && !stream->rdonly && (LOCAL->ld < 0))
    mmdf_abort (stream);	/* abort if can't get RW silent stream */
				/* parse mailbox */
  else if (mmdf_parse (stream,&lock,LOCK_SH)) {
    mmdf_unlock (LOCAL->fd,stream,&lock);
    mail_unlock (stream);
    mm_nocritical (stream);	/* done with critical */
  }
  if (!LOCAL) return NIL;	/* failure if stream died */
				/* make sure upper level knows readonly */
  stream->rdonly = (LOCAL->ld < 0);
				/* notify about empty mailbox */
  if (!(stream->nmsgs || stream->silent)) mm_log ("Mailbox is empty",NIL);
  if (!stream->rdonly) {	/* flags stick if readwrite */
    stream->perm_seen = stream->perm_deleted =
      stream->perm_flagged = stream->perm_answered = stream->perm_draft = T;
    if (!stream->uid_nosticky) {/* users with lives get permanent keywords */
      stream->perm_user_flags = 0xffffffff;
				/* and maybe can create them too! */
      stream->kwd_create = stream->user_flags[NUSERFLAGS-1] ? NIL : T;
    }
  }
  return stream;		/* return stream alive to caller */
}


/* MMDF mail close
 * Accepts: MAIL stream
 *	    close options
 */

void mmdf_close (MAILSTREAM *stream,long options)
{
  int silent = stream->silent;
  stream->silent = T;		/* go silent */
				/* expunge if requested */
  if (options & CL_EXPUNGE) mmdf_expunge (stream);
				/* else dump final checkpoint */
  else if (LOCAL->dirty) mmdf_check (stream);
  stream->silent = silent;	/* restore old silence state */
  mmdf_abort (stream);		/* now punt the file and local data */
}

/* MMDF mail fetch message header
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned header text length
 *	    option flags
 * Returns: message header in RFC822 format
 */

				/* lines to filter from header */
static STRINGLIST *mmdf_hlines = NIL;

char *mmdf_header (MAILSTREAM *stream,unsigned long msgno,
		   unsigned long *length,long flags)
{
  MESSAGECACHE *elt;
  char *s;
  *length = 0;			/* default to empty */
  if (flags & FT_UID) return "";/* UID call "impossible" */
  elt = mail_elt (stream,msgno);/* get cache */
  if (!mmdf_hlines) {		/* once only code */
    STRINGLIST *lines = mmdf_hlines = mail_newstringlist ();
    lines->text.size = strlen ((char *) (lines->text.data =
					 (unsigned char *) "Status"));
    lines = lines->next = mail_newstringlist ();
    lines->text.size = strlen ((char *) (lines->text.data =
					 (unsigned char *) "X-Status"));
    lines = lines->next = mail_newstringlist ();
    lines->text.size = strlen ((char *) (lines->text.data =
					 (unsigned char *) "X-Keywords"));
    lines = lines->next = mail_newstringlist ();
    lines->text.size = strlen ((char *) (lines->text.data =
					 (unsigned char *) "X-UID"));
    lines = lines->next = mail_newstringlist ();
    lines->text.size = strlen ((char *) (lines->text.data =
					 (unsigned char *) "X-IMAP"));
    lines = lines->next = mail_newstringlist ();
    lines->text.size = strlen ((char *) (lines->text.data =
					 (unsigned char *) "X-IMAPbase"));
  }
				/* go to header position */
  lseek (LOCAL->fd,elt->private.special.offset +
	 elt->private.msg.header.offset,L_SET);

  if (flags & FT_INTERNAL) {	/* initial data OK? */
    if (elt->private.msg.header.text.size > LOCAL->buflen) {
      fs_give ((void **) &LOCAL->buf);
      LOCAL->buf = (char *) fs_get ((LOCAL->buflen =
				     elt->private.msg.header.text.size) + 1);
    }
				/* read message */
    read (LOCAL->fd,LOCAL->buf,elt->private.msg.header.text.size);
				/* got text, tie off string */
    LOCAL->buf[*length = elt->private.msg.header.text.size] = '\0';
				/* squeeze out CRs (in case from PC) */
    if (s = strchr (LOCAL->buf,'\r')) {
      char *t,*tl;
      for (t = s,tl = LOCAL->buf + *length; t <= tl; t++)
	if ((*t != '\r') || (t[1] != '\n')) *s++ = *t;
				/* adjust length */
      LOCAL->buf[*length = s - LOCAL->buf - 1] = '\0';
    }
  }
  else {			/* need to make a CRLF version */
    read (LOCAL->fd,s = (char *) fs_get (elt->private.msg.header.text.size+1),
	  elt->private.msg.header.text.size);
				/* tie off string, and convert to CRLF */
    s[elt->private.msg.header.text.size] = '\0';
    *length = strcrlfcpy (&LOCAL->buf,&LOCAL->buflen,s,
			  elt->private.msg.header.text.size);
    fs_give ((void **) &s);	/* free readin buffer */
  }
  *length = mail_filter (LOCAL->buf,*length,mmdf_hlines,FT_NOT);
  return LOCAL->buf;		/* return processed copy */
}

/* MMDF mail fetch message text
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned stringstruct
 *	    option flags
 * Returns: T on success, NIL if failure
 */

long mmdf_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags)
{
  char *s;
  unsigned long i;
  MESSAGECACHE *elt;
				/* UID call "impossible" */
  if (flags & FT_UID) return NIL;
  elt = mail_elt (stream,msgno);/* get cache element */
				/* if message not seen */
  if (!(flags & FT_PEEK) && !elt->seen) {
				/* mark message seen and dirty */
    elt->seen = elt->private.dirty = LOCAL->dirty = T;
    mm_flags (stream,msgno);
  }
  s = mmdf_text_work (stream,elt,&i,flags);
  INIT (bs,mail_string,s,i);	/* set up stringstruct */
  return T;			/* success */
}

/* MMDF mail fetch message text worker routine
 * Accepts: MAIL stream
 *	    message cache element
 *	    pointer to returned header text length
 *	    option flags
 */

char *mmdf_text_work (MAILSTREAM *stream,MESSAGECACHE *elt,
		      unsigned long *length,long flags)
{
  FDDATA d;
  STRING bs;
  char *s,tmp[CHUNK];
				/* go to text position */
  lseek (LOCAL->fd,elt->private.special.offset +
	 elt->private.msg.text.offset,L_SET);
  if (flags & FT_INTERNAL) {	/* initial data OK? */
    if (elt->private.msg.text.text.size > LOCAL->buflen) {
      fs_give ((void **) &LOCAL->buf);
      LOCAL->buf = (char *) fs_get ((LOCAL->buflen =
				     elt->private.msg.text.text.size) + 1);
    }
				/* read message */
    read (LOCAL->fd,LOCAL->buf,elt->private.msg.text.text.size);
				/* got text, tie off string */
    LOCAL->buf[*length = elt->private.msg.text.text.size] = '\0';
				/* squeeze out CRs (in case from PC) */
    if (s = strchr (LOCAL->buf,'\r')) {
      char *t,*tl;
      for (t = s,tl = LOCAL->buf + *length; t <= tl; t++)
	if ((*t != '\r') || (t[1] != '\n')) *s++ = *t;
				/* adjust length */
      *length = s - LOCAL->buf - 1;
    }
  }

  else {			/* need to make a CRLF version */
    if (elt->rfc822_size > LOCAL->buflen) {
      /* excessively conservative, but the right thing is too hard to do */
      fs_give ((void **) &LOCAL->buf);
      LOCAL->buf = (char *) fs_get ((LOCAL->buflen = elt->rfc822_size) + 1);
    }
    d.fd = LOCAL->fd;		/* yes, set up file descriptor */
    d.pos = elt->private.special.offset + elt->private.msg.text.offset;
    d.chunk = tmp;		/* initial buffer chunk */
    d.chunksize = CHUNK;	/* file chunk size */
    INIT (&bs,fd_string,&d,elt->private.msg.text.text.size);
    for (s = LOCAL->buf; SIZE (&bs);) switch (CHR (&bs)) {
    case '\r':			/* carriage return seen */
      *s++ = SNX (&bs);		/* copy it and any succeeding LF */
      if (SIZE (&bs) && (CHR (&bs) == '\n')) *s++ = SNX (&bs);
      break;
    case '\n':
      *s++ = '\r';		/* insert a CR */
    default:
      *s++ = SNX (&bs);		/* copy characters */
    }
    *s = '\0';			/* tie off buffer */
    *length = s - LOCAL->buf;	/* calculate length */
  }
  return LOCAL->buf;
}

/* MMDF per-message modify flag
 * Accepts: MAIL stream
 *	    message cache element
 */

void mmdf_flagmsg (MAILSTREAM *stream,MESSAGECACHE *elt)
{
				/* only after finishing */
  if (elt->valid) elt->private.dirty = LOCAL->dirty = T;
}


/* MMDF mail ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 */

long mmdf_ping (MAILSTREAM *stream)
{
  DOTLOCK lock;
  struct stat sbuf;
				/* big no-op if not readwrite */
  if (LOCAL && (LOCAL->ld >= 0) && !stream->lock) {
    if (stream->rdonly) {	/* does he want to give up readwrite? */
				/* checkpoint if we changed something */
      if (LOCAL->dirty) mmdf_check (stream);
      flock (LOCAL->ld,LOCK_UN);/* release readwrite lock */
      close (LOCAL->ld);	/* close the readwrite lock file */
      LOCAL->ld = -1;		/* no more readwrite lock fd */
      unlink (LOCAL->lname);	/* delete the readwrite lock file */
    }
    else {			/* get current mailbox size */
      if (LOCAL->fd >= 0) fstat (LOCAL->fd,&sbuf);
      else stat (stream->mailbox,&sbuf);
				/* parse if mailbox changed */
      if ((sbuf.st_size != LOCAL->filesize) &&
	  mmdf_parse (stream,&lock,LOCK_SH)) {
				/* unlock mailbox */
	mmdf_unlock (LOCAL->fd,stream,&lock);
	mail_unlock (stream);	/* and stream */
	mm_nocritical (stream);	/* done with critical */
      }
    }
  }
  return LOCAL ? LONGT : NIL;	/* return if still alive */
}

/* MMDF mail check mailbox
 * Accepts: MAIL stream
 */

void mmdf_check (MAILSTREAM *stream)
{
  DOTLOCK lock;
				/* parse and lock mailbox */
  if (LOCAL && (LOCAL->ld >= 0) && !stream->lock &&
      mmdf_parse (stream,&lock,LOCK_EX)) {
				/* any unsaved changes? */
    if (LOCAL->dirty && mmdf_rewrite (stream,NIL,&lock)) {
      if (!stream->silent) mm_log ("Checkpoint completed",NIL);
    }
				/* no checkpoint needed, just unlock */
    else mmdf_unlock (LOCAL->fd,stream,&lock);
    mail_unlock (stream);	/* unlock the stream */
    mm_nocritical (stream);	/* done with critical */
  }
}


/* MMDF mail expunge mailbox
 * Accepts: MAIL stream
 */

void mmdf_expunge (MAILSTREAM *stream)
{
  unsigned long i;
  DOTLOCK lock;
  char *msg = NIL;
				/* parse and lock mailbox */
  if (LOCAL && (LOCAL->ld >= 0) && !stream->lock &&
      mmdf_parse (stream,&lock,LOCK_EX)) {
				/* count expunged messages if not dirty */
    if (!LOCAL->dirty) for (i = 1; i <= stream->nmsgs; i++)
      if (mail_elt (stream,i)->deleted) LOCAL->dirty = T;
    if (!LOCAL->dirty) {	/* not dirty and no expunged messages */
      mmdf_unlock (LOCAL->fd,stream,&lock);
      msg = "No messages deleted, so no update needed";
    }
    else if (mmdf_rewrite (stream,&i,&lock)) {
      if (i) sprintf (msg = LOCAL->buf,"Expunged %lu messages",i);
      else msg = "Mailbox checkpointed, but no messages expunged";
    }
				/* rewrite failed */
    else mmdf_unlock (LOCAL->fd,stream,&lock);
    mail_unlock (stream);	/* unlock the stream */
    mm_nocritical (stream);	/* done with critical */
    if (msg && !stream->silent) mm_log (msg,NIL);
  }
  else if (!stream->silent) mm_log("Expunge ignored on readonly mailbox",WARN);
}

/* MMDF mail copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    copy options
 * Returns: T if copy successful, else NIL
 */

long mmdf_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options)
{
  struct stat sbuf;
  int fd;
  char *s,file[MAILTMPLEN];
  DOTLOCK lock;
  time_t tp[2];
  unsigned long i,j;
  MESSAGECACHE *elt;
  long ret = T;
  mailproxycopy_t pc =
    (mailproxycopy_t) mail_parameters (stream,GET_MAILPROXYCOPY,NIL);
  if (!((options & CP_UID) ? mail_uid_sequence (stream,sequence) :
	mail_sequence (stream,sequence))) return NIL;
				/* make sure valid mailbox */
  if (!mmdf_isvalid (mailbox,file)) switch (errno) {
  case ENOENT:			/* no such file? */
    mm_notify (stream,"[TRYCREATE] Must create mailbox before copy",NIL);
    return NIL;
  case 0:			/* merely empty file? */
    break;
  case EINVAL:
    if (pc) return (*pc) (stream,sequence,mailbox,options);
    sprintf (LOCAL->buf,"Invalid MMDF-format mailbox name: %.80s",mailbox);
    mm_log (LOCAL->buf,ERROR);
    return NIL;
  default:
    if (pc) return (*pc) (stream,sequence,mailbox,options);
    sprintf (LOCAL->buf,"Not a MMDF-format mailbox: %.80s",mailbox);
    mm_log (LOCAL->buf,ERROR);
    return NIL;
  }
  LOCAL->buf[0] = '\0';
  mm_critical (stream);		/* go critical */
  if ((fd = mmdf_lock (dummy_file (file,mailbox),O_WRONLY|O_APPEND|O_CREAT,
		       S_IREAD|S_IWRITE,&lock,LOCK_EX)) < 0) {
    mm_nocritical (stream);	/* done with critical */
    sprintf (LOCAL->buf,"Can't open destination mailbox: %s",strerror (errno));
    mm_log (LOCAL->buf,ERROR);	/* log the error */
    return NIL;			/* failed */
  }
  fstat (fd,&sbuf);		/* get current file size */

				/* write all requested messages to mailbox */
  for (i = 1; ret && (i <= stream->nmsgs); i++)
    if ((elt = mail_elt (stream,i))->sequence) {
      lseek (LOCAL->fd,elt->private.special.offset,L_SET);
      read (LOCAL->fd,LOCAL->buf,elt->private.special.text.size);
      if (write (fd,LOCAL->buf,elt->private.special.text.size) < 0) ret = NIL;
      else {			/* internal header succeeded */
	s = mmdf_header (stream,i,&j,FT_INTERNAL);
				/* header size, sans trailing newline */
	if (j && (s[j - 2] == '\n')) j--;
	if (write (fd,s,j) < 0) ret = NIL;
	else {			/* message header succeeded */
	  j = mmdf_xstatus (stream,LOCAL->buf,elt,NIL);
	  if (write (fd,LOCAL->buf,j) < 0) ret = NIL;
	  else {		/* message status succeeded */
	    s = mmdf_text_work (stream,elt,&j,FT_INTERNAL);
	    if ((write (fd,s,j) < 0) || (write (fd,mmdfhdr,MMDFHDRLEN) < 0))
	      ret = NIL;
	  }
	}
      }
    }
  if (!ret || fsync (fd)) {	/* force out the update */
    sprintf (LOCAL->buf,"Message copy failed: %s",strerror (errno));
    ftruncate (fd,sbuf.st_size);
    ret = NIL;
  }
  tp[0] = sbuf.st_atime;	/* preserve atime */
  tp[1] = time (0);		/* set mtime to now */
  utime (file,tp);		/* set the times */
  mmdf_unlock (fd,NIL,&lock);	/* unlock and close mailbox */
  mm_nocritical (stream);	/* release critical */
				/* log the error */
  if (!ret) mm_log (LOCAL->buf,ERROR);
				/* delete if requested message */
  else if (options & CP_MOVE) for (i = 1; i <= stream->nmsgs; i++)
    if ((elt = mail_elt (stream,i))->sequence)
      elt->deleted = elt->private.dirty = LOCAL->dirty = T;
  return ret;
}

/* MMDF mail append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    append callback
 *	    data for callback
 * Returns: T if append successful, else NIL
 */

#define BUFLEN 8*MAILTMPLEN

long mmdf_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data)
{
  struct stat sbuf;
  int fd;
  unsigned long i,j;
  char *flags,*date,buf[BUFLEN],tmp[MAILTMPLEN],file[MAILTMPLEN];
  time_t tp[2];
  FILE *sf,*df;
  MESSAGECACHE elt;
  DOTLOCK lock;
  STRING *message;
  long ret = LONGT;
				/* default stream to prototype */
  if (!stream) stream = user_flags (&mmdfproto);
				/* make sure valid mailbox */
  if (!mmdf_valid (mailbox)) switch (errno) {
  case ENOENT:			/* no such file? */
    if (((mailbox[0] == 'I') || (mailbox[0] == 'i')) &&
	((mailbox[1] == 'N') || (mailbox[1] == 'n')) &&
	((mailbox[2] == 'B') || (mailbox[2] == 'b')) &&
	((mailbox[3] == 'O') || (mailbox[3] == 'o')) &&
	((mailbox[4] == 'X') || (mailbox[4] == 'x')) && !mailbox[5])
      mmdf_create (NIL,"INBOX");
    else {
      mm_notify (stream,"[TRYCREATE] Must create mailbox before append",NIL);
      return NIL;
    }
				/* falls through */
  case 0:			/* INBOX ENOENT or empty file? */
    break;
  case EINVAL:
    sprintf (tmp,"Invalid MMDF-format mailbox name: %.80s",mailbox);
    mm_log (tmp,ERROR);
    return NIL;
  default:
    sprintf (tmp,"Not a MMDF-format mailbox: %.80s",mailbox);
    mm_log (tmp,ERROR);
    return NIL;
  }
				/* get first message */
  if (!(*af) (stream,data,&flags,&date,&message)) return NIL;
  if (!(sf = tmpfile ())) {	/* must have scratch file */
    sprintf (tmp,"Unable to create scratch file: %.80s",strerror (errno));
    mm_log (tmp,ERROR);
  }

  do {				/* parse date */
    if (!date) rfc822_date (date = tmp);
    if (!mail_parse_date (&elt,date)) {
      sprintf (tmp,"Bad date in append: %.80s",date);
      mm_log (tmp,ERROR);
    }
    else {			/* user wants to suppress time zones? */
      if (mail_parameters (NIL,GET_NOTIMEZONES,NIL)) {
	time_t when = mail_longdate (&elt);
	date = ctime (&when);	/* use traditional date */
      }
				/* use POSIX-style date */
      else date = mail_cdate (tmp,&elt);
      if (!SIZE (message)) mm_log ("Append of zero-length message",ERROR);
      else if (!mmdf_append_msg (stream,sf,flags,date,message)) {
	sprintf (tmp,"Error writing scratch file: %.80s",strerror (errno));
	mm_log (tmp,ERROR);
      }
				/* get next message */
      else if ((*af) (stream,data,&flags,&date,&message)) continue;
    }
    fclose (sf);		/* punt scratch file */
    return NIL;			/* give up */
  } while (message);		/* until no more messages */
  if (fflush (sf) || fstat (fileno (sf),&sbuf)) {
    sprintf (tmp,"Error finishing scratch file: %.80s",strerror (errno));
    mm_log (tmp,ERROR);
    fclose (sf);		/* punt scratch file */
    return NIL;			/* give up */
  }
  i = sbuf.st_size;		/* size of scratch file */
  mm_critical (stream);		/* go critical */
  if (((fd = mmdf_lock (dummy_file (file,mailbox),O_WRONLY|O_APPEND|O_CREAT,
		       S_IREAD|S_IWRITE,&lock,LOCK_EX)) < 0) ||
      !(df = fdopen (fd,"ab"))) {
    mm_nocritical (stream);	/* done with critical */
    sprintf (tmp,"Can't open append mailbox: %s",strerror (errno));
    mm_log (tmp,ERROR);
    return NIL;
  }
  fstat (fd,&sbuf);		/* get current file size */
  rewind (sf);
  for (; i && ((j = fread (buf,1,min ((long) BUFLEN,i),sf)) &&
	       (fwrite (buf,1,j,df) == j)); i -= j);
  fclose (sf);			/* done with scratch file */
  tp[0] = sbuf.st_atime;	/* preserve atime */
				/* make sure append wins */
  if (i || (fflush (df) == EOF)) {
    sprintf (buf,"Message append failed: %s",strerror (errno));
    mm_log (buf,ERROR);
    ftruncate (fd,sbuf.st_size);
    tp[1] = sbuf.st_mtime;	/* preserve mtime */
    ret = NIL;			/* return error */
  }
  else tp[1] = time (0);	/* set mtime to now */
  utime (file,tp);		/* set the times */
  mmdf_unlock (fd,NIL,&lock);	/* unlock and close mailbox */
  fclose (df);
  mm_nocritical (stream);	/* release critical */
  return ret;
}

/* Write single message to append scratch file
 * Accepts: MAIL stream
 *	    scratch file
 *	    flags
 *	    message stringstruct
 * Returns: NIL if write error, else T
 */

int mmdf_append_msg (MAILSTREAM *stream,FILE *sf,char *flags,char *date,
		     STRING *msg)
{
  unsigned long uf;
  int c;
  long f = mail_parse_flags (stream,flags,&uf);
				/* build initial header */
  if ((fprintf (sf,"%sFrom %s@%s %sStatus: ",
		mmdfhdr,myusername (),mylocalhost (),date) < 0) ||
      (f&fSEEN && (putc ('R',sf) == EOF)) ||
      (fputs ("\nX-Status: ",sf) == EOF) ||
      (f&fDELETED && (putc ('D',sf) == EOF)) ||
      (f&fFLAGGED && (putc ('F',sf) == EOF)) ||
      (f&fANSWERED && (putc ('A',sf) == EOF)) ||
      (f&fDRAFT && (putc ('T',sf) == EOF)) ||
      (fputs ("\nX-Keywords:",sf) == EOF)) return NIL;
  while (uf)			/* write user flags */
    if (fprintf (sf," %s",stream->user_flags[find_rightmost_bit (&uf)]) < 0)
      return NIL;
				/* tie off flags */
  if (putc ('\n',sf) == EOF) return NIL;
  while (SIZE (msg)) {		/* copy text, flush CTRL/A and CR from CRLF */
    if (((c = 0xff & SNX (msg)) != MMDFCHR) &&
	(((c == '\r') && SIZE (msg) && ((c = SNX (msg)) != '\n') &&
	  (putc ('\r',sf) == EOF)) || (putc (c,sf) == EOF))) return NIL;
  }
				/* write trailer and return */
  return (fputs (mmdfhdr,sf) == EOF) ? NIL : T;
}

/* Internal routines */


/* MMDF mail abort stream
 * Accepts: MAIL stream
 */

void mmdf_abort (MAILSTREAM *stream)
{
  if (LOCAL) {			/* only if a file is open */
    if (LOCAL->fd >= 0) close (LOCAL->fd);
    if (LOCAL->ld >= 0) {	/* have a mailbox lock? */
      flock (LOCAL->ld,LOCK_UN);/* yes, release the lock */
      close (LOCAL->ld);	/* close the lock file */
      unlink (LOCAL->lname);	/* and delete it */
    }
    if (LOCAL->lname) fs_give ((void **) &LOCAL->lname);
				/* free local text buffers */
    if (LOCAL->buf) fs_give ((void **) &LOCAL->buf);
    if (LOCAL->line) fs_give ((void **) &LOCAL->line);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
    stream->dtb = NIL;		/* log out the DTB */
  }
}

/* MMDF open and lock mailbox
 * Accepts: file name to open/lock
 *	    file open mode
 *	    destination buffer for lock file name
 *	    type of locking operation (LOCK_SH or LOCK_EX)
 */

int mmdf_lock (char *file,int flags,int mode,DOTLOCK *lock,int op)
{
  int fd;
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  (*bn) (BLOCK_FILELOCK,NIL);
				/* try locking the easy way */
  if (dotlock_lock (file,lock,-1)) {
				/* got dotlock file, easy open */
    if ((fd = open (file,flags,mode)) >= 0) flock (fd,op);
    else dotlock_unlock (lock);	/* open failed, free the dotlock */
  }
				/* no dot lock file, open file now */
  else if ((fd = open (file,flags,mode)) >= 0) {
				/* try paranoid way to make a dot lock file */
    if (dotlock_lock (file,lock,fd)) {
      close (fd);		/* get fresh fd in case of timing race */
      if ((fd = open (file,flags,mode)) >= 0) flock (fd,op);
      else dotlock_unlock (lock); /* open failed, free the dotlock */
    }
    else flock (fd,op);		/* paranoid way failed, just flock() it */
  }
  (*bn) (BLOCK_NONE,NIL);
  return fd;
}


/* MMDF unlock and close mailbox
 * Accepts: file descriptor
 *	    (optional) mailbox stream to check atime/mtime
 *	    (optional) lock file name
 */

void mmdf_unlock (int fd,MAILSTREAM *stream,DOTLOCK *lock)
{
  struct stat sbuf;
  time_t tp[2];
  fstat (fd,&sbuf);		/* get file times */
				/* if stream and csh would think new mail */
  if (stream && (sbuf.st_atime <= sbuf.st_mtime)) {
    tp[0] = time (0);		/* set atime to now */
				/* set mtime to (now - 1) if necessary */
    tp[1] = tp[0] > sbuf.st_mtime ? sbuf.st_mtime : tp[0] - 1;
				/* set the times, note change */
    if (!utime (stream->mailbox,tp)) LOCAL->filetime = tp[1];
  }
  flock (fd,LOCK_UN);		/* release flock'ers */
  if (!stream) close (fd);	/* close the file if no stream */
  dotlock_unlock (lock);	/* flush the lock file if any */
}

/* MMDF mail parse and lock mailbox
 * Accepts: MAIL stream
 *	    space to write lock file name
 *	    type of locking operation
 * Returns: T if parse OK, critical & mailbox is locked shared; NIL if failure
 */

int mmdf_parse (MAILSTREAM *stream,DOTLOCK *lock,int op)
{
  int ti,zn,m;
  unsigned long i,j,k;
  char c,*s,*t,*u,tmp[MAILTMPLEN],date[30];
  int retain = T;
  unsigned long nmsgs = stream->nmsgs;
  unsigned long prevuid = nmsgs ? mail_elt (stream,nmsgs)->private.uid : 0;
  unsigned long recent = stream->recent;
  unsigned long oldnmsgs = stream->nmsgs;
  short silent = stream->silent;
  short pseudoseen = NIL;
  struct stat sbuf;
  STRING bs;
  FDDATA d;
  MESSAGECACHE *elt;
  mail_lock (stream);		/* guard against recursion or pingers */
				/* toss out previous descriptor */
  if (LOCAL->fd >= 0) close (LOCAL->fd);
  mm_critical (stream);		/* open and lock mailbox (shared OK) */
  if ((LOCAL->fd = mmdf_lock (stream->mailbox,(LOCAL->ld >= 0) ?
			      O_RDWR : O_RDONLY,NIL,lock,op)) < 0) {
    sprintf (tmp,"Mailbox open failed, aborted: %s",strerror (errno));
    mm_log (tmp,ERROR);
    mmdf_abort (stream);
    mail_unlock (stream);
    mm_nocritical (stream);	/* done with critical */
    return NIL;
  }
  fstat (LOCAL->fd,&sbuf);	/* get status */
				/* validate change in size */
  if (sbuf.st_size < LOCAL->filesize) {
    sprintf (tmp,"Mailbox shrank from %lu to %lu bytes, aborted",
	     (unsigned long) LOCAL->filesize,(unsigned long) sbuf.st_size);
    mm_log (tmp,ERROR);		/* this is pretty bad */
    mmdf_unlock (LOCAL->fd,stream,lock);
    mmdf_abort (stream);
    mail_unlock (stream);
    mm_nocritical (stream);	/* done with critical */
    return NIL;
  }

				/* new data? */
  else if (i = sbuf.st_size - LOCAL->filesize) {
    d.fd = LOCAL->fd;		/* yes, set up file descriptor */
    d.pos = LOCAL->filesize;	/* get to that position in the file */
    d.chunk = LOCAL->buf;	/* initial buffer chunk */
    d.chunksize = CHUNK;	/* file chunk size */
    INIT (&bs,fd_string,&d,i);	/* initialize stringstruct */
				/* skip leading whitespace for broken MTAs */
    while (((c = CHR (&bs)) == '\n') || (c == '\r') ||
	   (c == ' ') || (c == '\t')) SNX (&bs);
    if (SIZE (&bs)) {		/* read new data */
				/* remember internal header position */
      j = LOCAL->filesize + GETPOS (&bs);
      s = mmdf_mbxline (stream,&bs,&i);
      stream->silent = T;	/* quell main program new message events */
      do {			/* read MMDF header */
	if (!(i && ISMMDF (s))){/* see if valid MMDF header */
	  sprintf (tmp,"Unexpected changes to mailbox (try restarting): %.20s",
		   s);
				/* see if we can back up to a line */
	  if (i && (j > MMDFHDRLEN)) {
	    SETPOS (&bs,j -= MMDFHDRLEN);
				/* read previous line */
	    s = mmdf_mbxline (stream,&bs,&i);
				/* kill the error if it looks good */
	    if (i && ISMMDF (s)) tmp[0] = '\0';
	  }
	  if (tmp[0]) {
	    mm_log (tmp,ERROR);
	    mmdf_unlock (LOCAL->fd,stream,lock);
	    mmdf_abort (stream);
	    mail_unlock (stream);
	    mm_nocritical(stream);
	    return NIL;
	  }
	}
				/* instantiate first new message */
	mail_exists (stream,++nmsgs);
	(elt = mail_elt (stream,nmsgs))->valid = T;
	recent++;		/* assume recent by default */
	elt->recent = T;
				/* note position/size of internal header */
	elt->private.special.offset = j;
	elt->private.special.text.size = i;

	s = mmdf_mbxline (stream,&bs,&i);
	ti = 0;			/* assume not a valid date */
	zn = 0,t = NIL;
	if (i) VALID (s,t,ti,zn);
	if (ti) {		/* generate plausible IMAPish date string */
				/* this is also part of header */
	  elt->private.special.text.size += i;
	  date[2] = date[6] = date[20] = '-'; date[11] = ' ';
	  date[14] = date[17] = ':';
				/* dd */
	  date[0] = t[ti - 2]; date[1] = t[ti - 1];
				/* mmm */
	  date[3] = t[ti - 6]; date[4] = t[ti - 5]; date[5] = t[ti - 4];
				/* hh */
	  date[12] = t[ti + 1]; date[13] = t[ti + 2];
				/* mm */
	  date[15] = t[ti + 4]; date[16] = t[ti + 5];
	  if (t[ti += 6]==':'){	/* ss */
	    date[18] = t[++ti]; date[19] = t[++ti];
	    ti++;		/* move to space */
	  }
	  else date[18] = date[19] = '0';
				/* yy -- advance over timezone if necessary */
	  if (zn == ti) ti += (((t[zn+1] == '+') || (t[zn+1] == '-')) ? 6 : 4);
	  date[7] = t[ti + 1]; date[8] = t[ti + 2];
	  date[9] = t[ti + 3]; date[10] = t[ti + 4];
				/* zzz */
	  t = zn ? (t + zn + 1) : "LCL";
	  date[21] = *t++; date[22] = *t++; date[23] = *t++;
	  if ((date[21] != '+') && (date[21] != '-')) date[24] = '\0';
	  else {		/* numeric time zone */
	    date[24] = *t++; date[25] = *t++;
	    date[26] = '\0'; date[20] = ' ';
	  }
				/* set internal date */
	  if (!mail_parse_date (elt,date)) {
	    sprintf (tmp,"Unable to parse internal date: %s",date);
	    mm_log (tmp,WARN);
	  }
	}
	else {			/* make date from file date */
	  struct tm *tm = gmtime (&sbuf.st_mtime);
	  elt->day = tm->tm_mday; elt->month = tm->tm_mon + 1;
	  elt->year = tm->tm_year + 1900 - BASEYEAR;
	  elt->hours = tm->tm_hour; elt->minutes = tm->tm_min;
	  elt->seconds = tm->tm_sec;
	  elt->zhours = 0; elt->zminutes = 0;
	  t = NIL;		/* suppress line read */
	}
				/* header starts here */
	elt->private.msg.header.offset = elt->private.special.text.size;

	do {			/* look for message body */
	  if (t) s = t = mmdf_mbxline (stream,&bs,&i);
	  else t = s;		/* this line read was suppressed */
	  if (ISMMDF (s)) break;
				/* this line is part of header */
	  elt->private.msg.header.text.size += i;
	  if (i) switch (*s) {	/* check header lines */
	  case 'X':		/* possible X-???: line */
	    if (s[1] == '-') {	/* must be immediately followed by hyphen */
				/* X-Status: becomes Status: in S case */
	      if (s[2] == 'S' && s[3] == 't' && s[4] == 'a' && s[5] == 't' &&
		  s[6] == 'u' && s[7] == 's' && s[8] == ':') s += 2;
				/* possible X-Keywords */
	      else if (s[2] == 'K' && s[3] == 'e' && s[4] == 'y' &&
		       s[5] == 'w' && s[6] == 'o' && s[7] == 'r' &&
		       s[8] == 'd' && s[9] == 's' && s[10] == ':') {
		char uf[MAILTMPLEN];
		retain = NIL;	/* don't retain continuation */
		s += 11;	/* flush leading whitespace */
		while (*s && (*s != '\n') && ((*s != '\r') || (s[1] != '\n'))){
		  while (*s == ' ') s++;
				/* find end of keyword */
		  if (!(u = strpbrk (s," \n\r"))) u = s + strlen (s);
				/* got a keyword? */
		  if ((k = (u - s)) && (k < MAXUSERFLAG)) {
				/* copy keyword */
		    strncpy (uf,s,k);
		    uf[k] = '\0';/* make sure tied off */
		    ucase (uf);	/* coerce upper case */
		    for (j = 0; (j<NUSERFLAGS) && stream->user_flags[j]; ++j)
		      if (!strcmp(uf,
				  ucase (strcpy(tmp,stream->user_flags[j])))) {
			elt->user_flags |= ((long) 1) << j;
			break;
		      }
		  }
		  s = u;	/* advance to next keyword */
		}
		break;
	      }

				/* possible X-IMAP */
	      else if ((s[2] == 'I') && (s[3] == 'M') && (s[4] == 'A') &&
		       (s[5] == 'P') && ((m = (s[6] == ':')) ||
					 ((s[6] == 'b') && (s[7] == 'a') &&
					  (s[8] == 's') && (s[9] == 'e') &&
					  (s[10] == ':')))) {
		retain = NIL;	/* don't retain continuation */
		if ((nmsgs == 1) && !stream->uid_validity) {
				/* advance to data */
		  s += m ? 7 : 11;
				/* flush whitespace */
		  while (*s == ' ') s++;
		  j = 0;	/* slurp UID validity */
				/* found a digit? */
		  while (isdigit (*s)) {
		    j *= 10;	/* yes, add it in */
		    j += *s++ - '0';
		  }
				/* flush whitespace */
		  while (*s == ' ') s++;
				/* must have valid UID validity and UID last */
		  if (j && isdigit (*s)) {
				/* pseudo-header seen if X-IMAP */
		    if (m) pseudoseen = LOCAL->pseudo = T;
				/* save UID validity */
		    stream->uid_validity = j;
		    j = 0;	/* slurp UID last */
		    while (isdigit (*s)) {
		      j *= 10;	/* yes, add it in */
		      j += *s++ - '0';
		    }
				/* save UID last */
		    stream->uid_last = j;
				/* process keywords */
		    for (j = 0; (*s != '\n') && ((*s != '\r')||(s[1] != '\n'));
			 s = u,j++) {
				/* flush leading whitespace */
		      while (*s == ' ') s++;
		      u = strpbrk (s," \n\r");
				/* got a keyword? */
		      if ((k = (u - s)) && j < NUSERFLAGS) {
			if (stream->user_flags[j])
			  fs_give ((void **) &stream->user_flags[j]);
			stream->user_flags[j] = (char *) fs_get (k + 1);
			strncpy (stream->user_flags[j],s,k);
			stream->user_flags[j][k] = '\0';
		      }
		    }
		  }
		}
		break;
	      }

				/* possible X-UID */
	      else if (s[2] == 'U' && s[3] == 'I' && s[4] == 'D' &&
		       s[5] == ':') {
		retain = NIL;	/* don't retain continuation */
				/* only believe if have a UID validity */
		if (stream->uid_validity && ((nmsgs > 1) || !pseudoseen)) {
		  s += 6;	/* advance to UID value */
				/* flush whitespace */
		  while (*s == ' ') s++;
		  j = 0;
				/* found a digit? */
		  while (isdigit (*s)) {
		    j *= 10;	/* yes, add it in */
		    j += *s++ - '0';
		  }
				/* flush remainder of line */
		  while (*s != '\n') s++;
				/* make sure not duplicated */
		  if (elt->private.uid)
		    sprintf (tmp,"Message %lu UID %lu already has UID %lu",
			     pseudoseen ? elt->msgno - 1 : elt->msgno,
			     j,elt->private.uid);
				/* make sure UID doesn't go backwards */
		  else if (j <= prevuid)
		    sprintf (tmp,"Message %lu UID %lu less than %lu",
			     pseudoseen ? elt->msgno - 1 : elt->msgno,
			     j,prevuid + 1);
				/* or skip by mailbox's recorded last */
		  else if (j > stream->uid_last)
		    sprintf (tmp,"Message %lu UID %lu greater than last %lu",
			     pseudoseen ? elt->msgno - 1 : elt->msgno,
			     j,stream->uid_last);
		  else {	/* normal UID case */
		    prevuid = elt->private.uid = j;
		    break;	/* exit this cruft */
		  }
		  mm_log (tmp,WARN);
				/* invalidate UID validity */
		  stream->uid_validity = 0;
		  elt->private.uid = 0;
		}
		break;
	      }
	    }
				/* otherwise fall into S case */

	  case 'S':		/* possible Status: line */
	    if (s[0] == 'S' && s[1] == 't' && s[2] == 'a' && s[3] == 't' &&
		s[4] == 'u' && s[5] == 's' && s[6] == ':') {
	      retain = NIL;	/* don't retain continuation */
	      s += 6;		/* advance to status flags */
	      do switch (*s++) {/* parse flags */
	      case 'R':		/* message read */
		elt->seen = T;
		break;
	      case 'O':		/* message old */
		if (elt->recent) {
		  elt->recent = NIL;
		  recent--;	/* it really wasn't recent */
		}
		break;
	      case 'D':		/* message deleted */
		elt->deleted = T;
		break;
	      case 'F':		/* message flagged */
		elt->flagged = T;
		break;
	      case 'A':		/* message answered */
		elt->answered = T;
		break;
	      case 'T':		/* message is a draft */
		elt->draft = T;
		break;
	      default:		/* some other crap */
		break;
	      } while (*s && (*s != '\n') && ((*s != '\r') || (s[1] != '\n')));
	      break;		/* all done */
	    }
				/* otherwise fall into default case */

	  default:		/* ordinary header line */
	    if ((*s == 'S') || (*s == 's') ||
		(((*s == 'X') || (*s == 'x')) && (s[1] == '-'))) {
	      char *e,*v;
				/* must match what mail_filter() does */
	      for (u = s,v = tmp,e = u + min (i,MAILTMPLEN - 1);
		   (u < e) && ((c = *u) != ':') &&
		   ((c > ' ') || ((c != ' ') && (c != '\t') &&
				  (c != '\015') && (c != '\012')));
		   *v++ = *u++);
	      *v = '\0';	/* tie off */
				/* matches internal header? */
	      if (!strcmp (ucase (tmp),"STATUS") || !strcmp (tmp,"X-STATUS") ||
		  !strcmp (tmp,"X-KEYWORDS") || !strcmp (tmp,"X-UID") ||
		  !strcmp (tmp,"X-IMAP") || !strcmp (tmp,"X-IMAPBASE")) {
		char err[MAILTMPLEN];
		sprintf (err,"Discarding bogus %s header in message %lu",
			 tmp,elt->msgno);
		mm_log (err,WARN);
		retain = NIL;	/* don't retain continuation */
		break;		/* different case or something */
	      }
	    }
				/* retain or non-continuation? */
	    if (retain || ((*s != ' ') && (*s != '\t'))) {
	      retain = T;	/* retaining continuation now */
				/* line length in LF format newline */
	      k = i - (((i >= 2) && (s[i - 2] == '\r')) ? 1 : 0);
				/* "internal" header size */
	      elt->private.data += k;
				/* message size */
	      elt->rfc822_size += k + 1;
	    }
	    else {
	      char err[MAILTMPLEN];
	      sprintf (err,"Discarding bogus continuation in message %lu: ",
		       elt->msgno);
	      u = strncpy (err + strlen (err),s,(size_t) max (i,80));
	      u[80] = '\0';	/* make sure tired off */
	      if (u = strpbrk (u,"\r\n")) *u = '\0';
	      mm_log (err,WARN);
	      break;		/* different case or something */
	    }
	    break;
	  }
	} while (i && (*t != '\n') && ((*t != '\r') || (t[1] != '\n')));
				/* "internal" header sans trailing newline */
	if (i) elt->private.data--;
				/* assign a UID if none found */
	if (((nmsgs > 1) || !pseudoseen) && !elt->private.uid) {
	  prevuid = elt->private.uid = ++stream->uid_last;
	  elt->private.dirty = T;
	}
	else elt->private.dirty = elt->recent;

				/* note size of header, location of text */
	elt->private.msg.header.text.size =
	  (elt->private.msg.text.offset =
	   (LOCAL->filesize + GETPOS (&bs)) - elt->private.special.offset) -
	     elt->private.special.text.size;
				/* note current position */
	j = LOCAL->filesize + GETPOS (&bs);
	if (i) do {		/* look for next message */
	  s = mmdf_mbxline (stream,&bs,&i);
	  if (i) {		/* got new data? */
	    if (ISMMDF (s)) break;
	    else {
	      elt->rfc822_size += i + (((i < 2) || s[i - 2] != '\r') ? 1 : 0);
				/* update current position */
	      j = LOCAL->filesize + GETPOS (&bs);
	    }
	  }
	} while (i);		/* until found a header */
	elt->private.msg.text.text.size = j -
	  (elt->private.special.offset + elt->private.msg.text.offset);
	if (i) {		/* get next header line */
				/* remember first internal header position */
	  j = LOCAL->filesize + GETPOS (&bs);
	  s = mmdf_mbxline (stream,&bs,&i);
	}
      } while (i);		/* until end of buffer */
      if (pseudoseen) {		/* flush pseudo-message if present */
				/* decrement recent count */
	if (mail_elt (stream,1)->recent) recent--;
				/* and the exists count */
	mail_exists (stream,nmsgs--);
	mail_expunged(stream,1);/* fake an expunge of that message */
      }
				/* need to start a new UID validity? */
      if (!stream->uid_validity) {
	stream->uid_validity = time (0);
				/* in case a whiner with no life */
	if (mail_parameters (NIL,GET_USERHASNOLIFE,NIL))
	  stream->uid_nosticky = T;
	else if (nmsgs) {	/* don't bother if empty file */
	  LOCAL->dirty = T;	/* make dirty to restart UID epoch */
				/* need to rewrite msg 1 if not pseudo */
	  if (!LOCAL->pseudo) mail_elt (stream,1)->private.dirty = T;
	  mm_log ("Assigning new unique identifiers to all messages",NIL);
	}
      }
      stream->nmsgs = oldnmsgs;	/* whack it back down */
      stream->silent = silent;	/* restore old silent setting */
				/* notify upper level of new mailbox sizes */
      mail_exists (stream,nmsgs);
      mail_recent (stream,recent);
				/* mark dirty so O flags are set */
      if (recent) LOCAL->dirty = T;
    }
  }
				/* no change, don't babble if never got time */
  else if (LOCAL->filetime && LOCAL->filetime != sbuf.st_mtime)
    mm_log ("New mailbox modification time but apparently no changes",WARN);
				/* update parsed file size and time */
  LOCAL->filesize = sbuf.st_size;
  LOCAL->filetime = sbuf.st_mtime;
  return T;			/* return the winnage */
}

/* MMDF read line from mailbox
 * Accepts: mail stream
 *	    stringstruct
 *	    pointer to line size
 * Returns: pointer to input line
 */

char *mmdf_mbxline (MAILSTREAM *stream,STRING *bs,unsigned long *size)
{
  unsigned long i,j,k,m;
  char *s,*t,*te,p1[CHUNK];
  char *ret = "";
				/* flush old buffer */
  if (LOCAL->line) fs_give ((void **) &LOCAL->line);
				/* if buffer needs refreshing */
  if (!bs->cursize) SETPOS (bs,GETPOS (bs));
  if (SIZE (bs)) {		/* find newline */
				/* end of fast scan */
    te = (t = (s = bs->curpos) + bs->cursize) - 12;
    while (s < te) if ((*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
		       (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
		       (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
		       (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n')) {
      --s;			/* back up */
      break;			/* exit loop */
    }
				/* final character-at-a-time scan */
    while ((s < t) && (*s != '\n')) ++s;
				/* difficult case if line spans buffer */
    if ((i = s - bs->curpos) == bs->cursize) {
      memcpy (p1,bs->curpos,i);	/* remember what we have so far */
				/* load next buffer */
      SETPOS (bs,k = GETPOS (bs) + i);
				/* end of fast scan */
      te = (t = (s = bs->curpos) + bs->cursize) - 12;
				/* fast scan in overlap buffer */
      while (s < te) if ((*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
			 (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
			 (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n') ||
			 (*s++ == '\n') || (*s++ == '\n') || (*s++ == '\n')) {
	--s;			/* back up */
	break;			/* exit loop */
      }
				/* final character-at-a-time scan */
      while ((s < t) && (*s != '\n')) ++s;
				/* huge line? */
      if ((j = s - bs->curpos) == bs->cursize) {
	SETPOS (bs,GETPOS (bs) + j);
				/* look for end of line (s-l-o-w!!) */
	for (m = SIZE (bs); m && (SNX (bs) != '\n'); --m,++j);
	SETPOS (bs,k);		/* go back to where it started */
      }

				/* got size of data, make buffer for return */
      ret = LOCAL->line = (char *) fs_get (i + j + 2);
      memcpy (ret,p1,i);	/* copy first chunk */
      while (j) {		/* copy remainder */
	if (!bs->cursize) SETPOS (bs,GETPOS (bs));
	memcpy (ret + i,bs->curpos,k = min (j,bs->cursize));
	i += k;			/* account for this much read in */
	j -= k;
	bs->curpos += k;	/* increment new position */
	bs->cursize -= k;	/* eat that many bytes */
      }
      if (SIZE (bs)) SNX (bs);	/* skip over newline if one seen */
      ret[i++] = '\n';		/* make sure newline at end */
      ret[i] = '\0';		/* makes debugging easier */
    }
    else {			/* this is easy */
      ret = bs->curpos;		/* string it at this position */
      bs->curpos += ++i;	/* increment new position */
      bs->cursize -= i;		/* eat that many bytes */
    }
    *size = i;			/* return that to user */
  }
  else *size = 0;		/* end of data, return empty */
				/* embedded MMDF header at end of line? */
  if ((*size > sizeof (MMDFHDRTXT)) &&
      (s = ret + *size - (i = sizeof (MMDFHDRTXT) - 1)) && ISMMDF (s)) {
    SETPOS (bs,GETPOS (bs) - i);/* back up to start of MMDF header */
    *size -= i;			/* reduce length of line */
    ret[*size - 1] = '\n';	/* force newline at end */
  }
  return ret;
}

/* MMDF make pseudo-header
 * Accepts: MAIL stream
 *	    buffer to write pseudo-header
 * Returns: length of pseudo-header
 */

unsigned long mmdf_pseudo (MAILSTREAM *stream,char *hdr)
{
  int i;
  char *s,tmp[MAILTMPLEN];
  time_t now = time (0);
  rfc822_fixed_date (tmp);
  sprintf (hdr,"%sFrom %s %.24s\nDate: %s\nFrom: %s <%s@%.80s>\nSubject: %s\nMessage-ID: <%lu@%.80s>\nX-IMAP: %010lu %010lu",
	   mmdfhdr,pseudo_from,ctime (&now),
	   tmp,pseudo_name,pseudo_from,mylocalhost (),pseudo_subject,
	   (unsigned long) now,mylocalhost (),stream->uid_validity,
	   stream->uid_last);
  for (s = hdr + strlen (hdr),i = 0; i < NUSERFLAGS; ++i)
    if (stream->user_flags[i])
      sprintf (s += strlen (s)," %s",stream->user_flags[i]);
  sprintf (s += strlen (s),"\nStatus: RO\n\n%s\n%s",pseudo_msg,mmdfhdr);
  return strlen (hdr);
}

/* MMDF make status string
 * Accepts: MAIL stream
 *	    destination string to write
 *	    message cache entry
 *	    non-zero flag to write UID (.LT. 0 to write UID base info too)
 * Returns: length of string
 */

unsigned long mmdf_xstatus (MAILSTREAM *stream,char *status,MESSAGECACHE *elt,
			    long flag)
{
  char *t,stack[64];
  char *s = status;
  unsigned long n;
  int pad = 50;
  /* This used to use sprintf(), but thanks to certain cretinous C libraries
     with horribly slow implementations of sprintf() I had to change it to this
     mess.  At least it should be fast. */
				/* need to write X-IMAPbase: header? */
  if ((flag < 0) && !stream->uid_nosticky) {
    *s++ = 'X'; *s++ = '-'; *s++ = 'I'; *s++ = 'M'; *s++ = 'A'; *s++ = 'P';
    *s++ = 'b'; *s++ = 'a'; *s++ = 's'; *s++ = 'e'; *s++ = ':'; *s++ = ' ';
    t = stack;
    n = stream->uid_validity;	/* push UID validity digits on the stack */
    do *t++ = (char) (n % 10) + '0';
    while (n /= 10);
				/* pop UID validity digits from stack */
    while (t > stack) *s++ = *--t;
   *s++ = ' ';
    n = stream->uid_last;	/* push UID last digits on the stack */
    do *t++ = (char) (n % 10) + '0';
    while (n /= 10);
				/* pop UID last digits from stack */
    while (t > stack) *s++ = *--t;
    for (n = 0; n < NUSERFLAGS; ++n) if (t = stream->user_flags[n])
      for (*s++ = ' '; *t; *s++ = *t++);
    *s++ = '\n';
    pad += 30;			/* increased padding if have IMAPbase */
  }
  *s++ = 'S'; *s++ = 't'; *s++ = 'a'; *s++ = 't'; *s++ = 'u'; *s++ = 's';
  *s++ = ':'; *s++ = ' ';
  if (elt->seen) *s++ = 'R';
  *s++ = 'O'; *s++ = '\n';
  *s++ = 'X'; *s++ = '-'; *s++ = 'S'; *s++ = 't'; *s++ = 'a'; *s++ = 't';
  *s++ = 'u'; *s++ = 's'; *s++ = ':'; *s++ = ' ';
  if (elt->deleted) *s++ = 'D';
  if (elt->flagged) *s++ = 'F';
  if (elt->answered) *s++ = 'A';
  if (elt->draft) *s++ = 'T';
    *s++ = '\n';

  if (!stream->uid_nosticky) {	/* cretins with no life can't use this */
    *s++ = 'X'; *s++ = '-'; *s++ = 'K'; *s++ = 'e'; *s++ = 'y'; *s++ = 'w';
    *s++ = 'o'; *s++ = 'r'; *s++ = 'd'; *s++ = 's'; *s++ = ':';
    if (n = elt->user_flags) do {
      *s++ = ' ';
      for (t = stream->user_flags[find_rightmost_bit (&n)]; *t; *s++ = *t++);
    } while (n);
    n = s - status;		/* get size of stuff so far */
				/* pad X-Keywords to make size constant */
    if (n < pad) for (n = pad - n; n > 0; --n) *s++ = ' ';
    *s++ = '\n';
    if (flag) {			/* want to include UID? */
      t = stack;
      n = elt->private.uid;	/* push UID digits on the stack */
      do *t++ = (char) (n % 10) + '0';
      while (n /= 10);
      *s++ = 'X'; *s++ = '-'; *s++ = 'U'; *s++ = 'I'; *s++ = 'D'; *s++ = ':';
      *s++ = ' ';
				/* pop UID from stack */
      while (t > stack) *s++ = *--t;
      *s++ = '\n';
    }
  }
  *s++ = '\n'; *s = '\0';	/* end of extended message status */
  return s - status;		/* return size of resulting string */
}

/* Rewrite mailbox file
 * Accepts: MAIL stream, must be critical and locked
 *	    return pointer to number of expunged messages if want expunge
 *	    lock file name
 * Returns: T if success and mailbox unlocked, NIL if failure
 */

#define OVERFLOWBUFLEN 8192	/* initial overflow buffer length */

long mmdf_rewrite (MAILSTREAM *stream,unsigned long *nexp,DOTLOCK *lock)
{
  MESSAGECACHE *elt;
  MMDFFILE f;
  char *s;
  time_t tp[2];
  long ret,flag;
  unsigned long i,j;
  unsigned long recent = stream->recent;
  unsigned long size = LOCAL->pseudo ? mmdf_pseudo (stream,LOCAL->buf) : 0;
  if (nexp) *nexp = 0;		/* initially nothing expunged */
				/* calculate size of mailbox after rewrite */
  for (i = 1,flag = LOCAL->pseudo ? 1 : -1; i <= stream->nmsgs; i++)
    if (!(elt = mail_elt (stream,i))->deleted || !nexp) {
				/* add RFC822 size of this message */
      size += elt->private.special.text.size + elt->private.data +
	mmdf_xstatus (stream,LOCAL->buf,elt,flag) +
	  elt->private.msg.text.text.size + MMDFHDRLEN;
      flag = 1;			/* only count X-IMAPbase once */
    }
				/* extend the file as necessary */
  if (ret = mmdf_extend (stream,size)) {
    /* Set up buffered I/O file structure
     * curpos	current position being written through buffering
     * filepos	current position being written physically to the disk
     * bufpos	current position being written in the buffer
     * protect	current maximum position that can be written to the disk
     *		before buffering is forced
     * The code tries to buffer so that that disk is written in multiples of
     * OVERBLOWBUFLEN bytes.
     */
    f.stream = stream;		/* note mail stream */
    f.curpos = f.filepos = 0;	/* start of file */
    f.protect = stream->nmsgs ?	/* initial protection pointer */
    mail_elt (stream,1)->private.special.offset : 8192;
    f.bufpos = f.buf = (char *) fs_get (f.buflen = OVERFLOWBUFLEN);

    if (LOCAL->pseudo)		/* update pseudo-header */
      mmdf_write (&f,LOCAL->buf,mmdf_pseudo (stream,LOCAL->buf));
				/* loop through all messages */
    for (i = 1,flag = LOCAL->pseudo ? 1 : -1; i <= stream->nmsgs;) {
      elt = mail_elt (stream,i);/* get cache */
      if (nexp && elt->deleted){/* expunge this message? */
				/* one less recent message */
	if (elt->recent) --recent;
	mail_expunged(stream,i);/* notify upper levels */
	++*nexp;		/* count up one more expunged message */
      }
      else {			/* preserve this message */
	i++;			/* advance to next message */
	if ((flag < 0) ||	/* need to rewrite message? */
	    (elt->private.dirty || (f.curpos != elt->private.special.offset) ||
	     (elt->private.msg.header.text.size !=
	      (elt->private.data +
	       mmdf_xstatus (stream,LOCAL->buf,elt,flag))))) {
	  unsigned long newoffset = f.curpos;
				/* yes, seek to internal header */
	  lseek (LOCAL->fd,elt->private.special.offset,L_SET);
	  read (LOCAL->fd,LOCAL->buf,elt->private.special.text.size);
				/* see if need to squeeze out a CR */
	  if (LOCAL->buf[elt->private.special.text.size - 2] == '\r') {
	    LOCAL->buf[--elt->private.special.text.size - 1] = '\n';
	    --size;		/* squeezed out a CR from PC */
	  }
				/* protection pointer moves to RFC822 header */
	  f.protect = elt->private.special.offset +
	    elt->private.msg.header.offset;
				/* write internal header */
	  mmdf_write (&f,LOCAL->buf,elt->private.special.text.size);
				/* get RFC822 header */
	  s = mmdf_header (stream,elt->msgno,&j,FT_INTERNAL);
				/* in case this got decremented */
	  elt->private.msg.header.offset = elt->private.special.text.size;
				/* header size, sans trailing newline */
	  if ((j < 2) || (s[j - 2] == '\n')) j--;
	  if (j != elt->private.data) fatal ("header size inconsistent");
				/* protection pointer moves to RFC822 text */
	  f.protect = elt->private.special.offset +
	    elt->private.msg.text.offset;
	  mmdf_write (&f,s,j);	/* write RFC822 header */
				/* write status and UID */
	  mmdf_write (&f,LOCAL->buf,
		      j = mmdf_xstatus (stream,LOCAL->buf,elt,flag));
	  flag = 1;		/* only write X-IMAPbase once */
				/* new file header size */
	  elt->private.msg.header.text.size = elt->private.data + j;

				/* did text move? */
	  if (f.curpos != f.protect) {
				/* get message text */
	    s = mmdf_text_work (stream,elt,&j,FT_INTERNAL);
				/* this can happen if CRs were squeezed */
	    if (j < elt->private.msg.text.text.size) {
				/* so fix up counts */
	      size -= elt->private.msg.text.text.size - j;
	      elt->private.msg.text.text.size = j;
	    }
				/* can't happen it says here */
	    else if (j > elt->private.msg.text.text.size)
	      fatal ("text size inconsistent");
				/* new text offset, status/UID may change it */
	    elt->private.msg.text.offset = f.curpos - newoffset;
				/* protection pointer moves to next message */
	    f.protect = (i <= stream->nmsgs) ?
	      mail_elt (stream,i)->private.special.offset :
		(f.curpos + j + MMDFHDRLEN);
	    mmdf_write (&f,s,j);/* write text */
				/* write trailing newline */
	    mmdf_write (&f,mmdfhdr,MMDFHDRLEN);
	  }
	  else {		/* tie off header and status */
	    mmdf_write (&f,NIL,NIL);
	    f.curpos = f.protect =/* restart everything at end of message */
	      f.filepos += elt->private.msg.text.text.size + MMDFHDRLEN;
	  }
				/* new internal header offset */
	  elt->private.special.offset = newoffset;
	  elt->private.dirty =NIL;/* message is now clean */
	}
	else {			/* no need to rewrite this message */
				/* tie off previous message if needed */
	  mmdf_write (&f,NIL,NIL);
	  f.curpos = f.protect =/* restart everything at end of message */
	    f.filepos += elt->private.special.text.size +
	      elt->private.msg.header.text.size +
		elt->private.msg.text.text.size + MMDFHDRLEN;
	}
      }
    }

    mmdf_write (&f,NIL,NIL);	/* tie off final message */
    if (size != f.filepos) fatal ("file size inconsistent");
    fs_give ((void **) &f.buf);	/* free buffer */
				/* make sure tied off */
    ftruncate (LOCAL->fd,LOCAL->filesize = size);
    fsync (LOCAL->fd);		/* make sure the updates take */
    if (size && (flag < 0)) fatal ("lost UID base information");
    LOCAL->dirty = NIL;		/* no longer dirty */
  				/* notify upper level of new mailbox sizes */
    mail_exists (stream,stream->nmsgs);
    mail_recent (stream,recent);
				/* set atime to now, mtime a second earlier */
    tp[1] = (tp[0] = time (0)) - 1;
				/* set the times, note change */
    if (!utime (stream->mailbox,tp)) LOCAL->filetime = tp[1];
    close (LOCAL->fd);		/* close and reopen file */
    if ((LOCAL->fd = open (stream->mailbox,O_RDWR,NIL)) < 0) {
      sprintf (LOCAL->buf,"Mailbox open failed, aborted: %s",strerror (errno));
      mm_log (LOCAL->buf,ERROR);
      mmdf_abort (stream);
    }
    dotlock_unlock (lock);	/* flush the lock file */
  }
  return ret;			/* return state from algorithm */
}

/* Extend MMDF mailbox file
 * Accepts: MAIL stream
 *	    new desired size
 * Return: T if success, else NIL
 */

long mmdf_extend (MAILSTREAM *stream,unsigned long size)
{
  unsigned long i = (size > LOCAL->filesize) ? size - LOCAL->filesize : 0;
  if (i) {			/* does the mailbox need to grow? */
    if (i > LOCAL->buflen) {	/* make sure have enough space */
				/* this user won the lottery all right */
      fs_give ((void **) &LOCAL->buf);
      LOCAL->buf = (char *) fs_get ((LOCAL->buflen = i) + 1);
    }
    memset (LOCAL->buf,'\0',i);	/* get a block of nulls */
    while (T) {			/* until write successful or punt */
      lseek (LOCAL->fd,LOCAL->filesize,L_SET);
      if ((write (LOCAL->fd,LOCAL->buf,i) >= 0) && !fsync (LOCAL->fd)) break;
      else {
	long e = errno;		/* note error before doing ftruncate */
	ftruncate (LOCAL->fd,LOCAL->filesize);
	if (mm_diskerror (stream,e,NIL)) {
	  fsync (LOCAL->fd);	/* user chose to punt */
	  sprintf (LOCAL->buf,"Unable to extend mailbox: %s",strerror (e));
	  mm_log (LOCAL->buf,ERROR);
	  return NIL;
	}
      }
    }
  }
  return LONGT;
}

/* Write data to buffered file
 * Accepts: buffered file pointer
 *	    file data or NIL to indicate "flush buffer"
 *	    date size (ignored for "flush buffer")
 * Does not return until success
 */

void mmdf_write (MMDFFILE *f,char *buf,unsigned long size)
{
  unsigned long i,j,k;
  if (buf) {			/* doing buffered write? */
    i = f->bufpos - f->buf;	/* yes, get size of current buffer data */
				/* yes, have space in current buffer chunk? */
    if (j = i ? ((f->buflen - i) % OVERFLOWBUFLEN) : f->buflen) {
				/* yes, fill up buffer as much as we can */
      memcpy (f->bufpos,buf,k = min (j,size));
      f->bufpos += k;		/* new buffer position */
      f->curpos += k;		/* new current position */
      if (j -= k) return;	/* all done if still have buffer free space */
      buf += k;			/* full, get new unwritten data pointer */
      size -= k;		/* new data size */
      i += k;			/* new buffer data size */
    }
    /* This chunk of the buffer is full.  See if can make some space by
     * writing to the disk, if there's enough unprotected space to do so.
     * Try to fill out any unaligned chunk, along with any subsequent full
     * chunks that will fit in unprotected space.
     */
				/* any unprotected space we can write to? */
    if (j = min (i,f->protect - f->filepos)) {
				/* yes, filepos not at chunk boundary? */
      if ((k = f->filepos % OVERFLOWBUFLEN) && ((k = OVERFLOWBUFLEN - k) < j))
	j -= k;			/* yes, and can write out partial chunk */
      else k = 0;		/* no partial chunk to write */
				/* if at least a chunk free, write that too */
      if (j > OVERFLOWBUFLEN) k += j - (j % OVERFLOWBUFLEN);
      if (k) {			/* write data if there is anything we can */
	mmdf_phys_write (f,f->buf,k);
				/* slide buffer */
	if (i -= k) memmove (f->buf,f->buf + k,i);
	f->bufpos = f->buf + i;	/* new end of buffer */
      }
    }

    /* Have flushed the buffer as best as possible.  All done if no more
     * data to write.  Otherwise, if the buffer is empty AND if the unwritten
     * data is larger than a chunk AND the unprotected space is also larger
     * than a chunk, then write as many chunks as we can directly from the
     * data.  Buffer the rest, expanding the buffer as needed.
     */
    if (size) {			/* have more data that we need to buffer? */
				/* can write any of it to disk instead? */
      if ((f->bufpos == f->buf) && 
	  ((j = min (f->protect - f->filepos,size)) > OVERFLOWBUFLEN)) {
				/* write as much as we can right now */
	mmdf_phys_write (f,buf,j -= (j % OVERFLOWBUFLEN));
	buf += j;		/* new data pointer */
	size -= j;		/* new data size */
	f->curpos += j;		/* advance current pointer */
      }
      if (size) {		/* still have data that we need to buffer? */
				/* yes, need to expand the buffer? */
	if ((i = ((f->bufpos + size) - f->buf)) > f->buflen) {
				/* note current position in buffer */
	  j = f->bufpos - f->buf;
	  i += OVERFLOWBUFLEN;	/* yes, grow another chunk */
	  fs_resize ((void **) &f->buf,f->buflen = i - (i % OVERFLOWBUFLEN));
				/* in case buffer relocated */
	  f->bufpos = f->buf + j;
	}
				/* buffer remaining data */
	memcpy (f->bufpos,buf,size);
	f->bufpos += size;	/* new end of buffer */
	f->curpos += size;	/* advance current pointer */
      }
    }
  }
  else {			/* flush buffer to disk */
    mmdf_phys_write (f,f->buf,i = f->bufpos - f->buf);
    f->bufpos = f->buf;		/* reset buffer */
				/* update positions */
    f->curpos = f->protect = f->filepos;
  }
}

/* Physical disk write
 * Accepts: buffered file pointer
 *	    buffer address
 *	    buffer size
 * Does not return until success
 */

void mmdf_phys_write (MMDFFILE *f,char *buf,size_t size)
{
  MAILSTREAM *stream = f->stream;
				/* write data at desired position */
  while (size && ((lseek (LOCAL->fd,f->filepos,L_SET) < 0) ||
		  (write (LOCAL->fd,buf,size) < 0))) {
    int e;
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Unable to write to mailbox: %s",strerror (e = errno));
    mm_log (tmp,ERROR);
    mm_diskerror (NIL,e,T);	/* serious problem, must retry */
  }
  f->filepos += size;		/* update file position */
}
