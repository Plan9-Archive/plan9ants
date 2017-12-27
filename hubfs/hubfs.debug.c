#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

/* provides input/output multiplexing for 'screen' like functionality */
/* usually used in combination with hubshell client and hub wrapper script */

/* These flags are used to set the state of queued 9p requests and also ketchup/wait in paranoid mode */
enum flags{
	UP = 1,
	DOWN = 2,
	WAIT = 3,
	DONE = 4,
};

enum buffersizes{
	BUCKSIZE = 777777,			/* Make this bigger if you want larger buffers */
	MAGIC = 77777,			/* In paranoid mode let readers lag this many bytes */
	MAXQ = 13,				/* Maximum number of 9p requests to queue */
	SMBUF = 777,				/* Just for names, small strings, etc */
};

typedef struct Hub	Hub;			/* A Hub file functions as a multiplexed pipe-like data buffer */
typedef struct Msgq	Msgq;		/* The Msgq is a per-client fid structure to track location */

struct Hub{
	char name[SMBUF];			/* name */
	char bucket[BUCKSIZE];		/* data buffer */
	char *inbuckp;				/* location to store next message */
	int buckfull;				/* amount of data stored in bucket */
	char *buckwrap;			/* exact limit of written data before pointer reset */
	Req *qreqs[MAXQ];			/* pointers to queued read Reqs */
	int rstatus[MAXQ];			/* status of read requests */
	int qrnum;				/* index of read Reqs waiting to be filled */
	int qrans;					/* number of read Reqs answered */
//	int qrorphans;				/* Unanswered Reqs when qreqs wraps around */
	Req *qwrits[MAXQ];			/* Similar for write Reqs */
	int wstatus[MAXQ];
	int qwnum;
	int qwans;
	int ketchup;				/* used to track lag of readers relative to writers in paranoid mode */
	int tomatoflag;				/* readers put up the tomatoflag to tell writers to wait for them  */
	QLock wrlk;				/* writer lock during fear */
	QLock replk;				/* reply lock during fear */
	int killme;					/* in paranoid mode we fork new procs and need to kill old ones */
};

struct Msgq{
	ulong myfid;				/* Msgq is associated with client fids */
	char *nxt;					/* Location of this client in the buffer */
	int bufuse;				/* how much of the buffer has been used */
};

char *srvname;
int paranoia;					/* In paranoid mode loose reader/writer sync is maintained */
int freeze;						/* In frozen mode the hubs operate simply as a ramfs */
int trunc;						/* In trunc mode clients auto-truncate files when opened */

static char Ebad[] = "something bad happened";
static char Enomem[] = "no memory";

void wrsend(Hub *h);
void msgsend(Hub *h);
void hubcmd(char *cmd);
void zerohub(Hub *h);
void fsread(Req *r);
void fswrite(Req *r);
void fscreate(Req *r);
void fsopen(Req *r);
void fsdestroyfile(File *f);
void usage(void);

Srv fs = {
	.open=	fsopen,
	.read=	fsread,
	.write=	fswrite,
	.create=	fscreate,
};

/*
 * Basic logic - we have a buffer/bucket of data (a hub) that is mapped to a file.
 * For each hub we keep two queues of 9p requests, one for reads and one for writes.
 * As requests come in, we add them to the queue, then fill queued requests that are waiting.
 * The data buffers are statically sized at creation. This means that data is continuously read
 * and written in a "rotating" pattern. When we reach the end, we wrap back around to the beginning. 
 * Our job is accurately transferring the bytes in and out of the bucket and 
 * tracking the location of the 'read and write heads' for each writer and each reader. 
*/

