/* 
  
  File iso.c - routines for ISO segments and boot images for cdfs

  
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


struct _bootmedia_ {
  char text[20];
  unsigned int size;   /* in bytes */
} cdfs_bootmedia[5] = {
  {"No Emulation", 0},
  {"1.2 MB diskette", 1200*1024},
  {"1.44 MB diskette", 1440*1024},
  {"2.88 MB diskette", 2800*1024},
  {"Hard Disk", 0}
};

/************************/

void cdfs_check_bootable(struct super_block *sb){
  struct buffer_head * bh1, * bh2, *bh3;
  cd * this_cd = cdfs_info(sb);
  int boottrack;
  int no_bootimage=0;
  int sectionoffset=0;

  
  if (!(bh1 = bread(sb->s_dev, 17, CD_FRAMESIZE)))
    return;                            /* sector 17 is unreadable */
  else
    if (!strncmp(bh1->b_data+7, "EL TORITO", 9)) {      
     

      PRINT("BOOT, catalog at %d\n", *(unsigned int*)(bh1->b_data+71));
      bh2 = bread(sb->s_dev,  *(unsigned int*)(bh1->b_data+71), CD_FRAMESIZE);
      
      PRINT("Catalog:\n\tHeader ID=%d, Platform ID=%d, Developer ID=%s\n", 
             *(unsigned char*)(bh2->b_data), 
	     *(unsigned char*)(bh2->b_data+1), bh2->b_data+4);
      
      do {
	this_cd->tracks++;
        boottrack = this_cd->tracks+2;
		
        PRINT("\tInitial/Default entry: %x Bootable, media: %d\n",
               *(unsigned char*)(bh2->b_data+32+0+sectionoffset),
               *(unsigned char*)(bh2->b_data+32+1+sectionoffset) & 15);
        PRINT("\tSector count: %d, Load LBA: %d\n",
               *(unsigned short*)(bh2->b_data+32+6+sectionoffset),
               *(unsigned int*)(bh2->b_data+32+8+sectionoffset));         
  
        this_cd->track[boottrack].type      = BOOT;
        this_cd->track[boottrack].start_lba = *(unsigned int*)
		(bh2->b_data+32+8+sectionoffset);
        this_cd->track[boottrack].size      = cdfs_bootmedia[
		*(unsigned char*)(bh2->b_data+32+1+sectionoffset) & 15].size;
        if (!this_cd->track[boottrack].size)
            this_cd->track[boottrack].size  = *(unsigned short*)
		    (bh2->b_data+32+6+sectionoffset) * CD_FRAMESIZE ;
        this_cd->track[boottrack].stop_lba = this_cd->track[boottrack].
		start_lba + this_cd->track[boottrack].size/CD_FRAMESIZE -1;
        this_cd->track[boottrack].time      = 0;
        sprintf(this_cd->track[boottrack].name,"boot.image_%d",no_bootimage);
        strncpy(this_cd->track[boottrack].bootID,
			bh2->b_data+4+sectionoffset,24);  /* 27?? */
        this_cd->track[boottrack].bootID[24]=0;     

        /* get first sector from boot image */
        bh3=bread(sb->s_dev, this_cd->track[boottrack].start_lba, CD_FRAMESIZE);
        if ((*(unsigned char*)(bh3->b_data+511)=0xAA) && 
			(*(unsigned char*)(bh3->b_data+512)=0x55)) {
          strcat(this_cd->track[boottrack].bootID, 
			  "\n\tType: x86 boot sector, ");

          if (!strncmp(bh3->b_data+2, "LILO", 4)) 
            strcat(this_cd->track[boottrack].bootID, 
			    "LILO boot/chain loader with ");
          else if (!strncmp(bh3->b_data+495, "LDLINUX", 7)) {
            strncat(this_cd->track[boottrack].bootID, bh3->b_data+495, 12);
            strcat(this_cd->track[boottrack].bootID, " boot loader with ");
          }
        
          if (*(unsigned short*)(bh3->b_data+0x438)==0xEF53)         
            strcat(this_cd->track[boottrack].bootID, 
			    "Linux/i386 ext2 filesystem\n");
          else /* FAT */ 
            strncat(this_cd->track[boottrack].bootID, 
			    bh3->b_data+54, 8);  /* FAT type */
        }

        brelse(bh3);
        if (sectionoffset==0) sectionoffset=0x60;
        sectionoffset=sectionoffset+0x20;
      } while (*(unsigned char*)(bh2->b_data+0x42)>no_bootimage++);
      brelse(bh2); 
    }
    brelse(bh1);
}

/*************************/

struct iso_primary_descriptor * cdfs_get_iso_info(struct super_block *sb, int track_no){
  cd * this_cd = cdfs_info(sb);
  struct buffer_head * bh;
  int block;
  struct iso_primary_descriptor * iso_info = 
	  kmalloc(sizeof(struct iso_primary_descriptor), GFP_KERNEL);

  if (!iso_info) {
    printk("kmalloc failed\n");
    return NULL;
  }

  block = this_cd->track[track_no].start_lba+16;  /* ISO info at sector 16 */
  
  PRINT("Retrieving info for data track %d: block %d\n", 
        track_no, block);

  if (!(bh = bread(sb->s_dev, block, CD_FRAMESIZE))) {
    PRINT("FAILED\n");
    return NULL;
  }

  if (!strncmp(bh->b_data+1,"CD001",5)) {
    memcpy(iso_info, bh->b_data, 
		    sizeof(struct iso_primary_descriptor));   /* ISO session */
  } else {
    kfree(iso_info);
    iso_info=NULL;                                       /* DATA, but no ISO */
  }

  brelse(bh);

  return iso_info;

}

/***********************/


