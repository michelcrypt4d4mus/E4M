/* Program to extract function declarations from C source code */
/* Written by Eric R. Smith and placed in the public domain    */
/* Thanks are due to Jwahar R. Bammi for fixing several bugs   */
/* and providing the Unix makefiles.                           */

/* Several enhancements by Paul Le Roux paulca@rocketmail.com,
	also placed in the public domain */

#if defined(__STDC__) && !defined(minix)
#include <stddef.h>
#include <stdlib.h>
#else
#define EXIT_SUCCESS  0
#define EXIT_FAILURE  1
extern char *malloc();
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>

/*#define DEBUG(s) (fputs(s, stderr)) /* */
#define DEBUG(s) /* */

#define HEADER_BUF_SIZE 65536

#define ISCSYM(x) ((x) > 0 && (isalnum(x) || (x) == '_'))
#define ABORTED ( (Word *) -1 )
#define MAXPARAM 20 		/* max. number of parameters to a function */

int inquote = 0;		/* in a quote?? */
int newline_seen = 1;		/* are we at the start of a line */
long linenum  = 1L;		/* line number in current file */
int dostatic = 0;		/* do static functions? */
int donum    = 0;		/* print line numbers? */
int dohead   = 1;		/* do file headers? */
int docond   = 1;		/* conditionalize for non-ANSI compilers? */
int glastc   = ' ';		/* last char. seen by getsym() */
int doheader = 0; /* create/update header file based on c file name */
char* headerbuf,*headersave; /* used in doheader processing */
char copynotice[] = 
"/* Everything below this line is automatically updated by the %s";
char marker[] = "-mkproto-tool- */\n\n"; /* If this line is changed also change the usage */

typedef struct word {
	struct word *next;
	char   string[1];
} Word;

/*
 * Routines for manipulating lists of words.
 */

Word *word_alloc(s)
	char *s;
{
	Word *w;

	w = (Word *) malloc(sizeof(Word) + strlen(s) + 1); /* ++jrb */
	strcpy(w->string, s);
	w->next = NULL;
	return w;
}

void word_free(w)
	Word *w;
{
	Word *oldw;
	while (w) {
		oldw = w;
		w = w->next;
		free(oldw);
	}
}

/* return the length of a list; empty words are not counted */
int
List_len(w)
	Word *w;
{
	int count = 0;

	while (w) {
		if (*w->string) count++;
		w = w->next;
	}
	return count;
}

/* Append two lists, and return the result */

Word *word_append(w1, w2)
	Word *w1, *w2;
{
	Word *r, *w;

	r = w = word_alloc("");

	while (w1) {
		w->next = word_alloc(w1->string);
		w = w->next;
		w1 = w1->next;
	}
	while (w2) {
		w->next = word_alloc(w2->string);
		w = w->next;
		w2 = w2->next;
	}

	return r;
}
	
/* see if the last entry in w2 is in w1 */

int
foundin(w1, w2)
	Word *w1, *w2;
{
	while (w2->next)
		w2 = w2->next;

	while (w1) {
		if (!strcmp(w1->string, w2->string))
			return 1;
		w1 = w1->next;
	}
	return 0;
}

/* add the string s to the given list of words */

void addword(w, s)
	Word *w; char *s;
{
	while (w->next) w = w->next;
	w->next = word_alloc(s);
}

/* given a list representing a type and a variable name, extract just
 * the base type, e.g. "struct word *x" would yield "struct word"
 */

Word *typelist(p)
	Word *p;
{
	Word *w, *r;

	r = w = word_alloc("");
	while (p && p->next) {
		if (p->string[0] && !ISCSYM(p->string[0]))
			break;
		w->next = word_alloc(p->string);
		w = w->next;
		p = p->next;
	}
	return r;
}

/* typefixhack: promote formal parameters of type "char", "unsigned char",
   "short", or "unsigned short" to "int".
*/

void typefixhack(w)
	Word *w;
{
	Word *oldw = 0;

	while (w) {
		if (*w->string) {
			if ( (!strcmp(w->string, "char") ||
			      !strcmp(w->string, "short") )
			    && (List_len(w->next) < 2) )
			{
				if (oldw && !strcmp(oldw->string, "unsigned")) {
					oldw->next = w->next;
					free(w);
					w = oldw;
				}
				strcpy(w->string, "int");
			}
		}
		w = w->next;
	}
}

