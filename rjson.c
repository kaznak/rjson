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

struct tokl	{
  char name[BUFSIZ - sizeof(int) - sizeof(struct tokl*)];
  int index;
  struct tokl *prev, *next;
};
const size_t TOKLNLEN = BUFSIZ - sizeof(int) - sizeof(struct tokl*);

struct tokl toklh;

int prs_print_path(FILE *inf, FILE *outf);
int prs_print_primitive(struct tokl *ctoklp, FILE *inf, FILE *outf);
int prs_print_string(struct tokl *ctoklp, FILE *inf, FILE *outf);
int prs_set_string(struct tokl *ctoklp, FILE *inf, FILE *outf);
int prs_array(struct tokl *ctoklp, FILE *inf, FILE *outf);
int prs_object(struct tokl *ctoklp, FILE *inf, FILE *outf);

int prs_json(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  struct tokl ntokl;
  int c;

  ctoklp->name[0] = '\0';
  ctoklp->index = 0;
  ctoklp->next = NULL;
  ntokl.prev = ctoklp;

  for(c = getc(inf); c != EOF; c = getc(inf))	{
    switch(c)	{
    case '\"':
      if(EOF == ungetc(c, inf))	{
	errmsg("error while unread.\n");
	exit(1);
      }
      prs_print_string(ctoklp, inf, outf);
      break;
      
    case '-': case '0': case '1' : case '2': case '3' : case '4':
    case '5': case '6': case '7' : case '8': case '9':
    case 't': case 'f': case 'n' :
      if(EOF == ungetc(c, inf))	{
	errmsg("error while unread.\n");
	exit(1);
      }
      prs_print_primitive(ctoklp, inf, outf);
      break;

    case '{':
      ctoklp->next = &ntokl;
      prs_object(&ntokl, inf, outf);
      ctoklp->name[0] = '\0';
      ctoklp->index = 0;
      break;
    case '[':
      ctoklp->next = &ntokl;
      prs_array(&ntokl, inf, outf);
      ctoklp->name[0] = '\0';
      ctoklp->index = 0;
      break;

    case '\t' : case '\r' : case '\n' : case ' ':
      /* skip */
      break;

    default:
      errmsg("error unexpected character %c.\n", c);
      exit(1);
    }
  }

  return 0;
}

int prs_print_path(FILE *inf, FILE *outf)	{
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
  int c;

  prs_print_path(inf, outf); putc(' ', outf);

  for(c = getc(inf); c != EOF; c = getc(inf))	{
    switch(c)	{
    case '\t' : case '\r' : case '\n' : case ' ' :
    case ','  : case ']'  : case '}' :
      if(EOF == ungetc(c, inf))	{
	errmsg("error while unread.\n");
	exit(1);
      }
      putc('\n', outf);
      return 0;

    default:
      putc(c, outf);
      break;
    }
  }
  return 0;
}

int prs_print_string(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  int c, i;

  prs_print_path(inf, outf);

  putc(' ', outf); putc(getc(inf), outf);
  for(c = getc(inf); c != EOF; c = getc(inf))	{
    switch(c)	{
    case '"':
      putc(c, outf); putc('\n', outf);
      return 0;

    case '\\':
      c = getc(inf);
      if('u' == c)	{
	// TODO decode this
	putc('\\', outf); putc('u', outf);
	for(i = 0; i < 4; i++)	{
	  c = getc(inf);
	  if((('0' <= c) && (c <= '9'))	||
	     (('A' <= c) && (c <= 'F'))	||
	     (('a' <= c) && (c <= 'f'))	)	{
	    putc(c, outf);
	      } else	{
	    errmsg("invalid unicode sequence.\n");
	    exit(1);
	  }
	}
	break;
      } else	{
	putc('\\', outf); putc(c, outf);
      }
      break;

    default:
      putc(c, outf);
      break;
    }
  }

  errmsg("string expected, but EOF.\n");
  exit(1);
}

int prs_set_string(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  char *np = ctoklp->prev->name;
  int c, i;

  getc(inf);
  for(c = getc(inf); c != EOF; c = getc(inf))	{
    switch(c)	{
    case '"':
      *(np++) = '\0';
      return 0;

    case '\\':
      *(np++) = getc(inf);
      if('u' == c)	{
	// TODO decode this
	*(np++) = '\\'; *(np++) = 'u';
	for(i = 0; i < 4; i++)	{
	  c = getc(inf);
	  if((('0' <= c) && (c <= '9'))	||
	     (('A' <= c) && (c <= 'F'))	||
	     (('a' <= c) && (c <= 'f'))	)	{
	    *(np++) = c;
	      } else	{
	    errmsg("invalid unicode sequence.\n");
	    exit(1);
	  }
	}
	break;
      } else	{
	*(np++) = '\\'; *(np++) = c;
      }
      break;

    default:
      *(np++) = c;
      break;
    }
  }

  errmsg("string expected, but EOF.\n");
  exit(1);
}

