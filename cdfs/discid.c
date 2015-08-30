
#include "cdfs.h"

static unsigned int cddb_sum(int n);


unsigned long discid(cd * this_cd) {
  unsigned int i=0, t, n = 0;

  for (i=0; i< this_cd->tracks; i++)
    n += cddb_sum((this_cd->track[T2I(i)].start_lba+CD_MSF_OFFSET)/CD_FRAMES);

  t = this_cd->track[T2I(this_cd->tracks-1)].stop_lba/CD_FRAMES;

  return (((n % 0xFF) << 24) | (t << 8) | this_cd->tracks);
}


static unsigned int cddb_sum(int n) {
  unsigned int ret = 0;

  while (n > 0) {
    ret += (n % 10);
    n /= 10;
  }

  return ret;
}