/* read a character: if it's a newline, increment the line count */

#ifdef __GNUC__	/* ++jrb */
inline
#endif
int ngetc(f)
	FILE *f;
{
	int c;

	c = getc(f);
	if (c == '\n') linenum++;

	return c;
}

/* read the next character from the file. If the character is '\' then
 * read and skip the next character. Any comment sequence is converted
 * to a blank.
 */

int fnextch(f)
	FILE *f;
{
	int c, lastc, incomment;

	c = ngetc(f);
	while (c == '\\') {
DEBUG("fnextch: in backslash loop\n");
		c = ngetc(f);	/* skip a character */
		c = ngetc(f);
	}
	if (c == '/' && !inquote) {
		c = ngetc(f);
		if (c == '*') {
			incomment = 1;
			c = ' ';
DEBUG("fnextch: comment seen\n");
			while (incomment) {
				lastc = c;
				c = ngetc(f);
				if (lastc == '*' && c == '/')
					incomment = 0;
				else if (c < 0)
					return c;
			}
			return fnextch(f);
		}
		else {
			if (c == '\n') linenum--;
			ungetc(c, f);
			return '/';
		}
	}
	return c;
}


/* Get the next "interesting" character. Comments are skipped, and strings
 * are converted to "0". Also, if a line starts with "#" it is skipped.
 */

int nextch(f)
	FILE *f;
{
	int c;

	c = fnextch(f);
	if (newline_seen && c == '#') {
		do {
			c = fnextch(f);
		} while (c >= 0 && c != '\n');
		if (c < 0)
			return c;
	}
	newline_seen = (c == '\n');

	if (c == '\'' || c == '\"') {
DEBUG("nextch: in a quote\n");
		inquote = c;
		while ( (c = fnextch(f)) >= 0 ) {
			if (c == inquote) {
				inquote = 0;
DEBUG("nextch: out of quote\n");
				return '0';
			}
		}
DEBUG("nextch: EOF in a quote\n");
	}
	return c;
}

/*
 * Get the next symbol from the file, skipping blanks.
 * Return 0 if OK, -1 for EOF.
 * Also collapses everything between { and }
 */

int
getsym(buf, f)
	char *buf; FILE *f;
{
	register int c;
	int inbrack = 0;

DEBUG("in getsym\n");
	c = glastc;
	while ((c > 0) && isspace(c)) {
		c = nextch(f);
	}
DEBUG("getsym: spaces skipped\n");
	if (c < 0) {
DEBUG("EOF read in getsym\n");
		return -1;
	}
	if (c == '{') {
		inbrack = 1;
DEBUG("getsym: in bracket\n");
		while (inbrack) {
			c = nextch(f);
			if (c < 0) {
DEBUG("getsym: EOF seen in bracket loop\n");
				glastc = c;
				return c;
			}
			if (c == '{') inbrack++;
			else if (c == '}') inbrack--;
		}
		strcpy(buf, "{}");
		glastc = nextch(f);
DEBUG("getsym: out of in bracket loop\n");
		return 0;
	}
	if (!ISCSYM(c)) {
		*buf++ = c;
		*buf = 0;
		glastc = nextch(f);
DEBUG("getsym: returning special symbol\n");
		return 0;
	}
	while (ISCSYM(c)) {
		*buf++ = c;
		c = nextch(f);
	}
	*buf = 0;
	glastc = c;
DEBUG("getsym: returning word\n");
	return 0;
}

/*
 * skipit: skip until a ";" or the end of a function declaration is seen
 */
int skipit(buf, f)
	char *buf;
	FILE *f;
{
	int i;

	do {
DEBUG("in skipit loop\n");
		i = getsym(buf, f);
		if (i < 0) return i;
	} while (*buf != ';' && *buf != '{');

	return 0;
}

/*
 * Get a parameter list; when this is called the next symbol in line
 * should be the first thing in the list.
 */

