#!/bin/sh

prefix=/opt/netxms
config=/etc/nxagentd.conf
log=/tmp/nxagentupdate.log
#prefix=/usr/local
configureAdd=

###############################################################################
#
# Main code
#

# process args
while [ "x"$1 != "x" ]; do
	name=`echo $1|cut -d= -f1`
	val=`echo $1|cut -d= -f2`
	if [ "x$name" = "xopt" ]; then
		configureAdd="$configureAdd $val"
	else
		eval $name=$val
	fi
	shift
done

make=`which gmake`
if [ $? = 0 ]; then
	echo $make | grep "no gmake in " >/dev/null && make=make
else
	make=make
fi

case `uname -s` in
	SunOS)
		KILLALL() {
			pkill $*
		}
		;;
	AIX|HP-UX)
		KILLALL() {
			pids=`/usr/bin/ps -ef | grep $2 | grep -v grep | awk '{ print $2 }'`
			[ ! -z "$pids" ] && kill $1 $pids
		}
		;;
esac

cd `dirname $0`
name=`ls netxms-*.tar.gz 2>/dev/null|sed s',\.tar\.gz$,,'`
if [ "x$name" = "x" ]; then
	echo invalid package >>$log
	exit 1
fi

gzip -dc $name.tar.gz | tar xf - 2>/dev/null
if [ $? != 0 ]; then
	echo Unable to unpack >>$log
	exit 2
fi
cd $name
if [ $? != 0 ]; then
	echo Unable to change working dir >>$log
	exit 3
fi

# ask nxagentd gently
KILLALL -15 nxagentd >>$log 2>&1
# wait a few seconds and smash it down
sleep 15 && KILLALL -9 nxagentd >>$log 2>&1

# do configure
./configure --prefix=$prefix --with-agent $configureAdd >>$log 2>&1
	if [ $? != 0 ]; then
	echo configure failed, restarting old agent  >>$log
	# Try to restart existing agent
	$prefix/bin/nxagentd -d -c $config >>$log 2>&1
	exit 4
fi

# build
$make >>$log 2>&1
if [ $? != 0 ]; then
	echo build failed, restarting old agent >>$log
	# Try to restart existing agent
	$prefix/bin/nxagentd -d -c $config >>$log 2>&1
	exit 4
fi

# now we can install it...
$make install >>$log 2>&1
if [ $? != 0 ]; then
	echo install failed, restarting old agent >>$log
	# Try to restart existing agent
	$prefix/bin/nxagentd -d -c $config >>$log 2>&1
	exit 5
fi

# show config
echo "config ($config):" >>$log
cat $config >>$log 2>&1
echo "---------------" >>$log

# and restart
echo "Starting agent: $prefix/bin/nxagentd -d -c $config" >>$log
$prefix/bin/nxagentd -d -c $config >>$log 2>&1
ret=$?
if [ $ret != 0 ]; then
	echo nxagentd not started \($ret\) >>$log
	exit 5
fi
rm -f $log.tmp
