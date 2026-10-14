/* xarc - minimal libarchive extractor. MIT License, Hop-tunes contributors. */
/* minimal libarchive extractor: xarc <archive> <outdir> ; prototypes declared manually (no -dev headers) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
struct archive; struct archive_entry;
struct archive *archive_read_new(void);
int archive_read_support_format_all(struct archive*);
int archive_read_support_filter_all(struct archive*);
int archive_read_open_filename(struct archive*,const char*,size_t);
int archive_read_next_header(struct archive*,struct archive_entry**);
const char *archive_entry_pathname(struct archive_entry*);
long archive_read_data(struct archive*,void*,size_t);
const char *archive_error_string(struct archive*);
static void mkdirs(char *p){for(char*s=p+1;*s;s++){if(*s=='/'){*s=0;mkdir(p,0755);*s='/';}}}
int main(int c,char**v){
  struct archive*a=archive_read_new();archive_read_support_format_all(a);archive_read_support_filter_all(a);
  if(archive_read_open_filename(a,v[1],1<<20)){fprintf(stderr,"open: %s\n",archive_error_string(a));return 1;}
  struct archive_entry*e;static char buf[1<<20];int n=0,r;
  while((r=archive_read_next_header(a,&e))==0||r==-20){
    char path[4096];snprintf(path,sizeof path,"%s/%s",v[2],archive_entry_pathname(e));
    for(char*s=path;*s;s++){if(*s=='\\')*s='/';}
    /* normalise X86 -> x86 */ char*x=strstr(path,"/X86/");if(x)memcpy(x,"/x86/",5);
    if(strstr(path,"/../")){fprintf(stderr,"skip unsafe %s\n",path);continue;}
    mkdirs(path);FILE*o=fopen(path,"wb");long k;
    while((k=archive_read_data(a,buf,sizeof buf))>0)fwrite(buf,1,k,o);
    if(k<0)fprintf(stderr,"data err %s: %s\n",path,archive_error_string(a));
    fclose(o);n++;
  }
  if(r!=1)fprintf(stderr,"end r=%d %s\n",r,archive_error_string(a));
  printf("%d entries\n",n);return 0;}
