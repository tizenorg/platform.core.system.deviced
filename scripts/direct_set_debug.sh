#!/bin/sh

VERSION=

check_driver_version() {
	if [ -f /sys/class/usb_mode/version ]
	then
		VERSION=`cat /sys/class/usb_mode/version`
	else
		VERSION="0.0"
	fi
}

load_usb_gadget_0_0() {
	echo 4 > /sys/devices/platform/usb_mode/UsbMenuSel
}

load_usb_gadget_1_0() {
	echo 0 > /sys/class/usb_mode/usb0/enable
	echo 04e8 > /sys/class/usb_mode/usb0/idVendor
	echo $1 > /sys/class/usb_mode/usb0/idProduct
	echo $2 > /sys/class/usb_mode/usb0/functions
	echo 239 > /sys/class/usb_mode/usb0/bDeviceClass
	echo 2 > /sys/class/usb_mode/usb0/bDeviceSubClass
	echo 1 > /sys/class/usb_mode/usb0/bDeviceProtocol
	echo 1 > /sys/class/usb_mode/usb0/enable
}

load_usb_gadget_1_1() {
	echo 0 > /sys/class/usb_mode/usb0/enable
	echo 04e8 > /sys/class/usb_mode/usb0/idVendor
	echo $1 > /sys/class/usb_mode/usb0/idProduct
	echo " " > /sys/class/usb_mode/usb0/functions
	echo $2 > /sys/class/usb_mode/usb0/funcs_fconf
	echo $3 > /sys/class/usb_mode/usb0/funcs_sconf
	echo 239 > /sys/class/usb_mode/usb0/bDeviceClass
	echo 2 > /sys/class/usb_mode/usb0/bDeviceSubClass
	echo 1 > /sys/class/usb_mode/usb0/bDeviceProtocol
	echo 1 > /sys/class/usb_mode/usb0/enable
}

unload_usb_gadget_1() {
	echo 0 > /sys/class/usb_mode/usb0/enable
}

sdb_set() {
	case "$VERSION" in
	"1.0")
		load_usb_gadget_1_0 "6860" "mtp,acm,sdb"
		;;
	"1.1")
		load_usb_gadget_1_1 "6860" "mtp" "mtp,acm,sdb"
		;;
	*)
		echo "USB driver version $VERSION is not supported"
		return
		;;
	esac

	/usr/bin/systemctl start sdbd.service
	echo "SDB enabled"
}

ssh_set() {
	case "$VERSION" in
	"0.0")
		load_usb_gadget_0_0
		;;
	"1.0")
		load_usb_gadget_1_0 "6864" "rndis"
		;;
	"1.1")
		load_usb_gadget_1_1 "6864" "rndis" " "
		;;
	*)
		echo "USB driver version $VERSION is not supported"
		return
		;;
	esac

	/sbin/ifconfig usb0 192.168.129.3 up
	/sbin/route add -net 192.168.129.0 netmask 255.255.255.0 dev usb0
	/usr/bin/systemctl start sshd.service
	echo "SSH enabled"
}

usb_unset() {
	case "$VERSION" in
	"1.0" | "1.1")
		unload_usb_gadget_1
		;;
	*)
		echo "USB driver version $VERSION is not supported"
		return
		;;
	esac
}

sdb_unset() {
	usb_unset
	/usr/bin/systemctl stop sdbd.service
	echo "SDB disabled"
}

ssh_unset() {
	usb_unset
	/sbin/ifconfig usb0 down
	/usr/bin/systemctl stop sshd.service
	echo "SSH disabled"
}

show_options() {
	echo "direct_set_debug.sh: usage:"
	echo "    --help       This message"
	echo "    --sdb-set    Load sdb without usb-manager"
	echo "    --sdb-unset  Unload sdb without usb-manager"
	echo "    --ssh-set    Load ssh without usb-manager"
	echo "    --ssh-unset  Unload ssh without usb-manager"
}

check_driver_version

case "$1" in
"--sdb-set")
	sdb_set
	;;

"--ssh-set")
	ssh_set
	;;

"--sdb-unset")
	sdb_unset
	;;

"--ssh-unset")
	ssh_unset
	;;

"--help")
	show_options
	;;

*)
	echo "Wrong parameters. Please use option --help to check options "
	;;
esac
