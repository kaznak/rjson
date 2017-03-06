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
  SSTR, SESC, SUNC, SUNL,		// STRING
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
  struct tokl *next;
};
const size_t TOKLNLEN = BUFSIZ - sizeof(int) - sizeof(struct tokl*);

struct tokl toklh;

int prs_print_path(FILE *outf);

int prs_print_primitive(FILE *inf, FILE *outf);
int prs_print_string(FILE *inf, FILE *outf);
int prs_set_string(struct tokl *ctoklp, FILE *inf, FILE *outf);

int prs_array(struct tokl *ctoklp, FILE *inf, FILE *outf);
int prs_object(struct tokl *ctoklp, FILE *inf, FILE *outf);

int prs_json(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  struct tokl ntokl;
  int r = 0, c = getc(inf);

  strcpy(ctoklp->name, pathroot);
  ctoklp->index = 0;
  ctoklp->next = NULL;

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
    r = prs_print_primitive(inf, outf); putc('\n', outf);
  } else if('"' == c)	{
    /* STRING */
    p->c = c;
    prs_print_path(outf); putc(' ', outf);
    r = prs_print_string(inf, outf); putc('\n', outf);
  } else if('[' == c) {
    /* ARRAY */
    p->c = c;
    ctoklp->next = &ntokl;
    r = prs_array(&ntokl, inf, outf);
    ctoklp->next = NULL;
    
  } else if('{' == c) {
    /* OBJECT */
    p->c = c;
    ctoklp->next = &ntokl;
    r = prs_object(&ntokl, inf, outf);
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
  struct tokl *xtoklp = &toklh;

  printf("%s", xtoklp->name);
  for(xtoklp = xtoklp->next; xtoklp != NULL; xtoklp = xtoklp->next)	{
    if('\0' != *(xtoklp->name))	{
      printf(pathfmt, xtoklp->name);
    } else	{
      printf(indfmt, xtoklp->index);
    }
  }
  return 0;
}