Word *getparamlist(f)
	FILE *f;
{
	static Word *pname[MAXPARAM]; /* parameter names */
	Word	*tlist,		/* type name */
		*plist;		/* temporary */
	int  	np = 0;		/* number of parameters */
	int  	typed[MAXPARAM];  /* parameter has been given a type */
	int	tlistdone;	/* finished finding the type name */
	int	sawsomething;
	int  	i;
	int	inparen = 0;
	char buf[80];

DEBUG("in getparamlist\n");
	for (i = 0; i < MAXPARAM; i++)
		typed[i] = 0;

	plist = word_alloc("");

/* first, get the stuff inside brackets (if anything) */

	sawsomething = 0;	/* gets set nonzero when we see an arg */
	for (;;) {
		if (getsym(buf, f) < 0) return NULL;
		if (*buf == ')' && (--inparen < 0)) {
			if (sawsomething) {	/* if we've seen an arg */
				pname[np] = plist;
				plist = word_alloc("");
				np++;
			}
			break;
		}
		if (*buf == ';') {	/* something weird */
			return ABORTED;
		}
		sawsomething = 1;	/* there's something in the arg. list */
		if (*buf == ',' && inparen == 0) {
			pname[np] = plist;
			plist = word_alloc("");
			np++;
		}
		else {
			addword(plist, buf);
			if (*buf == '(') inparen++;
		}
	}

/* next, get the declarations after the function header */

	inparen = 0;

	tlist = word_alloc("");
	plist = word_alloc("");
	tlistdone = 0;
	sawsomething = 0;
	for(;;) {
		if (getsym(buf, f) < 0) return NULL;

/* handle a list like "int x,y,z" */
		if (*buf == ',' && !inparen) {
			if (!sawsomething)
				return NULL;
			for (i = 0; i < np; i++) {
				if (!typed[i] && foundin(plist, pname[i])) {
					typed[i] = 1;
					word_free(pname[i]);
					pname[i] = word_append(tlist, plist);
				/* promote types */
					typefixhack(pname[i]);
					break;
				}
			}
			if (!tlistdone) {
				tlist = typelist(plist);
				tlistdone = 1;
			}
			word_free(plist);
			plist = word_alloc("");
		}
/* handle the end of a list */
		else if (*buf == ';') {
			if (!sawsomething)
				return ABORTED;
			for (i = 0; i < np; i++) {
				if (!typed[i] && foundin(plist, pname[i])) {
					typed[i] = 1;
					word_free(pname[i]);
					pname[i] = word_append(tlist, plist);
					typefixhack(pname[i]);
					break;
				}
			}
			tlistdone = 0;
			word_free(tlist); word_free(plist);
			tlist = word_alloc("");
			plist = word_alloc("");
		}
/* handle the  beginning of the function */
		else if (!strcmp(buf, "{}")) break;
/* otherwise, throw the word into the list (except for "register") */
		else if (strcmp(buf, "register")) {
			sawsomething = 1;
			addword(plist, buf);
			if (*buf == '(') inparen++;
			if (*buf == ')') inparen--;
		}
	}

/* Now take the info we have and build a prototype list */

/* empty parameter list means "void" */
	if (np == 0)
		return word_alloc("void");

	plist = tlist = word_alloc("");
	for (i = 0; i < np; i++) {

/* If no type provided, make it an "int" */
		if ( !(pname[i]->next) ||
	   (!(pname[i]->next->next)&&strcmp(pname[i]->next->string, "void"))) {
			addword(tlist, "int");
		}
		while (tlist->next) tlist = tlist->next;
		tlist->next = pname[i];
		if (i < np - 1)
			addword(tlist, ", ");
	}
	return plist;
}

/*
 * emit a function declaration. The attributes and name of the function
 * are in wlist; the parameters are in plist.
 */
