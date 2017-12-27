#include <u.h>
#include <libc.h>
#include <ctype.h>

/* hubshell is the client for hubfs, usually started by the hub wrapper script */
/* it handles attaching and detaching from hub-connected rcs and creating new ones */

enum smallbuffer{
	SMBUF = 777,
};

typedef struct Shell	Shell;

/* A Shell opens fds of 3 hubfiles and bucket-brigades data between the hubfs and std i/o fds*/
struct Shell {
	int fd[3];
	int fdzerodelay;
	int fdonedelay;
	int fdtwodelay;
	char basename[SMBUF];
	char *fdname[3];
	char shellctl;
	char cmdresult;
};

/* string storage for names of hubs and paths */
char initname[SMBUF];
char hubdir[SMBUF];
char srvname[SMBUF];
char ctlname[SMBUF];
char basehub[SMBUF];

/* flags for rc commands to flush buffers */
int fortunate;
int echoes;

Shell* setupshell(char *name);
void startshell(Shell *s);
void fdzerocat(int infd, int outfd, Shell *s);
void fdonecat(int infd, int outfd, Shell *s);
void fdtwocat(int infd, int outfd, Shell *s);
int touch(char *name);
void closefds(Shell *s);
void parsebuf(Shell *s, char *buf, int outfd);

/* set shellgroup variables and open file descriptors */
Shell*
setupshell(char *name)
{
	Shell *s;
	int i;

	s = (Shell*)malloc(sizeof(Shell));
	if(s == nil)
		sysfatal("Hubshell malloc failed!\n");
	memset(s, 0, sizeof(Shell));
	strncat(s->basename, name, SMBUF);
	for(i = 1; i < 3; i++){
		s->fdname[i] = (char*)malloc((strlen(s->basename)+1));
		if(s->fdname[i] == nil)
			sysfatal("Hubshell malloc failed!\n");
		sprint(s->fdname[i], "%s%d", s->basename, i);
		if((s->fd[i] = open(s->fdname[i], OREAD)) < 0){
			fprint(2, "hubshell: giving up on task - cant open %s\n", s->fdname[i]);
			return(nil);
		};
	}
	s->fdname[0] =  (char*)malloc((strlen(s->basename)+1));
	if(s->fdname[0] == nil)
		sysfatal("Hubshell malloc failed!\n");
	sprint(s->fdname[0], "%s%d", s->basename, 0);
	if((s->fd[0] = open(s->fdname[0], OWRITE)) < 0){
		fprint(2, "hubshell: giving up on task - cant open %s\n", s->fdname[0]);
		return(nil);
	}
	sprint(basehub, s->fdname[0] + strlen(hubdir));
	s->fdzerodelay = 0;
	s->fdonedelay = 0;
	s->fdtwodelay = 30;
	s->cmdresult = 'a';
	return s;
}

/* fork two reader procs for fd1 and 2, start writing to fd0 */
void
startshell(Shell *s)
{
	/* fork cats for each file descriptor used by a shell */
	if(rfork(RFPROC|RFMEM|RFNOWAIT|RFNOTEG)==0){
		fdonecat(s->fd[1], 1, s);
		exits(nil);
	}
	if(rfork(RFPROC|RFMEM|RFNOWAIT|RFNOTEG)==0){
		fdtwocat(s->fd[2], 2, s);
		exits(nil);
	}
	fdzerocat(0, s->fd[0], s);
	exits(nil);
}

/* reader proc to transfer from hubfile to fd1 */
void
fdonecat(int infd, int outfd, Shell *s)
{
	char buf[8192];
	long n;

	while((n=read(infd, buf, (long)sizeof(buf)))>0){
		sleep(s->fdonedelay);
		if(write(outfd, buf, n)!=n)
			fprint(2, "hubshell: write error copying on fd %d\n", outfd);
		if(s->shellctl == 'q')
			exits(nil);
	}
	if(n == 0)
		fprint(2, "hubshell: zero length read on fd %d\n", infd);
	if(n < 0)
		fprint(2, "hubshell: error reading fd %d\n", infd);
}

/* reader proc to transfer from hubfile to fd2 */
void
fdtwocat(int infd, int outfd, Shell *s)
{
	char buf[8192];
	long n;

	while((n=read(infd, buf, (long)sizeof(buf)))>0){
		sleep(s->fdtwodelay);
		if(write(outfd, buf, n)!=n)
			fprint(2, "hubshell: write error copying on fd %d\n", outfd);
		if(s->shellctl == 'q')
			exits(nil);
	}
	if(n == 0)
		fprint(2, "hubshell: zero length read on fd %d\n", infd);
	if(n < 0)
		fprint(2, "hubshell: error reading fd %d\n", infd);
}

