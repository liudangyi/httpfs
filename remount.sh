sudo umount http:
sudo rmmod httpfs
sudo insmod httpfs.ko
sudo mount -t httpfs none http:
