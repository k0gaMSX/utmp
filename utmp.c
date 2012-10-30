/* See LICENSE for license details. */
#define _POSIX_C_SOURCE	200112L

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <unistd.h>
#include <utmpx.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>

static struct utmpx utmp;
static struct passwd *pass;
static gid_t egid, gid;


void
die(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	exit(EXIT_FAILURE);
}

/*
 * From utmp(5)
 * xterm and other terminal emulators directly create a USER_PROCESS
 * record and generate the ut_id  by using the string that suffix part of
 * the terminal name (the characters  following  /dev/[pt]ty). If they find
 * a DEAD_PROCESS for this ID, they recycle it, otherwise they create a new
 * entry.  If they can, they will mark it as DEAD_PROCESS on exiting and it
 * is advised that they null ut_line, ut_time, ut_user, and ut_host as well.
 */

struct utmpx *
findutmp(int type)
{
	struct utmpx *r;

	utmp.ut_type = type;
	setutxent();
	for(;;) {
	       /*
		* we can not use getutxline because we can search in
		* DEAD_PROCESS to
		*/
	       if(!(r = getutxid(&utmp)))
		       break;
	       if(!strcmp(r->ut_line, utmp.ut_line))
		       break;
	       memset(r, 0, sizeof(*r)); /* for Solaris, IRIX64 and HPUX */
	}
	return r;
}

void
addutmp(int fd)
{
	unsigned ptyid;
	char *pts, *cp, buf[5] = {'x'};

	if ((pts = ttyname(fd)) == NULL)
		die("error getting pty name\n");

	for (cp = pts + strlen(pts) - 1; isdigit(*cp); --cp)
		/* nothing */;

	ptyid = atoi(++cp);
	if (ptyid > 999 || strlen(pts + 5) > sizeof(utmp.ut_line))
		die("Incorrect pts name %s\n", pts);
	sprintf(buf + 1, "%03d", ptyid);
	strncpy(utmp.ut_id, buf, 4);

	/* remove /dev/ part of the string */
	strcpy(utmp.ut_line, pts + 5);

	if(!findutmp(DEAD_PROCESS))
		findutmp(USER_PROCESS);

	utmp.ut_type = USER_PROCESS;
	strcpy(utmp.ut_user, pass->pw_name);
	utmp.ut_pid = getpid();
	utmp.ut_tv.tv_sec = time(NULL);
	utmp.ut_tv.tv_usec = 0;
	/* don't use no standard fields host and session */

	setgid(egid);
	if(!pututxline(&utmp))
		perror("add utmp entry");
	setgid(gid);
	endutxent();
}

void
delutmp(void)
{
	struct utmpx *r;

	setutxent();
	if((r = getutxline(&utmp)) != NULL) {
		r->ut_type = DEAD_PROCESS;
		r->ut_tv.tv_usec = r->ut_tv.tv_sec = 0;
		setgid(egid);
		pututxline(r);
		setgid(gid);
	}
	endutxent();
}

int
main(int argc, char *argv[])
{
	int status;
	uid_t uid;

	egid = getegid();
	gid = getgid();
	setgid(gid);

	pass = getpwuid(uid = getuid());
	if(!pass || !pass->pw_name ||
			strlen(pass->pw_name) + 1 > sizeof(utmp.ut_user)) {
		die("Process is running with an incorrect uid %d\n", uid);
	}

	setenv("LOGNAME", pass->pw_name, 1);
	setenv("USER", pass->pw_name, 1);
	setenv("SHELL", pass->pw_shell, 0);
	setenv("HOME", pass->pw_dir, 0);

	switch (fork()) {
	case 0:
		execv(getenv("SHELL"), ++argv);
		die("error executing shell:%s\n", strerror(errno));
	case -1:
		die("error spawning child:%s\n", strerror(errno));
	default:
		addutmp(STDIN_FILENO);
		if (wait(&status) == -1) {
			fprintf(stderr, "error waiting child:%s\n",
				strerror(errno));
		}
		delutmp();
	}
	return 0;
}

