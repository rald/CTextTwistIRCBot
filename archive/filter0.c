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

int main(int argc,char **argv) {

	size_t i;

	if(argc!=4) {
		printf("Syntax: %s file minlen maxlen\n",argv[0]);
		return 1;
	}

	loadwords(argv[1],&w1,&nw1,atoi(argv[2]),atoi(argv[3]));

	for(i=0;i<nw1;i++) {
		printf("%s\n",w1[i]);
	}

	return 0;
}