void emit(wlist, plist, startline)
	Word *wlist, *plist;
	long  startline;
{
	Word *w;
	int count = 0;
	char tmp[256];

DEBUG("emit called\n");
	if (donum) {
		sprintf(tmp,"/*%8ld */ ", startline);
		if (!doheader) 
			printf(tmp);
		else 
			strcat(headerbuf,tmp);
	}

	for (w = wlist; w; w = w->next) {
		if (w->string[0])
			count ++;
	}

	if (count < 2) {
		sprintf(tmp,"int ");
		if (!doheader) 
			printf(tmp);
		else 
			strcat(headerbuf,tmp);
	}

	for (w = wlist; w; w = w->next) {
		sprintf(tmp,"%s", w->string);
		if (!doheader) 
			printf(tmp);
		else 
			strcat(headerbuf,tmp);
		if (ISCSYM(w->string[0])) {
			sprintf(tmp," ");
			if (!doheader) 
				printf(tmp);
			else 
				strcat(headerbuf,tmp);
		}
	}
	if (docond) {
		sprintf(tmp,"P((");
	} else {
		sprintf(tmp,"( ");
	}
	if (!doheader) 
		printf(tmp);
	else 
		strcat(headerbuf,tmp);

	for (w = plist; w; w = w->next) {
		sprintf(tmp,"%s", w->string);
		if (!doheader) 
			printf(tmp);
		else 
			strcat(headerbuf,tmp);
		if (ISCSYM(w->string[0])) {
			sprintf(tmp," ");
			if (!doheader) 
				printf(tmp);
			else 
				strcat(headerbuf,tmp);
		}
	}
	if (docond)
		sprintf(tmp,"));\n");
	else
		sprintf(tmp,");\n");
	if (!doheader) 
		printf(tmp);
	else 
		strcat(headerbuf,tmp);
}

/*
 * get all the function declarations
 */

void getdecl(f, header)
	FILE *f, *header;
{
	Word *plist, *wlist = NULL;
	char buf[80];
	int sawsomething;
	long startline;		/* line where declaration started */
	int oktoprint;
again:
	word_free(wlist);
	wlist = word_alloc("");
	sawsomething = 0;
	oktoprint = 1;

	for(;;) {
DEBUG("main getdecl loop\n");
		if (getsym(buf,f) < 0) {
DEBUG("EOF in getdecl loop\n");
			 return;
		}
/* try to guess when a declaration is not an external function definition */
		if (!strcmp(buf, ",") || !strcmp(buf, "{}") ||
		    !strcmp(buf, "=") || !strcmp(buf, "typedef") ||
		    !strcmp(buf, "extern")) {
			skipit(buf, f);
			goto again;
		}
		if (!dostatic && !strcmp(buf, "static")) {
			oktoprint = 0;
		}
/* for the benefit of compilers that allow "inline" declarations */
		if (!strcmp(buf, "inline") && !sawsomething)
			continue;
		if (!strcmp(buf, ";")) goto again;

/* A left parenthesis *might* indicate a function definition */
		if (!strcmp(buf, "(")) {
			startline = linenum;
			if (!sawsomething || !(plist = getparamlist(f))) {
				skipit(buf, f);
				goto again;
			}
			if (plist == ABORTED)
				goto again;

/* It seems to have been what we wanted */
			if (oktoprint)
				emit(wlist, plist, startline);
			word_free(plist);
			goto again;
		}
		addword(wlist, buf);
		sawsomething = 1;
	}
}

