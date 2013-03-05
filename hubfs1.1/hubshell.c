#include <u.h>
#include <libc.h>
#include <ctype.h>

enum smallbuffer{
	SMBUF = 777,
};

/* hubshell is the client for hubfs, usually started by the hub wrapper script */

/* A Shell gets "glued on" to an rc and bucket-brigades data between the rc fds and hubfs */
typedef struct Shell	Shell;

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

char initname[SMBUF];
char srvname[SMBUF];
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

Shell*
setupshell(char *name)
{
	Shell *s;
	int i;

	s = (Shell*)malloc(sizeof(Shell));
	if(s == nil){
		sysfatal("Hubshell malloc failed!\n");
	}
	memset(s, 0, sizeof(Shell));
	strncat(s->basename, name, SMBUF);
	for(i = 1; i < 3; i++){
		s->fdname[i] = (char*)malloc((strlen(s->basename)+1));
		if(s->fdname[i] == nil){
			sysfatal("Hubshell malloc failed!\n");
		}
		sprint(s->fdname[i], "%s%d", s->basename, i);
		if((s->fd[i] = open(s->fdname[i], OREAD)) < 0){
			fprint(2, "hubshell: giving up on task - cant open %s\n", s->fdname[i]);
			return(nil);
		};
	}
	s->fdname[0] =  (char*)malloc((strlen(s->basename)+1));
	if(s->fdname[0] == nil){
		sysfatal("Hubshell malloc failed!\n");
	}
	sprint(s->fdname[0], "%s%d", s->basename, 0);
	if((s->fd[0] = open(s->fdname[0], OWRITE)) < 0){
		fprint(2, "hubshell: giving up on task - cant open %s\n", s->fdname[0]);
		return(nil);
	}
	s->fdzerodelay = 0;
	s->fdonedelay = 0;
	s->fdtwodelay = 30;
	s->cmdresult = 'a';
	return s;
}

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

void
fdonecat(int infd, int outfd, Shell *s)
{
	char buf[8192];
	long n;

	while((n=read(infd, buf, (long)sizeof(buf)))>0){
		sleep(s->fdonedelay);
		if(write(outfd, buf, n)!=n){
			fprint(2, "hubshell: write error copying on fd %d\n", outfd);
		}
		if(s->shellctl == 'q'){
			exits(nil);
		}
	}
	if(n == 0){
		fprint(2, "hubshell: zero length read on fd %d\n", infd);
	}
	if(n < 0){
		fprint(2, "hubshell: error reading fd %d\n", infd);
	}
}

void
fdtwocat(int infd, int outfd, Shell *s)
{
	char buf[8192];
	long n;

	while((n=read(infd, buf, (long)sizeof(buf)))>0){
		sleep(s->fdtwodelay);
		if(write(outfd, buf, n)!=n){
			fprint(2, "hubshell: write error copying on fd %d\n", outfd);
		}
		if(s->shellctl == 'q'){
			exits(nil);
		}
	}
	if(n == 0){
		fprint(2, "hubshell: zero length read on fd %d\n", infd);
	}
	if(n < 0){
		fprint(2, "hubshell: error reading fd %d\n", infd);
	}
}

void
fdzerocat(int infd, int outfd, Shell *s)
{
	char buf[8192];
	long n;

	while((n=read(infd, buf, (long)sizeof(buf)))>0){
		if((strncmp(buf, "%", 1)) == 0){
			s->cmdresult = 'p';
			parsebuf(s, buf + 1, outfd);
			if(s->cmdresult != 'x'){
				memset(buf, 0, 8192);
				continue;
			}
		}
		sleep(s->fdzerodelay);
		if(write(outfd, buf, n)!=n){
			fprint(2, "hubshell: write error copying on fd %d\n", outfd);
		}
		if(s->shellctl == 'q'){
			exits(nil);
		}
	}
	if(n == 0){
		fprint(2, "hubshell: zero length read on fd %d\n", infd);
	}
	if(n < 0){
		fprint(2, "hubshell: error reading fd %d\n", infd);
	}
}

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

void
closefds(Shell *s)
{
	int i;

	for(i = 0; i < 3; i++){
		close(s->fd[i]);
	}
}

void
parsebuf(Shell *s, char *buf, int outfd)
{
	char tmpstr[SMBUF];
	char tmpname[SMBUF];
	Shell *newshell;
	char *fi[3];
	memset(tmpstr, 0, SMBUF);
	memset(tmpname, 0, SMBUF);
	int i;

	if(strncmp(buf, "detach", 6) == 0){
		fprint(2, "hubshell detaching\n");
		s->shellctl = 'q';
		if(fortunate)
			write(outfd, "fortune\n", 8);
		if(echoes)
			write(outfd, "echo\n", 5);
		sleep(1000);
		closefds(s);
		exits(nil);
	}

	/* %remote command makes new shell on hubfs host by creating new hubfiles starting new rc and exiting current */
	if(strncmp(buf, "remote", 6) == 0){
		if(isalpha(*(buf + 7)) == 0){
			fprint(2, "remote needs a name parameter to create new hubs\n:io ");
			return;
		}
		strncat(tmpname, buf + 7, strcspn(buf + 7, "\n"));
		fprint(2, "creating new shell %s %s on remote host\n", srvname, tmpname);
		for(i = 0; i < 3; i++){
			snprint(tmpstr, SMBUF, "/n/%s/%s%d", srvname, tmpname, i);
			if(touch(tmpstr) !=0){
				fprint(2, "cant create new hubs to remote to!\nio: ");
				return;
			}
			fi[i] = strdup(tmpstr);		
		}
		snprint(tmpstr, SMBUF, "rc -i <%s >%s >[2]%s &\n", fi[0], fi[1], fi[2]);
		write(outfd, tmpstr, strlen(tmpstr));
		sleep(700);
		snprint(tmpstr, SMBUF, "echo 'prompt=%s%%'' '' ' >>%s\n", tmpname, fi[0]);
		write(outfd, tmpstr, strlen(tmpstr));
		sleep(200);
		snprint(tmpstr, SMBUF, "/n/%s/%s", srvname, tmpname);
		newshell = setupshell(tmpstr);
		if(newshell == nil){
			fprint(2, "failed to setup up client shell, maybe problems on remote end\nio: ");
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
			fprint(2, "local needs a name parameter to create new hubs\nio: ");
			return;
		}
		strncpy(tmpstr, buf + 6, strcspn(buf + 6, "\n"));
		fprint(2, "creating new local shell using hub %s %s\n", srvname, tmpstr);
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
			fprint(2, "attach needs a name parameter to know what hubs to use, try %%list\nio: ");
			return;
		}
		strncat(tmpname, buf + 7, strcspn(buf + 7, "\n"));
		fprint(2, "attaching to  shell %s %s\n", srvname, tmpname);
		snprint(tmpstr, SMBUF, "/n/%s/%s", srvname, tmpname);
		newshell = setupshell(tmpstr);
		if(newshell == nil){
			fprint(2, "failed to setup up client shell - do you need to create it with remote NAME?\nio: ");
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
			fprint(2, "err hub delay setting requires numeric delay\nio: ");
			return;
		}
		s->fdtwodelay = atoi(buf + 4);
		fprint(2, "err hub delay set to %d\nio: ", atoi(buf +4));
		return;
	}
	if(strncmp(buf, "in", 2) == 0){
		if(isdigit(*(buf + 3)) == 0){
			fprint(2, "in hub delay setting requires numeric delay\nio: ");
			return;
		}
		s->fdzerodelay = atoi(buf + 3);
		fprint(2, "in hub delay set to %d\nio: ", atoi(buf +3));
		return;
	}
	if(strncmp(buf, "out", 3) == 0){
		if(isdigit(*(buf + 4)) == 0){
			fprint(2, "out hub delay setting requires numeric delay\nio: ");
			return;
		}
		s->fdonedelay = atoi(buf + 4);
		print("out hub delay set to %d\nio: ", atoi(buf +4));
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
	if(strncmp(buf, "fortun", 6) == 0){
		print("fortunes active\n");
		fortunate = 1;
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "unfort", 6) == 0){
		print("fortunes deactivated\n");
		fortunate = 0;
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "echoes", 6) == 0){
		print("echoes active\n");
		echoes = 1;
		fprint(2, "io: ");
		return;
	}
	if(strncmp(buf, "unecho", 6) == 0){
		print("echoes deactivated\n");
		echoes = 0;
		return;
	}

	fprint(2, "hubshell %% commands: \n\tdetach, remote NAME, local NAME, attach NAME \n\tstatus, list, err TIME, in TIME, out TIME\n");
	s->cmdresult = 'x';
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
	echoes = 1;
	strncpy(initname, argv[1], SMBUF);
	strncat(srvname, initname+3, SMBUF);
	sprint(srvname + strcspn(srvname, "/"), "\0");

	s = setupshell(initname);
	if(s == nil){
		sysfatal("couldnt setup shell, bailing out\n");
	}
	startshell(s);
}
