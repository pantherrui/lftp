/*
 * lftp and utils
 *
 * Copyright (c) 1996-1997 by Alexander V. Lukyanov (lav@yars.free.net)
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
#include <sys/types.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#ifdef NEED_TRIO
#include "trio.h"
#define vsnprintf trio_vsnprintf
#endif
#include <stdarg.h>
#include "xstring.h"
#include "xmalloc.h"

#include "ResMgr.h"
#include "StatusLine.h"

int  StatusLine::GetWidth()
{
#ifdef TIOCGWINSZ
   struct winsize sz;
   sz.ws_col=sz.ws_row=0;
   ioctl(fd,TIOCGWINSZ,&sz);
   if(sz.ws_col==0)
      sz.ws_col=80;
   if(sz.ws_row==0)
      sz.ws_row=24;
   return(LastWidth=sz.ws_col);
#else /* !TIOCGWINSZ */
   return 80;
#endif
}

StatusLine::StatusLine(int new_fd)
{
   fd=new_fd;
   update_delayed=false;
   update_time=0;
   strcpy(shown,"");
   strcpy(def_title,"");
   not_term=!isatty(fd);
   LastWidth=GetWidth();
}

StatusLine::~StatusLine()
{
}

void StatusLine::Clear()
{
   char newstr[sizeof(shown)];

   newstr[0]=0;
   update(newstr);
   update_delayed=false;
   update_time=0;

   WriteTitle(def_title, fd);
}

void StatusLine::DefaultTitle(const char *s)
{
   strncpy(def_title, s, sizeof(def_title));
   def_title[sizeof(def_title)-1] = 0;
}

void StatusLine::Show(const char *f,...)
{
   if(f==0 || f[0]==0)
   {
      Clear();
      return;
   }

   char newstr[sizeof(shown)];

   va_list v;
   va_start(v,f);
   vsnprintf(newstr,sizeof(newstr),f,v);
   va_end(v);

   if(now>update_time)
   {
      update(newstr);
      update_delayed=false;
      update_time=now;
   }
   else if(strcmp(to_be_shown,newstr))
   {
      strcpy(to_be_shown,newstr);
      update_delayed=true;
      TimeoutS(1);
   }
}

void StatusLine::WriteTitle(const char *s, int fd) const
{
   if(!(bool)ResMgr::Query("cmd:set-term-status", getenv("TERM")))
      return;

   const char *scan=ResMgr::Query("cmd:term-status", getenv("TERM"));
   char ch;
   char str[3];
   const char *to_add;

   for(;;)
   {
      ch=*scan++;
      if(ch==0)
	 break;

      if(ch=='\\' && *scan && *scan!='\\')
      {
	 ch=*scan++;
	 switch(ch)
	 {
	 case'0':case'1':case'2':case'3':case'4':case'5':case'6':case'7':
	 {
	    unsigned len;
	    unsigned code;
	    scan--;
	    sscanf(scan,"%3o%n",&code,&len);
	    ch=code;
	    scan+=len;
	    str[0]=ch;
	    str[1]=0;
	    to_add=str;
	    break;
	 }
	 case 'a':
	    to_add="\007";
	    break;
	 case 'e':
	    to_add="\033";
	    break;
	 case 'n':
 	    to_add="\n";
 	    break;
	 case 's':
 	    to_add="lftp";
 	    break;
	 case 'T':
 	    to_add=s;
 	    break;
 	 case 'v':
	    to_add=VERSION;
	    break;
	 default:
	    str[0]='\\';
	    str[1]=ch;
	    str[2]=0;
	    to_add=str;
	    break;
	 }
      }
      else
      {
	 if(ch=='\\' && *scan=='\\')
	    scan++;
	 str[0]=ch;
	 str[1]=0;
	 to_add=str;
      }

      if(to_add==0)
	 continue;

      write(fd, to_add, strlen(to_add));
   }
}

void StatusLine::update(char *newstr)
{
   if(not_term)
      return;

   if(tcgetpgrp(fd)!=getpgrp())
      return;

   /* Don't write blank titles into the title; let Clear() do that. */
   if(newstr[0]) WriteTitle(newstr, fd);

   char *end=newstr+strlen(newstr);

   int w=GetWidth();
   if(end-newstr>=w)
      end=newstr+w-1;

   while(end>newstr && end[-1]==' ')
      end--;

   *end=0;

   if(!strcmp(shown,newstr))
      return;

   int dif=strlen(shown)-strlen(newstr)+2;

   strcpy(shown,newstr);

   while(dif-->0 && end-newstr<w)
      *end++=' ';

   *end=0;

   if(end==newstr)
      return;

   *end++='\r';
   *end=0;

   write(fd,"\r",1);
   write(fd,newstr,strlen(newstr));
}


void StatusLine::WriteLine(const char *f,...)
{
   char *newstr=(char*)alloca(sizeof(shown)+strlen(f)+64);

   va_list v;
   va_start(v,f);
   vsprintf(newstr,f,v);
   va_end(v);

   if(not_term || shown[0]==0)
   {
      strcat(newstr,"\n");
      write(fd,newstr,strlen(newstr));
      update_delayed=false;
      return;
   }

   char *end=newstr+strlen(newstr);
   while(end>newstr && end[-1]==' ')
      end--;
   *end=0;
   if(!strcmp(shown,newstr))
   {
      write(fd,"\n",1);
      return;
   }

   int dif=strlen(shown)-strlen(newstr)+2;
   int w=GetWidth();

   while(dif-->0 && end-newstr<w-1)
      *end++=' ';

   *end++='\n';
   *end=0;

   write(fd,"\r",1);
   write(fd,newstr,strlen(newstr));

   strcpy(shown,"");
   update_delayed=false;
}

int StatusLine::Do()
{
   if(!update_delayed)
      return STALL;
   if(now>update_time)
   {
      update(to_be_shown);
      update_delayed=false;
      update_time=now;
      return STALL;
   }
   TimeoutS(1);
   return STALL;
}
