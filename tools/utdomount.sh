#!/bin/bash

ARGS=$@
UTDOMOUNT=/opt/SUNWut/lib/utdomount.bin
NTFS3G=/bin/ntfs-3g
EXFAT=/sbin/mount.exfat-fuse
FUSE_UMOUNT=/usr/lib/fs/fuse/fusermount.bin

AWK=/usr/bin/awk
ID=/usr/bin/id
GREP=/usr/bin/grep
LS=/usr/bin/ls
MKDIR=/usr/bin/mkdir
FSTYP=/usr/sbin/fstyp

MOUNT=0
UMOUNT=0
FSTYPE=unknown
BLKDEV=
USERID=0
PATH=
ZONEPATH=
RFLAG=0
LFLAG=0

while getopts "muf:b:i:lp:Z:r" OPTION
do
	case $OPTION in
	m)
		MOUNT=1
		;;
	u)
		UMOUNT=1
		;;
	f)
		FSTYPE=$OPTARG
		;;
	p)
		PATH=$OPTARG
		;;
	i) 
		USERID=$OPTARG
		;;
	b)
		BLKDEV=$OPTARG
		;;
	l)
		LFLAG=1
		;;
	esac
done

if [ $LFLAG -eq 1 ]; then
	USERNAME=$($AWK -F: '{ if($3 == "'$USERID'") print $1 }' /etc/passwd)
		
	I=0
	while : ;
	do
		PATH=/tmp/SUNWut/mnt/$USERNAME/noname_$I
		if ! [[ -e $PATH ]]; then
			break
		fi
		
		I=$(($I + 1))
	done	
	
	$MKDIR $PATH
fi 

if [ "$FSTYPE" == "pcfs" ]; then
	# Oh, my gosh! PCFS also means that utmountd found partitions and 'guessed it is PCFS'
	# If fstyp returns 'no matches', utmountd was right, correct it otherwise
	REALFSTYPE=$($FSTYP $BLKDEV 2>/dev/null)
	if [ -n "$REALFSTYPE" ]; then
		FSTYPE=$REALFSTYPE
	fi
fi

if [ $UMOUNT -eq 1 ]; then
	FSTYPE=$($AWK '{ if($2 == "'$PATH'") print $3 }' /etc/mnttab)
	MNTOPT=$($AWK '{ if($2 == "'$PATH'") print $4 }' /etc/mnttab)
	
	if [ -z "$FSTYPE" ]; then
		echo "$PATH not mounted" >&2 
		exit 1
	fi
fi

if [ $MOUNT -eq 1 ] && [ $FSTYPE == "ntfs" ]; then
	GROUPID=$($AWK -F: '{ if($3 == "'$USERID'") print $4 }' /etc/passwd)
	$NTFS3G -o uid=$USERID,gid=$GROUPID $BLKDEV $PATH
elif [ $MOUNT -eq 1 ] && [ $FSTYPE == "exfat" ]; then
	$EXFAT $BLKDEV $PATH
elif [ $UMOUNT -eq 1 ] && [ $FSTYPE == "fuse" ]; then
	CURUID=$($ID -u)
	OWNER=$($LS -ld $PATH | $AWK '{ print $3 }')
	USERNAME=$($AWK -F: '{ if($3 == "'$CURUID'") print $1 }' /etc/passwd)
	
	if [ "$OWNER" = "$USERNAME" ]; then
		$FUSE_UMOUNT -u $PATH
	else
		echo "Not owner" >&2 
		exit 1
	fi 
else
	$UTDOMOUNT $ARGS
fi