/* write user input from fd0 to hubfile */
void
fdzerocat(int infd, int outfd, Shell *s)
{
	char buf[8192];
	long n;
	char ctlbuf[8192];
	int ctlfd;

readloop:
	while((n=read(infd, buf, (long)sizeof(buf)))>0){
		/* check for user %command */
		if((strncmp(buf, "%", 1)) == 0){
			s->cmdresult = 'p';
			parsebuf(s, buf + 1, outfd);
			if(s->cmdresult != 'x'){
				memset(buf, 0, 8192);
				continue;
			}
		}
		sleep(s->fdzerodelay);
		if(write(outfd, buf, n)!=n)
			fprint(2, "hubshell: write error copying on fd %d\n", outfd);
		if(s->shellctl == 'q')
			exits(nil);
	}
	/* eof input from user, send message to hubfs ctl file */
	if(n == 0){
		if((ctlfd = open(ctlname, OWRITE)) == - 1){
			fprint(2, "hubshell: can't open ctl file\n");
			goto readloop;
		}
		sprint(ctlbuf, "eof %s\n", basehub);
		n=write(ctlfd, ctlbuf, strlen(ctlbuf) +1);
			if(n != strlen(ctlbuf) + 1)
				fprint(2, "hubshell: error writing to %s on fd %d\n", ctlname, ctlfd);
		close(ctlfd);
	}
	/* commented out error message because it printed after interrupt note-passing */
//	if(n < 0)
//		fprint(2, "hubshell: error reading fd %d\n", infd);
	goto readloop;		/* Use more gotos, they aren't harmful */
}

/* for creating new hubfiles */
int
touch(char *name)
{
	int fd;

	if((fd = create(name, OREAD|OEXCL, 0666)) < 0){
		fprint(2, "hubshell: %s: cannot create: %r\n", name);
		return 1;
	}
	close(fd);
	return 0;
}

/* close fds when a shell moves to new hubfs */
void
closefds(Shell *s)
{
	int i;

	for(i = 0; i < 3; i++)
		close(s->fd[i]);
}