enum arrstat	{ AINI, AVAL, ADLM };
int prs_array(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  struct tokl ntokl;
  int c;
  enum arrstat as = AINI;

  ctoklp->name[0] = '\0';
  ctoklp->index = 0;
  ctoklp->next = NULL;
  ntokl.prev = ctoklp;

  for(c = getc(inf); c != EOF; c = getc(inf))	{
    switch(c)	{
    case '\"':
      switch(as)	{
      case AINI: case AVAL:
	if(EOF == ungetc(c, inf))	{
	  errmsg("error while unread.\n");
	  exit(1);
	}
	prs_print_string(ctoklp, inf, outf);
	ctoklp->prev->index++;
	as = ADLM;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;
      
    case '-': case '0': case '1' : case '2': case '3' : case '4':
    case '5': case '6': case '7' : case '8': case '9':
    case 't': case 'f': case 'n' :
      switch(as)	{
      case AINI: case AVAL:
	if(EOF == ungetc(c, inf))	{
	  errmsg("error while unread.\n");
	  exit(1);
	}
	prs_print_primitive(ctoklp, inf, outf);
	ctoklp->prev->index++;
	as = ADLM;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;

    case ',':
      switch(as)	{
      case ADLM:
	as = AVAL;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;

    case '{':
      switch(as)	{
      case AINI: case AVAL:
	ctoklp->next = &ntokl;
	prs_object(&ntokl, inf, outf);
	ctoklp->name[0] = '\0';
	ctoklp->index = 0;
	ctoklp->prev->index++;
	as = ADLM;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;

    case '[':
      switch(as)	{
      case AINI: case AVAL:
	ctoklp->next = &ntokl;
	prs_array(&ntokl, inf, outf);
	ctoklp->name[0] = '\0';
	ctoklp->index = 0;
	ctoklp->prev->index++;
	as = ADLM;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;

    case ']':
      switch(as)	{
      case AINI: case ADLM:
	ctoklp->prev->next = NULL;
	if(0 == ctoklp->prev->index)	{
	  prs_print_path(inf, outf);
	  fprintf(outf, " []\n");
	}
	return ctoklp->prev->index;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;

    case '\t' : case '\r' : case '\n' : case ' ':
      /* skip */
      break;

    default:
      errmsg("error unexpected character %c.\n", c);
      exit(1);
    }
  }

  return 0;
}

enum objstat	{ OINI, KEY, KVD, OVAL, VKD };
int prs_object(struct tokl *ctoklp, FILE *inf, FILE *outf)	{
  struct tokl ntokl;
  int c;
  enum objstat os = OINI;

  ctoklp->name[0] = '\0';
  ctoklp->index = 0;
  ctoklp->next = NULL;
  ntokl.prev = ctoklp;

  for(c = getc(inf); c != EOF; c = getc(inf))	{
    switch(c)	{
    case '\"':
      if(EOF == ungetc(c, inf))	{
	errmsg("error while unread.\n");
	exit(1);
      }
      switch(os)	{
      case OINI: case KEY:
	prs_set_string(ctoklp, inf, outf);
	os = KVD;
	break;
      case OVAL:
	prs_print_string(ctoklp, inf, outf);
	ctoklp->prev->index++;
	os = VKD;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;
	
    case '-': case '0': case '1' : case '2': case '3' : case '4':
    case '5': case '6': case '7' : case '8': case '9':
    case 't': case 'f': case 'n' :
      switch(os)	{
      case OVAL:
	if(EOF == ungetc(c, inf))	{
	  errmsg("error while unread.\n");
	  exit(1);
	}
	prs_print_primitive(ctoklp, inf, outf);
	ctoklp->prev->index++;
	os = VKD;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;

    case ':':
      switch(os)	{
      case KVD:
	os = OVAL;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;

    case ',':
      switch(os)	{
      case VKD:
	os = KEY;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;

    case '{':
      switch(os)	{
      case OVAL:
	ctoklp->next = &ntokl;
	prs_object(&ntokl, inf, outf);
	ctoklp->name[0] = '\0';
	ctoklp->index = 0;
	ctoklp->prev->index++;
	os = VKD;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;

    case '[':
      switch(os)	{
      case OVAL:
	ctoklp->next = &ntokl;
	prs_array(&ntokl, inf, outf);	
	ctoklp->name[0] = '\0';
	ctoklp->index = 0;
	ctoklp->prev->index++;
	os = VKD;
	break;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;

    case '}':
      switch(os)	{
      case OINI: case VKD:
	ctoklp->prev->next = NULL;
	if(0 == ctoklp->prev->index)	{
	  prs_print_path(inf, outf);
	  fprintf(outf, " {}\n");
	}
	return ctoklp->prev->index;
      default:
	errmsg("unexpected token\n");
	exit(1);
      }
      break;
	
    case '\t' : case '\r' : case '\n' : case ' ':
      /* skip */
      break;

    default:
      errmsg("error unexpected character %c.\n", c);
      exit(1);
    }
  }

  return 0;
}

int main(int argc,char *argv[]) {
  initpparam(argc, argv);
  
  prs_json(&toklh, stdin, stdout);

  return 0;
}
