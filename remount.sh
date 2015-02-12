sudo umount http:
sudo rmmod httpfs
sudo dmesg -C
sudo insmod httpfs.ko
sudo mount -t httpfs none http:
