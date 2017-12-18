#include	"u.h"
#include	<trace.h>
#include	"tos.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"ureg.h"
#include	"edf.h"

#include	<pool.h>

enum
{
	Qdir,
	Qtrace,
	Qargs,
	Qctl,
	Qfd,
	Qfpregs,
	Qkregs,
	Qmem,
	Qnote,
	Qnoteid,
	Qnotepg,
	Qns,
	Qppid,
	Qproc,
	Qregs,
	Qsegment,
	Qstatus,
	Qtext,
	Qwait,
	Qprofile,
	Qsyscall,
	Qwatchpt,
};

enum
{
	CMclose,
	CMclosefiles,
	CMfixedpri,
	CMhang,
	CMkill,
	CMnohang,
	CMnoswap,
	CMpri,
	CMprivate,
	CMprofile,
	CMstart,
	CMstartstop,
	CMstartsyscall,
	CMstop,
	CMwaitstop,
	CMwired,
	CMtrace,
	CMinterrupt,
	CMnointerrupt,
	/* real time */
	CMperiod,
	CMdeadline,
	CMcost,
	CMsporadic,
	CMdeadlinenotes,
	CMadmit,
	CMextra,
	CMexpel,
	CMevent,
};

enum{
	Nevents = 0x4000,
	Emask = Nevents - 1,
};

#define	STATSIZE	(2*KNAMELEN+12+9*12)
/*
 * Status, fd, and ns are left fully readable (0444) because of their use in debugging,
 * particularly on shared servers.
 * Arguably, ns and fd shouldn't be readable; if you'd prefer, change them to 0000
 */
Dirtab procdir[] =
{
	"args",		{Qargs},	0,			0660,
	"ctl",		{Qctl},		0,			0000,
	"fd",		{Qfd},		0,			0444,
	"fpregs",	{Qfpregs},	sizeof(FPsave),		0000,
	"kregs",	{Qkregs},	sizeof(Ureg),		0400,
	"mem",		{Qmem},		0,			0000,
	"note",		{Qnote},	0,			0000,
	"noteid",	{Qnoteid},	0,			0664,
	"notepg",	{Qnotepg},	0,			0000,
	"ns",		{Qns},		0,			0644,
	"ppid",		{Qppid},	0,			0444,
	"proc",		{Qproc},	0,			0400,
	"regs",		{Qregs},	sizeof(Ureg),		0000,
	"segment",	{Qsegment},	0,			0444,
	"status",	{Qstatus},	STATSIZE,		0444,
	"text",		{Qtext},	0,			0000,
	"wait",		{Qwait},	0,			0400,
	"profile",	{Qprofile},	0,			0400,
	"syscall",	{Qsyscall},	0,			0400,	
	"watchpt",	{Qwatchpt},	0,			0600,
};

static
Cmdtab proccmd[] = {
	CMclose,		"close",		2,
	CMclosefiles,		"closefiles",		1,
	CMfixedpri,		"fixedpri",		2,
	CMhang,			"hang",			1,
	CMnohang,		"nohang",		1,
	CMnoswap,		"noswap",		1,
	CMkill,			"kill",			1,
	CMpri,			"pri",			2,
	CMprivate,		"private",		1,
	CMprofile,		"profile",		1,
	CMstart,		"start",		1,
	CMstartstop,		"startstop",		1,
	CMstartsyscall,		"startsyscall",		1,
	CMstop,			"stop",			1,
	CMwaitstop,		"waitstop",		1,
	CMwired,		"wired",		2,
	CMtrace,		"trace",		0,
	CMinterrupt,		"interrupt",		1,
	CMnointerrupt,		"nointerrupt",		1,
	CMperiod,		"period",		2,
	CMdeadline,		"deadline",		2,
	CMcost,			"cost",			2,
	CMsporadic,		"sporadic",		1,
	CMdeadlinenotes,	"deadlinenotes",	1,
	CMadmit,		"admit",		1,
	CMextra,		"extra",		1,
	CMexpel,		"expel",		1,
	CMevent,		"event",		1,
};

/* Segment type from portdat.h */
static char *sname[]={ "Text", "Data", "Bss", "Stack", "Shared", "Phys", "Fixed", "Sticky" };

/*
 * Qids are, in path:
 *	 4 bits of file type (qids above)
 *	23 bits of process slot number + 1
 *	     in vers,
 *	32 bits of pid, for consistency checking
 * If notepg, c->pgrpid.path is pgrp slot, .vers is noteid.
 */
#define	QSHIFT	5	/* location in qid of proc slot # */

#define	QID(q)		((((ulong)(q).path)&0x0000001F)>>0)
#define	SLOT(q)		(((((ulong)(q).path)&0x07FFFFFE0)>>QSHIFT)-1)
#define	PID(q)		((q).vers)
#define	NOTEID(q)	((q).vers)

void	procctlreq(Proc*, char*, int);
void procnsreq(Proc*, char*, int);
long	procctlmemio(Chan*, Proc*, uintptr, void*, long, int);
Chan*	proctext(Chan*, Proc*);
int	procstopped(void*);
ulong	procpagecount(Proc *);
long procbindmount(int ismount, int fd, int afd, char* arg0, char* arg1, ulong flag, char* spec, Proc* targp);

static Traceevent *tevents;
static Lock tlock;
static int topens;
static int tproduced, tconsumed;
void (*proctrace)(Proc*, int, vlong);

static void
profclock(Ureg *ur, Timer *)
{
	Tos *tos;

	if(up == nil || up->state != Running)
		return;

	/* user profiling clock */
	if(userureg(ur)){
		tos = (Tos*)(USTKTOP-sizeof(Tos));
		tos->clock += TK2MS(1);
		segclock(ur->pc);
	}
}

static int lenwatchpt(Proc *);

/* begin sysfile.c borrowed functions */
static void
punlockfgrp(Fgrp *f)
{
	int ex;

	ex = f->exceed;
	f->exceed = 0;
	unlock(f);
	if(ex)
		pprint("warning: process exceeds %d file descriptors\n", ex);
}

int
pgrowfd(Fgrp *f, int fd)	/* fd is always >= 0 */
{
	Chan **newfd, **oldfd;

	if(fd < f->nfd)
		return 0;
	if(fd >= f->nfd+DELTAFD)
		return -1;	/* out of range */
	/*
	 * Unbounded allocation is unwise; besides, there are only 16 bits
	 * of fid in 9P
	 */
	if(f->nfd >= 5000){
    Exhausted:
		print("no free file descriptors\n");
		return -1;
	}
	newfd = malloc((f->nfd+DELTAFD)*sizeof(Chan*));
	if(newfd == 0)
		goto Exhausted;
	oldfd = f->fd;
	memmove(newfd, oldfd, f->nfd*sizeof(Chan*));
	f->fd = newfd;
	free(oldfd);
	f->nfd += DELTAFD;
	if(fd > f->maxfd){
		if(fd/100 > f->maxfd/100)
			f->exceed = (fd/100)*100;
		f->maxfd = fd;
	}
	return 1;
}

/*
 *  this assumes that the fgrp is locked
 */
int
pfindfreefd(Fgrp *f, int start)
{
	int fd;

	for(fd=start; fd<f->nfd; fd++)
		if(f->fd[fd] == 0)
			break;
	if(fd >= f->nfd && pgrowfd(f, fd) < 0)
		return -1;
	return fd;
}

Chan*
pfdtochan(int fd, int mode, int chkmnt, int iref, Proc *targp)
{
	Chan *c;
	Fgrp *f;

	c = 0;
	f = targp->fgrp;

	lock(f);
	if(fd<0 || f->nfd<=fd || (c = f->fd[fd])==0) {
		unlock(f);
		error(Ebadfd);
	}
	if(iref)
		incref(c);
	unlock(f);

	if(chkmnt && (c->flag&CMSG)) {
		if(iref)
			cclose(c);
		error(Ebadusefd);
	}

	if(mode<0 || c->mode==ORDWR)
		return c;

	if((mode&OTRUNC) && c->mode==OREAD) {
		if(iref)
			cclose(c);
		error(Ebadusefd);
	}

	if((mode&~OTRUNC) != c->mode) {
		if(iref)
			cclose(c);
		error(Ebadusefd);
	}

	return c;
}

int
pnewfd(Chan *c, Proc *targp)
{
	int fd;
	Fgrp *f;

	f = targp->fgrp;
	lock(f);
	fd = pfindfreefd(f, 0);
	if(fd < 0){
		punlockfgrp(f);
		return -1;
	}
	if(fd > f->maxfd)
		f->maxfd = fd;
	f->fd[fd] = c;
	punlockfgrp(f);
	return fd;
}

uintptr
psysopen(Proc *targp, va_list list)
{
	int fd;
	Chan *c;
	char *filename;
	ulong flags;

	filename = va_arg(list, char*);
	flags = va_arg(list, ulong);

	openmode(flags);	/* error check only */
//	validaddr(arg[0], 1, 0);
	c = pnamec(filename, Aopen, flags, 0, targp);
	if(waserror()){
		cclose(c);
		nexterror();
	}
	fd = pnewfd(c, targp);
	if(fd < 0)
		error(Enofd);
	poperror();
	return (uintptr)fd;
}

void
pfdclose(int fd, int flag, Proc *targp)
{
	int i;
	Chan *c;
	Fgrp *f = targp->fgrp;

	lock(f);
	c = f->fd[fd];
	if(c == 0){
		/* can happen for users with shared fd tables */
		unlock(f);
		return;
	}
	if(flag){
		if(c==0 || !(c->flag&flag)){
			unlock(f);
			return;
		}
	}
	f->fd[fd] = 0;
	if(fd == f->maxfd)
		for(i=fd; --i>=0 && f->fd[i]==0; )
			f->maxfd = i;

	unlock(f);
	cclose(c);
}
/* End borrowed functions from sysfile.c */

static int
procgen(Chan *c, char *name, Dirtab *tab, int, int s, Dir *dp)
{
	Qid qid;
	Proc *p;
	char *ename;
	Segment *q;
	ulong pid, path, perm, len;

	if(s == DEVDOTDOT){
		mkqid(&qid, Qdir, 0, QTDIR);
		devdir(c, qid, "#p", 0, eve, 0555, dp);
		return 1;
	}

	if(c->qid.path == Qdir){
		if(s == 0){
			strcpy(up->genbuf, "trace");
			mkqid(&qid, Qtrace, -1, QTFILE);
			devdir(c, qid, up->genbuf, 0, eve, 0444, dp);
			return 1;
		}

		if(name != nil){
			/* ignore s and use name to find pid */
			pid = strtol(name, &ename, 10);
			if(pid==0 || ename[0]!='\0')
				return -1;
			s = procindex(pid);
			if(s < 0)
				return -1;
		}
		else if(--s >= conf.nproc)
			return -1;

		p = proctab(s);
		pid = p->pid;
		if(pid == 0)
			return 0;
		sprint(up->genbuf, "%lud", pid);
		/*
		 * String comparison is done in devwalk so name must match its formatted pid
		*/
		if(name != nil && strcmp(name, up->genbuf) != 0)
			return -1;
		mkqid(&qid, (s+1)<<QSHIFT, pid, QTDIR);
		devdir(c, qid, up->genbuf, 0, p->user, DMDIR|0555, dp);
		return 1;
	}
	if(c->qid.path == Qtrace){
		strcpy(up->genbuf, "trace");
		mkqid(&qid, Qtrace, -1, QTFILE);
		devdir(c, qid, up->genbuf, 0, eve, 0444, dp);
		return 1;
	}
	if(s >= nelem(procdir))
		return -1;
	if(tab)
		panic("procgen");

	tab = &procdir[s];
	path = c->qid.path&~(((1<<QSHIFT)-1));	/* slot component */

	/* p->procmode determines default mode for files in /proc */
	p = proctab(SLOT(c->qid));
	perm = tab->perm;
	if(perm == 0)
		perm = p->procmode;
	else	/* just copy read bits */
		perm |= p->procmode & 0444;

	len = tab->length;
	switch(QID(c->qid)) {
	case Qwait:
		len = p->nwait;	/* incorrect size, but >0 means there's something to read */
		break;
	case Qprofile:
		q = p->seg[TSEG];
		if(q != nil && q->profile != nil) {
			len = (q->top-q->base)>>LRESPROF;
			len *= sizeof(*q->profile);
		}
		break;
	}
	switch(QID(tab->qid)){
	case Qwatchpt:
		len = lenwatchpt(p);
		break;
	}

	mkqid(&qid, path|tab->qid.path, c->qid.vers, QTFILE);
	devdir(c, qid, tab->name, len, p->user, perm, dp);
	return 1;
}

static void
_proctrace(Proc* p, Tevent etype, vlong ts)
{
	Traceevent *te;

	if (p->trace == 0 || topens == 0 ||
		tproduced - tconsumed >= Nevents)
		return;

	te = &tevents[tproduced&Emask];
	te->pid = p->pid;
	te->etype = etype;
	if (ts == 0)
		te->time = todget(nil);
	else
		te->time = ts;
	tproduced++;
}

static void
procinit(void)
{
	if(conf.nproc >= (1<<(16-QSHIFT))-1)
		print("warning: too many procs for devproc\n");
	addclock0link((void (*)(void))profclock, 113);	/* Relative prime to HZ */
}

static Chan*
procattach(char *spec)
{
	return devattach('p', spec);
}

static Walkqid*
procwalk(Chan *c, Chan *nc, char **name, int nname)
{
	return devwalk(c, nc, name, nname, 0, 0, procgen);
}

static int
procstat(Chan *c, uchar *db, int n)
{
	return devstat(c, db, n, 0, 0, procgen);
}

/*
 *  none can't read or write state on other
 *  processes.  This is to contain access of
 *  servers running as none should they be
 *  subverted by, for example, a stack attack.
 */
static void
nonone(Proc *p)
{
	if(p == up)
		return;
	if(strcmp(up->user, "none") != 0)
		return;
	if(iseve())
		return;
	error(Eperm);
}

static void clearwatchpt(Proc *p);