/* msgsend replies to Reqs queued by fsread */
void
msgsend(Hub *h)
{
	Req *r;
	Msgq *mq;
	u32int count;
	int i;

#ifdef DEBUG
	print("msgsend %x\n", h);
#endif

	if(h->qrnum == 0){
#ifdef DEBUG
		print("\th->qrnum == 0, returning\n");
#endif
		return;
	}

	/* LOOP through all queued 9p read requests for this hub and answer if needed */
	for(i = h->qrans; i <= h->qrnum; i++){
#ifdef DEBUG
		print("\tin read loop, counter == %d, h->qrans == %d, h->qrnum == %d\n", i, h->qrans, h->qrnum);
#endif
		if(paranoia == UP){
			qlock(&h->replk);
		}
		if(h->rstatus[i] != WAIT){
#ifdef DEBUG
			print("\th->rstatus[%d] == %d !=WAIT\n", i, h->rstatus[i]);
#endif
			if((i == h->qrans) && (i < h->qrnum)){
#ifdef DEBUG
			print("\t(counter %d == h->qrans) && (counter %d < h->qrnum(%d)), incrementing h->qrans\n", i, i, h->qrnum);
#endif
				h->qrans++;
			}
			if(paranoia == UP){
				qunlock(&h->replk);
			}
			continue;
		}
		/* request found, if it has already read all data then keep it waiting for more and return */
		r = h->qreqs[i];
		mq = r->fid->aux;
#ifdef DEBUG
		print("\treq found, r = h->qreqs[%d], mq = r->fid->aux = %x\n", i, mq);
#endif
		if(mq->nxt == h->inbuckp){
#ifdef DEBUG
			print("\treader has read all data, mq->nxt %x == h->inbuckp %x\n", mq->nxt, h->inbuckp);
#endif

			if(paranoia == UP){
				qunlock(&h->replk);
			}
			continue;
		}
		count = r->ifcall.count;

		/* BUCKET WRAPAROUND LOGIC CHECK HERE FOR BUGS */
		if(mq->nxt >= h->buckwrap){
			mq->nxt = h->bucket;
			mq->bufuse = 0;
		}
		if(mq->nxt + count > h->buckwrap){
			count = h->buckwrap - mq->nxt;
		}
		if((mq->bufuse + count > h->buckfull) && (mq->bufuse < h->buckfull)){
			count = h->buckfull - mq->bufuse;
		}

		/* Done with wraparound checks, now we can send the data */
		memmove(r->ofcall.data, mq->nxt, count);
		r->ofcall.count = count;
		mq->nxt += count;
		mq->bufuse += count;
		h->rstatus[i] = DONE;
#ifdef DEBUG
		print("\tabout to respond, h->rstatus[%d] = DONE\n", i);
#endif
		if((i == h->qrans) && (i < h->qrnum)){
#ifdef DEBUG
			print("\t(i == h->qrans == %d) && (i < h->qrnum(%d)), incrementing qrans\n", i, h->qrnum);
#endif
			h->qrans++;
		}
		respond(r, nil);

		if(paranoia == UP){
			h->ketchup = mq->bufuse;
			if(mq->bufuse <= h->buckfull){
				h->tomatoflag = DOWN;	/* DOWN means do not wait for us */
			} else {
				h->tomatoflag = UP;
			}
			qunlock(&h->replk);
		}
	}
}	

/* wrsend replies to Reqs queued by fswrite */
void
wrsend(Hub *h)
{
	Req *r;
	u32int count;
	int i;
	int j;

	if(h->qwnum == 0){
		return;
	}

	if(paranoia == UP){		/* If we are paranoid, we fork and slack off while the readers catch up */
		qlock(&h->wrlk);
		if((h->ketchup < h->buckfull - MAGIC) || (h->ketchup > h->buckfull)){
			if(rfork(RFPROC|RFMEM) == 0){
				sleep(100);
				h->killme = UP;
				for(j = 0; ((j < 77) && (h->tomatoflag == UP)); j++){
					sleep(7);		/* Give the readers some time to catch up and drop the flag */
				}
			} else {
				return;	/* We want this flow of control to become an incoming read request */
			}
		}
	}
	/* LOOP through queued 9p write requests for this hub */
	for(i = h->qwans; i <= h->qwnum; i++){
		if(h->wstatus[i] != WAIT){
			if((i == h->qwans) && (i < h->qwnum)){
				h->qwans++;
			}
			continue;
		}
		r = h->qwrits[i];
		count = r->ifcall.count;

		/* BUCKET WRAPAROUND LOGIC CHECK HERE FOR BUGS */
		if((h->buckfull + count) >= BUCKSIZE - 8192){
			h->buckwrap = h->inbuckp;
			h->inbuckp = h->bucket;
			h->buckfull = 0;
		}

		/* Move the data into the bucket, update our counters, and respond */
		memmove(h->inbuckp, r->ifcall.data, count);
		h->inbuckp += count;
		h->buckfull += count;
		r->fid->file->length = h->buckfull;
		r->ofcall.count = count;
		h->wstatus[i] = DONE;
		if((i == h->qwans) && (i < h->qwnum)){
			h->qwans++;
		}
		respond(r, nil);

		if(paranoia == UP){
			if(h->wrlk.locked == 1){
				qunlock(&h->wrlk);
			}
			if(h->killme == UP){		/* If killme is up we forked another flow of control and need to die */
				h->killme = DOWN;
				exits(nil);
			}
		}
	}
}

