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
  
  return 0;
}

int eprintmsg(int lineno, char *fmt, ...)	{
  va_list ap;

  fprintf(stderr, "%s %s %d %d ", pname, stime_s, pid, lineno);
  
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  return 0;
}
#define errmsg(...) eprintmsg(__LINE__, __VA_ARGS__)


/* parse *//////////////////////////////////////////////////////////////////////////////////

const char *pathroot = "$";
const char *pathfmt = ".%s";
// const char *indfmt = "[%04d]";
const char *indfmt = "[%d]";

enum	pstate	{
  TOP,					// TOP loop
  OINI, OKEY, OKVD, OVAL, OVKD,		// OBJECT
  AINI, AVAL, AVDL,			// ARRAY
  PRIM,					// NUMBER, true, false, null
  SSTR, SESC, SUNC,			// STRING
  ALL
};

struct parser	{
  enum pstate ps;
  char c;
};
struct parser prs, *p =&prs;

struct tokl	{
  char name[BUFSIZ - sizeof(int) - sizeof(struct tokl*)];
  int index;
  struct tokl *prev, *next;
};
const size_t TOKLNLEN = BUFSIZ - sizeof(int) - sizeof(struct tokl*);

struct tokl toklh;

int prs_print_path(FILE *outf);

int prs_print_primitive(struct tokl *ctoklp, FILE *inf, FILE *outf);
int prs_print_string(struct tokl *ctoklp, FILE *inf, FILE *outf);
int prs_set_string(struct tokl *ctoklp, FILE *inf, FILE *outf);
int prs_array(struct tokl *ctoklp, FILE *inf, FILE *outf);
int prs_object(struct tokl *ctoklp, FILE *inf, FILE *outf);

int prs_json(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  struct tokl ntokl;
  int r = 0, c = getc(inf);

  ctoklp->name[0] = '\0';
  ctoklp->index = 0;
  ctoklp->next = NULL;
  ntokl.prev = ctoklp;

  /* state: top loop */

  p->ps = TOP;

  /* skip white spaces */
 top:
  if(NULL != memchr(" \n\r\t", c, strlen(" \n\r\t")))	{
    c = getc(inf); goto top;
  } else if(NULL != memchr("-0123456789tfn", c, strlen("-0123456789tfn")))	{
    /* NUMBER, true, false, null */
    p->c = c;
    prs_print_path(outf); putc(' ', outf);
    r = prs_print_primitive(ctoklp, inf, outf); putc('\n', outf);
  } else if('"' == c)	{
    /* STRING */
    p->c = c;
    prs_print_path(outf); putc(' ', outf);
    r = prs_print_string(ctoklp, inf, outf); putc('\n', outf);
  } else if('[' == c) {
    /* ARRAY */
    p->c = c;
    ctoklp->next = &ntokl;
    r = prs_array(&ntokl, inf, outf);

    ctoklp->name[0] = '\0';
    ctoklp->index = 0;
    ctoklp->next = NULL;
    
  } else if('{' == c) {
    /* OBJECT */
    p->c = c;
    ctoklp->next = &ntokl;
    r = prs_object(&ntokl, inf, outf);
    
    ctoklp->name[0] = '\0';
    ctoklp->index = 0;
    ctoklp->next = NULL;
    
  } else if(EOF == c)	{
    return c;
  } else	{
    errmsg("ERROR invalid characer %c.\n", c);
    exit(1);
  }

  return (p->c = r);
}

int prs_print_path(FILE *outf)	{
  struct tokl *xtoklp;

  printf("%s", pathroot);
  for(xtoklp = &toklh; xtoklp->next != NULL; xtoklp = xtoklp->next)	{
    if('\0' != *(xtoklp->name))	{
      printf(pathfmt, xtoklp->name);
    } else	{
      printf(indfmt, xtoklp->index);
    }
  }
  return 0;
}

