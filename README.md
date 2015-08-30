# smount

Two programs to easily mount/unmount all sessions of a multisession disk.
This program depends on cdfs, by Michiel Ronsse.

Since cdfs works only for kernels 2.4.X, smount and sumount works only for 
these kernels too.

To create and install them:
  ./configure
	make install

This program is under the GNU/GPL (see http://www.gnu.org)

If you want to intergrate another version of cdfs, than the one provided, you
have to edit configure script at the very beginning.

The usage is straight forward:
Type "smount" to mount a multisessin CD
Type "sumount" to unmount a multisession CD
	

Author:

Panayotis Katsaloulis, http://panayotis.com