void
main(argc, argv)
int argc; char **argv;
{
	FILE *f, *header;
	char *t;
	char initbuf[256];
	extern void Usage();

	argv++; argc--;

	while (*argv && **argv == '-') {
		t = *argv++; --argc; t++;
		while (*t) {
			if (*t == 's')
				dostatic = 1;
			else if (*t == 'n')
				donum = 1;
			else if (*t == 'p')
				docond = 0;
			else if (*t == 'h')
				doheader = 1;
			else
				Usage();
			t++;
		}
	}

	if (!(headerbuf = malloc(HEADER_BUF_SIZE)) 
		|| !(headersave = malloc(HEADER_BUF_SIZE))) {
		fprintf(stderr, "Out of memory.\n");
		exit(EXIT_FAILURE);
	}  else 
		*headerbuf = 0;

	if (argc == 0 && doheader) {
		fputs("-h has no effect when using stdin\n", stderr);
		exit (EXIT_FAILURE);
	}

	if (!doheader) {
		printf(copynotice, marker);
	}

	if (docond) {
		char tmp[256];
		sprintf(tmp,"#ifdef __STDC__\n");
		if (!doheader) 
			printf(tmp);
		else 
			strcat(initbuf,tmp);
		sprintf(tmp,"# define\tP(s) s\n");
		if (!doheader) 
			printf(tmp);
		else 
			strcat(initbuf,tmp);
		sprintf(tmp,"#else\n");
		if (!doheader) 
			printf(tmp);
		else 
			strcat(initbuf,tmp);
		sprintf(tmp,"# define P(s) ()\n");
		if (!doheader) 
			printf(tmp);
		else 
			strcat(initbuf,tmp);
		sprintf(tmp,"#endif\n\n");
		if (!doheader) 
			printf(tmp);
		else 
			strcat(initbuf,tmp);
	} 

	if (argc == 0)
		getdecl(stdin);
	else
		while (argc > 0 && *argv) {
DEBUG("trying a new file\n");
			
			if (!(f = fopen(*argv, "r"))) {
				perror(*argv);
				exit(EXIT_FAILURE);
			} else if (doheader) {
				char *ptr = strrchr(*argv,'.');
				if (ptr) {
					if (_strcmpi(ptr,".c")==0)
						strcpy(ptr,".h");
					else if (_strcmpi(ptr,".cpp")==0) 
						strcpy(ptr,".h");
					else {
						fprintf(stderr, "%s no .c or .cpp extenson found for -h option\n", *argv);
						exit(EXIT_FAILURE);
					}
				
					if ((header = fopen(*argv, "r"))) {
						int nLen;
						nLen = fread(headerbuf, 1, HEADER_BUF_SIZE, header);
						/* VC5 bug fix here. */
						headerbuf[nLen] = 0;
						if (nLen==0) {
							fprintf(stderr, "%s this file looks empty\n", *argv);
							exit(EXIT_FAILURE);
						} else if (ferror(header)) {
							perror(*argv);
							exit(EXIT_FAILURE);
						} else {
							strcpy(headersave,headerbuf);
							ptr = strstr(headerbuf, marker);
							if (!ptr) {
								fprintf(stderr, "%s no marker found in existing file, see -h option\n", *argv, marker);
								exit(EXIT_FAILURE);
							} else {
								*(ptr + strlen(marker)) = 0;
							}
							fclose(header);
						}
					} else {
						*headersave = 0;
						sprintf(headerbuf, copynotice, marker);
					}
					
					if (docond) 
						strcat(headerbuf, initbuf);

				} else  {
					fprintf(stderr, "%s no .c or .cpp extenson found for -h option\n", *argv);
					exit(EXIT_FAILURE);
				}
			} else 
				header = stdout;

			if (dohead && !doheader) {
				char tmp[256];
				sprintf(tmp,"\n/* %s */\n", *argv);
				printf(tmp);
			}
			linenum = 1;
			newline_seen = 1;
			glastc = ' ';
DEBUG("calling getdecl\n");
			getdecl(f,header);
DEBUG("back from getdecl\n");

			if (doheader) {
				if (docond) {
					char tmp[256];
					sprintf(tmp,"\n#undef P\n");	/* clean up namespace */
					strcat(headerbuf,tmp); 
				}

				if (strcmp(headerbuf,headersave) != 0) {
					if (!(header = fopen(*argv, "w"))) {
						perror(*argv);
						exit(EXIT_FAILURE);
					} else {
						fprintf(header, headerbuf);
						fclose(header);
						fprintf(stderr, "%s updated!\n", *argv);
					}
				} else {
					fprintf(stderr, "%s skipped - file not out of date!\n", *argv);
				}
			}
				
			argc--; argv++;
			fclose(f);
			DEBUG("back from fclose\n");
		}

	if (docond && !doheader) {
		char tmp[256];
		sprintf(tmp,"\n#undef P\n");	/* clean up namespace */
		printf(tmp);
	}
	if (headerbuf)  {
		free(headerbuf);
	}
	if (headersave)  {
		free(headersave);
	}
	exit(EXIT_SUCCESS);
}


void Usage()
{
	fputs("Usage: mkproto [-n][-s][-p][-h][files ...]\n",stderr);
	fputs("   -n: put line numbers of declarations as comments\n",stderr);
	fputs("   -s: include declarations for static functions\n", stderr);
	fputs("   -p: don't make header files readable by non-ANSI compilers\n",
	      stderr);
	fputs("   -h: update header, marker is \"-mkproto-tool- */\\n\\n\"\n\n",
	      stderr);
	fputs("   The filename for the -h option is the base .c name replaced with .h\n",
	      stderr);
	fputs("   so the header for myfile.c would be myfile.h\n",
	      stderr);
	fputs("   The limit for the header size for -h is 64kb.\n",
	      stderr);
	exit(EXIT_FAILURE);
}

