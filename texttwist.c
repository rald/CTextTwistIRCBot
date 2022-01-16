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

#define TEXTTWIST_IMPLEMENTATION
#include "texttwist.h"

#define GAME_TITLE "TEXTTWIST"
#define PFX "."
#define SCORE_FILE "score.txt"

static char *srv = "sakura.jp.as.dal.net";
static int prt = 6667;
static char *chn = "#pantasya";
static char *nck = "sieste";
static char *pss = NULL;

static size_t allotedtime=180;

static char *cmd=NULL, *usr=NULL, *par=NULL, *txt=NULL;

static int isReg=0;

typedef enum GameState GameState;

enum GameState {
	GAME_STATE_INIT=0,
	GAME_STATE_START,
	GAME_STATE_MAX
};

static GameState gamestate=GAME_STATE_INIT;

typedef struct Player Player;

struct Player {
	char *nick;
	int score;
};

static Player **players=NULL;
static size_t nplayers=0;

static char **words=NULL;
static size_t nwords=0;

static char **rands=NULL;
static size_t nrands=0;

static char **anagrams=NULL;
static size_t nanagrams=0;

static char *word=NULL;
static char *shufword=NULL;

static int *guessed=NULL;
static size_t numguessed=0;

static char *bonusWord=NULL;
static size_t longWordLen=0;

static size_t ticks=0;



static void sendf(dyad_Stream * s, char *fmt, ...);
static char *trim(char *str);
static char *skip(char *s, char c);
static char *append(char **a,const char *fmt, ...);

static Player *CreatePlayer(char *nick,int score);
static void DestroyPlayer(Player **player);
static void DestroyPlayers(Player ***players,size_t *nplayers);
static void AddPlayer(Player ***players,size_t *nplayers,char *nick,int score);
static ssize_t FindPlayer(Player **players,size_t nplayers,char *nick);

static int loadscores(char *path,Player ***players,size_t *nplayers);
static int savescores(char *path,Player **players,size_t nplayers);

static char *getlist(int *guessed,char **anagrams,size_t numAnagrams,int show);

static int cmpbyscoredesc(const void *a,const void *b);
static int cmpbylengthdesc(const void *a,const void *b);

static void onConnect(dyad_Event *e);
static void onError(dyad_Event *e);
static void onTick(dyad_Event *e);
static void onLine(dyad_Event *e);



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

static void AddPlayer(Player ***players,size_t *nplayers,char *nick,int score) {
	(*players)=realloc(*players,sizeof(*players)*((*nplayers)+1));
	(*players)[(*nplayers)++]=CreatePlayer(nick,score);
}

static ssize_t FindPlayer(Player **players,size_t nplayers,char *nick) {
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



static int loadscores(char *path,Player ***players,size_t *nplayers) {
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
		AddPlayer(players,nplayers,nick,score);
	}

	(void)fclose(fh);

	return 0;
}

