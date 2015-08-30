/*
#define PRDEBUG
*/

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#error Sorry, this version of cdfs needs at least kernel 2.4.0
#endif


#define MODULE
#define __KERNEL__ 

#define FSNAME "cdfs"
#define VERSION CONFIG_CDFS_VERSION


#ifdef PRDEBUG
# define PRINT(format, arg...) printk(FSNAME "-> " format, ## arg)
#else
# define PRINT(format, arg...) 
#endif

#include <linux/autoconf.h>
#ifdef CONFIG_MODVERSIONS
#include <linux/modversions.h>
#endif


#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/init.h>
#include <linux/cdrom.h>
#include <linux/iso_fs.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <asm/uaccess.h>


#define CDFS_MAGIC 0xCDDA
#define CDFS_MAXFN 128

#define TRUE 1
#define FALSE 0

#define MAX_TRACKS 99
#define WAV_HEADER_SIZE 44

#define READ_AHEAD 0

#define NONE  0
#define AUDIO 1
#define DATA  2
#define BOOT  3
#define HFS   4

#define AUDIO_NAME       "track-%02d.wav"
#define RAW_AUDIO_NAME       "track-%02d.raw"
#define AVI_AUDIO_NAME       "track-%02d.avi"
#define DATA_NAME_ISO    "sessions_1-%d.iso"
#define DATA_NAME_SINGLE "session-%d.iso"
#define DATA_NAME_VCD    "videocd-%d.mpeg"

#define UID   0
#define GID   0
#define MODE  (S_IFREG | S_IRUSR | S_IRGRP | S_IROTH) 

#define cdfs_info(X)    ((X)->u.generic_sbp)

#define CACHE_SIZE 1

// Convert track to inode number
#define T2I(X) ((X)+3)

/*
 sb        ->  inode 0
 .         ->  inode 1
 ..        ->  inode 2
 track-01  ->  inode 3  (track 0!)
 track-02  ->  inode 4  (track 1!)
 etc.
*/

typedef struct _iso_track_ {
  unsigned start;
  unsigned stop;
} iso_track;

typedef struct _track_info {
  unsigned size;                              /* in bytes */
  unsigned start_lba;
  unsigned stop_lba;
  unsigned hfs_offset;                        /* in 512 bytes */
  unsigned track_size;                        /* in bytes */
  unsigned iso_size;                          /* in bytes */
  unsigned type;                              /* audio, data of boot */
  char name[99];
  unsigned inode;
  time_t time;                                /* only for data tracks */
  struct iso_primary_descriptor * iso_info;   /* only for data tracks */
  int xa_data_size;                           /* only for xa data tracks */
  int xa_data_offset;                         /* only for xa data tracks */
  char bootID[99];
  int avi; /* only for raw audio tracks */
  int avi_offset;
  int avi_swab; /* swap audio bytes? */
} track_info;

typedef struct _cd_ {
  mode_t mode;
  gid_t gid;
  uid_t uid;    
  int single;
  unsigned size;                      /* bytes */
  unsigned tracks;                    /* number of tracks */
  track_info track[MAX_TRACKS];       /* info per track */
  int nr_iso_sessions;
  int discid;                         /* disc id hash */
  iso_track lba_iso_sessions[MAX_TRACKS];
  char videocd_type[9];
  char videocd_title[17];
  char * cache;
  int cache_sector;
  int raw_audio;
} cd;

typedef unsigned char byte; 

int cdfs_read_proc(char *buf, char **start, off_t offset, int len, int *eof, void *data );

extern struct file_operations cdfs_cdda_file_operations;
extern struct file_operations cdfs_cdXA_file_operations;
extern struct file_operations cdfs_cddata_file_operations;
extern struct file_operations cdfs_cdhfs_file_operations;

extern struct address_space_operations cdfs_cdda_aops;
extern struct address_space_operations cdfs_cdda_raw_aops;
extern struct address_space_operations cdfs_cdXA_aops;
extern struct address_space_operations cdfs_cddata_aops;
extern struct address_space_operations cdfs_cdhfs_aops;

time_t cdfs_constructtime(char * time);
unsigned cdfs_constructsize(char * size);
void cdfs_constructMSFsize(char * result, unsigned length);
int cdfs_ioctl(struct super_block *s, int cmd, unsigned long arg);
struct iso_primary_descriptor * cdfs_get_iso_info(struct super_block *sb, int track_no);
int cdfs_get_hfs_info(struct super_block *sb, unsigned track);
void cdfs_check_bootable(struct super_block *sb);
void cdfs_get_XA_info(struct super_block * sb, int inode);
void cdfs_copy_from_cdXA(struct super_block * sb, int inode, unsigned int start,
	unsigned int stop, char * buf);
void cdfs_copy_from_cddata(struct super_block * sb, int inode, unsigned int start,
	unsigned int stop, char * buf);
void cdfs_copy_from_cdhfs(struct super_block * sb, int inode, unsigned int start,
	 unsigned int stop, char * buf);
void cdfs_cdda_file_read(struct inode * inode, char * buf, size_t count, unsigned start /*loff_t *ppos*/,int raw);

int kcdfsd_add_cdXA_request(struct file * file, struct page *page);
int kcdfsd_add_cddata_request(struct file * file, struct page *page);
int kcdfsd_add_cdda_request(struct file * file, struct page *page);
int kcdfsd_add_cdda_raw_request(struct file * file, struct page *page);
int kcdfsd_add_cdhfs_request(struct file * file, struct page *page);

int kcdfsd_add_request(struct dentry *dentry, struct page *page, unsigned type);
int kcdfsd_thread(void *unused);
void kcdfsd_cleanup_thread(void);
extern int kcdfsd_pid;

/* for discid stuff */
unsigned long discid(cd *);

// REQUEST TYPES
#define CDDA_REQUEST   1
#define CDXA_REQUEST   2
#define CDHFS_REQUEST  3
#define CDDATA_REQUEST 4
#define CDDA_RAW_REQUEST 5
