#!/bin/bash
#
# script to control IDS cron service
# Version: 3.2
#
# History:
# 0.0.1 2013 January - Created
# 1.0 2103 July - dedicated 
# 1.1 2013 August - fix hsotnames and lognames
# 1.2 2013 November - add start/stop perl script
# 2.0 2014 February - check before start is any other process running
# 3.0 2014 December - fix for /repo
# 3.1 2015 November - add log folder as tmpfs
# 3.2 2017 January - some minor fixes (reschedule assigment hostname->node name)
#
#
# Copyright by BROWARSKI
#
if [ -z "$1" ]; then
        echo "Please provide user name";
        exit 1;
fi

LOGNAME=$1
HOME=/home/$LOGNAME
BASEDIR=/home/$LOGNAME/get
BIN=$BASEDIR/idscron/bin
SCRIPT=$BASEDIR/scripts/
PID_PERL=$BASEDIR/pid/main_perl.pid
CFG=cfg/idscron.cfg
CZAS=`/bin/date +%F-%H%M`
FILEC=$BASEDIR/netbone/bin/filec
PID_FILES=$BASEDIR/pid/idscron.pid
PID=0
PID0=0

log_dir_as_tmpfs () {
	LOG_DIR=$BASEDIR/log
	echo -n "check is $LOG_DIR already mounted as tmpfs ... "
	# can't use LOG_DIR as get is symlink to get-<version>
	# so, mount output is
	# tmpfs on /home/prod/get-4.0/log type tmpfs (rw,relatime)
	# not as
	# tmpfs on /home/prod/get/log type tmpfs (rw,relatime)
	#
	check=`/bin/mount | grep tmpfs | grep $BASEDIR | grep log`
        if [ -z "$check" ]; then
		echo "No"
		echo -n "Is $LOG_DIR  empty ..."
		check=`ls -1 $LOG_DIR | wc -l`

		if [ $check -eq 0 ]; then
			echo "Yes"
			echo -n "mount it as tmpfs ... "
			check=`/bin/mount -t tmpfs -o uid=$LOGNAME tmpfs /home/$LOGNAME/get/log`
			if [ -z $check ]; then
				echo "SUCCESS"
			else 
				echo "Please check error: $check"
			fi
		else 
			echo "NO !! There is $check files. Please clear foler $LOG_DIR for tmpfs"
		fi
        else
        	echo "YES"
        fi

}

# log_dir_as_tmpfs

#
# log name determine environment
#
HOST=$LOGNAME
#
# hostname determine instance number
#
HOSTNAME=`hostname`;
case $LOGNAME in
	mlea2710)
		case $HOSTNAME in
			cmitvn1)
			HOST=$HOST"01"
			;;
			cmitvn2)
			HOST=$HOST"02"
			;;
			cmitvn3)
			HOST=$HOST"03"
			;;
	*)
	HOST=${HOSTNAME}00
	;;
esac

cd $BIN
#
# get pid
#
if [ -f $PID_FILES ]
then
	PID=`cat $PID_FILES`
	echo idscron pid $PID 
fi
if [ -f $PID_PERL ]
then
	PID_P=`cat $PID_PERL`
	echo PERL main pid $PID_P
fi

echo "HOST is $HOST"

case $2 in
	start_perl)
		#
                # idscron need to be run from root
                # as this is require for ping 
                #
		if [[ $EUID -eq 0 ]]; then
			log_dir_as_tmpfs
                        check=`ps -ef | grep $HOST | grep $SCRIPT | wc -l`
			
                        if [ "$check" == "0" ]; then
				$SCRIPT/main.pl $HOST $LOGNAME
                        else
                                echo "other main.pl process is running, please check: $check";
                        fi
                else
                        echo "Please run it from root account"
                fi

	;;
	stop_perl)
		echo stop: perl main $PID_P
		kill -INT $PID_P
		rm $PID_PERL
	;;
	start)
		#
		# idscron need to be run from root
		# as this is require for ping (in emergency
		# when start_perl isn't running)
		#
		if [[ $EUID -eq 0 ]]; then
			log_dir_as_tmpfs
			#
			# check is other process is running
			#
			check=`ps -ef | grep $HOST | grep $BIN | wc -l`
			if [ $check == "0" ]; then
				$BIN/idscron $HOST
			else 
				echo "other process is running, please check";
			fi
		else 
			echo "Please run it from root account"
		fi
	;;
	stop)
		#
		# is this a number
		#
		echo stop: files $PID
		kill -INT $PID
		rm $PID_FILES
	;;
	get)
		if [ -f $CFG ]
		then 
			echo "backup to $CFG.$CZAS"
			cp $CFG $CFG.$CZAS
		fi
	
		$FILEC get /repo/idscron/$CFG > $CFG
		echo "New config loaded"
	;;
	diff)
		 $FILEC get /repo/idscron/$CFG | diff $CFG -
	;;
	put)
		$FILEC lput /repo/idscron/$CFG $BIN/$CFG
		echo "File put into IDS system"
	;;
	
	status)
		if [ $PID == 0 ]
		then 
			echo "No PID file and number"
		else 
			if kill -0 $PID
			then
				echo IDSCRON exists;
				ps -eo pid,stat,pcpu,rss,vsz,start,args | grep ^$PID
			else
				/bin/rm $PID_FILES;
				echo "No $PID process found";
			fi
		fi

	;;
	*)
		echo "Unknown system command, allowed: stop, start, status, get, put, diff, start_perl, stop_perl";
	;;
esac