static Chan*
procopen(Chan *c, int omode0)
{
	Proc *p;
	Pgrp *pg;
	Chan *tc;
	int pid;
	int omode;

	if(c->qid.type & QTDIR)
		return devopen(c, omode0, 0, 0, procgen);

	if(QID(c->qid) == Qtrace){
		if (omode0 != OREAD) 
			error(Eperm);
		lock(&tlock);
		if (waserror()){
			topens--;
			unlock(&tlock);
			nexterror();
		}
		if (topens++ > 0)
			error("already open");
		if (tevents == nil){
			tevents = (Traceevent*)malloc(sizeof(Traceevent) * Nevents);
			if(tevents == nil)
				error(Enomem);
			tproduced = tconsumed = 0;
		}
		proctrace = _proctrace;
		unlock(&tlock);
		poperror();

		c->mode = openmode(omode0);
		c->flag |= COPEN;
		c->offset = 0;
		return c;
	}
		
	p = proctab(SLOT(c->qid));
	eqlock(&p->debug);
	if(waserror()){
		qunlock(&p->debug);
		nexterror();
	}
	pid = PID(c->qid);
	if(p->pid != pid)
		error(Eprocdied);

	omode = openmode(omode0);

	switch(QID(c->qid)){
	case Qtext:
		if(omode != OREAD)
			error(Eperm);
		tc = proctext(c, p);
		tc->offset = 0;
		qunlock(&p->debug);
		poperror();
		cclose(c);
		return tc;

	case Qproc:
	case Qkregs:
	case Qsegment:
	case Qprofile:
	case Qns:
		if((omode != OREAD) && (p->privatemem))
			error(Eperm);
		break;

	case Qfd:
		if(omode != OREAD)
			error(Eperm);
		break;

	case Qnote:
		if(p->privatemem)
			error(Eperm);
		break;

	case Qmem:
	case Qctl:
		if(p->privatemem)
			error(Eperm);
		nonone(p);
		break;

	case Qargs:
	case Qnoteid:
	case Qstatus:
	case Qwait:
	case Qregs:
	case Qfpregs:
	case Qsyscall:	
	case Qppid:
	case Qwatchpt:
		nonone(p);
		break;

	case Qnotepg:
		nonone(p);
		pg = p->pgrp;
		if(pg == nil)
			error(Eprocdied);
		if(omode!=OWRITE)
			error(Eperm);
		c->pgrpid.path = pg->pgrpid+1;
		c->pgrpid.vers = p->noteid;
		break;

	default:
		print("procopen %#lux\n", QID(c->qid));
		error(Egreg);
	}

	/* Affix pid to qid */
	if(p->state != Dead)
		c->qid.vers = p->pid;

	/* make sure the process slot didn't get reallocated while we were playing */
	coherence();
	if(p->pid != pid)
		error(Eprocdied);

	tc = devopen(c, omode, 0, 0, procgen);
	if(waserror()){
		cclose(tc);
		nexterror();
	}
	
	switch(QID(c->qid)){
	case Qwatchpt:
		if((omode0 & OTRUNC) != 0)
			clearwatchpt(p);
		break;
	}
	
	poperror();
	qunlock(&p->debug);
	poperror();

	return tc;
}

static int
procwstat(Chan *c, uchar *db, int n)
{
	Proc *p;
	Dir *d;

	if(c->qid.type&QTDIR)
		error(Eperm);

	if(QID(c->qid) == Qtrace)
		return devwstat(c, db, n);
		
	p = proctab(SLOT(c->qid));
	nonone(p);
	d = nil;

	eqlock(&p->debug);
	if(waserror()){
		qunlock(&p->debug);
		free(d);
		nexterror();
	}

	if(p->pid != PID(c->qid))
		error(Eprocdied);

	if(strcmp(up->user, p->user) != 0 && !iseve())
		error(Eperm);

	d = smalloc(sizeof(Dir)+n);
	n = convM2D(db, n, &d[0], (char*)&d[1]);
	if(n == 0)
		error(Eshortstat);
	if(!emptystr(d->uid) && strcmp(d->uid, p->user) != 0){
		if(!iseve())
			error(Eperm);
		kstrdup(&p->user, d->uid);
	}
	/* p->procmode determines default mode for files in /proc */
	if(d->mode != ~0UL)
		p->procmode = d->mode&0777;

	qunlock(&p->debug);
	poperror();
	free(d);
	return n;
}

static void
procclose(Chan *c)
{
	Segio *sio;

	if((c->flag & COPEN) == 0)
		return;

	switch(QID(c->qid)){
	case Qtrace:
		lock(&tlock);
		if(topens > 0)
			topens--;
		if(topens == 0)
			proctrace = nil;
		unlock(&tlock);
		return;
	case Qmem:
		sio = c->aux;
		if(sio != nil){
			c->aux = nil;
			segio(sio, nil, nil, 0, 0, 0);
			free(sio);
		}
		return;
	}
}

static int
procargs(Proc *p, char *buf, int nbuf)
{
	int j, k, m;
	char *a;
	int n;

	a = p->args;
	if(p->setargs){
		snprint(buf, nbuf, "%s [%s]", p->text, p->args);
		return strlen(buf);
	}
	n = p->nargs;
	for(j = 0; j < nbuf - 1; j += m){
		if(n <= 0)
			break;
		if(j != 0)
			buf[j++] = ' ';
		m = snprint(buf+j, nbuf-j, "%q",  a);
		k = strlen(a) + 1;
		a += k;
		n -= k;
	}
	return j;
}

static int
eventsavailable(void *)
{
	return tproduced > tconsumed;
}

static int
prochaswaitq(void *x)
{
	Chan *c;
	Proc *p;

	c = (Chan *)x;
	p = proctab(SLOT(c->qid));
	return p->pid != PID(c->qid) || p->waitq != nil;
}

static void
int2flag(int flag, char *s)
{
	if(flag == 0){
		*s = '\0';
		return;
	}
	*s++ = '-';
	if(flag & MAFTER)
		*s++ = 'a';
	if(flag & MBEFORE)
		*s++ = 'b';
	if(flag & MCREATE)
		*s++ = 'c';
	if(flag & MCACHE)
		*s++ = 'C';
	*s = '\0';
}

static int
readns1(Chan *c, Proc *p, char *buf, int nbuf)
{
	Pgrp *pg;
	Mount *t, *cm;
	Mhead *f, *mh;
	ulong minid, bestmid;
	char flag[10], *srv;
	int i;

	pg = p->pgrp;
	if(pg == nil || p->dot == nil || p->pid != PID(c->qid))
		error(Eprocdied);

	bestmid = ~0;
	minid = c->nrock;
	if(minid == bestmid)
		return 0;

	rlock(&pg->ns);

	mh = nil;
	cm = nil;
	for(i = 0; i < MNTHASH; i++) {
		for(f = pg->mnthash[i]; f != nil; f = f->hash) {
			for(t = f->mount; t != nil; t = t->next) {
				if(t->mountid >= minid && t->mountid < bestmid) {
					bestmid = t->mountid;
					cm = t;
					mh = f;
				}
			}
		}
	}

	if(bestmid == ~0) {
		c->nrock = bestmid;
		i = snprint(buf, nbuf, "cd %s\n", p->dot->path->s);
	} else {
		c->nrock = bestmid+1;

		int2flag(cm->mflag, flag);
		if(strcmp(cm->to->path->s, "#M") == 0){
			srv = srvname(cm->to->mchan);
			if(srv == nil)
				srv = zrvname(cm->to->mchan);
			i = snprint(buf, nbuf, "mount %s %s %s %s\n", flag,
				srv==nil? cm->to->mchan->path->s : srv,
				mh->from->path->s, cm->spec? cm->spec : "");
			free(srv);
		}else{
			i = snprint(buf, nbuf, "bind %s %s %s\n", flag,
				cm->to->path->s, mh->from->path->s);
		}
	}

	runlock(&pg->ns);

	return i;
}