/* handles %commands */
void
parsebuf(Shell *s, char *buf, int outfd)
{
	char tmpstr[SMBUF];
	char tmpname[SMBUF];
	Shell *newshell;
	memset(tmpstr, 0, SMBUF);
	memset(tmpname, 0, SMBUF);
	char ctlbuf[SMBUF];
	int ctlfd;
	int n;

	/* %detach closes hubshell fds and exits */
	if(strncmp(buf, "detach", 6) == 0){
		fprint(2, "hubshell: detaching\n");
		s->shellctl = 'q';
		if(fortunate)
			write(outfd, "fortune\n", 8);
		if(echoes)
			write(outfd, "echo\n", 5);
		sleep(1000);
		closefds(s);
		exits(nil);
	}

	/* %remote command makes new shell on hubfs host by sending hub -b command */
	if(strncmp(buf, "remote", 6) == 0){
		if(isalpha(*(buf + 7)) == 0){
			fprint(2, "hubshell: remote needs a name parameter to create new hubs\n:io ");
			return;
		}
		strncat(tmpname, buf + 7, strcspn(buf + 7, "\n"));
		fprint(2, "hubshell: creating new shell %s %s on remote host\n", srvname, tmpname);
		snprint(tmpstr, SMBUF, "hub -b %s %s\n", srvname, tmpname);
		write(outfd, tmpstr, strlen(tmpstr));
		sleep(1000);
		snprint(tmpstr, SMBUF, "/n/%s/%s", srvname, tmpname);
		newshell = setupshell(tmpstr);
		if(newshell == nil){
			fprint(2, "hubshell: failed to setup up client shell, maybe problems on remote end\nio: ");
			return;
		}
		s->shellctl = 'q';
		if(fortunate)
			write(outfd, "fortune\n", 8);
		if(echoes)
			write(outfd, "echo\n", 5);
		sleep(700);
		closefds(s);
		startshell(newshell);
		exits(nil);
	}

	/* %local command makes new shell on local machine by executing the hub command and exiting */
	if(strncmp(buf, "local", 5) == 0){
		if(isalpha(*(buf + 6)) == 0){
			fprint(2, "hubshell: local needs a name parameter to create new hubs\nio: ");
			return;
		}
		strncpy(tmpstr, buf + 6, strcspn(buf + 6, "\n"));
		fprint(2, "hubshell: creating new local shell using hub %s %s\n", srvname, tmpstr);
		s->shellctl = 'q';
		if(fortunate)
			write(outfd, "fortune\n", 8);
		if(echoes)
			write(outfd, "echo\n", 5);
		sleep(1000);
		closefds(s);
		execl("/bin/hub", "hub", srvname, tmpstr, 0);
		exits(nil);
	}

	/* %attach name starts new shell and exits the current one */
	if(strncmp(buf, "attach", 6) == 0){
		if(isalpha(*(buf + 7)) == 0){
			fprint(2, "hubshell: attach needs a name parameter to know what hubs to use, try %%list\nio: ");
			return;
		}
		strncat(tmpname, buf + 7, strcspn(buf + 7, "\n"));
		fprint(2, "hubshell: attaching to  shell %s %s\n", srvname, tmpname);
		snprint(tmpstr, SMBUF, "/n/%s/%s", srvname, tmpname);
		newshell = setupshell(tmpstr);
		if(newshell == nil){
			fprint(2, "hubshell: failed to setup up client shell - do you need to create it with remote NAME?\nio: ");
			return;
		}
		s->shellctl = 'q';
		if(fortunate)
			write(outfd, "fortune\n", 8);
		if(echoes)
			write(outfd, "echo\n", 5);
		sleep(1000);
		closefds(s);
		startshell(newshell);
		exits(nil);
	}

	/* %err %in %out INT set the delay before reading/writing on that fd to INT milliseconds */
	if(strncmp(buf, "err", 3) == 0){
		if(isdigit(*(buf + 4)) == 0){
			fprint(2, "hubshell: err hub delay setting requires numeric delay\nio: ");
			return;
		}
		s->fdtwodelay = atoi(buf + 4);
		fprint(2, "hubshell: err hub delay set to %d\nio: ", atoi(buf +4));
		return;
	}
	if(strncmp(buf, "in", 2) == 0){
		if(isdigit(*(buf + 3)) == 0){
			fprint(2, "hubshell: in hub delay setting requires numeric delay\nio: ");
			return;
		}
		s->fdzerodelay = atoi(buf + 3);
		fprint(2, "hubshell: in hub delay set to %d\nio: ", atoi(buf +3));
		return;
	}
	if(strncmp(buf, "out", 3) == 0){
		if(isdigit(*(buf + 4)) == 0){
			fprint(2, "hubshell: out hub delay setting requires numeric delay\nio: ");
			return;
		}
		s->fdonedelay = atoi(buf + 4);
		fprint(2, "hubshell: out hub delay set to %d\nio: ", atoi(buf +4));
		return;
	}

	/* %fortun and %echoes turn on buffer flush commands %unfort and %unecho deactivate */
	if(strncmp(buf, "fortun", 6) == 0){
		fprint(2, "hubshell: fortunes active\n");
		fortunate = 1;
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "unfort", 6) == 0){
		fprint(2, "hubshell: fortunes deactivated\n");
		fortunate = 0;
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "echoes", 6) == 0){
		fprint(2, "hubshell: echoes active\n");
		echoes = 1;
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "unecho", 6) == 0){
		fprint(2, "hubshell: echoes deactivated\n");
		echoes = 0;
		return;
	}

	/* send eof or freeze/melt/fear/calm/trunc/notrunc messages to ctl file */
	if(strncmp(buf, "eof", 3) == 0){
		if((ctlfd = open(ctlname, OWRITE)) == - 1){
			fprint(2, "hubshell: can't open ctl file\n");
			return;
		}
		sprint(ctlbuf, "eof %s\n", basehub);
		n=write(ctlfd, ctlbuf, strlen(ctlbuf) +1);
			if(n != strlen(ctlbuf) + 1)
				fprint(2, "hubshell: error writing to %s on fd %d\n", ctlname, ctlfd);
		close(ctlfd);
		return;
	}
	if(strncmp(buf, "freeze", 6) == 0){
		if((ctlfd = open(ctlname, OWRITE)) == - 1){
			fprint(2, "hubshell: can't open ctl file\n");
			return;
		}
		sprint(ctlbuf, "freeze\n");
		n=write(ctlfd, ctlbuf, strlen(ctlbuf) +1);
			if(n != strlen(ctlbuf) + 1)
				fprint(2, "hubshell: error writing to %s on fd %d\n", ctlname, ctlfd);
		close(ctlfd);
		return;
	}
	if(strncmp(buf, "melt", 4) == 0){
		if((ctlfd = open(ctlname, OWRITE)) == - 1){
			fprint(2, "hubshell: can't open ctl file\n");
			return;
		}
		sprint(ctlbuf, "melt\n");
		n=write(ctlfd, ctlbuf, strlen(ctlbuf) +1);
			if(n != strlen(ctlbuf) + 1)
				fprint(2, "hubshell: error writing to %s on fd %d\n", ctlname, ctlfd);
		close(ctlfd);
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "fear", 4) == 0){
		if((ctlfd = open(ctlname, OWRITE)) == - 1){
			fprint(2, "hubshell: can't open ctl file\n");
			return;
		}
		sprint(ctlbuf, "fear\n");
		n=write(ctlfd, ctlbuf, strlen(ctlbuf) +1);
			if(n != strlen(ctlbuf) + 1)
				fprint(2, "hubshell: error writing to %s on fd %d\n", ctlname, ctlfd);
		close(ctlfd);
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "calm", 4) == 0){
		if((ctlfd = open(ctlname, OWRITE)) == - 1){
			fprint(2, "hubshell: can't open ctl file\n");
			return;
		}
		sprint(ctlbuf, "calm\n");
		n=write(ctlfd, ctlbuf, strlen(ctlbuf) +1);
			if(n != strlen(ctlbuf) + 1)
				fprint(2, "hubshell: error writing to %s on fd %d\n", ctlname, ctlfd);
		close(ctlfd);
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "trunc", 5) == 0){
		if((ctlfd = open(ctlname, OWRITE)) == - 1){
			fprint(2, "hubshell: can't open ctl file\n");
			return;
		}
		sprint(ctlbuf, "trunc\n");
		n=write(ctlfd, ctlbuf, strlen(ctlbuf) +1);
			if(n != strlen(ctlbuf) + 1)
				fprint(2, "hubshell: error writing to %s on fd %d\n", ctlname, ctlfd);
		close(ctlfd);
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "notrunc", 7) == 0){
		if((ctlfd = open(ctlname, OWRITE)) == - 1){
			fprint(2, "hubshell: can't open ctl file\n");
			return;
		}
		sprint(ctlbuf, "notrunc\n");
		n=write(ctlfd, ctlbuf, strlen(ctlbuf) +1);
			if(n != strlen(ctlbuf) + 1)
				fprint(2, "hubshell: error writing to %s on fd %d\n", ctlname, ctlfd);
		close(ctlfd);
		fprint(2, "io: ");
		return;
	}

	/* %list displays attached hubs %status reports variable settings */
	if(strncmp(buf, "list", 4) == 0){
		sprint(tmpstr, "/n/%s", srvname);
		print("listing mounted hubfs at /n/%s\n", srvname);
		if(rfork(RFPROC|RFNOTEG|RFNOWAIT) == 0){
			execl("/bin/lc", "lc", tmpstr, 0);
			exits(nil);
		}
		sleep(1500);
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "status", 6) == 0){
		print("\tHubshell status: attached to mounted %s of /srv/%s\n", s->basename, srvname);
		print("\tfdzero delay: %d  fdone delay: %d  fdtwo delay: %d\n", s->fdzerodelay, s->fdonedelay, s->fdtwodelay);
		if(fortunate)
			print("\tfortune fd flush active\n");
		if(echoes)
			print("\techo fd flush active\n");
		fprint(2, "io: ");
		return;
	}

	/* no matching command found, print list of commands as reminder */
	fprint(2, "hubshell %% commands: \n\tdetach, remote NAME, local NAME, attach NAME \n\tstatus, list, err TIME, in TIME, out TIME\n\tfortun unfort echoes unecho trunc notrunc eof\n");
	s->cmdresult = 'x';
}

