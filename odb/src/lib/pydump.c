/* odb python dump interface */

#include <ctype.h>
#include <math.h>

#include "odbdump.h"

/* ENDIANESS */
extern int ec_is_little_endian();
extern double util_walltime_();


PRIVATE char *lld_dotify(ll_t n) 
     /* See ifsaux/support/drhook.c for a little variation of this beast [lld_commie] */
{ 
  const char dot= '.' ;
  char      *sd = NULL;
  char      *pd = NULL;
  char      s[100]    ;
  char      *p        ;
  int len, ndots      ;
  sprintf(s,"%lld",n) ;
  len = STRLEN(s)     ;
  ndots = (len-1)/3;
  if (ndots > 0) {
    int lensd = len + ndots + 1;
    ALLOC(sd, lensd);
    pd = sd + len + ndots;
    *pd-- = '\0';
    p = s + len - 1;
    len = 0;
    while (p-s >= 0) {
      *pd-- = *p--;
      ++len;
      if (p-s >= 0 && len%3 == 0) *pd-- = dot;
    }
  }
  else {
    sd = STRDUP(s);
  }
  return sd;
}

#define MEGA ((double)1048576.0)
#define PRINT_TIMING(txt, nrows, ncols) \
if (print_title && print_newline) { \
  double wthis  = util_walltime_(); \
  double wdelta = wthis - wlast; \
  Bool packed = true ; \
  /*if (txt) printf("# %s in %.3f secs\n", txt, wdelta); */\
if (nrows > 0 && ncols > 0) { \
    double rows_per_sec = nrows/wdelta; \
    ll_t nbytes = nrows * ((ll_t) ncols) * ((ll_t) sizeof(double)); \
    double MBytes_per_sec = (nbytes/wdelta)/MEGA; \
    char *dotified = lld_dotify(nbytes); \
    fprintf(fp, "# Total %lld row%s, %d col%s, %s %sbytes in %.3f secs : %.0f rows/s, %.0f MB/s\n", \
            (long long int)nrows, (nrows != 1) ? "s" : "",              \
            ncols, (ncols != 1) ? "s" : "", \
            dotified, packed ? "packed-" : "", \
            wdelta, rows_per_sec, MBytes_per_sec); \
    FREE(dotified); \
  } \
  if (txt || (nrows > 0 && ncols > 0)) wlast = wthis; \
}