int
procfdprint(Chan *c, int fd, char *s, int ns)
{
	return snprint(s, ns, "%3d %.2s %C %4ld (%.16llux %lud %.2ux) %5ld %8lld %s\n",
		fd,
		&"r w rw"[(c->mode&3)<<1],
		devtab[c->type]->dc, c->dev,
		c->qid.path, c->qid.vers, c->qid.type,
		c->iounit, c->offset, c->path->s);
}

static int
readfd1(Chan *c, Proc *p, char *buf, int nbuf)
{
	Fgrp *fg;
	int n, i;

	fg = p->fgrp;
	if(fg == nil || p->dot == nil || p->pid != PID(c->qid))
		return 0;

	if(c->nrock == 0){
		c->nrock = 1;
		return snprint(buf, nbuf, "%s\n", p->dot->path->s);
	}

	lock(fg);
	n = 0;
	for(;;){
		i = c->nrock-1;
		if(i < 0 || i > fg->maxfd)
			break;
		c->nrock++;
		if(fg->fd[i] != nil){
			n = procfdprint(fg->fd[i], i, buf, nbuf);
			break;
		}
	}
	unlock(fg);

	return n;
}

/*
 * setupwatchpts(Proc *p, Watchpt *wp, int nwp) is defined for all arches separately.
 * It tests whether wp is a valid set of watchpoints and errors out otherwise.
 * If and only if they are valid, it sets up all watchpoints (clearing any preexisting ones).
 * This is to make sure that failed writes to watchpt don't touch the existing watchpoints.
 */

static void
clearwatchpt(Proc *p)
{
	setupwatchpts(p, nil, 0);
	free(p->watchpt);
	p->watchpt = nil;
	p->nwatchpt = 0;
}

static int
lenwatchpt(Proc *pr)
{
	/* careful, not holding debug lock */
	return pr->nwatchpt * (10 + 4 * sizeof(uintptr));
}

static int
readwatchpt(Proc *pr, char *buf, int nbuf)
{
	char *p, *e;
	Watchpt *w;
	
	p = buf;
	e = buf + nbuf;
	/* careful, length has to match lenwatchpt() */
	for(w = pr->watchpt; w < pr->watchpt + pr->nwatchpt; w++)
		p = seprint(p, e, sizeof(uintptr) == 8 ? "%c%c%c %#.16p %#.16p\n" : "%c%c%c %#.8p %#.8p\n",
			(w->type & WATCHRD) != 0 ? 'r' : '-',
			(w->type & WATCHWR) != 0 ? 'w' : '-',
			(w->type & WATCHEX) != 0 ? 'x' : '-',
			(void *) w->addr, (void *) w->len);
	return p - buf;
}

static int
writewatchpt(Proc *pr, char *buf, int nbuf, uvlong offset)
{
	char *p, *q, *e;
	char line[256], *f[4];
	Watchpt *wp, *wq;
	int rc, nwp, nwp0;
	uvlong x;
	
	p = buf;
	e = buf + nbuf;
	if(offset != 0)
		nwp0 = pr->nwatchpt;
	else
		nwp0 = 0;
	nwp = 0;
	for(q = p; q < e; q++)
		nwp += *q == '\n';
	if(nwp > 65536) error(Egreg);
	wp = malloc((nwp0+nwp) * sizeof(Watchpt));
	if(wp == nil) error(Enomem);
	if(waserror()){
		free(wp);
		nexterror();
	}
	if(nwp0 > 0)
		memmove(wp, pr->watchpt, sizeof(Watchpt) * nwp0);
	for(wq = wp + nwp0;;){
		q = memchr(p, '\n', e - p);
		if(q == nil)
			break;
		if(q - p > sizeof(line) - 1)
			error("line too long");
		memmove(line, p, q - p);
		line[q - p] = 0;
		p = q + 1;
		
		rc = tokenize(line, f, nelem(f));
		if(rc == 0) continue;
		if(rc != 3)
			error("wrong number of fields");
		for(q = f[0]; *q != 0; q++)
			switch(*q){
			case 'r': if((wq->type & WATCHRD) != 0) goto tinval; wq->type |= WATCHRD; break;
			case 'w': if((wq->type & WATCHWR) != 0) goto tinval; wq->type |= WATCHWR; break;
			case 'x': if((wq->type & WATCHEX) != 0) goto tinval; wq->type |= WATCHEX; break;
			case '-': break;
			default: tinval: error("invalid type");
			}
		x = strtoull(f[1], &q, 0);
		if(f[1] == q || *q != 0 || x != (uintptr) x) error("invalid address");
		wq->addr = x;
		x = strtoull(f[2], &q, 0);
		if(f[2] == q || *q != 0 || x > (uintptr)-wq->addr) error("invalid length");
		wq->len = x;
		if(wq->addr + wq->len > USTKTOP) error("bad address");
		wq++;
	}
	nwp = wq - (wp + nwp0);
	if(nwp == 0 && nwp0 == pr->nwatchpt){
		poperror();
		free(wp);
		return p - buf;
	}
	setupwatchpts(pr, wp, nwp0 + nwp);
	poperror();
	free(pr->watchpt);
	pr->watchpt = wp;
	pr->nwatchpt = nwp0 + nwp;
	return p - buf;
}

/*
 * userspace can't pass negative file offset for a
 * 64 bit kernel address, so we use 63 bit and sign
 * extend to 64 bit.
 */
static uintptr
off2addr(vlong off)
{
	off <<= 1;
	off >>= 1;
	return off;
}

