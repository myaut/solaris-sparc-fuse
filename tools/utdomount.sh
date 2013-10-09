#!/bin/bash

ARGS=$@
UTDOMOUNT=/opt/SUNWut/lib/utdomount.bin
AWK=/usr/bin/awk
ID=/usr/bin/id
GREP=/usr/bin/grep
LS=/usr/bin/ls
MKDIR=/usr/bin/mkdir

NTFS3G=/bin/ntfs-3g
FUSE_UMOUNT=/usr/lib/fs/fuse/fusermount.bin

MOUNT=0
UMOUNT=0
FSTYPE=unknown
BLKDEV=
USERID=0
PATH=
ZONEPATH=
RFLAG=0

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
	esac
done

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
elif [ $UMOUNT -eq 1 ] && [ $FSTYPE == "fuse" ]; then
	OWNER=$($LS -ld $PATH | $AWK '{ print $3 }')
	USERNAME=$($AWK -F: '{ if($3 == "'$USERID'") print $1 }' /etc/passwd)
	
	if [ "$OWNER" = "$USERNAME" ]; then
		$FUSE_UMOUNT -u $PATH
	else
		echo "Not owner" >&2 
		exit 1
	fi 
else
	$UTDOMOUNT $ARGS
fi