int prs_print_primitive(FILE *inf, FILE *outf)	{
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

inline unsigned int unicode2utf8(unsigned int uc) {
	return 
	(0xe0 | ((uc >> 12) & 0x0f)) << 16 |
	(0x80 | ((uc >>  6) & 0x3f)) << 8  |
	(0x80 | (uc & 0x3f));
}
int prs_print_string(FILE *inf, FILE *outf)	{
  int i, c;
  p->ps = SSTR;
  char uenc[5];
  unsigned int uchi = 0, uclo = 0;

  uenc[4] = '\0';

  // putc(p->c, outf);

  switch(p->ps)	{
  case SSTR:
  sstr:
    p->ps = SSTR;
    switch(c = getc(inf))	{
    case '"':
      // putc(c, outf);
      return (p->c = c);
    case EOF:
      errmsg("ERROR unexpected EOF.\n");
      exit(1);
    case '\\':
      goto sesc;
    default:
      putc(c, outf);
      goto sstr;
    }
    break;
    
  case SESC:
  sesc:
    p->ps = SESC;
    switch(c = getc(inf))	{
    case EOF:
      errmsg("ERROR unexpected EOF.\n");
      exit(1);
    case 'u':
      goto sunc;
    case '/': case '"':
      putc(c, outf);
      goto sstr;
    default:
      putc('\\', outf); putc(c, outf);
      goto sstr;
    }
    break;

    // https://ja.wikipedia.org/wiki/UTF-8
  case SUNC:
  sunc:
    p->ps = SUNC;
    for(i = 0; i < 4; i++)	{
      uenc[i] = getc(inf);
    }
    sscanf(uenc, "%4x", &uchi);
    switch(uchi)	{
      /* TODO is this enough for escaping? */
    case '"':	putc('\\', outf); putc('"', outf);	break;
    case '\\':	putc('\\', outf); putc('\\', outf);	break;
    case '\n':	putc('\\', outf); putc('n', outf);	break;
    case '\r':	putc('\\', outf); putc('r', outf);	break;
    case '\t':	putc('\\', outf); putc('t', outf);	break;
    default:
      if(uchi < 0x0080)	{
	putc(uchi, outf);
      } else if(uchi < 0x0800)	{
	putc(((uchi >>  6) & 0b00011111) | 0b11000000, outf);
	putc(((uchi >>  0) & 0b00111111) | 0b10000000, outf);
      } else if(uchi < 0xFFFF)	{
	putc(((uchi >> 12) & 0b00001111) | 0b11100000, outf);
	putc(((uchi >>  6) & 0b00111111) | 0b10000000, outf);
	putc(((uchi >>  0) & 0b00111111) | 0b10000000, outf);
      }
    }
    goto sstr;
    break;

  case SUNL:
  sunl:
    /* TODO handle UTF-16 surrogate pair */
    p->ps = SUNL;
    if('\\' != getc(inf)	||
       'u'  != getc(inf)	)	{
      errmsg("ERROR unexpected sequence.\n");
      exit(1);
    }
    for(i = 0; i < 4; i++)	{
      uenc[i] = getc(inf);
    }
    if (uchi < 0xdc00) {
      sscanf(uenc, "%4x", &uclo);
      if (uclo < 0xdc00 || uclo >= 0xdfff) {
	errmsg("ERROR invalid UCS-2 String.\n");
	exit(1);
      }
      uchi = ((uchi - 0xd800) << 16) + (uclo - 0xdc00) + 0x10000;
      uchi = unicode2utf8(uchi);
      putc((uchi >> 16) & 0xff, outf);
      putc((uchi >> 8) & 0xff, outf);
      putc(uchi & 0xff, outf);
    } else {
      errmsg("ERROR invalid UCS-2 String\n");
      exit(1);
    }
    goto sstr;
    break;

  default:
    break;
  }

  errmsg("ERROR unreachable.\n");
  exit(3);
}

int prs_print_string_raw(FILE *inf, FILE *outf)	{
  int i, c;
  p->ps = SSTR;

  // putc(p->c, outf);

  switch(p->ps)	{
  case SSTR:
  sstr:
    p->ps = SSTR;
    switch(c = getc(inf))	{
    case '"':
      // putc(c, outf);
      return (p->c = c);
    case EOF:
      errmsg("ERROR unexpected EOF.\n");
      exit(1);
    case '\\':
      goto sesc;
    default:
      putc(c, outf);
      goto sstr;
    }
    break;
    
  case SESC:
  sesc:
    p->ps = SESC;
    switch(c = getc(inf))	{
    case EOF:
      errmsg("ERROR unexpected EOF.\n");
      exit(1);
    case 'u':
      goto sunc;
      //    case '/':
      //      putc(c, outf);
      //      goto sstr;
    default:
      putc('\\', outf); putc(c, outf);
      goto sstr;
    }
    break;

  case SUNC:
  sunc:
    p->ps = SUNC;
    /* TODO decode */
    putc('\\', outf); putc('u', outf);
    for(i = 0; i < 4; i++)	putc(getc(inf), outf);
    goto sstr;
    break;

  default:
    ;
  }

  errmsg("ERROR unreachable.\n");
  exit(3);
}

int prs_set_string(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  int i, c;
  char *np = ctoklp->name;
  
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
    for(i = 0; i < 4; i++)	*(np++) = getc(inf);
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

  *(ctoklp->name) = '\0';
  ctoklp->index = 0;
  ctoklp->next = NULL;

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
      if(0 == ctoklp->index)	{
	prs_print_path(outf);
	// fprintf(outf, " []\n");
	fprintf(outf, " \n");
      }
      return ctoklp->index;
    } else if(NULL != memchr("-0123456789tfn", c, strlen("-0123456789tfn")))	{
      /* NUMBER, true, false, null */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_primitive(inf, outf); putc('\n', outf);

      ctoklp->index++;
    } else if('"' == c)	{
      /* STRING */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_string(inf, outf); putc('\n', outf);

      ctoklp->index++;
      c = getc(inf);
    } else if('[' == c) {
      /* ARRAY */
      p->c = c;
      ctoklp->next = &ntokl;
      r = prs_array(&ntokl, inf, outf);
      ctoklp->next = NULL;

      ctoklp->index++;
      c = getc(inf);
    } else if('{' == c) {
      /* OBJECT */
      p->c = c;
      ctoklp->next = &ntokl;
      r = prs_object(&ntokl, inf, outf);
      ctoklp->next = NULL;
    

      ctoklp->index++;
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
      c = prs_print_primitive(inf, outf); putc('\n', outf);
      ctoklp->index++;
    } else if('"' == c)	{
      /* STRING */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_string(inf, outf); putc('\n', outf);

      ctoklp->index++;
      c = getc(inf);
    } else if('[' == c) {
      /* ARRAY */
      p->c = c;
      ctoklp->next = &ntokl;
      c = prs_array(&ntokl, inf, outf);
      ctoklp->next = NULL;

      ctoklp->index++;
      c = getc(inf);
    } else if('{' == c) {
      /* OBJECT */
      p->c = c;
      ctoklp->next = &ntokl;
      c = prs_object(&ntokl, inf, outf);
      ctoklp->next = NULL;
    
      ctoklp->index++;    
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
      return ctoklp->index;
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
      if(0 == ctoklp->index)	{
	ctoklp->next = NULL;
	prs_print_path(outf);
	// fprintf(outf, " {}\n");
	fprintf(outf, ". \n");
      }
      return ctoklp->index;
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
    break;

  case OVAL:
  oval:
    p->ps = OVAL;
    if(NULL != memchr(" \n\r\t", c, strlen(" \n\r\t")))	{
      c = getc(inf); goto oval;
    } else if(NULL != memchr("-0123456789tfn", c, strlen("-0123456789tfn")))	{
      /* NUMBER, true, false, null */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_primitive(inf, outf); putc('\n', outf);
      ctoklp->index++;
    } else if('"' == c)	{
      /* STRING */
      p->c = c;
      prs_print_path(outf); putc(' ', outf);
      c = prs_print_string(inf, outf); putc('\n', outf);
      ctoklp->index++;

      c = getc(inf);
    } else if('[' == c) {
      /* ARRAY */
      p->c = c;
      ctoklp->next = &ntokl;
      r = prs_array(&ntokl, inf, outf);
      ctoklp->next = NULL;

      ctoklp->index++;
      c = getc(inf);
    } else if('{' == c) {
      /* OBJECT */
      p->c = c;
      ctoklp->next = &ntokl;
      r = prs_object(&ntokl, inf, outf);
      ctoklp->next = NULL;
    
      ctoklp->index++;    
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
      return ctoklp->index;
    } else if(EOF == c)	{
      errmsg("ERROR unexpected EOF.\n", c);
      exit(1);
    } else	{
      errmsg("ERROR invalid characer %c.\n", c);
      exit(1);
    }
    break;

  default:
    break;
  }

  errmsg("ERROR unreachable.\n");
  exit(3);
}

int main(int argc,char *argv[]) {
  initpparam(argc, argv);

  while(EOF != prs_json(&toklh, stdin, stdout));

  return 0;
}