static long
procread(Chan *c, void *va, long n, vlong off)
{
	char *a, *sps, statbuf[1024];
	int i, j, navail, ne, rsize;
	ulong l;
	uchar *rptr;
	uintptr addr;
	ulong offset;
	Confmem *cm;
	Proc *p;
	Segment *sg, *s;
	Ureg kur;
	Waitq *wq;
	
	a = va;
	offset = off;
	if(c->qid.type & QTDIR)
		return devdirread(c, a, n, 0, 0, procgen);

	if(QID(c->qid) == Qtrace){
		if(!eventsavailable(nil))
			return 0;

		rptr = (uchar*)va;
		navail = tproduced - tconsumed;
		if(navail > n / sizeof(Traceevent))
			navail = n / sizeof(Traceevent);
		while(navail > 0) {
			ne = ((tconsumed & Emask) + navail > Nevents)? 
					Nevents - (tconsumed & Emask): navail;
			memmove(rptr, &tevents[tconsumed & Emask], 
					ne * sizeof(Traceevent));

			tconsumed += ne;
			rptr += ne * sizeof(Traceevent);
			navail -= ne;
		}
		return rptr - (uchar*)va;
	}

	p = proctab(SLOT(c->qid));
	if(p->pid != PID(c->qid))
		error(Eprocdied);

	switch(QID(c->qid)){
	case Qargs:
		eqlock(&p->debug);
		j = procargs(p, statbuf, sizeof(statbuf));
		qunlock(&p->debug);
		if(offset >= j)
			return 0;
		if(offset+n > j)
			n = j-offset;
	statbufread:
		memmove(a, statbuf+offset, n);
		return n;

	case Qsyscall:
		eqlock(&p->debug);
		if(waserror()){
			qunlock(&p->debug);
			nexterror();
		}
		if(p->pid != PID(c->qid))
			error(Eprocdied);
		j = 0;
		if(p->syscalltrace != nil)
			j = readstr(offset, a, n, p->syscalltrace);
		qunlock(&p->debug);
		poperror();
		return j;

	case Qmem:
		addr = off2addr(off);
		if(addr < KZERO)
			return procctlmemio(c, p, addr, va, n, 1);

		if(!iseve() || poolisoverlap(secrmem, (uchar*)addr, n))
			error(Eperm);

		/* validate kernel addresses */
		if(addr < (uintptr)end) {
			if(addr+n > (uintptr)end)
				n = (uintptr)end - addr;
			memmove(a, (char*)addr, n);
			return n;
		}
		for(i=0; i<nelem(conf.mem); i++){
			cm = &conf.mem[i];
			/* klimit-1 because klimit might be zero! */
			if(cm->kbase <= addr && addr <= cm->klimit-1){
				if(addr+n >= cm->klimit-1)
					n = cm->klimit - addr;
				memmove(a, (char*)addr, n);
				return n;
			}
		}
		error(Ebadarg);

	case Qprofile:
		s = p->seg[TSEG];
		if(s == nil || s->profile == nil)
			error("profile is off");
		i = (s->top-s->base)>>LRESPROF;
		i *= sizeof(s->profile[0]);
		if(i < 0 || offset >= i)
			return 0;
		if(offset+n > i)
			n = i - offset;
		memmove(a, ((char*)s->profile)+offset, n);
		return n;

	case Qnote:
		eqlock(&p->debug);
		if(waserror()){
			qunlock(&p->debug);
			nexterror();
		}
		if(p->pid != PID(c->qid))
			error(Eprocdied);
		if(n < 1)	/* must accept at least the '\0' */
			error(Etoosmall);
		if(p->nnote == 0)
			n = 0;
		else {
			i = strlen(p->note[0].msg) + 1;
			if(i < n)
				n = i;
			memmove(a, p->note[0].msg, n-1);
			a[n-1] = '\0';
			if(--p->nnote == 0)
				p->notepending = 0;
			memmove(p->note, p->note+1, p->nnote*sizeof(Note));
		}
		poperror();
		qunlock(&p->debug);
		return n;

	case Qproc:
		if(offset >= sizeof(Proc))
			return 0;
		if(offset+n > sizeof(Proc))
			n = sizeof(Proc) - offset;
		memmove(a, ((char*)p)+offset, n);
		return n;

	case Qregs:
		rptr = (uchar*)p->dbgreg;
		rsize = sizeof(Ureg);
		goto regread;

	case Qkregs:
		memset(&kur, 0, sizeof(Ureg));
		setkernur(&kur, p);
		rptr = (uchar*)&kur;
		rsize = sizeof(Ureg);
		goto regread;

	case Qfpregs:
		if(p->fpstate != FPinactive)
			error(Enoreg);
		rptr = (uchar*)p->fpsave;
		rsize = sizeof(FPsave);
	regread:
		if(rptr == nil)
			error(Enoreg);
		if(offset >= rsize)
			return 0;
		if(offset+n > rsize)
			n = rsize - offset;
		memmove(a, rptr+offset, n);
		return n;

	case Qstatus:
		if(offset >= STATSIZE)
			return 0;
		if(offset+n > STATSIZE)
			n = STATSIZE - offset;

		sps = p->psstate;
		if(sps == nil)
			sps = statename[p->state];

		memset(statbuf, ' ', sizeof statbuf);
		readstr(0, statbuf+0*KNAMELEN, KNAMELEN-1, p->text);
		readstr(0, statbuf+1*KNAMELEN, KNAMELEN-1, p->user);
		readstr(0, statbuf+2*KNAMELEN, 11, sps);

		j = 2*KNAMELEN + 12;
		for(i = 0; i < 6; i++) {
			l = p->time[i];
			if(i == TReal)
				l = MACHP(0)->ticks - l;
			readnum(0, statbuf+j+NUMSIZE*i, NUMSIZE, tk2ms(l), NUMSIZE);
		}

		readnum(0, statbuf+j+NUMSIZE*6, NUMSIZE, procpagecount(p)*BY2PG/1024, NUMSIZE);
		readnum(0, statbuf+j+NUMSIZE*7, NUMSIZE, p->basepri, NUMSIZE);
		readnum(0, statbuf+j+NUMSIZE*8, NUMSIZE, p->priority, NUMSIZE);
		goto statbufread;

	case Qsegment:
		j = 0;
		for(i = 0; i < NSEG; i++) {
			sg = p->seg[i];
			if(sg == nil)
				continue;
			j += sprint(statbuf+j, "%-6s %c%c %8p %8p %4ld\n",
				sname[sg->type&SG_TYPE],
				sg->type&SG_FAULT ? 'F' : (sg->type&SG_RONLY ? 'R' : ' '),
				sg->profile ? 'P' : ' ',
				sg->base, sg->top, sg->ref);
		}
		if(offset >= j)
			return 0;
		if(offset+n > j)
			n = j-offset;
		goto statbufread;

	case Qwait:
		if(!canqlock(&p->qwaitr))
			error(Einuse);

		if(waserror()) {
			qunlock(&p->qwaitr);
			nexterror();
		}

		lock(&p->exl);
		while(p->waitq == nil && p->pid == PID(c->qid)) {
			if(up == p && p->nchild == 0) {
				unlock(&p->exl);
				error(Enochild);
			}
			unlock(&p->exl);
			sleep(&p->waitr, prochaswaitq, c);
			lock(&p->exl);
		}
		if(p->pid != PID(c->qid)){
			unlock(&p->exl);
			error(Eprocdied);
		}
		wq = p->waitq;
		p->waitq = wq->next;
		p->nwait--;
		unlock(&p->exl);

		qunlock(&p->qwaitr);
		poperror();

		j = snprint(statbuf, sizeof(statbuf), "%d %lud %lud %lud %q",
			wq->w.pid,
			wq->w.time[TUser], wq->w.time[TSys], wq->w.time[TReal],
			wq->w.msg);
		free(wq);
		if(j < n)
			n = j;
		offset = 0;
		goto statbufread;

	case Qns:
	case Qfd:
		eqlock(&p->debug);
		if(waserror()){
			qunlock(&p->debug);
			nexterror();
		}
		if(offset == 0 || offset != c->mrock)
			c->nrock = c->mrock = 0;
		do {
			if(QID(c->qid) == Qns)
				j = readns1(c, p, statbuf, sizeof(statbuf));
			else
				j = readfd1(c, p, statbuf, sizeof(statbuf));
			if(j == 0)
				break;
			c->mrock += j;
		} while(c->mrock <= offset);
		i = c->mrock - offset;
		qunlock(&p->debug);
		poperror();

		if(i <= 0 || i > j)
			return 0;
		if(i < n)
			n = i;
		offset = j - i;
		goto statbufread;

	case Qnoteid:
		return readnum(offset, va, n, p->noteid, NUMSIZE);

	case Qppid:
		return readnum(offset, va, n, p->parentpid, NUMSIZE);
	
	case Qwatchpt:
		eqlock(&p->debug);
		j = readwatchpt(p, statbuf, sizeof(statbuf));
		qunlock(&p->debug);
		if(offset >= j)
			return 0;
		if(offset+n > j)
			n = j - offset;
		goto statbufread;

	}
	error(Egreg);
	return 0;		/* not reached */
}