/* queue all reads unless Hubs are set to freeze, in which case we behave like ramfiles */
void
fsread(Req *r)
{
	Hub *h;
	Msgq *mq;
	u32int count;
	vlong offset;
	char tmpstr[SMBUF];
	int i;
	int j;

	h = r->fid->file->aux;

	if(strncmp(h->name, "ctl", 3) == 0){
		count = r->ifcall.count;
		offset = r->ifcall.offset;
		if(offset > 0){
			r->ofcall.count = 0;
			respond(r, nil);
			return;
		}
		sprint(tmpstr, "\tHubfs %s status:\nParanoia == %d\tFreeze == %d\n", srvname, paranoia, freeze);
		if(strlen(tmpstr) <= count){
			count = strlen(tmpstr);
		} else {
			respond(r, "read count too small to answer\b");
		}
		memmove(r->ofcall.data, tmpstr, count);
		r->ofcall.count = count;
		respond(r, nil);
		return;
	}
	if(freeze == UP){
		mq = r->fid->aux;
		if(mq->bufuse > 0){
			if(h->qrnum >= MAXQ -2){
#ifdef DEBUG
				print("\ttransferring unanswered reqs to start of queue!\n");
#endif
				j = 1;
				for(i = h->qrans; i <= h->qrnum; i++) {
					h->qreqs[j] = h->qreqs[i];
					h->rstatus[j] = h->rstatus[i];
#ifdef DEBUG
					print("\setting qreqs[%d] equal to qreqs[%d]\n", j, i);
#endif
					j++;
				}
				h->qrnum = h->qrnum - h->qrans + 1;
#ifdef DEBUG
				print("\th->qrnum set to %d\n", h->qrnum);
#endif
				h->qrans = 1;
			}
			h->qrnum++;
			h->rstatus[h->qrnum] = WAIT;
			h->qreqs[h->qrnum] = r;
			return;			
		}
		count = r->ifcall.count;
		offset = r->ifcall.offset;
		while(offset >= BUCKSIZE){
			offset -= BUCKSIZE;
		}
		if(offset >= h->buckfull){
			r->ofcall.count = 0;
			respond(r, nil);
			return;
		}
		if((offset + count >= h->buckfull) && (offset < h->buckfull)){
			count = h->buckfull - offset;
		}
		memmove(r->ofcall.data, h->bucket + offset, count);
		r->ofcall.count = count;
		respond(r, nil);
		return;
	}
	/* Actual queue logic, ctl file and freeze mode logic is rarely used */

	if(h->qrnum >= MAXQ - 2){
#ifdef DEBUG
		print("\ttransferring unanswered reqs to start of queue!\n");
#endif
		j = 1;
		for(i = h->qrans; i <= h->qrnum; i++) {
			h->qreqs[j] = h->qreqs[i];
			h->rstatus[j] = h->rstatus[i];
#ifdef DEBUG
			print("\setting qreqs[%d] equal to qreqs[%d]\n", j, i);
#endif
			j++;
		}
		h->qrnum = h->qrnum - h->qrans + 1;
#ifdef DEBUG
		print("\th->qrnum set to %d\n", h->qrnum);
#endif
		h->qrans = 1;
	}
	h->qrnum++;
	h->rstatus[h->qrnum] = WAIT;
	h->qreqs[h->qrnum] = r;
	msgsend(h);
}

