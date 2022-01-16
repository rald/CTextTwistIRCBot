#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

static char **w1=NULL;
static size_t nw1=0;
static char **w2=NULL;
static size_t nw2=0;

static char **anagrams=NULL;
static size_t nanagrams=0;

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

size_t *freq(const char *w) {
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


bool iszero(size_t *f) {
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


int cmpbylengthdesc(const void *a,const void *b) {
	size_t l=strlen((char*)a);
	size_t r=strlen((char*)b);
	if(l<r) return 1;
	else if(l>r) return -1;
	return 0;
}


int main(void) {

	size_t i,j;
	FILE *fh;

	loadwords("wordlist.txt",&w1,&nw1,3,8);
	loadwords("randlist.txt",&w2,&nw2,6,8);

	if((fh=fopen("x.txt","wt"))==NULL) {
		printf("Error: cannot open x.txt\n");
		return 1;
	}

	for(i=0;i<nw2;i++) {
		getanagrams(&anagrams,&nanagrams,w1,nw1,w2[i]);
		if(nanagrams>=10 && nanagrams<=30) {

			printf("%s\n",w2[i]);		
			fprintf(fh,"%s\n",w2[i]);

			fflush(fh);

/*
			qsort(anagrams,nanagrams,sizeof(*anagrams),cmpbylengthdesc);
			for(j=0;j<nanagrams;j++) {
				if(j!=0) printf(",");
				printf("%s",anagrams[j]);
			}
			printf("\n");
*/
		}

		for(j=0;j<nanagrams;j++) {
			free(anagrams[j]);
			anagrams[j]=NULL;
		}
		free(anagrams);
		anagrams=NULL;
		nanagrams=0;
	}

	fclose(fh);

	return 0;
}