static long
procwrite(Chan *c, void *va, long n, vlong off)
{
	int id, m;
	Proc *p, *t, *et;
	char *a, *arg, buf[ERRMAX];
	ulong offset;

	a = va;
	offset = off;
	if(c->qid.type & QTDIR)
		error(Eisdir);

	p = proctab(SLOT(c->qid));

	/* Use the remembered noteid in the channel rather
	 * than the process pgrpid
	 */
	if(QID(c->qid) == Qnotepg) {
		pgrpnote(NOTEID(c->pgrpid), va, n, NUser);
		return n;
	}

	eqlock(&p->debug);
	if(waserror()){
		if(p->debug.locked == 1)
			qunlock(&p->debug);
		nexterror();
	}
	if(p->pid != PID(c->qid))
		error(Eprocdied);

	switch(QID(c->qid)){
	case Qargs:
		if(n == 0)
			error(Eshort);
		if(n >= ERRMAX)
			error(Etoobig);
		arg = malloc(n+1);
		if(arg == nil)
			error(Enomem);
		memmove(arg, va, n);
		m = n;
		if(arg[m-1] != 0)
			arg[m++] = 0;
		free(p->args);
		p->nargs = m;
		p->args = arg;
		p->setargs = 1;
		break;

	case Qmem:
		if(p->state != Stopped)
			error(Ebadctl);
		n = procctlmemio(c, p, off2addr(off), va, n, 0);
		break;

	case Qregs:
		if(offset >= sizeof(Ureg))
			n = 0;
		else if(offset+n > sizeof(Ureg))
			n = sizeof(Ureg) - offset;
		if(p->dbgreg == nil)
			error(Enoreg);
		setregisters(p->dbgreg, (char*)(p->dbgreg)+offset, va, n);
		break;

	case Qfpregs:
		if(offset >= sizeof(FPsave))
			n = 0;
		else if(offset+n > sizeof(FPsave))
			n = sizeof(FPsave) - offset;
		if(p->fpstate != FPinactive || p->fpsave == nil)
			error(Enoreg);
		memmove((uchar*)p->fpsave+offset, va, n);
		break;

	case Qctl:
		procctlreq(p, va, n);
		break;

	case Qnote:
		if(p->kp)
			error(Eperm);
		if(n >= ERRMAX-1)
			error(Etoobig);
		memmove(buf, va, n);
		buf[n] = 0;
		if(!postnote(p, 0, buf, NUser))
			error("note not posted");
		break;
	case Qnoteid:
		if(p->kp)
			error(Eperm);
		id = atoi(a);
		if(id <= 0)
			error(Ebadarg);
		if(id == p->pid) {
			p->noteid = id;
			break;
		}
		t = proctab(0);
		for(et = t+conf.nproc; t < et; t++) {
			if(t->state == Dead || t->kp)
				continue;
			if(id == t->noteid) {
				nonone(t);
				if(strcmp(p->user, t->user) != 0)
					error(Eperm);
				p->noteid = id;
				break;
			}
		}
		if(p->noteid != id)
			error(Ebadarg);
		break;
	case Qwatchpt:
		writewatchpt(p, va, n, off);
		break;
	case Qns:
//		print("procnsreq on p, %s, %ld\n", a, n);
		if(p->debug.locked == 1)
			qunlock(&p->debug);
		nonone(p);
		procnsreq(p, va, n);
		break;

	default:
		print("unknown qid in procwrite\n");
		error(Egreg);
	}
	poperror();
	if(p->debug.locked == 1)
		qunlock(&p->debug);
	return n;
}

Dev procdevtab = {
	'p',
	"proc",

	devreset,
	procinit,
	devshutdown,
	procattach,
	procwalk,
	procstat,
	procopen,
	devcreate,
	procclose,
	procread,
	devbread,
	procwrite,
	devbwrite,
	devremove,
	procwstat,
};

Chan*
proctext(Chan *c, Proc *p)
{
	Chan *tc;
	Image *i;
	Segment *s;

	s = p->seg[TSEG];
	if(s == nil)
		error(Enonexist);
	if(p->state==Dead)
		error(Eprocdied);

	i = s->image;
	if(i == nil)
		error(Eprocdied);

	lock(i);
	if(waserror()) {
		unlock(i);
		nexterror();
	}
		
	tc = i->c;
	if(tc == nil)
		error(Eprocdied);

	if(incref(tc) == 1 || (tc->flag&COPEN) == 0 || tc->mode != OREAD) {
		cclose(tc);
		error(Eprocdied);
	}

	if(p->pid != PID(c->qid)) {
		cclose(tc);
		error(Eprocdied);
	}

	unlock(i);
	poperror();

	return tc;
}

void
procstopwait(Proc *p, int ctl)
{
	int pid;

	if(p->pdbg != nil)
		error(Einuse);
	if(procstopped(p) || p->state == Broken)
		return;
	pid = p->pid;
	if(pid == 0)
		error(Eprocdied);
	if(ctl != 0)
		p->procctl = ctl;
	if(p == up)
		return;
	p->pdbg = up;
	qunlock(&p->debug);
	up->psstate = "Stopwait";
	if(waserror()) {
		qlock(&p->debug);
		p->pdbg = nil;
		nexterror();
	}
	sleep(&up->sleep, procstopped, p);
	poperror();
	qlock(&p->debug);
	if(p->pid != pid)
		error(Eprocdied);
}

void
procctlclosefiles(Proc *p, int all, int fd)
{
	Fgrp *f;
	Chan *c;

	if(fd < 0)
		error(Ebadfd);
	f = p->fgrp;
	if(f == nil)
		error(Eprocdied);

	incref(f);
	lock(f);
	while(fd <= f->maxfd){
		c = f->fd[fd];
		if(c != nil){
			f->fd[fd] = nil;
			unlock(f);
			qunlock(&p->debug);
			cclose(c);
			qlock(&p->debug);
			lock(f);
		}
		if(!all)
			break;
		fd++;
	}
	unlock(f);
	closefgrp(f);
}

