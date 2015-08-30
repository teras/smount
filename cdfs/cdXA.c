/*
  
  File cdXA.c - routines for frames <>2048 and <> 2352 byte frames for cdfs

  
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

struct file_operations cdfs_cdXA_file_operations = {
  read:               generic_file_read,
  mmap:               generic_file_mmap
};

struct address_space_operations cdfs_cdXA_aops = {
  readpage:           kcdfsd_add_cdXA_request
};

/************************************************************/

int kcdfsd_add_cdXA_request(struct file * file, struct page *page){
  return kcdfsd_add_request(file->f_dentry, page, CDXA_REQUEST);
}

/***********************************************************/            

int cdfs_read_raw_frame(struct super_block * sb, int lba, unsigned char *buf) {     
  struct cdrom_msf *msf;
  msf = (struct cdrom_msf*) buf;
  msf->cdmsf_min0   = (lba + CD_MSF_OFFSET) / CD_FRAMES / CD_SECS;
  msf->cdmsf_sec0   = (lba + CD_MSF_OFFSET) / CD_FRAMES % CD_SECS;
  msf->cdmsf_frame0 = (lba + CD_MSF_OFFSET) % CD_FRAMES;
  return cdfs_ioctl(sb, CDROMREADMODE2, (unsigned long)msf);
}

/***********************************************************/            

void cdfs_get_XA_info(struct super_block * sb, int inode) {
  char frame[CD_FRAMESIZE_RAW0];
  cd * this_cd = cdfs_info(sb);
  track_info * this_track = &(this_cd->track[inode]);
  unsigned start_lba = this_track->start_lba;
  int status;

  if((status = cdfs_read_raw_frame(sb, start_lba, frame))) {
    printk("get_XA_info: ioctl failed: %d\n", status);
    return;
  }
  
  if (frame[0] == frame[4] &&
      frame[1] == frame[5] &&
      frame[2] == frame[6] &&
      frame[3] == frame[7] &&
      frame[2] != 0 &&
      frame[0] < 8 &&
      frame[1] < 8) {
    this_track->xa_data_size   = (frame[2] & 0x20) ? 2324 : 2048;
    this_track->xa_data_offset = 8;
  } else {
    this_track->xa_data_size   = 2048;
    this_track->xa_data_offset = 0;
  }

  // Get type & title
  if((status = cdfs_read_raw_frame(sb, 150, frame))) {
    printk("get_XA_info: ioctl failed: %d\n", status);
    return;
  }

  strncpy(this_cd->videocd_type, frame+this_track->xa_data_offset, 8);
  this_cd->videocd_type[8]=0;
  strncpy(this_cd->videocd_title, frame+this_track->xa_data_offset+10, 16);
  this_cd->videocd_title[16]=0;

}

/***********************************************************/

void cdfs_copy_from_cdXA(struct super_block * sb, int inode, unsigned int start,
	unsigned int stop, char * buf){
  int start_sector, start_byte, stop_sector, stop_byte, sector;
  int status;
  cd * this_cd = cdfs_info(sb);
  track_info * this_track = &(this_cd->track[inode]);
  unsigned start_lba = this_track->start_lba;
  unsigned int data_size = this_track->xa_data_size;
  unsigned int data_offset = this_track->xa_data_offset;
  
  PRINT("start_lba=%d\n", start_lba);
  
  start_sector = start / data_size;
  start_byte   = start % data_size;
  stop_sector  = stop  / data_size;
  stop_byte    = stop  % data_size;

  if (!stop_byte) {            /* empty frame */
    stop_sector -= 1;
    stop_byte    = CD_FRAMESIZE_RAW0;
  }
  
  PRINT("%d[%d-%d] -> 0x%x...0x%x  ... (%d,%d),(%d,%d)\n",
        inode, start, stop, (int)buf, (int)buf+stop-start,
        start_sector,start_byte,stop_sector,stop_byte);
  
  for (sector=start_sector; sector<=stop_sector; sector++) {
    
    int lba=sector+start_lba;
    
    PRINT("reading sector %d, lba=%d\n", sector, lba);
    
    if (this_cd->cache_sector == lba) {
      PRINT("using cache\n");
    } else {
      this_cd->cache_sector = lba;
      if((status = cdfs_read_raw_frame(sb, lba, this_cd->cache))) {
        printk("copy_from_cdXA(%d): ioctl failed: %d\n", lba, status);
        return;
      }
    }

    {
      char * copy_start;
      int copy_length;
      
      if (sector==start_sector) {
        copy_start  = this_cd->cache+data_offset+start_byte;
        if (sector!=stop_sector)
          copy_length = data_size-start_byte;
        else
          copy_length = stop_byte-start_byte;
      } else if (sector==stop_sector) {
        copy_start  = this_cd->cache+data_offset;
        copy_length = stop_byte;
      } else {
        copy_start  = this_cd->cache+data_offset;
        copy_length = data_size;
      }
      PRINT("memcpy(0x%x, %x, %d)\n", (int)buf, (int)copy_start, copy_length);      
      memcpy(buf, (char*)copy_start, copy_length);
      buf+=copy_length;
    }

  }
}


/***************************************************************************/