int prs_print_primitive(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  int c = getc(inf);
  p->ps = PRIM;
  putc(p->c, outf);
  for(;
      EOF == c || NULL == memchr(",:]} \n\r\t\"[{", c, strlen(",:]} \n\r\t\"[{"));
      c = getc(inf)	)	{
    putc(c, outf);
  }
  return (p->c = c);
}

int prs_print_string(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  int i, c;
  p->ps = SSTR;

  putc(p->c, outf);

  switch(p->ps)	{
  case SSTR:
  sstr:
    while('"' != (c = getc(inf)))	{
      switch(c)	{
      case EOF:
	errmsg("ERROR unexpected EOF.\n");
	exit(1);
      case '\\':
	goto sesc;
      default:
	putc(c, outf);
      }
    }
    break;

  case SESC:
  sesc:
    switch(c = getc(inf))	{
    case EOF:
      errmsg("ERROR unexpected EOF.\n");
      exit(1);
    case 'u':
      goto sunc;
    case '/':
      putc(c, outf);
      goto sstr;
    default:
      putc('\\', outf); putc(c, outf);
      goto sstr;
    }
    errmsg("ERROR unreachable.\n");
    exit(3);

  case SUNC:
  sunc:
    /* TODO decode */
    putc('\\', outf); putc('u', outf);
    for(i = 0; i < 4; putc(getc(inf), outf));
    goto sstr;
    break;

  default:
    errmsg("ERROR unreachable.\n");
    exit(3);
  }

  putc(c, outf);
  return (p->c = c);
}

int prs_set_string(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  int i, c;
  char *np = ctoklp->prev->name;
  
  p->ps = SSTR;

  switch(p->ps)	{
  case SSTR:
  sstr:
    while('"' != (c = getc(inf)))	{
      switch(c)	{
      case EOF:
	errmsg("ERROR unexpected EOF.\n");
	exit(1);
      case '\\':
	goto sesc;
      default:
	*(np++) = c;
      }
    }
    break;

  case SESC:
  sesc:
    switch(c = getc(inf))	{
    case EOF:
      errmsg("ERROR unexpected EOF.\n");
      exit(1);
    case 'u':
      goto sunc;
    case '/':
      *(np++) = c;
      goto sstr;
    default:
      *(np++) = '\\'; *(np++) = c;
      goto sstr;
    }
    errmsg("ERROR unreachable.\n");
    exit(3);

  case SUNC:
  sunc:
    /* TODO decode */
    *(np++) = '\\'; *(np++) = 'u';
    for(i = 0; i < 4;*(np++) = getc(inf));
    goto sstr;
    break;

  default:
    errmsg("ERROR unreachable.\n");
    exit(3);
  }

  *np = '\0';
  return (p->c = c);
}