static char *
parsetime(vlong *rt, char *s)
{
	uvlong ticks;
	ulong l;
	char *e, *p;
	static int p10[] = {100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};

	if (s == nil)
		return("missing value");
	ticks=strtoul(s, &e, 10);
	if (*e == '.'){
		p = e+1;
		l = strtoul(p, &e, 10);
		if(e-p > nelem(p10))
			return "too many digits after decimal point";
		if(e-p == 0)
			return "ill-formed number";
		l *= p10[e-p-1];
	}else
		l = 0;
	if (*e == '\0' || strcmp(e, "s") == 0){
		ticks = 1000000000 * ticks + l;
	}else if (strcmp(e, "ms") == 0){
		ticks = 1000000 * ticks + l/1000;
	}else if (strcmp(e, "µs") == 0 || strcmp(e, "us") == 0){
		ticks = 1000 * ticks + l/1000000;
	}else if (strcmp(e, "ns") != 0)
		return "unrecognized unit";
	*rt = ticks;
	return nil;
}

void
procctlreq(Proc *p, char *va, int n)
{
	Segment *s;
	int npc, pri;
	Cmdbuf *cb;
	Cmdtab *ct;
	vlong time;
	char *e;
	void (*pt)(Proc*, int, vlong);

	if(p->kp)	/* no ctl requests to kprocs */
		error(Eperm);

	cb = parsecmd(va, n);
	if(waserror()){
		free(cb);
		nexterror();
	}

	ct = lookupcmd(cb, proccmd, nelem(proccmd));

	switch(ct->index){
	case CMclose:
		procctlclosefiles(p, 0, atoi(cb->f[1]));
		break;
	case CMclosefiles:
		procctlclosefiles(p, 1, 0);
		break;
	case CMhang:
		p->hang = 1;
		break;
	case CMkill:
		switch(p->state) {
		case Broken:
			unbreak(p);
			break;
		case Stopped:
			p->procctl = Proc_exitme;
			postnote(p, 0, "sys: killed", NExit);
			ready(p);
			break;
		default:
			p->procctl = Proc_exitme;
			postnote(p, 0, "sys: killed", NExit);
		}
		break;
	case CMnohang:
		p->hang = 0;
		break;
	case CMnoswap:
		p->noswap = 1;
		break;
	case CMpri:
		pri = atoi(cb->f[1]);
		if(pri > PriNormal && !iseve())
			error(Eperm);
		procpriority(p, pri, 0);
		break;
	case CMfixedpri:
		pri = atoi(cb->f[1]);
		if(pri > PriNormal && !iseve())
			error(Eperm);
		procpriority(p, pri, 1);
		break;
	case CMprivate:
		p->privatemem = 1;
		break;
	case CMprofile:
		s = p->seg[TSEG];
		if(s == nil || (s->type&SG_TYPE) != SG_TEXT)
			error(Ebadctl);
		if(s->profile != nil)
			free(s->profile);
		npc = (s->top-s->base)>>LRESPROF;
		s->profile = malloc(npc*sizeof(*s->profile));
		if(s->profile == nil)
			error(Enomem);
		break;
	case CMstart:
		if(p->state != Stopped)
			error(Ebadctl);
		ready(p);
		break;
	case CMstartstop:
		if(p->state != Stopped)
			error(Ebadctl);
		p->procctl = Proc_traceme;
		ready(p);
		procstopwait(p, Proc_traceme);
		break;
	case CMstartsyscall:
		if(p->state != Stopped)
			error(Ebadctl);
		p->procctl = Proc_tracesyscall;
		ready(p);
		procstopwait(p, Proc_tracesyscall);
		break;
	case CMstop:
		procstopwait(p, Proc_stopme);
		break;
	case CMwaitstop:
		procstopwait(p, 0);
		break;
	case CMwired:
		procwired(p, atoi(cb->f[1]));
		break;
	case CMtrace:
		switch(cb->nf){
		case 1:
			p->trace ^= 1;
			break;
		case 2:
			p->trace = (atoi(cb->f[1]) != 0);
			break;
		default:
			error("args");
		}
		break;
	case CMinterrupt:
		postnote(p, 0, nil, NUser);
		break;
	case CMnointerrupt:
		if(p->nnote == 0)
			p->notepending = 0;
		else
			error("notes pending");
		break;
	/* real time */
	case CMperiod:
		if(p->edf == nil)
			edfinit(p);
		if(e=parsetime(&time, cb->f[1]))	/* time in ns */
			error(e);
		edfstop(p);
		p->edf->T = time/1000;	/* Edf times are in µs */
		break;
	case CMdeadline:
		if(p->edf == nil)
			edfinit(p);
		if(e=parsetime(&time, cb->f[1]))
			error(e);
		edfstop(p);
		p->edf->D = time/1000;
		break;
	case CMcost:
		if(p->edf == nil)
			edfinit(p);
		if(e=parsetime(&time, cb->f[1]))
			error(e);
		edfstop(p);
		p->edf->C = time/1000;
		break;
	case CMsporadic:
		if(p->edf == nil)
			edfinit(p);
		p->edf->flags |= Sporadic;
		break;
	case CMdeadlinenotes:
		if(p->edf == nil)
			edfinit(p);
		p->edf->flags |= Sendnotes;
		break;
	case CMadmit:
		if(p->edf == nil)
			error("edf params");
		if(e = edfadmit(p))
			error(e);
		break;
	case CMextra:
		if(p->edf == nil)
			edfinit(p);
		p->edf->flags |= Extratime;
		break;
	case CMexpel:
		if(p->edf != nil)
			edfstop(p);
		break;
	case CMevent:
		pt = proctrace;
		if(up->trace && pt != nil)
			pt(up, SUser, 0);
		break;
	}

	poperror();
	free(cb);
}

int
procstopped(void *a)
{
	return ((Proc*)a)->state == Stopped;
}

long
procctlmemio(Chan *c, Proc *p, uintptr offset, void *a, long n, int read)
{
	Segio *sio;
	Segment *s;
	int i;

	s = seg(p, offset, 0);
	if(s == nil)
		error(Ebadarg);
	eqlock(&p->seglock);
	if(waserror()) {
		qunlock(&p->seglock);
		nexterror();
	}
	sio = c->aux;
	if(sio == nil){
		sio = smalloc(sizeof(Segio));
		c->aux = sio;
	}
	for(i = 0; i < NSEG; i++) {
		if(p->seg[i] == s)
			break;
	}
	if(i == NSEG)
		error(Egreg);	/* segment gone */
	eqlock(s);
	if(waserror()){
		qunlock(s);
		nexterror();
	}
	if(!read && (s->type&SG_TYPE) == SG_TEXT) {
		s = txt2data(s);
		p->seg[i] = s;
	}
	offset -= s->base;
	incref(s);		/* for us while we copy */
	qunlock(s);
	poperror();
	qunlock(&p->seglock);
	poperror();

	if(waserror()) {
		putseg(s);
		nexterror();
	}
	n = segio(sio, s, a, n, offset, read);
	putseg(s);
	poperror();

	if(!read)
		p->newtlb = 1;

	return n;
}
/* modified version of bindmount from sysfile.c. This operates on a target process */

