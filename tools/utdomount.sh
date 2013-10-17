#!/bin/bash

ARGS=$@
UTDOMOUNT=/opt/SUNWut/lib/utdomount.bin
NTFS3G=/bin/ntfs-3g
EXFAT=/sbin/mount.exfat-fuse
FUSE_UMOUNT=/usr/lib/fs/fuse/fusermount.bin

AWK=/usr/bin/awk
ID=/usr/bin/id
CAT=/usr/bin/cat
RM=/usr/bin/rm
GREP=/usr/bin/grep
LS=/usr/bin/ls
MKDIR=/usr/bin/mkdir
FSTYP=/usr/sbin/fstyp
BASENAME=/usr/bin/basename
CHOWN=/usr/bin/chown
CHMOD=/usr/bin/chmod

BROWSER=/usr/bin/nautilus
# ICONMEDIA=/usr/share/icons/gnome/32x32/devices/media-flash.png
# ICONEJECT=/usr/share/icons/gnome/32x32/actions/media-eject.png
ICONMEDIA=drive-removable-media-usb-pendrive
ICONEJECT=media-eject


MOUNT=0
UMOUNT=0
FSTYPE=unknown
BLKDEV=
USERID=0
PATH=
ZONEPATH=
RFLAG=0
LFLAG=0

# create_desktop_file USERNAME FNAME NAME EXEC ICON 
function create_desktop_file() {
	USERNAME=$1
	FNAME=$2
	NAME=$3
	EXEC=$4
	ICON=$5

$CAT > $FNAME << EOF
#!/usr/bin/env xdg-open
[Desktop Entry]
Encoding=UTF-8
Name=$NAME
Exec=$EXEC
Type=Application
StartupNotify=true
Icon=$ICON
Terminal=false
Categories=Application;
EOF

	$CHOWN $USERNAME $FNAME
	$CHMOD a+x $FNAME
}

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
fi 

# Makes mountpoint and changes permissions
function mkmntpt() {
	if [ $LFLAG -eq 1 ]; then
		USERNAME=$($AWK -F: '{ if($3 == "'$CURUID'") print $1 }' /etc/passwd)
		GROUPNAME=$($ID -u -nr $USERNAME)

		$MKDIR $1
		$CHOWN $USERNAME:$GROUPNAME $1
		$CHMOD 777 $1
	fi
}

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

RVAL=-1
if [ $MOUNT -eq 1 ] && [ $FSTYPE == "ntfs" ]; then
	GROUPID=$($AWK -F: '{ if($3 == "'$USERID'") print $4 }' /etc/passwd)
	mkmntpt $PATH
	$NTFS3G -o uid=$USERID,gid=$GROUPID $BLKDEV $PATH
	RVAL=$?
elif [ $MOUNT -eq 1 ] && [ $FSTYPE == "exfat" ]; then
	mkmntpt $PATH
	$EXFAT $BLKDEV $PATH
	RVAL=$?
elif [ $UMOUNT -eq 1 ] && [ $FSTYPE == "fuse" ]; then
	OWNER=$($LS -ld $PATH | $AWK '{ print $3 }')
	USERNAME=$($AWK -F: '{ if($3 == "'$USERID'") print $1 }' /etc/passwd)
	
	if [ "$OWNER" = "$USERNAME" -o "$OWNER" = "root" ]; then
		$FUSE_UMOUNT -u $PATH
		RVAL=$?
	else
		echo "Not owner: owner is $OWNER and you're $USERNAME" >&2 
		exit 1
	fi 
else
	$UTDOMOUNT $ARGS
	RVAL=$?
fi

if [ $MOUNT -eq 1 ]; then
	PATH=$($AWK '{ if($1 == "'$BLKDEV'" || $1 == "'$BLKDEV':c") print $2 }' /etc/mnttab)
fi

if [ $RVAL -eq 0 ]; then
	USERNAME=$($AWK -F: '{ if($3 == "'$USERID'") print $1 }' /etc/passwd)
	BASEPATH=$($BASENAME $PATH)
	DESKTOP=/var/opt/SUNWkio/home/$USERNAME/Desktop
	
	if ! [ -d "$DESKTOP" ]; then
		exit 0;		
	fi
	
	if [ $MOUNT -eq 1 ]; then
		create_desktop_file $USERNAME "$DESKTOP/open_$BASEPATH.desktop" \
					"Открыть $BASEPATH" \
					"$BROWSER $PATH" $ICONMEDIA 
		create_desktop_file $USERNAME "$DESKTOP/umount_$BASEPATH.desktop" \
					"Извлечь $BASEPATH" \
					"$0 -u -p $PATH -i $USERID" $ICONEJECT
	elif [ $UMOUNT -eq 1 ]; then
		$RM "$DESKTOP/open_$BASEPATH.desktop"
		$RM "$DESKTOP/umount_$BASEPATH.desktop"
	fi 
fi
