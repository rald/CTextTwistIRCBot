#ifndef TEXTTWIST_H
#define TEXTTWIST_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

int loadwords(const char *path,char ***words,size_t *nwords,size_t minlen,size_t maxlen);
bool isanagram(const char *w1,const char *w2);
void getanagrams(char ***anagrams,size_t *nanagrams,char **words,size_t nwords,char *word);
char *shuffleword(char *word);



#ifdef TEXTTWIST_IMPLEMENTATION

static size_t *freq(const char *w);
static bool iszero(size_t *f);


int loadwords(const char *path,char ***words,size_t *nwords,size_t minlen,size_t maxlen) {
	FILE *fh;
	char *line=NULL;
	size_t llen=0;
	ssize_t rlen=0;

	if((fh=fopen(path,"rt"))==NULL) {
		printf("Error: cannot open %s.\n",path);
		return 1;
	}

	while((rlen=getline(&line,&llen,fh))!=-1) {
		char *p=strchr(line,'\n');
		if(p!=NULL) *p='\0';
		size_t len=strlen(line);
		if(len>=minlen && len<=maxlen) {
			(*words)=realloc(*words,sizeof(**words)*((*nwords)+1));
			(*words)[(*nwords)++]=strdup(line);
		}
		free(line);
		line=NULL;
		llen=0;
	}

	fclose(fh);

	return 0;
}

static size_t *freq(const char *w) {
	size_t i;
	size_t *f=calloc(26,sizeof(*f));

	i=0;
	while(w[i]) {
		int c=toupper(w[i]);
		if(c>='A' && c<='Z') {
			f[c-'A']++;
		}
		i++;
	}

	return f;
}


static bool iszero(size_t *f) {
	size_t i;
	for(i=0;i<26;i++) {
		if(f[i]!=0) return false;
	}
	return true;
}

bool isanagram(const char *w1,const char *w2) {

	bool result=true;

	size_t i;

	size_t *f1=freq(w1);
	size_t *f2=freq(w2);

	if(iszero(f1) || iszero(f2)) {
		result=false;
	} else {
		for(i=0;i<26;i++) {
			if(f1[i]<f2[i]) {
				result=false;
				break;
			}
		}
	}

	free(f2);
	free(f1);

	return result;

}


void getanagrams(char ***anagrams,size_t *nanagrams,char **words,size_t nwords,char *word) {
	size_t i;
	for(i=0;i<nwords;i++) {
		if(isanagram(word,words[i])) {
			(*anagrams)=realloc(*anagrams,sizeof(**anagrams)*((*nanagrams)+1));
			(*anagrams)[(*nanagrams)++]=strdup(words[i]);
			if((*nanagrams)>30) return;
		}
	}
}



char *shuffleword(char *word) {
	size_t i,j;
	char k;

	for(i=strlen(word)-1;i>0;i--) {
		j=(size_t)(rand()%(i+1));
		k=word[i];
		word[i]=word[j];
		word[j]=k;
	}

	return word;
}


#endif /* TEXTTWIST_IMPLEMENTATION  */

#endif /* TEXTTWIST_H */