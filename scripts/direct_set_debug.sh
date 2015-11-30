#!/bin/sh

#
# SLP gadget backend
#

SLP_GADGET_PATH=/sys/class/usb_mode/usb0/

is_slp_gadget_valid() {
    if [ -d "$SLP_GADGET_PATH" ]; then
	echo "true"
    else
	echo "false"
    fi
}

slp_load_usb_gadget() {
	echo 0 > ${SLP_GADGET_PATH}/enable
	echo 04e8 > ${SLP_GADGET_PATH}/idVendor
	echo $1 > ${SLP_GADGET_PATH}/idProduct
	echo $2 > ${SLP_GADGET_PATH}/funcs_fconf
	echo $3 > ${SLP_GADGET_PATH}/funcs_sconf
	echo 239 > ${SLP_GADGET_PATH}/bDeviceClass
	echo 2 > ${SLP_GADGET_PATH}/bDeviceSubClass
	echo 1 > ${SLP_GADGET_PATH}/bDeviceProtocol
	echo 1 > ${SLP_GADGET_PATH}/enable
}

slp_unload_usb_gadget() {
	echo 0 > ${SLP_GADGET_PATH}/enable
}

slp_sdb_set() {
	slp_load_usb_gadget "6860" "sdb" "sdb"
	/usr/bin/systemctl start sdbd.service
	echo "SDB enabled"
}

slp_sdb_unset() {
	slp_unload_usb_gadget
	/usr/bin/systemctl stop sdbd.service
	echo "SDB disabled"
}

#
# ConfigFS backend
#

CFS_GADGET_NAME="slp-gadget"
GT=/usr/bin/gt

cfs_load_usb_config() {
    funcs=$3
    label=$1
    config_id=$2
    if [ "${funcs}yes" != "yes" ] ; then
	# this will fail if config exist but we don't care
	${GT} config create ${CFS_GADGET_NAME} $label $config_id &> /dev/null
	# remove all bindings if any
	${GT} config show -r ${CFS_GADGET_NAME} $label $config_id | grep "\->" | awk '{print $3" "$4}' | while read binding ; do
	    ${GT} config del ${CFS_GADGET_NAME} $label $config_id $binding
	done
	# add new bindings
	echo $funcs | tr ',' '\n' | while read func ; do
	    echo $func | tr '.' ' '| xargs -n 2 gt config add ${CFS_GADGET_NAME} $label $config_id
	done
    else
	${GT} config rm -rf ${CFS_GADGET_NAME} $label $config_id &> /dev/null
    fi
}

cfs_load_usb_gadget() {
    ${GT} gadget ${CFS_GADGET_NAME} &> /dev/null
    if [ $? -ne 0 ]; then
	#if gadget not found then load it fro scheme
	${GT} load ${CFS_GADGET_NAME} -o --file=/etc/deviced/cfs-gadget.gs
    else
	#if gadget found make sure that it is disabled
	${GT} disable &> /dev/null
    fi

    ${GT} set ${CFS_GADGET_NAME} \
	idVendor=0x04e8 \
	idProduct=$1 \
	bDeviceClass=239 \
	bDeviceSubClass=2 \
	bDeviceProtocol=1

    cfs_load_usb_config cfs_first_config 1 $2
    cfs_load_usb_config cfs_second_config 2 $3
}

cfs_unload_usb_gadget() {
    ${GT} rm -rf ${CFS_GADGET_NAME}
}

cfs_sdb_set() {
    cfs_load_usb_gadget "0x6860" "ffs.sdb" ""
    /bin/mount sdb -t functionfs /dev/usb-funcs/sdb
    /usr/bin/systemctl start sdbd.service
    sleep 1
    ${GT} enable ${CFS_GADGET_NAME}
    echo "SDB enabled"

}

cfs_sdb_unset() {
    ${GT} disable ${CFS_GADGET_NAME}
    /usr/bin/systemctl stop sdbd.service
    sleep 1
    /bin/umount /dev/usb-funcs/sdb
    cfs_unload_usb_gadget
    echo "SDB disabled"
}

is_cfs_gadget_valid() {
    ${GT} gadget &> /dev/null
    if [ $? -eq 0 ];then
	echo "true"
    else
	echo "false"
    fi
}

#   is_vali_func    func_prefix
BACKENDS="
    is_slp_gadget_valid    slp_
    is_cfs_gadget_valid    cfs_
"

#
# Generic function call mechanism
#

execute_function() {
    while read backend ; do
	word_count=`echo $backend | wc  -w`
	if [ $word_count -ne 2 ]; then
	    continue;
	fi

	backend=( $backend )

	is_backend_valid=`${backend[0]}`
	if [ "$is_backend_valid" == "true" ]; then
	    ${backend[1]}$1 ${@:2}
	    exit $?
	fi
    done <<<"$BACKENDS"
}

show_options() {
	echo "direct_set_debug.sh: usage:"
	echo "    --help       This message"
	echo "    --sdb-set    Load sdb without usb-manager"
	echo "    --sdb-unset  Unload sdb without usb-manager"
}

case "$1" in
"--sdb-set")
	execute_function sdb_set
	;;

"--sdb-unset")
	execute_function sdb_unset
	;;

"--help")
	show_options
	;;

*)
	echo "Wrong parameters. Please use option --help to check options "
	;;
esac
