
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <util.h>
#include <grp.h>
#include <utmp.h>
#include <pwd.h>

#include "utmp.h"

extern void die(const char *fmt, ...);
extern struct passwd *pass;
extern gid_t egid, gid;
static struct utmp utmp;

void
addutmp(void)
{
	unsigned ptyid;
	char *pts, *cp, *host;


	if (!(host = getenv("DISPLAY")))
		host = "-";

	if (strlen(pass->pw_name) > sizeof(utmp.ut_name))
		die("incorrect username %s", pass->pw_name);

	if ((pts = ttyname(STDIN_FILENO)) == NULL)
		die("error getting pty name:%s", strerror(errno));

	for (cp = pts + strlen(pts) - 1; isdigit(*cp); --cp)
		/* nothing */;

	ptyid = atoi(++cp);
	if (ptyid > 999 || strlen(pts + 5) > sizeof(utmp.ut_line))
		die("Incorrect pts name %s\n", pts);

	/* remove /dev/ from pts */
	strncpy(utmp.ut_line, pts + 5, sizeof(utmp.ut_line));
	strncpy(utmp.ut_name, pass->pw_name, sizeof(utmp.ut_name));
	strncpy(utmp.ut_host, host, sizeof(utmp.ut_host));
	time(&utmp.ut_time);

	setgid(egid);
	login(&utmp);
	setgid(gid);
}

void
delutmp(void)
{
	setgid(egid);
	logout(utmp.ut_line);
	setgid(gid);
}

