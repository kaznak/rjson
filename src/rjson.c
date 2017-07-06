#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <time.h>


/* initprog *//////////////////////////////////////////////////////////////////////////////////

const char *tfmt = "%Y%m%d%H%M%S";
const size_t tfmt_len = 15;
 
char *pname;
time_t stime_t;
char *stime_s;
pid_t pid;

int initpparam(int argc,char *argv[])	{
  struct tm *tm;

  // pname
  pname = argv[0];

  // stime
  if(time(&stime_t) < 0)
    return -1;
  if((tm = localtime(&stime_t)) == NULL)
    return -1;
  if(NULL == (stime_s = malloc(tfmt_len)))
    return -1;
  if(strftime(stime_s, sizeof(stime_s), tfmt, tm) == 0)
    return -1;

  // pid
  pid = getpid();
  
  return 1;
}

int eprintmsg(int lineno, char *fmt, ...)	{
  va_list ap;

  fprintf(stderr, "%s %s %d %d ", pname, stime_s, pid, lineno);
  
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  return 1;
}
#define errmsg(...) eprintmsg(__LINE__, __VA_ARGS__)


/* parse *//////////////////////////////////////////////////////////////////////////////////

const char *pathroot = "$";
const char *pathfmt = ".%s";
const char *indfmt = "[%04d]";

struct parser	{
  char c;

  char *path;
  size_t phead;

  FILE *inf, *outf;
} prs, *p = &prs;

int init_parser(FILE *inf, FILE *outf)	{

  if(!((p->path = malloc(BUFSIZ))       	&&
       (p->path == strcpy(p->path, pathroot))	))	{
    errmsg("ERROR fatal\n");
    return 0;
  }

  p->phead = strlen(pathroot);
  p->inf = inf; p->outf = outf;
  p->c = getc(inf);

  return 1;
}

int prs_primitive();

/* prs_json *//////////////////////////////////////////////////////////////////////////////////
int prs_json()	{
  const char *white_spaces = " \n\r\t";
  const char *primitive_headder = "-0123456789tfn";
  
  int r = 0;
  size_t phsav = p->phead;

  /* skip white spaces */
 top:
  if(NULL != memchr(white_spaces, p->c, strlen(white_spaces)))	{
    p->c = getc(p->inf); goto top;
  }
  else if(NULL != memchr(primitive_headder, p->c, strlen(primitive_headder)))	{
    /* NUMBER, true, false, null */
    p->c = prs_primitive();
  }
  }
  else if(EOF == p->c)	{
    return EOF;
  } else	{
    errmsg("ERROR invalid characer %c.\n", p->c);
    exit(1);
  }
  
  return p->c;
}

/* prs_primitive *//////////////////////////////////////////////////////////////////////////////////
int prs_primitive()	{
  const char *primitive_stopper = ",:]} \n\r\t\"[{";

  printf("%s ", p->path);
  
  putc(p->c, p->outf);
  for(p->c = getc(p->inf);
      EOF == p->c || NULL == memchr(primitive_stopper, p->c, strlen(primitive_stopper));
      p->c = getc(p->inf)	)	{
    putc(p->c, p->outf);
  }
  putc('\n', p->outf);

  return p->c;
}

/* main *//////////////////////////////////////////////////////////////////////////////////

int main(int argc,char *argv[]) {
  if(!(initpparam(argc, argv)		&&
       init_parser(stdin, stdout)	))
    return 1;

  while(EOF != prs_json());

  return 0;
}