/* receive interrupt messages (delete key) and pass them through to attached shells */
int
sendinterrupt(void *regs, char *notename)
{
	char notehub[SMBUF];
	int notefd;

	if(strcmp(notename, "interrupt") != 0)
		return 0;
	if(regs == nil)		/* this is just to shut up a compiler warning */
		fprint(2, "error in note registers\n");
	sprint(notehub, "%s%s.note", hubdir, basehub);
	if((notefd = open(notehub, OWRITE)) == -1){
		fprint(2, "can't open %s\n", notehub);
		return 1;
	}
	fprint(notefd, "interrupt");
	close(notefd);
	return 1;
}

void
main(int argc, char *argv[])
{
	Shell *s;

	if(argc != 2){
		fprint(2, "usage: hubshell hubsname - and probably you want the hub wrapper script instead.");
		sysfatal("usage\n");
	}

	fortunate = 0;
	echoes = 1;		/* maybe a questionable default */

	/* parse initname and set srvname hubdir and ctlname from it */
	strncpy(initname, argv[1], SMBUF);
	strncat(srvname, initname+3, SMBUF);
	sprint(srvname + strcspn(srvname, "/"), "\0");
	sprint(hubdir, "/n/");
	strncat(hubdir, srvname, SMBUF-6);
	strcat(hubdir, "/");
	sprint(ctlname, "/n/");
	strncat(ctlname, srvname, SMBUF-6);
	strcat(ctlname, "/ctl");

	atnotify(sendinterrupt, 1);
	s = setupshell(initname);
	if(s == nil)
		sysfatal("couldnt setup shell, bailing out\n");
	startshell(s);
}
