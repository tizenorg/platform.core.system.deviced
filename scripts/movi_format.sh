#!/bin/sh
 
fdisk -H 1 /dev/mmcblk0 << EOF
d
1
d
2
d
3
n
p
1
2
728576
n
p
2

990720
n
p
3

1003520
p
wq
EOF

sleep 1

fat.format -s 32 -S 512 -F 32 /dev/mmcblk0p1
fat.format -s 32 -S 512 -F 32 /dev/mmcblk0p2
fat.format -s 4 -S 4096 -F 16 /dev/mmcblk0p3