int pyodb (char *database , char *sql_query  )
{
  int i_am_little = ec_is_little_endian();
  //char *database  = NULL;
  //char *sql_query = NULL;
  char *poolmask  = NULL;
  char *varvalue  = NULL;
  char *outfile   = NULL;
  char *queryfile = NULL;
  char *delim     = NULL;
  Bool print_newline = true;
  Bool print_mdi  = true; /* by default prints "NULL", not value of the NULL */
  Bool print_title= true;
  Bool debug_on   = true; 
  Bool raw        = true;
  const char dummydb[] = "$ODB_SYSDBPATH/DUMMY";
  int maxlines = -1;
  char *dbl_fmt   = NULL;
  void *h         = NULL;
  int maxcols     = 0;
  int rc          = 0;
  int errflg      = 0;
  int c ;
  double wlast;
  FILE *fp        = stdout;
  extern int optind;
 

  printf ("ODB NAME: %s\n" , database ) ; 
  if (maxlines == 0) return rc;

  dbl_fmt = STRDUP("%.10g");
  if (!delim) delim = STRDUP(" ");

  if (outfile) fp = fopen(outfile, "w");

  printf (" query ==  %s \n",   sql_query ) ;

  wlast = util_walltime_();
  printf( "%s\n" , database  ) ;
  h     = odbdump_open(database, sql_query, queryfile, poolmask, varvalue, &maxcols);
  if ( h ) {
  printf ("I got the database : %s \n" , database ) ;
   }

  if (h && maxcols > 0) {
	 
    int new_dataset = 0   ;
    colinfo_t *ci   = NULL;
    int nci = 0           ; // NUMBER OF COLUMNS IN QUERY 
    double *d = NULL;       // DATA VALUES CONTAINER  
    int nd          ; 
    int query_num =0;
    ll_t nrows =   0;
    ll_t nrtot =   0;
    int  ncols =   0;
    Bool packed = false;
    int (*nextrow)(void *, void *, int, int *) = 
      packed ? odbdump_nextrow_packed : odbdump_nextrow;
    int dlen = packed ? maxcols * sizeof(*d) : maxcols;

    ALLOCX(d, maxcols);

    while ( (nd = nextrow(h, d, dlen, &new_dataset)) > 0) {
      int i;
      if (new_dataset) {
	/* New query ? */
	//CLEAN STRUCTURES 
	ci = odbdump_destroy_colinfo(ci, nci);
	ci = odbdump_create_colinfo(h, &nci);
       // TITLE IN ORGINAL VERSION (ONLY HEADER NOW )
       if (print_title) {
	 
	 //PRINT_TIMING(" ", nrows, ncols);
	 //printf(  "Nquery : %d", ++query_num );
         //fprintf(fp, "#[%d]",++query_num);
         //if (stat_only) fprintf(fp, " ncols=%d", nci); 
          char *separ = " ";
             for (i=0; i<nd; i++) {
              colinfo_t *pci = &ci[i];
	      printf( "%s %s",separ, pci->nickname ? pci->nickname : pci->name  ) ;  
              separ = delim;
	 }
	 fprintf(fp, "\n");
	 fflush(fp);
       }
       new_dataset = 0;
       nrows = 0;
       ncols = nci;
       //Nbytes = 0;
      }  /* if  NEW DATASET */

      if(raw){
	char *separ = " ";
	for (i=0; i<nd; i++) {
            colinfo_t *pci = &ci[i];
	    printf("%s",separ);
	    fflush(fp) ;
	  if (print_mdi && pci->dtnum != DATATYPE_STRING && ABS(d[i]) == mdi) {
	    printf("NULL");
	  }
	  else {
            printf ( "pci->dtnum %x\n" ,pci->dtnum  );	    
	    switch (pci->dtnum) {
	    case DATATYPE_STRING:
	      {
		int js  ;
		char cc[sizeof(double)+1];
		char *scc = cc ;
		union {
		  char s[sizeof(double)] ;
		  double d;
		} u;
		u.d = d[i];
		
	
		for (js=0; js<sizeof(double); js++) {
		  char c = u.s[js]; 
		  *scc++ = isprint(c) ? c : ' '; /* unprintables as blanks */
		} /* for (js=0; js<sizeof(double); js++) */
		*scc = '\0';
		printf("\"%s\"",cc);
	      }

	      break;
	    case DATATYPE_YYYYMMDD:
              fprintf(fp, "%8.8d", (int)d[i]);
	      break;
	    case DATATYPE_HHMMSS:
	      fprintf(fp, "%6.6d", (int)d[i]);
	      break;
	    case DATATYPE_INT4:
	      fprintf(fp, "%d", (int)d[i]);
	      break;
	    default:
	      fprintf(fp, dbl_fmt, d[i]);
	      break;
	 
	    } /* switch (pci->dtnum) */
	  }
	  separ = delim;
	} /* for (i=0; i<nd; i++) */

	fprintf(fp, print_newline ? "\n" : "\e[0K\r"); /* The "\e[0K" hassle clears up to the EOL */
	
	if (!print_newline) fflush(fp);
       } /* if (!raw_binary)*/

    next_please:
      ++nrows;
      if (maxlines > 0 && ++nrtot >= maxlines) break; /* while (...) */
    } /* while (...) */
    ci = odbdump_destroy_colinfo(ci, nci);
    rc = odbdump_close(h);
    FREEX(d);

    fflush(fp);
  } /* if (h && maxcols > 0) ... */
  else {
    rc = -1;
  }

  if (fp) {
    if (rc != 0) fprintf(fp, "# return code = %d : please look at the file 'odbdump.stderr'\n",rc);

    fclose(fp);
  }
  return rc;
}

