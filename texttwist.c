#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "dyad.h"

#define PFX "."

int prt = 6667;
char *srv = "irc.undernet.org";
char *chn = "#gametime";
char *nck = "siesto";
char *pss = NULL;
char *mst = "siesta";
char *mps = "143445254";

int isReg = 0;
int isAuth = 0;

int ticks = 0;

char *trim(char *str)
{
	size_t len = 0;
	char *frontp = str;
	char *endp = NULL;

	if (str == NULL) {
		return NULL;
	}

	if (str[0] == '\0') {
		return str;
	}

	len = strlen(str);
	endp = str + len;

	while (isspace((unsigned char)*frontp)) {
		++frontp;
	}

	if (endp != frontp) {
		while (isspace((unsigned char)*(--endp)) && endp != frontp) {
		}
	}

	if (frontp != str && endp == frontp) {
		*str = '\0';
	} else if (str + len - 1 != endp) {
		*(endp + 1) = '\0';
	}

	endp = str;
	if (frontp != str) {
		while (*frontp) {
			*endp++ = *frontp++;
		}
		*endp = '\0';
	}

	return str;
}

static char *skip(char *s, char c)
{
	while (*s != c && *s != '\0')
		s++;
	if (*s != '\0')
		*s++ = '\0';
	return s;
}

static void sendf(dyad_Stream * s, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	dyad_vwritef(s, fmt, args);
	va_end(args);
}

static void onConnect(dyad_Event * e)
{
	if (pss) {
		dyad_writef(e->stream, "PASS %s\r\n", pss);
	}
	dyad_writef(e->stream, "NICK %s\r\n", nck);
	dyad_writef(e->stream, "USER %s %s %s :%s\r\n", nck, nck, nck, nck);
}

static void onError(dyad_Event * e)
{
	printf("error: %s\n", e->msg);
}

static void onLine(dyad_Event * e)
{
	char *tmp, *cmd, *usr, *par, *txt;

	printf("%s\n", e->data);

	tmp = strdup(e->data);

	cmd = tmp;

	usr = srv;

	if (!cmd) {
		return;
	}

	if (!*cmd) {
		free(cmd);
		return;
	}

	if (cmd[0] == ':') {
		usr = cmd + 1;
		cmd = skip(usr, ' ');
		if (cmd[0] == '\0')
			return;
		skip(usr, '!');
	}
	skip(cmd, '\r');
	par = skip(cmd, ' ');
	txt = skip(par, ':');
	trim(par);

	trim(txt);

/*
	printf("usr: %s\n",usr);
	printf("cmd: %s\n",cmd);
	printf("par: %s\n",par);
	printf("txt: %s\n",txt);
	printf("\n");
//*/

	if (!strcmp(cmd, "PING")) {
		sendf(e->stream, "PONG :%s\r\n", txt);
	} else if (!strcmp(cmd, "001")) {
		printf("connected.\n");
		sendf(e->stream, "JOIN :%s\r\n", chn);
		isReg = 1;
	} else if (!strcmp(cmd, "PRIVMSG")) {

		printf("<%s> %s\n", usr, txt);

		if (!strcmp(usr, mst)) {

			if (!strcmp(par, nck)) {

				if (!strncmp(txt, PFX "auth", 5)) {
					if (strlen(txt) > 6
					    && !strcmp(txt + 6, mps)) {
						isAuth = 1;
						sendf(e->stream,
						      "PRIVMSG %s :%s\r\n", usr,
						      "access granted");
					} else {
						isAuth = 0;
						sendf(e->stream,
						      "PRIVMSG %s :%s\r\n", usr,
						      "access denied");
					}
				}

			}

			if (isAuth) {

				if (!strncmp(txt, PFX "quit", 5)) {
					if (strlen(txt) > 6) {
						sendf(e->stream, "QUIT :%s\r\n",
						      txt + 6);
					} else {
						sendf(e->stream, "QUIT\r\n");
					}
				}

			}

		}

		if (par[0] == '#') {
			if (!strncmp(txt, "hello", 5)) {
				sendf(e->stream, "PRIVMSG %s :%s\r\n", par,
				      "hi");
			} else if (!strncmp(txt, "hi", 5)) {
				sendf(e->stream, "PRIVMSG %s :%s\r\n", par,
				      "hello");
			}
		}

	} else if (!strcmp(cmd, "JOIN")) {
		printf("%s joined %s\n", usr, strlen(par) ? par : txt);
	} else if (!strcmp(cmd, "PART")) {
		printf("%s parted %s\n", usr, strlen(par) ? par : txt);
	} else if (!strcmp(cmd, "QUIT")) {
		printf("%s exits '%s'\n", usr, txt);
	}

	free(tmp);
	tmp = NULL;

}

static void onTick(dyad_Event * e)
{
	ticks++;
}

int main(int argc, char **argv)
{

	dyad_Stream *s;

	dyad_init();

	s = dyad_newStream();

	dyad_addListener(s, DYAD_EVENT_CONNECT, onConnect, NULL);
	dyad_addListener(s, DYAD_EVENT_ERROR, onError, NULL);
	dyad_addListener(s, DYAD_EVENT_LINE, onLine, NULL);
	dyad_addListener(s, DYAD_EVENT_TICK, onTick, NULL);

	printf("connecting...\n");

	dyad_connect(s, srv, prt);

	while (dyad_getStreamCount() > 0) {
		dyad_update();
	}

	dyad_shutdown();

	return 0;
}
