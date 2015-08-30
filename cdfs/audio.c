/*

   File audio.c - routines for 2352 byte frames cdfs


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
#include <asm/byteorder.h>

/***************************************************************************/

void cdfs_make_header(char * temp, unsigned int size){
  PRINT("MAKING HEADER, size=%d @ 0x%x\n", size, temp);

  /* RIFF header */
  strcpy(             temp,     "RIFF");
  *(u32 *) (temp+ 4) = cpu_to_le32(size-8);    /* size of RIFF */

  /* WAVE file */
  strcpy(             temp+ 8,  "WAVE");

  /* WAVE format*/
  strcpy(             temp+12,  "fmt ");
  *(u32 *) (temp+16) = cpu_to_le32(16);        /* size of format descriptor */
  /* common fields */
  *(u16 *) (temp+20) = cpu_to_le16(1);         /* format category: WAVE_FORMAT_PCM */
  *(u16 *) (temp+22) = cpu_to_le16(2);         /* number of channels */
  *(u32 *) (temp+24) = cpu_to_le32(44100);     /* sample rate */
  *(u32 *) (temp+28) = cpu_to_le32(44100*4);   /* bytes/sec */
  *(u16 *) (temp+32) = cpu_to_le16(4);         /* data block size */
  /* format specific fields (PCM) */
  *(u16 *) (temp+34) = cpu_to_le16(16);        /* bits per sample */

  /* WAVE data */
  strcpy(             temp+36,  "data");
  *(u32 *) (temp+40) = cpu_to_le32(size-44);   /* size of data chunk */

}
/***************************************************************************/

void cdfs_copy_from_cd(struct super_block * sb, int inode, unsigned int start, 
    unsigned int stop, char * buf){
  int start_sector, start_byte, stop_sector, stop_byte, sector;
  int status;
  struct cdrom_read_audio cdda;
  unsigned int read_size=CD_FRAMESIZE_RAW;
  cd * this_cd = cdfs_info(sb);      
  unsigned start_lba=this_cd->track[inode].start_lba;

  /* cache */
  char * temp, * temp2;
  char * temp_start;
  int temp_length;

  PRINT("copy_from_cd(%x, %d, %d, %d, %x)\n", sb, inode, start, stop, buf);

  temp = this_cd->cache;

  start_sector = start/read_size;
  start_byte   = start%read_size;
  stop_sector  = stop/read_size;
  stop_byte    = stop%read_size;

  start_sector += start_lba;
  stop_sector  += start_lba;

  if (!stop_byte) {            /* empty frame */
    stop_sector -= 1;
    stop_byte    = CD_FRAMESIZE_RAW;
  }

  PRINT("%d[%d-%d] -> 0x%x...0x%x  ... (%d,%d),(%d,%d)\n", 
      inode, start, stop, (int)buf, (int)buf+stop-start,
      start_sector,start_byte,stop_sector,stop_byte);

  cdda.addr_format = CDROM_LBA;
  cdda.nframes     = CACHE_SIZE;
  cdda.buf         = temp;

  // testen of eindadres>CD

  for (sector=start_sector; sector<=stop_sector; sector++){

    PRINT("cache holds [%d-%d], we want sector=%d\n", this_cd->cache_sector, this_cd->cache_sector+CACHE_SIZE-1,  sector);

    if (!((this_cd->cache_sector<=sector) && (sector<this_cd->cache_sector+CACHE_SIZE))) { 
      PRINT("reading sector %d from CD\n", sector);
      this_cd->cache_sector = cdda.addr.lba = sector;
      status = cdfs_ioctl(sb, CDROMREADAUDIO, (unsigned long)&cdda);
      if (status) {
	printk("copy_from_cd(%d) ioctl failed: %d\n", cdda.addr.lba, status);
	return;
      }      
    } else {
      PRINT("getting sector %d from cache\n", sector);
    }

    temp2=temp+(sector-this_cd->cache_sector)*CD_FRAMESIZE_RAW;

    if (sector==start_sector) {
      temp_start  = temp2+start_byte;
      if (sector!=stop_sector) 
	temp_length = read_size-start_byte;
      else
	temp_length = stop_byte-start_byte;
    } else if (sector==stop_sector) {
      temp_start  = temp2;
      temp_length = stop_byte;
    } else {
      temp_start  = temp2;
      temp_length = read_size;
    }

    PRINT("memcpy(0x%x, %x, %d)\n",(int)buf, (int)temp_start, temp_length);
    memcpy(buf, (char*)temp_start, temp_length);
    buf += temp_length;

  } 
}

/***************************************************************************/

void cdfs_cdda_file_read(struct inode * inode, char * buf, size_t count, unsigned start, int raw){
  unsigned stop=start+count;

  PRINT("cdda_file_read(%x, %x, %d, %d)\n", inode, buf, count, start);

  if (raw) {
    cdfs_copy_from_cd(inode->i_sb, inode->i_ino, start, stop, buf);      
  } else {
    if (start < WAV_HEADER_SIZE) {
      if (stop > WAV_HEADER_SIZE) {
	cdfs_cdda_file_read( inode,  buf,                        WAV_HEADER_SIZE-start,  start , 0 );
	cdfs_cdda_file_read( inode,  buf+WAV_HEADER_SIZE-start,  stop-WAV_HEADER_SIZE,   WAV_HEADER_SIZE, 0 );
      } else {
	char temp[44];
	cdfs_make_header(temp, inode->i_size);
	memcpy(buf, temp+start, stop-start);
      }
    } else {
      start -= WAV_HEADER_SIZE;
      stop  -= WAV_HEADER_SIZE;
      cdfs_copy_from_cd(inode->i_sb, inode->i_ino, start, stop, buf);
    }
  }
}

/***************************************************************************/

struct file_operations cdfs_cdda_file_operations = {
read:               generic_file_read,
mmap:               generic_file_mmap
};

int kcdfsd_add_cdda_request(struct file * file, struct page *page){
  PRINT("kcdfsd_add_cdda_request(%x, %x)\n", file, page);
  return kcdfsd_add_request(file->f_dentry, page, CDDA_REQUEST);
}

int kcdfsd_add_cdda_raw_request(struct file * file, struct page *page){
  PRINT("kcdfsd_add_cdda_request(%x, %x)\n", file, page);
  return kcdfsd_add_request(file->f_dentry, page, CDDA_RAW_REQUEST);
}

struct address_space_operations cdfs_cdda_aops = {
readpage:           kcdfsd_add_cdda_request
};

struct address_space_operations cdfs_cdda_raw_aops = {
readpage:           kcdfsd_add_cdda_raw_request
};