/* queue writes unless hubs are set to frozen mode */
void
fswrite(Req *r)
{
	Hub *h;
	u32int count;
	vlong offset;

	h = r->fid->file->aux;
	if(strncmp(h->name, "ctl", 3) == 0){
		hubcmd(r->ifcall.data);
	}

	if(freeze == UP){
		count = r->ifcall.count;
		offset = r->ifcall.offset;
		while(offset >= BUCKSIZE){
			offset -= BUCKSIZE;
		}
		h->inbuckp = h->bucket +offset;
		h->buckfull = h->inbuckp - h->bucket;
		if(h->buckfull + count >= BUCKSIZE){
			h->inbuckp = h->bucket;
			h->buckfull = 0;
		}
		memmove(h->inbuckp, r->ifcall.data, count);
		h->inbuckp += count;
		h->buckfull += count;
		r->fid->file->length = h->buckfull;
		r->ofcall.count = count;
		respond(r, nil);
		return;
	}
	/* Actual queue logic here */
	if(h->qwnum >= MAXQ - 2){
#ifdef DEBUG
		print("\ttransferring unanswered write reqs to start of queue!\n");
#endif
		j = 1;
		for(i = h->qwans; i <= h->qwnum; i++) {
			h->qwrits[j] = h->qwrits[i];
			h->wstatus[j] = h->wstatus[i];
#ifdef DEBUG
			print("\setting qwrits[%d] equal to qwrits[%d]\n", j, i);
#endif
			j++;
		}
		h->qwnum = h->qwnum - h->qwans + 1;
#ifdef DEBUG
		print("\th->qwnum set to %d\n", h->qwnum);
#endif
		h->qwans = 1;
	}
	h->qwnum++;
	h->wstatus[h->qwnum] = WAIT;
	h->qwrits[h->qwnum] = r;
	wrsend(h);
	msgsend(h);
	/* CRUCIAL - we do msgsend here after wrsend because we KNOW a write has happened */
	/* THEREFORE we know that there will be new data for readers and should send it to them ASAP */
}

/* making a file is making a new hub so we allocate and zero it to prepare for i/o */
void
fscreate(Req *r)
{
	Hub *h;
	File *f;

	if(f = createfile(r->fid->file, r->ifcall.name, r->fid->uid, r->ifcall.perm, nil)){
		h = emalloc9p(sizeof(Hub));
		zerohub(h);
		strcat(h->name, r->ifcall.name);
		f->aux = h;
		r->fid->file = f;
		r->ofcall.qid = f->qid;
		respond(r, nil);
		return;
	}
	respond(r, Ebad);
}


/* new client for the hubfile so associate a new message queue with client fid and hub file */
void
fsopen(Req *r)
{
	Hub *h;
	Msgq *q;

	h = r->fid->file->aux;
	q = (Msgq*)emalloc9p(sizeof(Msgq));
	memset(q, 0, sizeof(Msgq));
	q->myfid = r->fid->fid;
	q->nxt = h->bucket;
	q->bufuse = 0;
	if (trunc == 1){
		q->nxt = h->inbuckp;
		q->bufuse = h->buckfull;
	}
	r->fid->aux = q;
	if(h && (r->ifcall.mode&OTRUNC)){
		h->inbuckp = h->bucket;
		h->buckfull = 0;
		r->fid->file->length = 0;
	}
	respond(r, nil);
}

/* delete the hub. Note that we don't track the associated mqs of clients so we leak them. */
/* This is a small leak and in normal usage hubs aren't deleted anyway. */
void
fsdestroyfile(File *f)
{
	Hub *h;

	h = f->aux;
	if(h){
		free(h);
	}
}

/* called when a hubfile is created ?Why is qrans being set to 1 and qwans to 0 when both are set to 1 upon looping? */
void
zerohub(Hub *h)
{
	memset(h, 0, sizeof(Hub));
	h->inbuckp = h->bucket;
	h->qrnum = 0;
	h->qrans = 1;
	h->qwnum = 0;
	h->qwans = 0;
	h->ketchup = 0;
	h->buckwrap = h->inbuckp + BUCKSIZE;
}