long
procbindmount(int ismount, int fd, int afd, char* arg0, char* arg1, ulong flag, char* spec, Proc* targp)
{
	int ret;
	Chan *c0, *c1, *ac, *bc;

	if((flag&~MMASK) || (flag&MORDER)==(MBEFORE|MAFTER))
		error(Ebadarg);

	if(ismount){
		if(waserror()) {
			print("nsmod /proc mounts locked on process %uld\n", targp->pid);
			nexterror();
		}
		if(canqlock(&targp->procmount) == 0){
			error("mounts on this process via /proc locked by previous error");
			poperror();
			return -1;
		}
		poperror();

//		validaddr((ulong)spec, 1, 0);
		spec = validnamedup(spec, 1);
		if(waserror()){
			free(spec);
			nexterror();
		}

		if(targp->pgrp->noattach)
			error(Enoattach);

		ac = nil;
		bc = pfdtochan(fd, ORDWR, 0, 1, targp);
		if(waserror()) {
			if(ac)
				cclose(ac);
			cclose(bc);
			nexterror();
		}

		if(afd >= 0)
			ac = pfdtochan(afd, ORDWR, 0, 1, targp);

//		ret = devno('M', 0);
//		c0 = devtab[ret]->attach((char*)&bogus);
		c0 = mntattach(bc, ac, spec, flag&MCACHE);
		qunlock(&targp->procmount);
//		print("c0 devtab attach assigned\n");
		poperror();	/* ac bc */
		if(ac)
			cclose(ac);
		cclose(bc);
	}else{
		spec = nil;
//		validaddr((ulong)arg0, 1, 0);
//		print("c0 = pnamec(%s, Abind, 0, 0, targp)\n", arg0);
		c0 = pnamec(arg0, Abind, 0, 0, targp);
	}

	if(waserror()){
		cclose(c0);
		nexterror();
	}
//	validaddr((ulong)arg1, 1, 0);
//	print("c1 = pnamec(%s, Abind, 0, 0, targp)\n", arg1);
	c1 = pnamec(arg1, Amount, 0, 0, targp);
	if(waserror()){
		cclose(c1);
		nexterror();
	}
//	print("ret = pcmount(&c0, c1, flag, bogus.spec, targp)\n");
	ret = pcmount(&c0, c1, flag, spec, targp);

	poperror();
	cclose(c1);
	poperror();
	cclose(c0);
	if(ismount){
		pfdclose(fd, 0, targp);
		poperror();	/* spec */
		free(spec);	
	}
	return ret;
}

long
procunmount(char *new, char *old, Proc *targp)
{
	Chan *cmount, *cmounted;

	cmounted = 0;

//	validaddr(arg[1], 1, 0);
	cmount = pnamec(old, Amount, 0, 0, targp);

	if(new) {
		if(waserror()) {
			cclose(cmount);
			nexterror();
		}
//		validaddr(arg[0], 1, 0);
		/*
		 * This has to be namec(..., Aopen, ...) because
		 * if arg[0] is something like /srv/cs or /fd/0,
		 * opening it is the only way to get at the real
		 * Chan underneath.
		 */
		cmounted = pnamec(new, Aopen, OREAD, 0, targp);
		poperror();
	}

	if(waserror()) {
		cclose(cmount);
		if(cmounted)
			cclose(cmounted);
		nexterror();
	}

	pcunmount(cmount, cmounted, targp);
	cclose(cmount);
	if(cmounted)
		cclose(cmounted);
	poperror();
	return 0;
}

int
psysopenwrap(Proc *p, int nargs, ...)
{
	int fd;
	va_list args;
	va_start(args,nargs);
	fd=psysopen(p, args);
	va_end(args);
	return fd;
}

/* parses writes on the ns file converts to namespace operations */
/* the completed command is sent to procbindmount or procunmount */

void
procnsreq(Proc *p, char *va, int n)
{
	Cmdbuf *cbf;
	char *new, *old, *charflags, *spec;
	ulong flags;
	int fd;
	int countnf;

	flags=MREPL;
	countnf=1;
	spec = "";

	cbf = parsecmd(va, n);
	if(waserror()){
		free(cbf);
		nexterror();
	}

//	print("cbf->nf=%d\n", cbf->nf);
	if((cbf->nf < 2) || (cbf->nf > 5)){
		error(Ebadarg);
		poperror();
		return;
	} else if(cbf->nf == 2){
		if(strcmp(cbf->f[0], "unmount") != 0){
			error(Ebadarg);
			poperror();
			return;		
		}
		new = nil;
		old = cbf->f[1];
		if(strncmp(old, "#|", 2)==0){
			error(Ebadsharp);
			poperror();
			return;
		}
//		print("procunmount(everything from %s on %uld)\n", old, p->pid);
		procunmount(new, old, p);
		poperror();
		return;		
	} else if(cbf->nf == 3){
		new = cbf->f[1];
		old = cbf->f[2];
	} else {
		if(*(cbf->f[1]) == '-'){
			charflags = cbf->f[1];
			for(int i = 1; *(charflags+i) != '\0'; i++){
				if(*(charflags+i) == 'a')
					flags |= MAFTER;
				if(*(charflags+i) == 'b')
					flags |= MBEFORE;
				if(*(charflags+i) == 'c')
					flags |= MCREATE;
				if(*(charflags+i) == 'C')
					flags |= MCACHE;
			}
			countnf++;
		}
		new = cbf->f[countnf++];
		old = cbf->f[countnf++];
		if(countnf < cbf->nf){
			spec=cbf->f[countnf];
		}
	}
	if(strcmp(cbf->f[0], "bind")==0){
//		print("procbindmount(0, -1, -1, %s, %s, %uld, nil, %uld)\n", new, old, flags, p->pid);
		procbindmount(0, -1, -1, new, old, flags, nil, p);
	} else if (strcmp(cbf->f[0], "mount")==0){
		if((strncmp(new, "#|", 2)==0) || (strncmp(old, "#|", 2)==0)){
			error(Ebadsharp);
			poperror();
			return;
		}
//		print("psysopenwrap(targp, 2, %s, ORDWR)\n", new);
		fd=psysopenwrap(p, 2, new, ORDWR);
		if(fd < 0){
			error(Ebadfd);
			poperror();
			return;
		}
//		print("procbindmount(1, %d, -1, nil, %s, %uld, spec %s, %uld)\n", fd, old, flags, spec, p->pid);
		procbindmount(1, fd, -1, nil, old, flags, spec, p);
	} else if (strcmp(cbf->f[0], "unmount")==0){
		/* forbid access to pipe devices */
		if((strncmp(new, "#|/", 3)==0) || (strncmp(old, "#|", 2)==0)){
			error(Ebadsharp);
			poperror();
			return;
		}
		/* forbid access to tls device */
		if((strncmp(new, "#a/", 3)==0) || (strncmp(old, "#a", 2)==0)){
			error(Ebadsharp);
			poperror();
			return;
		}
//		print("procunmount(%s from %s on %uld)\n", new, old, p->pid);
		procunmount(new, old, p);
	}

	poperror();
	free(cbf);
}
