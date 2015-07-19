#include <u.h>
#include <libc.h>
#include <fcall.h>
#include "../boot/boot.h"

/* This is a simplified and conjoined version of boot and init designed for 'rootless' bootup*/


#define PARTSRV "partfs.sdXX"

enum {
	Dontpost,
	Post,
};

/* vars and functions from standard boot.c */
char	cputype[64];
char	sys[2*64];
char 	reply[256];
int	printcol;
int	mflag;
int	fflag;
int	kflag;
int	debugboot;

char *rdparts;
char	*bargv[Nbarg];
int	bargc;

void boot(int argc, char *argv[]);
static void	swapproc(void);
static void	kbmap(void);

/* vars and functions from standard init.c */
char*	readenv(char*);
void	settheenv(char*, char*);
void	cpenv(char*, char*);
void	closefds(void);
void	fexec(void(*)(void));
void	rcexec(void);
void	plan9start(void);
void init(void);
void pinhead(void*, char *msg);

char	*service;
char	*cmd;
char	*cpu;
char *checkmanual;
int	manual;

void
boot(int argc, char *argv[])
{
	int fd;

	fmtinstall('r', errfmt);

	/*
	 * we should inherit the standard fds all referring to /dev/cons,
	 * but we're being paranoid.
	 */
	close(0);
	close(1);
	close(2);
	bind("#c", "/dev", MREPL);
	open("/dev/cons", OREAD);
	open("/dev/cons", OWRITE);
	open("/dev/cons", OWRITE);
	/*
	 * init will reinitialize its namespace.
	 * #ec gets us plan9.ini settings (*var variables).
	 */
	bind("#ec", "/env", MREPL);
	bind("#e", "/env", MBEFORE|MCREATE);
	bind("#s", "/srv", MREPL|MCREATE);
#ifdef DEBUG
	print("argc=%d\n", argc);
	for(fd = 0; fd < argc; fd++)
		print("%#p %s ", argv[fd], argv[fd]);
	print("\n");
#endif DEBUG

	readfile("#e/cputype", cputype, sizeof(cputype));

	/*
	 *  set up usb keyboard, mouse and disk, if any.
	 */
	print("usbinit...");
	usbinit(Dontpost);

	/*
	 *  load keymap if it's there.
	 */
	print("kbmap...");
	kbmap();

	/*
	 * read disk partition tables here so that readnvram via factotum
	 * can see them.  ideally we would have this information in
	 * environment variables before attaching #S, which would then
	 * parse them and create partitions.
	 */
	rdparts = getenv("readparts");
	if(rdparts)
		readparts();
	free(rdparts);

	print("bind / /...");
	if(bind("/", "/", MREPL) < 0)
		fatal("bind /");
	setenv("rootdir", "/root");
	settime(1, -1, nil);
	swapproc();
	print("init...");
	init();

	print("init failed!!");
	sleep(10000);
	fatal("early init failed\n");
}

static void
swapproc(void)
{
	int fd;

	fd = open("#c/swap", OWRITE);
	if(fd < 0){
		warning("opening #c/swap");
		return;
	}
	if(write(fd, "start", 5) <= 0)
		warning("starting swap kproc");
	close(fd);
}

/*
static void
usbinit(void)
{
	static char usbd[] = "/boot/usbd";

	if(access("#u/usb/ctl", 0) >= 0 && bind("#u", "/dev", MAFTER) >= 0 &&
	    access(usbd, AEXIST) >= 0)
		run(usbd, nil);
}
*/

static void
kbmap(void)
{
	char *f;
	int n, in, out;
	char buf[1024];

	f = getenv("kbmap");
	if(f == nil)
		return;
	if(bind("#κ", "/dev", MAFTER) < 0){
		warning("can't bind #κ");
		return;
	}

	in = open(f, OREAD);
	if(in < 0){
		warning("can't open kbd map: %r");
		return;
	}
	out = open("/dev/kbmap", OWRITE);
	if(out < 0) {
		warning("can't open /dev/kbmap: %r");
		close(in);
		return;
	}
	while((n = read(in, buf, sizeof(buf))) > 0)
		if(write(out, buf, n) != n){
			warning("write to /dev/kbmap failed");
			break;
		}
	close(in);
	close(out);
}

/* End of boot.c code, beginning of init.c code */

