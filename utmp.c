

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>


#ifndef _POSIX_SAVED_IDS
#error "This program needs saved id behaviour"
#endif


struct passwd *pass;
gid_t egid, gid;


void
die(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	putc('\n', stderr);
	va_end(va);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int status;
	uid_t uid;
	extern void addutmp(void), delutmp(void);

	egid = getegid();
	gid = getgid();
	setgid(gid);

	pass = getpwuid(uid = getuid());
	if (!pass || !pass->pw_name)
		die("Process is running with an incorrect uid %d", uid);

	setenv("LOGNAME", pass->pw_name, 1);
	setenv("USER", pass->pw_name, 1);
	setenv("SHELL", pass->pw_shell, 0);
	setenv("HOME", pass->pw_dir, 0);

	switch (fork()) {
	case 0:
		execv(getenv("SHELL"), ++argv);
		die("error executing shell:%s", strerror(errno));
	case -1:
		die("error spawning child:%s", strerror(errno));
	default:
		addutmp();
		if (wait(&status) == -1) {
			fprintf(stderr, "error waiting child:%s\n",
				strerror(errno));
		}
		delutmp();
	}
	return 0;
}
