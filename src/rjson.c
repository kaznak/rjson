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

int prs_skip_ws()	{
  const char *white_spaces = " \n\r\t";

  while(NULL != memchr(white_spaces, p->c, strlen(white_spaces)))
    p->c = getc(p->inf);

  return p->c;
}

int prs_primitive();
int prs_string();
int prs_array();

/* prs_json *//////////////////////////////////////////////////////////////////////////////////
int prs_json()	{
  const char *primitive_headder = "-0123456789tfn";
  
  prs_skip_ws();

  if(NULL != memchr(primitive_headder, p->c, strlen(primitive_headder)))	{
    /* NUMBER, true, false, null */
    p->c = prs_primitive();
  }
  else if('"' == p->c)	{
    /* STRING */
    p->c = prs_string();
  }
  else if('[' == p->c) {
    /* ARRAY */
    p->c = prs_array();
  }
  else if(EOF == p->c)	{
    ;
  }
  else	{
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

/* prs_string *//////////////////////////////////////////////////////////////////////////////////

inline unsigned int unicode2utf8(unsigned int uc) {
	return 
	(0xe0 | ((uc >> 12) & 0x0f)) << 16 |
	(0x80 | ((uc >>  6) & 0x3f)) << 8  |
	(0x80 | (uc & 0x3f));
}

int prs_string()	{

  int i;
  char uenc[5];
  unsigned int uchi = 0;
  uenc[4] = '\0';

  printf("%s ", p->path);
  putc(p->c, p->outf);
  
 str:
  switch(p->c = getc(p->inf))	{
  case '"':
    putc(p->c, p->outf);
    putc('\n', p->outf);
    return (p->c = getc(p->inf));
  case '\\':
    goto esc;
  case EOF:
    errmsg("ERROR unexpected EOF.\n");
    exit(1);
  default:
    putc(p->c, p->outf);
    goto str;
  }
  errmsg("ERROR unreachable.\n");
  exit(3);

 esc:
  switch(p->c = getc(p->inf))	{
  case 'u':
    goto unc;
  case '/': case '"':
    putc(p->c, p->outf);
    goto str;
  case EOF:
    errmsg("ERROR unexpected EOF.\n");
    exit(1);
  default:
    putc('\\', p->outf); putc(p->c, p->outf);
    goto str;
  }
  errmsg("ERROR unreachable.\n");
  exit(3);

 unc:
  // https://ja.wikipedia.org/wiki/UTF-8
  for(i = 0; i < 4; i++)	{
    uenc[i] = getc(p->inf);
  }
  sscanf(uenc, "%4x", &uchi);
  switch(uchi)	{
    /* TODO is this enough for escaping? */
  case '"':	putc('\\', p->outf); putc('"',  p->outf);	break;
  case '\\':	putc('\\', p->outf); putc('\\', p->outf);	break;
  case '\n':	putc('\\', p->outf); putc('n',  p->outf);	break;
  case '\r':	putc('\\', p->outf); putc('r',  p->outf);	break;
  case '\t':	putc('\\', p->outf); putc('t',  p->outf);	break;
  default:
    /* decode */
    if(uchi < 0x0080)	{
      putc(uchi, p->outf);
    } else if(uchi < 0x0800)	{
      putc(((uchi >>  6) & 0b00011111) | 0b11000000, p->outf);
      putc(((uchi >>  0) & 0b00111111) | 0b10000000, p->outf);
    } else if(uchi < 0xFFFF)	{
      putc(((uchi >> 12) & 0b00001111) | 0b11100000, p->outf);
      putc(((uchi >>  6) & 0b00111111) | 0b10000000, p->outf);
      putc(((uchi >>  0) & 0b00111111) | 0b10000000, p->outf);
    }
  }
  goto str;

  errmsg("ERROR unreachable.\n");
  exit(3);
}

/* prs_array *//////////////////////////////////////////////////////////////////////////////////
int prs_array()	{
  const char *primitive_headder = "-0123456789tfn";
  
  size_t phsav = p->phead;

  int index = 0;
  int (*action)();

  p->c = getc(p->inf);

  prs_skip_ws();

  if(']' == p->c) {
    /* exit array*/
    p->phead = phsav;
    return (p->c = getc(p->inf));
  }
  goto val2;

 val:
  prs_skip_ws();
 val2:
  if(NULL != memchr(primitive_headder, p->c, strlen(primitive_headder)))	{
    /* NUMBER, true, false, null */
    action = prs_primitive;
  }
  else if('"' == p->c)	{
    /* STRING */
    action = prs_string;
  }
  else if('[' == p->c) {
    /* ARRAY */
    action = prs_array;
  }
  else if(EOF == p->c)	{
    errmsg("ERROR unexpected EOF.\n");
    exit(1);
  }
  else	{
    errmsg("ERROR invalid characer %c.\n", p->c);
    exit(1);
  }

  /* set path */
  if(0 > sprintf((p->path + p->phead), indfmt, index))	{
    errmsg("ERROR fatal.\n", p->c);
    exit(3);
  }
  p->c = action();
  index++;
  goto vdl;

 vdl:
  prs_skip_ws();
  if(',' == p->c) {
    /* next value */
    p->c = getc(p->inf); goto val;
  }
  else if(']' == p->c) {
    /* exit array*/
    p->phead = phsav;
    return (p->c = getc(p->inf));
  }
  else if(EOF == p->c)	{
    errmsg("ERROR unexpected EOF.\n");
    exit(1);
  }
  else	{
    errmsg("ERROR invalid characer %c.\n", p->c);
    exit(1);
  }

  errmsg("ERROR unreachable.\n");
  exit(3);
}

/* main *//////////////////////////////////////////////////////////////////////////////////

int main(int argc,char *argv[]) {
  if(!(initpparam(argc, argv)		&&
       init_parser(stdin, stdout)	))
    return 1;

  while(EOF != prs_json());

  return 0;
}
