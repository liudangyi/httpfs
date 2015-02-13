#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>    /* copy_to_user */

#include "klibhttp.h"

/*
 * Boilerplate stuff.
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leedy");

#define HTTPFS_MAGIC 0x19940304
#define TMPSIZE 20
#define URL_MAX 256


/*
 * File operations
 */

/*
 * Open a file. Here we send the request and get the response.
 */
static int httpfs_open(struct inode *inode, struct file *filp)
{
    char *response;

    pr_debug("%s: opening %s\n", __func__, (char *) inode->i_private);
    response = http_open_url(inode->i_private);
    BUG_ON(response == NULL);
    if (IS_ERR(response))
        return PTR_ERR(response);
    filp->private_data = response;
    return 0;
}

static ssize_t httpfs_read_file(struct file *filp, char *buf,
        size_t count, loff_t *offset)
{
    int len;
    char *tmp = (char *) filp->private_data;
/*
 * Encode the value, and figure out how much of it we can pass back.
 */
    len = strlen(tmp);
    if (*offset > len)
        return 0;
    if (count > len - *offset)
        count = len - *offset;
/*
 * Copy it back, increment the offset, and we're done.
 */
    if (copy_to_user(buf, tmp + *offset, count))
        return -EFAULT;
    *offset += count;
    return count;
}

static int httpfs_release(struct inode *inode, struct file *file)
{
    pr_debug("%s: closing %s\n", __func__, (char *) inode->i_private);
    kfree(file->private_data);
    return 0;
}


/*
 * Dentry cleanup function, here we need to free the url memory.
 */
static void httpfs_d_iput(struct dentry *dentry, struct inode *inode) {
    kfree(inode->i_private);
    iput(inode);
}


static struct dentry *httpfs_inode_lookup(struct inode *dir,
        struct dentry *dentry, unsigned int flags);

static const struct file_operations httpfs_file_operations = {
    .open    = httpfs_open,
    .release = httpfs_release,
    .read    = httpfs_read_file,
};

static const struct dentry_operations httpfs_dentry_operations = {
    .d_delete = always_delete_dentry,
    .d_iput   = httpfs_d_iput,
};

static const struct inode_operations httpfs_inode_operations = {
    .lookup = httpfs_inode_lookup,
};

static const struct super_operations simple_super_operations = {
    .statfs = simple_statfs,
};


/*
 * Lookup (in fact create) an inode.
 */
static struct dentry *httpfs_inode_lookup(struct inode *dir,
    struct dentry *dentry, unsigned int flags)
{
    struct qstr paths[TMPSIZE];
    struct dentry *parent = dentry;
    struct inode *inode;
    char *fullpath;
    int count = 0, len = 0;

    fullpath = kmalloc(URL_MAX, GFP_KERNEL);
    while (parent && parent != parent->d_parent) {
        paths[count] = parent->d_name;
        parent = parent->d_parent;
        count++;
    }
    while (count) {
        count--;
        len += paths[count].len + 1;
        if (len > URL_MAX)
            return ERR_PTR(-ENAMETOOLONG);
        strcat(fullpath, paths[count].name);
        if (count != 0)
            strcat(fullpath, "/");
    }

    inode = new_inode(dir->i_sb);
    if (!inode) {
        d_add(dentry, NULL);
        return NULL;
    }
    inode->i_mode = S_IFDIR | 0555;
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    inode->i_op = &httpfs_inode_operations;
    inode->i_fop = &httpfs_file_operations;
    inode->i_private = fullpath;

    d_set_d_op(dentry, &httpfs_dentry_operations);
    d_add(dentry, inode);
    return NULL;
}


/*
 * Superblock stuff.  This is all boilerplate to give the vfs something
 * that looks like a filesystem to work with.
 */

/*
 * "Fill" a superblock with mundane stuff.
 */
static int httpfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *inode;
    struct dentry *root;

    sb->s_blocksize = PAGE_CACHE_SIZE;
    sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
    sb->s_magic = HTTPFS_MAGIC;
    sb->s_op = &simple_super_operations;
    sb->s_time_gran = 1;

    inode = new_inode(sb);
    if (!inode)
        return -ENOMEM;
    inode->i_mode = S_IFDIR | 0555;
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    inode->i_op = &httpfs_inode_operations;
    inode->i_fop = &simple_dir_operations;
    set_nlink(inode, 2);
    root = d_make_root(inode);
    if (!root)
        return -ENOMEM;
    sb->s_root = root;
    return 0;
}


/*
 * Stuff to pass in when registering the filesystem.
 */
static struct dentry *httpfs_get_super(struct file_system_type *fst,
        int flags, const char *devname, void *data)
{
    return mount_single(fst, flags, data, httpfs_fill_super);
}

static struct file_system_type httpfs_type = {
    .owner      = THIS_MODULE,
    .name       = "httpfs",
    .mount      = httpfs_get_super,
    .kill_sb    = kill_litter_super,
};


/*
 * Get things set up.
 */
static int __init httpfs_init(void)
{
    return register_filesystem(&httpfs_type);
}

static void __exit httpfs_exit(void)
{
    unregister_filesystem(&httpfs_type);
}

module_init(httpfs_init);
module_exit(httpfs_exit);