static int savescores(char *path,Player **players,size_t nplayers) {
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

static char *getlist(int *guessed,char **anagrams,size_t numAnagrams,int show) {
	size_t i,j;
	char *list=NULL;
	for(i=0; i<numAnagrams; i++) {
		if(i!=0) { (void)append(&list,", "); }
		if(show || guessed[i]) {
			if(guessed[i]) {
				(void)append(&list,"%s+",anagrams[i]);
			} else {
				(void)append(&list,"%s-",anagrams[i]);
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

static int cmpbyscoredesc(const void *a,const void *b) {
	Player *l=*(Player**)a;
	Player *r=*(Player**)b;
	if(l->score<r->score) return 1;
	else if(l->score>r->score) return -1;
	return 0;
}

static int cmpbylengthdesc(const void *a,const void *b) {
	size_t l=strlen(*(char**)a);
	size_t r=strlen(*(char**)b);
	if(l<r) return 1;
	else if(l>r) return -1;
	return rand()%3-1;
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
	ticks++;
	if(gamestate==GAME_STATE_START) {
		if(ticks>=allotedtime) {

			sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " Time Up\r\n",chn);

			char *list=NULL;
			list=getlist(guessed,anagrams,nanagrams,1);
			sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s\r\n",chn,list);
			free(list);
			list=NULL;

			sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " Game Over\r\n",chn);
			gamestate=GAME_STATE_INIT;
		}
	}
}

static void onLine(dyad_Event *e) {

	size_t i;

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

		loadscores(SCORE_FILE,&players,&nplayers);

		loadwords("wordlist.txt",&words,&nwords,3,8);
		loadwords("randlist.txt",&rands,&nrands,6,8);

	} else if (strcmp(cmd, "PRIVMSG")==0) {
		printf("<%s> %s\n", usr, txt);

		if(strcasecmp(txt,PFX "start")==0) {
			if(gamestate==GAME_STATE_INIT) {
				word=rands[rand()%nrands];

				if(shufword!=NULL) free(shufword);

				shufword=strdup(word);
				shuffleword(shufword);

				if(anagrams!=NULL) {
					for(i=0;i<nanagrams;i++) {
						free(anagrams[i]);
						anagrams[i]=NULL;
					}
					free(anagrams);
					anagrams=NULL;
					nanagrams=0;
				}

				getanagrams(&anagrams,&nanagrams,words,nwords,word);

				qsort(anagrams,nanagrams,sizeof(*anagrams),cmpbylengthdesc);


				if(guessed!=NULL) {
					free(guessed);
					guessed=NULL;
				}

				guessed=malloc(sizeof(*guessed)*nanagrams);
				for(i=0;i<nanagrams;i++) {
					guessed[i]=0;
				}
				numguessed=0;

				bonusWord=anagrams[rand()%nanagrams];

				longWordLen=strlen(word);

				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s\r\n",chn,shufword);

				ticks=0;

				gamestate=GAME_STATE_START;

			}
		} else if(strcasecmp(txt,PFX "text")==0) {
			if(gamestate==GAME_STATE_START) {
				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s\r\n",chn,shufword);
			}
		} else if(strcasecmp(txt,PFX "twist")==0) {
			if(gamestate==GAME_STATE_START) {
				shuffleword(shufword);
				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s\r\n",chn,shufword);
			}
		} else if(strcasecmp(txt,PFX "list")==0) {
			if(gamestate==GAME_STATE_START) {
				char *list=NULL;
				list=getlist(guessed,anagrams,nanagrams,0);
				sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s\r\n",chn,usr,list);
				free(list);
				list=NULL;
			}
		} else if(strncasecmp(txt,PFX "score",6)==0) {
			ssize_t k;
			if(strlen(txt)>7) {
				k=FindPlayer(players,nplayers,txt+7);
				if(k!=-1) sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: %s score is %d.\r\n",chn,usr,players[k]->nick,players[k]->score);
			} else {
				k=FindPlayer(players,nplayers,usr);
				if(k!=-1) sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: your score is %d.\r\n",chn,usr,players[k]->score);
			}
		} else if(strcasecmp(txt,PFX "top")==0) {
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
		} else {

			if(gamestate==GAME_STATE_START) {

				size_t i;
				ssize_t j;

				char *msg=NULL;
				char *bonus=NULL;

				int points=0;

				j=-1;
				for(i=0; i<nanagrams; i++) {
					if(!strcasecmp(txt,anagrams[i])) {
						j=(ssize_t)i;
						break;
					}
				}

				if(j!=-1) {

					if(guessed[j]) {
						sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s: '%s' already guessed.\r\n",chn,usr,anagrams[j]);
					} else {

						numguessed++;

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

						if(numguessed==nanagrams) {
							points+=100;
							append(&bonus," finishing game bonus! ");
						}

						append(&msg,"%s guessed '%s' plus %d points. %s",usr,anagrams[j],points,bonus && strlen(bonus) ? bonus : "");

						sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s\r\n",chn,msg);

						if(points>0) {
							ssize_t k=FindPlayer(players,nplayers,usr);
							if(k!=-1) {
								players[k]->score+=points;
							} else {
								AddPlayer(&players,&nplayers,usr,points);
							}
							(void)savescores(SCORE_FILE,players,nplayers);
						}

						if(numguessed==nanagrams) {

							sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " Game Finished\r\n",chn);


							char *list=NULL;
							list=getlist(guessed,anagrams,nanagrams,1);
							sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " %s\r\n",chn,list);
							free(list);
							list=NULL;

							sendf(e->stream,"PRIVMSG %s :" GAME_TITLE " Game Over\r\n",chn);

							gamestate=GAME_STATE_INIT;
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



