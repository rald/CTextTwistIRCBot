#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>



#define TEXTTWIST_IMPLEMENTATION
#include "texttwist.h"

static char **words=NULL;
static size_t nwords=0;

static char **rands=NULL;
static size_t nrands=0;

static char **anagrams=NULL;
static size_t nanagrams=0;

static char *word=NULL;
static char *shufword=NULL;



static int cmpbylengthdesc(const void *a,const void *b);


int main(void) {

	size_t i;

	srand((unsigned int)time(NULL));

	loadwords("wordlist.txt",&words,&nwords,3,8);
	loadwords("randlist.txt",&rands,&nrands,6,8);

	word=rands[rand()%nrands];

	shufword=strdup(word);

	shuffleword(shufword);

	getanagrams(&anagrams,&nanagrams,words,nwords,word);

	printf("%s\n\n",word);

	qsort(anagrams,nanagrams,sizeof(*anagrams),cmpbylengthdesc);

	for(i=0;i<nanagrams;i++) {
		if(i!=0) printf(", ");
		printf("%s",anagrams[i]);
	}
	printf("\n");

	return 0;
}



static int cmpbylengthdesc(const void *a,const void *b) {
	size_t l=strlen(*(char**)a);
	size_t r=strlen(*(char**)b);

	if(l<r) return 1;
	else if(l>r) return -1;
	return rand()%3-1;
}
