/*

   File proc.c - /proc routines for cdfs


   Copyright (c) 1999, 2000, 2001 by Michiel Ronsse 


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  

*/


#include "cdfs.h"

cd * cdfs_proc_cd;

struct proc_dir_entry * cdfs_proc_entry;

int cdfs_read_proc(char *buf, char **start, off_t offset,
    int len, int *eof, void *data ){

  int t,i;
  struct _track_info * track;
  char proc_info[4096];
  char * proc_counter = proc_info;
  char MSFsize[10];


  if (!cdfs_proc_cd) {

    proc_counter+=sprintf(proc_counter, "[%s\t%s]\n\tNo CD mounted\n", 
	FSNAME, VERSION);

  } else {

    proc_counter+=sprintf(proc_counter,
	"[%s\t%s]\n\nCD (discid=%08X) contains %d track%s:\n\n", 
	FSNAME, VERSION, cdfs_proc_cd->discid, cdfs_proc_cd->tracks, 
	cdfs_proc_cd->tracks-1 ? "s" : "");

    /* add stuff here */

    for (t=0; t<cdfs_proc_cd->tracks; t++) {
      i=T2I(t);
      proc_counter+=sprintf(proc_counter,"\n");
      track = &cdfs_proc_cd->track[i];
      if (track->type == DATA) {
	if (track->iso_size) {  /* DATA & ISO */
	  proc_counter+=sprintf(proc_counter,"Track %2d: data track (%s), [%d-%d/%d], length=%d MB\n",
	      t+1, track->name, track->start_lba, track->iso_size/2048,
	      track->stop_lba, track->track_size/1024/1024);
	  proc_counter+=sprintf(proc_counter,
	      "\ttype: %c info: %.5s version: %c\n"
	      "\tdate: %.2s/%.2s/%.4s time: %.2s:%.2s:%.2s\n"
	      "\tsystem: %.32s\n\tvolume: %.32s\n",
	      track->iso_info->type[0]+48, 
	      track->iso_info->id, 
	      track->iso_info->version[0]+48, 
	      track->iso_info->creation_date+6,
	      track->iso_info->creation_date+4,
	      track->iso_info->creation_date,
	      track->iso_info->creation_date+8,
	      track->iso_info->creation_date+10,
	      track->iso_info->creation_date+12,
	      track->iso_info->system_id, 
	      track->iso_info->volume_id
	      );
	  proc_counter+=sprintf(proc_counter, "\tpublisher: %.128s\n", track->iso_info->publisher_id);
	  proc_counter-=2; while (*--proc_counter==' ') ; proc_counter+=2; *proc_counter++='\n';
	  proc_counter+=sprintf(proc_counter, "\tpreparer: %.128s\n", track->iso_info->preparer_id);
	  proc_counter-=2; while (*--proc_counter==' ') ; proc_counter+=2; *proc_counter++='\n';
	  proc_counter+=sprintf(proc_counter, "\tapplication: %.128s\n", track->iso_info->application_id);
	  proc_counter-=2; while (*--proc_counter==' ') ; proc_counter+=2; *proc_counter++='\n';
	  proc_counter+=sprintf(proc_counter, "\tlength: %d MB / %d MB / %d MB / %d MB\n", 
	      (track->iso_size-track->start_lba*CD_FRAMESIZE)/1024/1024, 
	      track->track_size/1024/1024, 
	      track->iso_size/1024/1024,
	      track->size/1024/1024);
	} else {  /* DATA, geen ISO */
	  proc_counter+=sprintf(proc_counter,"Track %2d: data track (%s), [%d-%d], length=%d kB\n",
	      t+1, track->name, track->start_lba, track->stop_lba, track->track_size/1024);
	  proc_counter+=sprintf(proc_counter, "\ttype:  %s\n", cdfs_proc_cd->videocd_type);
	  proc_counter+=sprintf(proc_counter, "\ttitle: %s\n", cdfs_proc_cd->videocd_title);
	  proc_counter+=sprintf(proc_counter, "\tframesize: %d B\n", track->xa_data_size);
	}
      } else if (track->type==BOOT) { 
	proc_counter+=sprintf(proc_counter,"Bootimage (%s), [%d-%d], length=%d kB\n\tID string:%s\n",
	    track->name, track->start_lba, track->stop_lba, track->size/1024, track->bootID); 
      } else if (track->type==HFS) { 
	proc_counter+=sprintf(proc_counter,"Apple HFS (%s), [%d-%d], length=%d MB\n\tID string:%s\n",
	    track->name, track->start_lba, track->stop_lba, track->size/1024/1024, track->bootID);
      } else if (track->type==AUDIO) { 
	cdfs_constructMSFsize(MSFsize, track->size);
	proc_counter+=sprintf(proc_counter,"Track %2d: audio track (%s), [%8d -%8d], length=%s\n",
	    t+1, track->name, track->start_lba, track->stop_lba, MSFsize);
      }      
    }
  }

  strncpy(buf, proc_info, proc_counter-proc_info);
  return proc_counter-proc_info; 
}
