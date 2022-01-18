#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "dyad.h"

static char *srv = "sakura.jp.as.dal.net";
static int prt = 6667;
static char *chn = "#pantasya";
static char *nck = "sieste";
static char *pss = NULL;
static char *mst = "siesta";

static char *cmd=NULL, *usr=NULL, *par=NULL, *txt=NULL;

static int isReg=0;

static void sendf(dyad_Stream * s, char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	(void)vprintf(fmt, args);
	va_end(args);

	va_start(args, fmt);
	dyad_vwritef(s, fmt, args);
	va_end(args);
}

static char *trim(char *str) {
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
		while (*frontp!='\0') {
			*endp++ = *frontp++;
		}
		*endp = '\0';
	}

	return str;
}

static char *skip(char *s, char c) {
	while (*s != c && *s != '\0')
		s++;
	if (*s != '\0')
		*s++ = '\0';
	return s;
}

static void onConnect(dyad_Event *e) {
	if (pss) {
		dyad_writef(e->stream, "PASS %s\r\n", pss);
	}
	dyad_writef(e->stream, "NICK %s\r\n", nck);
	dyad_writef(e->stream, "USER %s %s bla :%s\r\n", nck, nck, nck);
}

static void onError(dyad_Event *e) {
	printf("error: %s\n", e->msg);
}

static void onTick(dyad_Event *e) {
}

static void onLine(dyad_Event *e) {

	char *tmp=NULL;

	printf("%s\n", e->data);

	tmp = strdup(e->data);

	cmd = tmp;

	usr = srv;

	if (cmd==NULL) {
		return;
	}

	if (*cmd=='\0') {
		free(cmd);
		return;
	}

	if (cmd[0] == ':') {
		usr = cmd + 1;
		cmd = skip(usr, ' ');
		if (cmd[0] == '\0')
			return;
		(void)skip(usr, '!');
	}
	(void)skip(cmd, '\r');
	par = skip(cmd, ' ');
	txt = skip(par, ':');

	(void)trim(par);

	(void)trim(txt);

	if (strcmp(cmd, "PING")==0) {
		sendf(e->stream, "PONG :%s\r\n", txt);
	} else if (strcmp(cmd, "001")==0) {
		printf("connected.\n");
		sendf(e->stream, "JOIN %s\r\n", chn);
		isReg = 1;
	} else if (strcmp(cmd, "PRIVMSG")==0) {
		printf("<%s> %s\n", usr, txt);


	}

	free(tmp);
	tmp = NULL;

}



int main(void) {

	dyad_Stream *stm;

	srand((unsigned int)time(NULL));

	dyad_init();

	stm = dyad_newStream();

	dyad_addListener(stm, DYAD_EVENT_CONNECT, onConnect, NULL);
	dyad_addListener(stm, DYAD_EVENT_ERROR, onError, NULL);
	dyad_addListener(stm, DYAD_EVENT_LINE, onLine, NULL);
	dyad_addListener(stm, DYAD_EVENT_TICK, onTick, NULL);

	printf("connecting...\n");

	(void)dyad_connect(stm, srv, prt);

	while (dyad_getStreamCount() > 0) {
		dyad_update();
	}

	dyad_shutdown();

	return 0;
}
