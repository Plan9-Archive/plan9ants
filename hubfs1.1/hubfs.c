#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#define BUCKSIZE 777777
#define MAGIC 77777
#define MAXQ 777
#define SMBUF 777

/* provides input/output multiplexing for 'screen' like functionality */
/* usually used in combination with hubshell client and hub wrapper script */

typedef struct Hub	Hub;			/* A Hub is a file that functions as a multiplexed pipe */
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
	int qans;					/* number of read Reqs answered */
	Req *qwrits[MAXQ];			/* Similar for write Reqs */
	int wstatus[MAXQ];
	int qwnum;
	int qwans;
	int ketchup;				/* makes it all taste better */
	int tomatoflag;				/* fruit vs veggies  */
	QLock wrlk;				/* writer lock during fear */
	QLock replk;				/* reply lock during fear */
	int killme;					/* i need to die */
};

struct Msgq{
	ulong myfid;				/* Msgq is associated with client fids */
	char *nxt;					/* Location of this client in the buffer */
	int bufuse;				/* how much of the buffer has been used */
};

enum {
	UP = 1,
	DOWN = 2,
	WAIT = 3,
	DONE = 4,
};

char *srvname;
int paranoia;					/* In paranoid mode loose reader/writer sync is maintained */
int freeze;						/* In frozen mode the hubs operate simply as a ramfs */

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

/* msgsend replies to Reqs queued by fsread */
void
msgsend(Hub *h)
{
	Req *r;
	Msgq *mq;
	u32int count;

	if(h->qrnum == 0){
		return;
	}
	for(int i = h->qans; i <= h->qrnum; i++){

		if(paranoia == UP){
			qlock(&h->replk);
		}
		if(h->rstatus[i] != WAIT){
			if((i == h->qans) && (i < h->qrnum)){
				h->qans++;
			}
			if(paranoia == UP){
				qunlock(&h->replk);
			}
			continue;
		}

		r = h->qreqs[i];
		mq = r->fid->aux;
		if(mq->nxt == h->inbuckp){
			if(paranoia == UP){
				qunlock(&h->replk);
			}
			continue;
		}

		count = r->ifcall.count;
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
		memmove(r->ofcall.data, mq->nxt, count);
		r->ofcall.count = count;
		mq->nxt += count;
		mq->bufuse += count;
		h->rstatus[i] = DONE;
		if((i == h->qans) && (i < h->qrnum)){
			h->qans++;
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

	if(h->qwnum == 0){
		return;
	}

	if(paranoia == UP){		/* If we are paranoid, we fork and slack off while the readers catch up */
		qlock(&h->wrlk);
		if((h->ketchup < h->buckfull - MAGIC) || (h->ketchup > h->buckfull)){
			if(rfork(RFPROC|RFMEM) == 0){
				sleep(100);
				h->killme = UP;
				for(int i = 0; ((i < 77) && (h->tomatoflag == UP)); i++){
					sleep(7);		/* Give the readers some time to catch up and drop the flag */
				}
			} else {
				return;	/* We want this flow of control to become an incoming read request */
			}
		}
	}

	for(int i = h->qwans; i <= h->qwnum; i++){
		if(h->wstatus[i] != WAIT){
			if((i == h->qwans) && (i < h->qwnum)){
				h->qwans++;
			}
			continue;
		}

		r = h->qwrits[i];
		count = r->ifcall.count;
		if((h->buckfull + count) >= BUCKSIZE - 8192){
			h->buckwrap = h->inbuckp;
			h->inbuckp = h->bucket;
			h->buckfull = 0;
		}
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
				h->qrnum = 1;
				h->qans = 1;
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

	h->qrnum++;
	if(h->qrnum >= MAXQ - 2){
		msgsend(h);
		h->qrnum = 1;
		h->qans = 1;
	}
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
	h->qwnum++;
	if(h->qwnum >= MAXQ - 2){
		msgsend(h);
		h->qwnum = 1;
		h->qwans = 1;
	}
	h->wstatus[h->qwnum] = WAIT;
	h->qwrits[h->qwnum] = r;
	wrsend(h);
	msgsend(h);
}

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
	r->fid->aux = q;
	if(h && (r->ifcall.mode&OTRUNC)){
		h->inbuckp = h->bucket;
		h->buckfull = 0;
		r->fid->file->length = 0;
	}
	respond(r, nil);
}

void
fsdestroyfile(File *f)
{
	Hub *h;

	h = f->aux;
	if(h){
		free(h);
	}
}

void
zerohub(Hub *h)
{
	memset(h, 0, sizeof(Hub));
	h->inbuckp = h->bucket;
	h->qrnum = 0;
	h->qans = 1;
	h->qwnum = 0;
	h->qwans = 0;
	h->ketchup = 0;
	h->buckwrap = h->inbuckp + BUCKSIZE;
}

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