//  AINI, AVAL, AVDL,			// ARRAY
int prs_array(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  struct tokl ntokl;
  int r = 0, c = getc(inf);

  ctoklp->name[0] = '\0';
  ctoklp->index = 0;
  ctoklp->next = NULL;
  ntokl.prev = ctoklp;

  /* state: array init */
  p->ps = AINI;

  /* skip white spaces */
  switch(p->ps)	{
  case AINI:
  aini:
    p->ps = AINI;
    if(NULL != memchr(" \n\r\t", c, strlen(" \n\r\t")))	{
      c = getc(inf); goto aini;
    } else if(']' == c) {
      /* exit array*/
      return ctoklp->prev->index;
    } else if(NULL != memchr("-0123456789tfn", c, strlen("-0123456789tfn")))	{
      /* NUMBER, true, false, null */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_primitive(ctoklp, inf, outf); putc('\n', outf);
      ctoklp->prev->index++;
    } else if('"' == c)	{
      /* STRING */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_string(ctoklp, inf, outf); putc('\n', outf);
      ctoklp->prev->index++;
      
      c = getc(inf);
    } else if('[' == c) {
      /* ARRAY */
      p->c = c;
      ctoklp->next = &ntokl;
      r = prs_array(&ntokl, inf, outf);

      ctoklp->name[0] = '\0';
      ctoklp->index = 0;

      ctoklp->prev->index++;
      ctoklp->next = NULL;
      
      c = getc(inf);
    } else if('{' == c) {
      /* OBJECT */
      errmsg("INFO not yet.\n");

      p->c = c;
      ctoklp->next = &ntokl;
      r = prs_object(&ntokl, inf, outf);
    
      ctoklp->name[0] = '\0';
      ctoklp->index = 0;

      ctoklp->prev->index++;
      ctoklp->next = NULL;
      
      c = getc(inf);
    } else if(EOF == c)	{
      errmsg("ERROR unexpected EOF.\n", c);
      exit(1);
    } else	{
      errmsg("ERROR invalid characer %c.\n", c);
      exit(1);
    }
    goto avdl;

  case AVAL:
  aval:
    p->ps = AVAL;
    if(NULL != memchr(" \n\r\t", c, strlen(" \n\r\t")))	{
      c = getc(inf); goto aini;
    } else if(NULL != memchr("-0123456789tfn", c, strlen("-0123456789tfn")))	{
      /* NUMBER, true, false, null */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_primitive(ctoklp, inf, outf); putc('\n', outf);
      ctoklp->prev->index++;
    } else if('"' == c)	{
      /* STRING */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_string(ctoklp, inf, outf); putc('\n', outf);
      ctoklp->prev->index++;

      c = getc(inf);
    } else if('[' == c) {
      /* ARRAY */
      p->c = c;
      ctoklp->next = &ntokl;
      c = prs_array(&ntokl, inf, outf);

      ctoklp->name[0] = '\0';
      ctoklp->index = 0;

      ctoklp->prev->index++;
      ctoklp->next = NULL;

      c = getc(inf);
    } else if('{' == c) {
      /* OBJECT */
      errmsg("INFO not yet.\n");

      p->c = c;
      ctoklp->next = &ntokl;
      c = prs_object(&ntokl, inf, outf);
    
      ctoklp->name[0] = '\0';
      ctoklp->index = 0;

      ctoklp->prev->index++;    
      ctoklp->next = NULL;

      c = getc(inf);
    } else if(EOF == c)	{
      errmsg("ERROR unexpected EOF.\n", c);
      exit(1);
    } else	{
      errmsg("ERROR invalid characer %c.\n", c);
      exit(1);
    }
    goto avdl;

  case AVDL:
  avdl:
    p->ps = AVDL;
    if(NULL != memchr(" \n\r\t", c, strlen(" \n\r\t")))	{
      c = getc(inf); goto avdl;
    } else if(',' == c)	{
      c = getc(inf);
      goto aval;
    } else if(']' == c) {
      /* exit array*/
      return ctoklp->prev->index;
    } else if(EOF == c)	{
      errmsg("ERROR unexpected EOF.\n", c);
      exit(1);
    } else	{
      errmsg("ERROR invalid characer %c.\n", c);
      exit(1);
    }

  default:
    errmsg("ERROR unreachable.\n");
    exit(3);
  }

  errmsg("ERROR unreachable.\n");
  exit(3);
}

