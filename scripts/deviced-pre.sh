#!/bin/sh

DEVICED_ENV_F=/run/deviced/deviced_env
[ -e $DEVICED_ENV_F ] && /bin/rm -f $DEVICED_ENV_F
[ -d ${DEVICED_ENV_F%/*} ] || /bin/mkdir -p ${DEVICED_ENV_F%/*}

echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib" >> $DEVICED_ENV_F

DEV_INPUT=
TOUCHSCREEN=400
TOUCHKEY=200

for file in /sys/class/input/event*; do
        if [ -e $file ]; then
                dev_keytype=`/bin/cat ${file}/device/capabilities/key`
                if [ "$dev_keytype" != 0 ]; then
                        DEV_INPUT=$DEV_INPUT:/dev/input/${file#/sys/class/input/}
                        var=${dev_keytype%%' '*}
                        if [ $var == $TOUCHSCREEN  ]; then
                                DEV_TOUCHSCREEN=/sys/class/input/${file#/sys/class/input/}/device/enabled
                                echo ${var} ${file#/sys/class/input/}
                        fi
                        if [ $var == $TOUCHKEY ]; then
                                dev_ledtype=`/bin/cat ${file}/device/capabilities/led`
                                if [ "$dev_ledtype" != 0 ]; then
                                        DEV_TOUCHKEY=/sys/class/input/${file#/sys/class/input/}/device/enabled
                                        echo ${var} ${file#/sys/class/input/}
                                fi
                        fi
                fi
        fi
done

for file in /sys/class/lcd/*; do
        if [ -e $file ]; then
		hbm_state=`/bin/cat ${file}/device/hbm`
		if [ "$hbm_state" != 0 ]; then
			HBM_NODE=${file}/device/hbm
		fi
		alpm_state=`/bin/cat ${file}/device/alpm`
		if [ "$alpm_state" != 0 ]; then
			ALPM_NODE=${file}/device/alpm
		fi
	fi
done

echo "PM_INPUT=$DEV_INPUT" >> $DEVICED_ENV_F
echo "PM_TOUCHSCREEN=$DEV_TOUCHSCREEN" >> $DEVICED_ENV_F
echo "PM_TOUCHKEY=$DEV_TOUCHKEY" >> $DEVICED_ENV_F
echo "HBM_NODE=$HBM_NODE" >> $DEVICED_ENV_F
echo "ALPM_NODE=$ALPM_NODE" >> $DEVICED_ENV_F
echo "PM_TO_NORMAL=30000" >> $DEVICED_ENV_F
echo "PM_TO_LCDDIM=5000" >> $DEVICED_ENV_F
echo "PM_SYS_DIMBRT=0" >> $DEVICED_ENV_F

