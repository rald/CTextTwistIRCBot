#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "dyad.h"

#define GAME_TITLE "TEXTTWIST"

#define RANDOM_WORDS_FILE "rand.txt"

#define PFX "."

int prt = 6667;
char *srv = "irc.undernet.org";
char *chn = "#gametime";
char *nck = "siesto";
char *pss = NULL;
char *mst = "siesta";
char *mps = "143445254";
typedef enum GameState GameState;

enum GameState {
	GAME_STATE_INIT=0,
	GAME_STATE_WAIT,
	GAME_STATE_START,
	GAME_STATE_PLAY,
	GAME_STATE_END,
	GAME_STATE_MAX
};

char *cmd=NULL, *usr=NULL, *par=NULL, *txt=NULL;

int isReg = 0;
int isAuth = 0;

int ticks = 0;

GameState gameState=GAME_STATE_INIT;
char **anagrams=NULL;
int numAnagrams=0;
char *bonusWord=NULL;
int longWordLen=0;
int *guessed=NULL;
int numGuessed=0;
char *word=NULL;
int waitingTime=0;

double drand() {
	return rand()/(RAND_MAX+1.0);
}

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

char *strlwr(char *s) {
	int i;
	for(i=0;i<strlen(s);i++) {
		s[i]=tolower(s[i]);
	}
	return s;
}

char *randline(char *fn) {
	FILE *fin=NULL;

	char *buf=NULL;
	size_t len=0;
	ssize_t num=0;

	char *line=NULL;
	size_t linenum=0;

	fin=fopen(fn,"r");

	if(!fin) {
		printf("randline: error opening file %s\n",fn);
	}

	while((num=getline(&buf,&len,fin))!=-1) {
		char *p=strchr(buf,'\0');  if(p) *p='\0';
		if(drand()<(1.0/++linenum)) {
			if(line) {
				free(line);
				line=NULL;
			}
			line=malloc(sizeof(*line)*(num+1));
			strcpy(line,buf);
		}
		free(buf);
		buf=NULL;
	}

	fclose(fin);

	return line;

}

int tokenize(char *str,char ***toks,char *dels) {
	int n=0;
	char *tok=NULL;
	tok=strtok(str,dels);
	while(tok) {
		(*toks)=realloc((*toks),sizeof(**toks)*(n+1));
		(*toks)[n++]=strdup(tok);
		tok=strtok(NULL,dels);
	}
	return n;
}

void shuffleAnagrams(char ***anagrams,int numAnagrams) {
	int i,j;
	char *tmp;
	for(i=numAnagrams-1;i>0;i--) {
		j=rand()%(i+1);
		tmp=(*anagrams)[i];
		(*anagrams)[i]=(*anagrams)[j];
		(*anagrams)[j]=tmp;
	}
}

void shuffleWord(char **word) {
	int i,j,k;
	for(i=strlen(*word)-1;i>0;i--) {
		j=rand()%(i+1);
		k=(*word)[i];
		(*word)[i]=(*word)[j];
		(*word)[j]=k;
	}
}

int cmplen(const void *a,const void *b) {
	int l=strlen(*(char**)a);
	int r=strlen(*(char**)b);
	return l-r;
}

char *append(char **a,char *fmt,...) {

	char *b=NULL;
	ssize_t lenb=0;

	va_list args;
	va_start(args,fmt);
	lenb=vsnprintf(NULL,0,fmt,args);
	b=malloc(sizeof(*b)*(lenb+1));
	vsprintf(b,fmt,args);
	va_end(args);

	if(*a) {
		(*a)=realloc(*a,sizeof(**a)*(strlen(*a)+lenb+1));
	} else {
		(*a)=realloc(*a,sizeof(**a)*(lenb+1));
		(*a)[0]='\0';
	}
	strcat(*a,b);
	return *a;
}