//  OINI, OKEY, OKVD, OVAL, OVKD,		// OBJECT
int prs_object(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  struct tokl ntokl;
  int r = 0, c = getc(inf);

  ctoklp->name[0] = '\0';
  ctoklp->index = 0;
  ctoklp->next = NULL;
  ntokl.prev = ctoklp;

  /* state: object init */
  p->ps = OINI;

  /* skip white spaces */
  switch(p->ps)	{
  case OINI:
  oini:
    p->ps = OINI;
    if(NULL != memchr(" \n\r\t", c, strlen(" \n\r\t")))	{
      c = getc(inf); goto oini;
    } else if('}' == c) {
      /* exit object */
      return ctoklp->prev->index;
    } else if('"' == c)	{
      /* STRING */
      p->c = c;
      c = prs_set_string(ctoklp, inf, outf);

      c = getc(inf);
    } else if(EOF == c)	{
      errmsg("ERROR unexpected EOF.\n", c);
      exit(1);
    } else	{
      errmsg("ERROR invalid characer %c.\n", c);
      exit(1);
    }
    goto okvd;

  case OKEY:
  okey:
    p->ps = OKEY;
    if(NULL != memchr(" \n\r\t", c, strlen(" \n\r\t")))	{
      c = getc(inf); goto okey;
    } else if('"' == c)	{
      /* STRING */
      p->c = c;
      c = prs_set_string(ctoklp, inf, outf);

      c = getc(inf);
    } else if(EOF == c)	{
      errmsg("ERROR unexpected EOF.\n", c);
      exit(1);
    } else	{
      errmsg("ERROR invalid characer %c.\n", c);
      exit(1);
    }
    goto okvd;

  case OKVD:
  okvd:
    p->ps = OKVD;
    if(NULL != memchr(" \n\r\t", c, strlen(" \n\r\t")))	{
      c = getc(inf); goto okvd;
    } else if(':' == c)	{
      c = getc(inf);
      goto oval;
    } else if(EOF == c)	{
      errmsg("ERROR unexpected EOF.\n", c);
      exit(1);
    } else	{
      errmsg("ERROR invalid characer %c.\n", c);
      exit(1);
    }
    errmsg("ERROR unreachabel.\n");
    exit(3);

  case OVAL:
  oval:
    p->ps = OVAL;
    if(NULL != memchr(" \n\r\t", c, strlen(" \n\r\t")))	{
      c = getc(inf); goto oval;
    } else if(NULL != memchr("-0123456789tfn", c, strlen("-0123456789tfn")))	{
      /* NUMBER, true, false, null */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_primitive(ctoklp, inf, outf); putc('\n', outf);
      ctoklp->prev->index++;
    } else if('"' == c)	{
      /* STRING */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_string(ctoklp, inf, outf); putc('\n', outf);
      ctoklp->prev->index++;

      c = getc(inf);
    } else if('[' == c) {
      /* ARRAY */
      errmsg("INFO not yet.\n");

      p->c = c;
      ctoklp->next = &ntokl;
      r = prs_array(&ntokl, inf, outf);

      ctoklp->name[0] = '\0';
      ctoklp->index = 0;

      ctoklp->prev->index++;
      ctoklp->next = NULL;

      c = getc(inf);
    } else if('{' == c) {
      /* OBJECT */
      errmsg("INFO not yet.\n");

      p->c = c;
      ctoklp->next = &ntokl;
      r = prs_object(&ntokl, inf, outf);
    
      ctoklp->name[0] = '\0';
      ctoklp->index = 0;

      ctoklp->prev->index++;    
      ctoklp->next = NULL;

      c = getc(inf);
    } else if(EOF == c)	{
      errmsg("ERROR unexpected EOF.\n", c);
      exit(1);
    } else	{
      errmsg("ERROR invalid characer %c.\n", c);
      exit(1);
    }
    goto ovkd;

  case OVKD:
  ovkd:
    p->ps = OVAL;
    if(NULL != memchr(" \n\r\t", c, strlen(" \n\r\t")))	{
      c = getc(inf); goto ovkd;
    } else if(',' == c)	{
      c = getc(inf);
      goto okey;
    } else if('}' == c)	{
      /* exit object */
      return ctoklp->prev->index;
    } else if(EOF == c)	{
      errmsg("ERROR unexpected EOF.\n", c);
      exit(1);
    } else	{
      errmsg("ERROR invalid characer %c.\n", c);
      exit(1);
    }

  default:
    errmsg("ERROR unreachable.\n");
    exit(3);
  }

  errmsg("ERROR unreachable.\n");
  exit(3);
}  

int main(int argc,char *argv[]) {
  initpparam(argc, argv);

  while(EOF != prs_json(&toklh, stdin, stdout));

  return 0;
}
