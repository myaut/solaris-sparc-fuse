solaris-sparc-fuse
==================

Drivers for FUSE and NTFS/exFAT flash drives on Solaris 11 SPARC with SunRay Software
Full information may be found in my blog: http://www.tune-it.ru/web/myaut/home/-/blogs/добавляем-поддержку-ntfs-exfat-в-solaris-11
(on Russian)

Contains: 
- FUSE from old OpenSolaris repository (hg.opensolaris.org)
- Patched drivers for NTFS (ntfs-3g) and exFAT (fuse-exfat) with support of MBR partitions (on SPARC it is not provided by kernel)
- fstyp plugins and utdomount proxy in tools/directory

build & install
===============

Install prerequisites:
 - Solaris Studio 12.3 and developer/build/onbld package to build FUSE
 - GCC 4.5 (in solaris it located in developer/gcc-45 package) to build ntfs-3g and fuse-exfat
 - SCons build system for fuse-exfat

Build FUSE:
` # export PATH=$PATH:/opt/solarisstudio12.3/bin:/opt/onbld/bin/sparc/

# dmake
# dmake install
# dmake pkg

# pkgrm <pkg-name>
# pkgadd -d packages/ <pkg-name>
`

Build ntfs-3g:
` # ./configure --with-fuse=external --prefix=/opt/ntfs/
# make
# make install
`

Build fuse-exfat:
` # scons install
`

Build fstyp plugins for ntfs and exfat:
` # gcc -o fstyp.so.1 fstyp.c -fpic -lntfs-3g -shared
# gcc -o fstyp.so.1 fstyp.c -D_FILE_OFFSET_BITS=64 -O2 -fPIC -lexfat -shared
`
Put them to /usr/lib/fs/ntfs and /usr/lib/fs/exfat directories and create hardlinks to /usr/bin/fstyp there

Add "ntfs" and "exfat" to variables FST_LIST and FSM_LIST in SRSS files /opt/SUNWut/bin/utdiskadm and /opt/SUNWut/lib/utprepmount
Add suid bits to /bin/ntfs-3g, /sbin/mount.exfat-fuse and /usr/lib/fs/fuse/fusermount.bin

Move /opt/SUNWut/lib/utdomount to /opt/SUNWut/lib/utdomount.bin and place tools/utdomount.sh script there

license
=======

Since this code consists of OpenSource components, licensing is determined by their authors, not me.