char *getList(int *guessed,char **anagrams,int numAnagrams) {
	int i,j;
	char *list=NULL;
	for(i=0;i<numAnagrams;i++) {
		if(i!=0) append(&list,", ");
		if(guessed[i]) {
			append(&list,anagrams[i]);
		} else {
			for(j=0;j<strlen(anagrams[i]);j++) {
				append(&list,"*");
			}
		}
		if(!strcmp(anagrams[i],bonusWord)) {
			append(&list,"?");
		}
	}
	return list;
}


void list(dyad_Stream *s,char *src,char *dst,int *guessed,char **anagrams,int numAnagrams) {
	char *list=NULL;
	list=getList(guessed,anagrams,numAnagrams);
	sendf(s,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",dst,src,list);
	free(list);
	list=NULL;
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

	int i,j;

	char *tmp=NULL;

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
						sendf(e->stream, "QUIT :%s\r\n", txt + 6);
					} else {
						sendf(e->stream, "QUIT\r\n");
					}
				}

			}

		}

		if (par[0] == '#') {

			if (!strcmp(txt, PFX "start")) {

				if(gameState==GAME_STATE_INIT) {

					char *line=randline(RANDOM_WORDS_FILE);

					strlwr(line);

					numAnagrams=tokenize(line,&anagrams,",");

					word=strdup(anagrams[0]);

					shuffleWord(&word);

					bonusWord=anagrams[rand()%numAnagrams];

					if(numAnagrams>0) {
						longWordLen=strlen(anagrams[0]);
						for(i=1;i<numAnagrams;i++) {
							if(longWordLen<strlen(anagrams[i])) {
								longWordLen=strlen(anagrams[i]);
							}
						}
					}

					guessed=malloc(sizeof(*guessed)*numAnagrams);
					for(i=0;i<numAnagrams;i++) {
						guessed[i]=0;
					}

					shuffleAnagrams(&anagrams,numAnagrams);
					qsort(anagrams,numAnagrams,sizeof(*anagrams),cmplen);

					free(line);
					line=NULL;

					sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",par,usr,"Game is starting in 10 seconds...");

					waitingTime=ticks+10;

					gameState=GAME_STATE_WAIT;
				}

			} else if (!strcmp(txt, PFX "text")) {
				if(gameState==GAME_STATE_START) {
					sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",par,usr,word);
				} else {
					sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",par,usr,"Game not started");
				}
			} else if (!strcmp(txt, PFX "twist")) {
				if(gameState==GAME_STATE_START) {
					shuffleWord(&word);
					sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",par,usr,word);
				} else {
					sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",par,usr,"Game not started");
				}
			} else if (!strcmp(txt, PFX "list")) {
				if(gameState==GAME_STATE_START) {
					list(e->stream,usr,par,guessed,anagrams,numAnagrams);
				} else {
					sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",par,usr,"Game not started");
				}
			} else {
				if(gameState==GAME_STATE_START) {

					char *msg=NULL;
					char *bonus=NULL;

					int points=0;

				  j=-1;
					for(i=0;i<numAnagrams;i++) {
						if(!strcmp(txt,anagrams[i])) {
							j=i;
							break;
						}
					}

					if(j!=-1) {

						numGuessed++;

						guessed[i]=1;

						points+=strlen(anagrams[j]);

						if(!strcmp(bonusWord,anagrams[j])) {
							points+=100;
							append(&bonus," secret word bonus! ");
						}

						if(strlen(anagrams[j])==longWordLen) {
							points+=100;
							append(&bonus," long word bonus! ");
						}

						if(numGuessed==numAnagrams) {
							points+=100;
							append(&bonus," finishing game bonus! ");
						}

						append(&msg,"%s guessed '%s' plus %d points. %s",usr,anagrams[j],points,bonus && strlen(bonus) ? bonus : "");

						sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",par,usr,msg);

					}

					if(bonus) {
						free(bonus);
						bonus=NULL;
					}

					if(msg) {
						free(msg);
						msg=NULL;
					}

				}
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
	if(gameState==GAME_STATE_WAIT) {
		if(ticks>=waitingTime) {
			sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s\r\n",par,word);
			gameState=GAME_STATE_START;
		}
	}
}

int main(int argc, char **argv)
{

	dyad_Stream *s;


	srand(time(NULL));

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
