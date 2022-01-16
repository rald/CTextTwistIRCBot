#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "dyad.h"

#define GAME_TITLE "TEXTTWIST"

#define RANDOM_WORDS_FILE "rand.txt"

#define SCORE_FILE "score.txt"

#define PFX "."

static int prt = 6667;
static char *srv = "sakura.jp.as.dal.net";
static char *chn = "#pantasya";
static char *nck = "sieste";
static char *pss = NULL;
static char *mst = "siesta";
static char *mps = "143445254";

typedef enum GameState GameState;

enum GameState {
	GAME_STATE_INIT=0,
	GAME_STATE_START,
	GAME_STATE_MAX
};

static char *cmd=NULL, *usr=NULL, *par=NULL, *txt=NULL;

static int isReg = 0;
static int isAuth = 0;

static int ticks = 0;

static GameState gameState=GAME_STATE_INIT;
static char **anagrams=NULL;
static size_t numAnagrams=0;
static char *bonusWord=NULL;
static size_t longWordLen=0;
static int *guessed=NULL;
static size_t numGuessed=0;
static char *word=NULL;
static int waitingTime=180;


typedef struct Player Player;

struct Player {
	char *nick;
	int score;
};

static Player **players=NULL;
static size_t nplayers=0;

static double drand() {
	return rand()/(RAND_MAX+1.0);
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
		while (*frontp) {
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

static void sendf(dyad_Stream * s, char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	(void)vprintf(fmt, args);
	va_end(args);

	va_start(args, fmt);
	dyad_vwritef(s, fmt, args);
	va_end(args);
}

static char *strlwr(char *s) {
	size_t i;
	for(i=0; i<strlen(s); i++) {
		s[i]=tolower(s[i]);
	}
	return s;
}

static char *randline(char *fn) {
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
		char *p=strchr(buf,'\0');
		if(p) *p='\0';
		if(drand()<(1.0/++linenum)) {
			if(line) {
				free(line);
				line=NULL;
			}
			line=calloc((size_t)num+1,sizeof(*line));
			strcpy(line,buf);
		}
		free(buf);
		buf=NULL;
	}

	(void)fclose(fin);

	return line;

}

static size_t tokenize(char *str,char ***toks,char *dels) {
	size_t n=0;
	char *tok=NULL;
	tok=strtok(str,dels);
	while(tok) {
		(*toks)=realloc(*toks,sizeof(**toks)*(n+1));
		(*toks)[n++]=strdup(tok);
		tok=strtok(NULL,dels);
	}
	return n;
}

static void shuffleAnagrams(char ***anagrams,size_t numAnagrams) {
	size_t i,j;
	char *tmp;
	for(i=numAnagrams-1; i>0; i--) {
		j=(size_t)(rand()%(i+1));
		tmp=(*anagrams)[i];
		(*anagrams)[i]=(*anagrams)[j];
		(*anagrams)[j]=tmp;
	}
}

static void shuffleWord(char **word) {
	size_t i,j;
	char k;
	for(i=strlen(*word)-1; i>0; i--) {
		j=(size_t)(rand()%(i+1));
		k=(*word)[i];
		(*word)[i]=(*word)[j];
		(*word)[j]=k;
	}
}

static int cmplen(const void *a,const void *b) {
	size_t l=strlen(*(char**)a);
	size_t r=strlen(*(char**)b);
	if(l<r) return 1;
	else if(l>r) return -1;
	return 0;
}

static char *append(char **a,const char *fmt,...) {

	char *b=NULL;
	ssize_t lenb=0;

	va_list args;

	va_start(args,fmt);
	lenb=vsnprintf(NULL,0,fmt,args);
	va_end(args);

	va_start(args,fmt);
	b=calloc(lenb+1,sizeof(*b));
	(void)vsnprintf(b,lenb+1,fmt,args);
	va_end(args);

	if(*a) {
		(*a)=realloc(*a,sizeof(**a)*(strlen(*a)+lenb+1));
	} else {
		(*a)=calloc(lenb+1,sizeof(**a));
	}

	strcat(*a,b);

	free(b);

	return *a;
}

static char *getList(int *guessed,char **anagrams,size_t numAnagrams,int show) {
	size_t i,j;
	char *list=NULL;
	for(i=0; i<numAnagrams; i++) {
		if(i!=0) { (void)append(&list,", "); }
		if(show || guessed[i]) {
			if(guessed[i]) {
				(void)append(&list,"%s",anagrams[i]);
			} else {
				(void)append(&list,"%s",anagrams[i]);
			}
		} else {
			for(j=0; j<strlen(anagrams[i]); j++) {
				(void)append(&list,"*");
			}
		}
		if(!strcasecmp(anagrams[i],bonusWord)) {
			(void)append(&list,"?");
		}
	}
	return list;
}

static Player *CreatePlayer(char *nick,int score) {
	Player *player=malloc(sizeof(*player));
	if(player) {
		player->nick=strdup(nick);
		player->score=score;
	}
	return player;
}

static void DestroyPlayer(Player **player) {
	free((*player)->nick);
	(*player)->nick=NULL;
	(*player)->score=0;
	free(*player);
	player=NULL;
}

static void DestroyPlayers(Player ***players,size_t *nplayers) {
	size_t i;
	for(i=0; i<(*nplayers); i++) {
		DestroyPlayer(&((*players)[i]));
	}
	free(*players);
	(*players)=NULL;
	(*nplayers)=0;
}

static void addPlayer(Player ***players,size_t *nplayers,char *nick,int score) {
	(*players)=realloc(*players,sizeof(*players)*((*nplayers)+1));
	(*players)[(*nplayers)++]=CreatePlayer(nick,score);
}

static ssize_t findPlayer(Player **players,size_t nplayers,char *nick) {
	size_t i;
	ssize_t j=-1;
	for(i=0; i<nplayers; i++) {
		if(!strcasecmp(nick,players[i]->nick)) {
			j=(ssize_t)i;
			break;
		}
	}
	return j;
}

static int loadScores(char *path,Player ***players,size_t *nplayers) {
	FILE *fh;
	char nick[32];
	int score=0;

	if((fh=fopen(path,"rt"))==NULL) {
		printf("Error: cannot open %s.\n",path);
		return 1;
	}

	if((*players)!=NULL) {
		DestroyPlayers(players,nplayers);
	}

	while(fscanf(fh,"%s %d\n",nick,&score)==2) {
		addPlayer(players,nplayers,nick,score);
	}

	(void)fclose(fh);

	return 0;
}

static int saveScores(char *path,Player **players,size_t nplayers) {
	FILE *fh;
	size_t i;

	if((fh=fopen(path,"wt"))==NULL) {
		printf("Error: cannot open %s.\n",path);
		return 1;
	}

	for(i=0; i<nplayers; i++) {
		fprintf(fh,"%s %d\n",players[i]->nick,players[i]->score);
	}

	(void)fclose(fh);

	return 0;
}

static int cmpbyscoredesc(const void *a,const void *b) {
	Player l=*(Player*)a;
	Player r=*(Player*)b;
	if(l.score<r.score) return 1;
	else if(l.score>r.score) return -1;
	return 0;
}

static void onConnect(dyad_Event * e) {
	if (pss) {
		dyad_writef(e->stream, "PASS %s\r\n", pss);
	}
	dyad_writef(e->stream, "NICK %s\r\n", nck);
	dyad_writef(e->stream, "USER %s %s %s :%s\r\n", nck, nck, nck, nck);
}

static void onError(dyad_Event * e) {
	printf("error: %s\n", e->msg);
}

static void onLine(dyad_Event * e) {

	size_t i;
	ssize_t j,k;

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
		(void)skip(usr, '!');
	}
	(void)skip(cmd, '\r');
	par = skip(cmd, ' ');
	txt = skip(par, ':');

	(void)trim(par);

	(void)trim(txt);

	/*
		printf("usr: %s\n",usr);
		printf("cmd: %s\n",cmd);
		printf("par: %s\n",par);
		printf("txt: %s\n",txt);
		printf("\n");
	*/

	if (!strcmp(cmd, "PING")) {
		sendf(e->stream, "PONG :%s\r\n", txt);
	} else if (!strcmp(cmd, "001")) {
		printf("connected.\n");
		sendf(e->stream, "JOIN %s\r\n", chn);
		isReg = 1;

		loadScores(SCORE_FILE,&players,&nplayers);

	} else if (!strcmp(cmd, "PRIVMSG")) {

		printf("<%s> %s\n", usr, txt);

		if (!strcasecmp(usr, mst)) {

			if (!strcasecmp(par, nck)) {

				if (!strncmp(txt, PFX "auth", 5)) {
					if (strlen(txt) > 6 && !strcmp(txt + 6, mps)) {
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

				if (!strncasecmp(txt, PFX "quit", 5)) {
					if (strlen(txt) > 6) {
						sendf(e->stream, "QUIT :%s\r\n", txt + 6);
					} else {
						sendf(e->stream, "QUIT\r\n");
					}
				}

			}
		}

		if (!strcasecmp(txt, PFX "start")) {

			if(gameState==GAME_STATE_INIT) {

				char *line;

				loadScores(SCORE_FILE,&players,&nplayers);

				line=randline(RANDOM_WORDS_FILE);

				(void)strlwr(line);

				for(i=0;i<numAnagrams;i++) {
					free(anagrams[i]);
					anagrams[i]=NULL;
				}
				numAnagrams=0;

				numAnagrams=tokenize(line,&anagrams,",");

				word=strdup(anagrams[0]);

				shuffleWord(&word);

				bonusWord=anagrams[rand()%numAnagrams];

				if(numAnagrams>0) {
					longWordLen=strlen(anagrams[0]);
					for(i=1; i<numAnagrams; i++) {
						if(longWordLen<strlen(anagrams[i])) {
							longWordLen=strlen(anagrams[i]);
						}
					}
				}

				guessed=malloc(sizeof(*guessed)*numAnagrams);
				for(i=0; i<numAnagrams; i++) {
					guessed[i]=0;
				}
				numGuessed=0;

				shuffleAnagrams(&anagrams,numAnagrams);
				qsort(anagrams,numAnagrams,sizeof(*anagrams),cmplen);

				free(line);
				line=NULL;

				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s\r\n",chn,word);

				ticks=0;

				gameState=GAME_STATE_START;
			}
		} else if (!strcasecmp(txt,PFX "top")) {
			char *msg=NULL;
			qsort(players,nplayers,sizeof(*players),cmpbyscoredesc);
			for(i=0; i<(nplayers<10?nplayers:10); i++) {
				if(i!=0) { (void)append(&msg,", "); }
				(void)append(&msg,"%d. %s %d",i+1,players[i]->nick,players[i]->score);
			}
			if(msg) {
				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " TOP: %s\r\n",chn,msg);
				free(msg);
				msg=NULL;
			}
		} else if (!strncasecmp(txt,PFX "score",6)) {
			if(strlen(txt)>7) {
				k=findPlayer(players,nplayers,txt+7);
				if(k!=-1) sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s score is %d.\r\n",chn,usr,players[k]->nick,players[k]->score);
			} else {
				k=findPlayer(players,nplayers,usr);
				if(k!=-1) sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: your score is %d.\r\n",chn,usr,players[k]->score);
			}
		} else if (!strcasecmp(txt, PFX "text")) {
			if(gameState==GAME_STATE_START) {
				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",chn,usr,word);
			} else {
				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",chn,usr,"Game not started");
			}
		} else if (!strcasecmp(txt, PFX "twist")) {
			if(gameState==GAME_STATE_START) {
				shuffleWord(&word);
				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",chn,usr,word);
			} else {
				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",chn,usr,"Game not started");
			}
		} else if (!strcasecmp(txt, PFX "list")) {
			if(gameState==GAME_STATE_START) {
				char *list=NULL;
				list=getList(guessed,anagrams,numAnagrams,0);
				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",chn,usr,list);
				free(list);
				list=NULL;
			} else {
				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",chn,usr,"Game not started");
			}
		} else {
			if(gameState==GAME_STATE_START) {

				char *msg=NULL;
				char *bonus=NULL;

				int points=0;

				j=-1;
				for(i=0; i<numAnagrams; i++) {
					if(!strcasecmp(txt,anagrams[i])) {
						j=(ssize_t)i;
						break;
					}
				}

				if(j!=-1) {

					if(guessed[j]) {
						sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: '%s' already guessed.\r\n",chn,usr,anagrams[j]);

					} else {

						numGuessed++;

						guessed[j]=1;

						points+=strlen(anagrams[j]);

						if(!strcasecmp(bonusWord,anagrams[j])) {
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

						sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s\r\n",chn,msg);

						if(points>0) {
							k=findPlayer(players,nplayers,usr);
							if(k!=-1) {
								players[k]->score+=points;
							} else {
								addPlayer(&players,&nplayers,usr,points);
							}
							(void)saveScores(SCORE_FILE,players,nplayers);
						}

					}

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

	free(tmp);
	tmp = NULL;

}

static void onTick(dyad_Event * e) {
	ticks++;
	if(gameState==GAME_STATE_START) {
		if(ticks>=waitingTime) {

			char *list=NULL;

			sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " Time Up\r\n",chn);

			list=getList(guessed,anagrams,numAnagrams,1);

			sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s\r\n",chn,list);

			if(list) {
				free(list);
				list=NULL;
			}

			sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " Game Over\r\n",chn);
			gameState=GAME_STATE_INIT;
		}
	}
}

int main(void) {

	dyad_Stream *s;

	srand((unsigned int)time(NULL));

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