/* set status of paranoid mode and frozen/normal from ctl messages */
void
hubcmd(char *cmd)
{
	if(strncmp(cmd, "quit", 4) == 0){
		sysfatal("quit command sent to hubfs!");
	}
	if(strncmp(cmd, "fear", 4) == 0){
		paranoia = UP;
		print("you feel a cold creeping feeling coming up your spine...\n");
		return;
	}
	if(strncmp(cmd, "calm", 4) == 0){
		paranoia = DOWN;
		print("you feel relaxed and confident.\n");
		return;
	}
	if(strncmp(cmd, "freeze", 6) == 0){
		freeze = UP;
		print("the pipes freeze in place!\n");
		return;
	}
	if(strncmp(cmd, "melt", 4) == 0){
		freeze = DOWN;
		print("the pipes thaw and data flows freely again\n");
		return;
	}
	fprint(2, "no matching command found\n");
}

void
usage(void)
{
	fprint(2, "usage: hubfs [-D] [-s srvname] [-m mtpt]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *addr = nil;
	char *mtpt = nil;
	Qid q;
	srvname = nil;
	fs.tree = alloctree(nil, nil, DMDIR|0777, fsdestroyfile);
	q = fs.tree->root->qid;
	trunc = 0;

	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 'a':
		addr = EARGF(usage());
		break;
	case 's':
		srvname = EARGF(usage());
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 't':
		trunc = 1;
		break;
	default:
		usage();
	}ARGEND;

	if(argc)
		usage();
	if(chatty9p)
		fprint(2, "hubsrv.nopipe %d srvname %s mtpt %s\n", fs.nopipe, srvname, mtpt);
	if(addr == nil && srvname == nil && mtpt == nil)
		sysfatal("must specify -a, -s, or -m option");
	if(addr)
		listensrv(&fs, addr);
	if(srvname || mtpt)
		postmountsrv(&fs, srvname, mtpt, MREPL|MCREATE);
	exits(0);
}

/* basic 9pfile implementation taken from /sys/src/lib9p/ramfs.c */

/* A note on paranoid mode and writer/reader synchronization.  The
default behavior is "broadcast like" rather than pipelike, because
writers are never blocked by default.  This is because in a network
situation with remote readers, blocking writes to wait for remote
client reads to complete produces an unpleasant user experience where
the remote latency limits the local environment.  As a consequence, by
default it is up to the writer to a file to limit the quantity and
speed of data writen to what clients are able receive.  The normal
intended mode of operation is for shells, and in this case data is
both 'bursty' and usually fairly small.  */

/* Paranoid mode is intended as "safe mode" and changes this
first-come first served behavior.  In paranoid mode, the readers and
writers attempt to synchronize.  The hub ketchup and tomatoflag
variables are used to monitor if readers have 'fallen behind' the
current data pointer, and if so, the writer is qlocked and sleeps
while we fork off a new flow of control.  We need to do more than just
answer the queued reads - because we are inside the 9p library (we are
the functions called by its srv pointer) we want the 9p library to
actually answer the incoming reads so we have read messages queued to
answer.  Just clearing out the read message queue isn't enough to
prioritize readers - we need to fork off so the controlling 9p library
has a chance to answer NEW reads.  By forking and sleeping the writer,
we allow the os to answer a new read request, which will unlock the
writer, which then needs to die at the end of its function because we
are a single-threaded 9p server and need to maintain one master flow
of control.  */

/* The paranoid/safe mode code is still limited in its logic for
handling multiple readers.  The ketchup and tomatoflag are per hub,
and a hub can have multiple clients.  It is intentional that these
multiple clients will 'race for the flag' and the writer will stop
waiting when one reader catches up enough to set ketchup and tomato
flag.  A more comprehensive solution would require adding new
structure to the client mq and a ref-counting implementation for
attaches to the hub so the hub could check the status of each client
individually.  There are additional problems with this because clients
are free to 'stop reading' at any time and thus a single client
unattaching will end up forcing the hub into 'as slow as possible'
mode.  */

/* To avoid completely freezing a hub, there is still a default "go
ahead" time even when clients have not caught up.  This time is
difficult to assess literally because it is a repeated sleep loop so
the os may perform many activities during the sleep.  Extending the
length of this delay time increases the safety guarantee for lagging
clients, but also increases the potential for lag and for
molasses-like shells after remote clients disconnect.  */

/* In general for the standard use of interactive shells paranoid mode
is unnecessary and all of this can and should be ignored.  For data
critical applications aiming for high speed constant throughput,
paranoid mode can and should be used, but additional data
verification, such as a cryptographic hashing protocol, would still be
recommended.  */
