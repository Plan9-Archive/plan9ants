#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include "../boot/boot.h"

void
main(int argc, char *argv[])
{
	char cputype[64];
	char buf[32];
	char bootcmd[64];

	fmtinstall('r', errfmt);

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
	bind("#σ", "/shr", MREPL);

	if(Debug){
		int i;

		print("argc=%d\n", argc);
		for(i = 0; i < argc; i++)
			print("%p %s ", argv[i], argv[i]);
		print("\n");
	}
	USED(argc);

	readfile("#e/cputype", cputype, sizeof(cputype));
	readfile("#e/bootcmd", bootcmd, sizeof(bootcmd));

	/* setup the boot namespace */
	bind("/boot", "/bin", MAFTER);
	run("/bin/paqfs", "-qa", "-c", "8", "-m" "/root", "/boot/bootfs.paq", nil);
	bind("/root", "/", MAFTER);
	snprint(buf, sizeof(buf), "/%s/bin", cputype);
	bind(buf, "/bin", MAFTER);
	bind("/rc/bin", "/bin", MAFTER);
	if(strcmp(bootcmd, "plan9rc")==0)
		exec("/boot/plan9rc", argv);
	exec("/bin/bootrc", argv);
}
