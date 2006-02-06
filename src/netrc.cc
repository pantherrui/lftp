/*
 * lftp and utils
 *
 * Copyright (c) 1996-2005 by Alexander V. Lukyanov (lav@yars.free.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id$ */

#include <config.h>

#include "trio.h"
#include <stdlib.h>
#include <errno.h>
#include "xstring.h"
#include "netrc.h"
#include "xmalloc.h"
#include "log.h"

NetRC::Entry::Entry(const char *h,const char *u,const char *p,const char *a)
{
   host=xstrdup(h);
   user=xstrdup(u);
   pass=xstrdup(p);
   acct=xstrdup(a);
}

NetRC::Entry::~Entry()
{
   xfree(host);
   xfree(user);
   xfree(pass);
   xfree(acct);
}

static bool comment(const char *s, FILE *f)
{
   if(s[0]!='#')
      return false;
   // skip entire line
   for(;;)
   {
      int ch=getc(f);
      if(ch==EOF || ch=='\n')
	 break;
   }
   return true;
}

NetRC::Entry *NetRC::LookupHost(const char *h,const char *u)
{
   char str[256];
   char chost[256]="";
   char cuser[256]="";
   char cpass[256]="";
   char cacct[256]="";
   const char *home=getenv("HOME");
   if(!home)
      return 0;
   char *netrc=(char*)alloca(strlen(home)+8);
   sprintf(netrc,"%s/.netrc",home);
   FILE *f=fopen(netrc,"r");
   bool host_found=false;
   bool user_found=false;

   if(f==NULL)
   {
      Log::global->Format(10,"notice: cannot open %s: %s\n",netrc,strerror(errno));
      return NULL;
   }

   while(fscanf(f,"%255s",str)==1)
   {
      if(comment(str,f))
	 continue;
      if(!strcmp(str,"macdef"))
      {
	 // currently macdef is ignored
	 if(fgets(str,255,f)==0)
	    break;
	 for(;;)
	 {
	    if(fgets(str,255,f)==0)
	       break;
	    if(str[strspn(str," \t\n")]==0) // macdef ends with empty line
	       break;
	 }
	 continue;
      }
      if(!strcmp(str,"default"))
      {
	 strcpy(chost,""); // ignore the default
	 continue;
      }
      if(!strcmp(str,"machine"))
      {
	 if(host_found && user_found)
	    break;
	 if(fscanf(f,"%255s",str)!=1)
	    break;
	 strcpy(chost,str);
	 // reset data for new machine.
	 cuser[0]=0;
	 cpass[0]=0;
	 cacct[0]=0;
	 host_found=!strcasecmp(chost,h);
	 user_found=false;
	 continue;
      }
      if(!strcmp(str,"login"))
      {
	 if(fscanf(f,"%255s",str)!=1)
	    break;
	 if(strcasecmp(chost,h))
	    continue;
	 strcpy(cuser,str);
	 cpass[0]=0;
	 cacct[0]=0;
	 user_found=(!u || !strcasecmp(cuser,u));
	 continue;
      }
      if(!strcmp(str,"password"))
      {
	 if(fscanf(f,"%255s",str)!=1)
	    break;
	 if(strcasecmp(chost,h) || (u && strcasecmp(cuser,u)) || cpass[0])
	    continue;
	 strcpy(cpass,str);
	 for(char *s=cpass; *s; s++)
	 {
	    if(*s=='\\' && s[1]>='0' && s[1]<'8')
	    {
	       int ch=0;
	       int n=0;
	       if(sscanf(s+1,"%3o%n",&ch,&n)!=1)
		  continue;
	       if(ch==0)
		  continue;
	       *s=ch;
	       memmove(s+1,s+1+n,strlen(s+1+n)+1);
	    }
	 }
	 continue;
      }
      if(!strcmp(str,"account"))
      {
	 if(fscanf(f,"%255s",str)!=1)
	    break;
	 if(strcasecmp(chost,h) || (u && strcasecmp(cuser,u)) || cacct[0])
	    continue;
	 strcpy(cacct,str);
	 continue;
      }
   }
   fclose(f);
   if(host_found && user_found)
   {
      Log::global->Format(10,"found netrc entry: host=%s, user=%s, pass=%s, acct=%s\n",h,cuser,cpass,cacct);
      return new Entry(h,cuser[0]?cuser:0,cpass[0]?cpass:0,cacct[0]?cacct:0);
   }
   return 0;
}
