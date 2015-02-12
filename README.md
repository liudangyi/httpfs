# HTTPFS

Mount the web as a directory.

# Usage

1. `apt-get install keyutils` if you have no /sbin/request-key.
2. `make`
3. `sudo insmod httpfs.ko`
4. `sudo mount -t httpfs none http:`
5. Try `cat http://baidu.com`
6. If you are tired, `sudo umount http: && sudo rmmod httpfs`

# Problems

1. Only HTTP 1.0 is supported right now.
2. Files are downloaded upon `open`. However, [Range](http://en.wikipedia.org/wiki/Byte_serving) is a better choice.
3. Because of 2, it's hard to support larger files.
4. There's little promise of further work, since the whole thing should be implemented in user space (using [FUSE](http://en.wikipedia.org/wiki/Filesystem_in_Userspace) with [libcurl](http://curl.haxx.se/libcurl/)).