void
init(void)
{
	int fd;
	char ctl[128];

	closefds();

	if(cpuflag)
		service = "cpu";
	else
		service = "terminal";
	manual = 0;

	snprint(ctl, sizeof(ctl), "#p/%d/ctl", getpid());
	fd = open(ctl, OWRITE);
	if(fd < 0)
		print("init: warning: can't open %s: %r\n", ctl);
	else
		if(write(fd, "pri 10", 6) != 6)
			print("init: warning: can't set priority: %r\n");
	close(fd);

	cpu = readenv("#e/cputype");
	if(access("#ec/service", 0) == 0){
		service = readenv("#ec/service");
	}
	if(access("#ec/manual", 0) == 0)
		checkmanual = readenv("#ec/manual");
	else
		checkmanual = strdup("noenv");
	if(access("#ec/cmd", 0) == 0)
		cmd = readenv("#ec/cmd");
	if(strcmp(checkmanual, "manual") == 0)
		manual = 1;
	settheenv("#e/objtype", cpu);
	settheenv("#e/service", service);
	print("service: %s...", service);

	print("bind # trees...");
	bind("#d", "/fd", MREPL);
	bind("#p", "/proc", MREPL);
	bind("#¤", "/dev", MAFTER);
	bind("#S", "/dev", MAFTER);
	bind("#l", "/net", MAFTER);
	bind("#I", "/net", MAFTER);
	bind("/boot", "/bin", MAFTER);

	if(manual == 0){
		print("plan9rc...\n");
		fexec(plan9start);
	}
	for(;;){
		print("\ninit: starting rc\n");
		fexec(rcexec);
		manual = 1;
		cmd = 0;
		sleep(1000);
	}
}

static int gotnote;

void
pinhead(void*, char *msg)
{
	gotnote = 1;
	fprint(2, "init got note '%s'\n", msg);
	noted(NCONT);
}

void
fexec(void (*execfn)(void))
{
	Waitmsg *w;
	int pid;

	switch(pid=fork()){
	case 0:
		rfork(RFNOTEG);
		(*execfn)();
		print("init: exec error: %r\n");
		exits("exec");
	case -1:
		print("init: fork error: %r\n");
		exits("fork");
	default:
	casedefault:
		notify(pinhead);
		gotnote = 0;
		w = wait();
		if(w == nil){
			if(gotnote)
				goto casedefault;
			print("init: wait error: %r\n");
			break;
		}
		if(w->pid != pid){
			free(w);
			goto casedefault;
		}
		if(strstr(w->msg, "exec error") != 0){
			print("init: exit string %s\n", w->msg);
			print("init: sleeping because exec failed\n");
			free(w);
			for(;;)
				sleep(1000);
		}
		if(w->msg[0])
			print("init: rc exit status: %s\n", w->msg);
		free(w);
		break;
	}
}

void
rcexec(void)
{
	if(cmd)
		execl("/boot/rc", "rc", "-c", cmd, nil);
	execl("/boot/rc", "rc", nil);
}

void
plan9start(void)
{
	if(rfork(RFPROC|RFNAMEG|RFFDG)==0)
		execl("/boot/rc", "rc", "-c", "/boot/plan9rc", nil);
	else
		wait();
		print("plan9rc completed, console is kernel only namespace\n");
		exits(nil);
}

char*
readenv(char *name)
{
	int f, len;
	Dir *d;
	char *val;

	f = open(name, OREAD);
	if(f < 0){
		print("init: can't open %s: %r\n", name);
		return "*unknown*";	
	}
	d = dirfstat(f);
	if(d == nil){
		print("init: can't stat %s: %r\n", name);
		return "*unknown*";
	}
	len = d->length;
	free(d);
	if(len == 0)	/* device files can be zero length but have contents */
		len = 64;
	val = malloc(len+1);
	if(val == nil){
		print("init: can't malloc %s: %r\n", name);
		return "*unknown*";
	}
	len = read(f, val, len);
	close(f);
	if(len < 0){
		print("init: can't read %s: %r\n", name);
		return "*unknown*";
	}else
		val[len] = '\0';
	return val;
}

void
settheenv(char *var, char *val)
{
	int fd;

	fd = create(var, OWRITE, 0644);
	if(fd < 0)
		print("init: can't open %s\n", var);
	else{
		fprint(fd, val);
		close(fd);
	}
}

void
cpenv(char *file, char *var)
{
	int i, fd;
	char buf[8192];

	fd = open(file, OREAD);
	if(fd < 0)
		print("init: can't open %s\n", file);
	else{
		i = read(fd, buf, sizeof(buf)-1);
		if(i <= 0)
			print("init: can't read %s: %r\n", file);
		else{
			close(fd);
			buf[i] = 0;
			settheenv(var, buf);
		}
	}
}

/*
 *  clean up after /boot
 */
void
closefds(void)
{
	int i;

	for(i = 3; i < 30; i++)
		close(i);
}

