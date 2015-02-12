#include <linux/net.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/dns_resolver.h>
#include "klibhttp.h"

#define MAX_BUF_SIZE 2 * 1024
#define MAX_RESPONSE_SIZE 2 * 1024 * 1024

static int errno;

static char *http_send(struct socket *sock, char *str)
{
    struct msghdr msg;
    struct iovec iov;
    int len, total, left;
    mm_segment_t oldmm;
    char *response;

    msg.msg_name     = 0;
    msg.msg_namelen  = 0;
    msg.msg_iov      = &iov;
    msg.msg_iovlen   = 1;
    msg.msg_control  = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags    = 0;

    oldmm = get_fs(); set_fs(KERNEL_DS);

    left = total = strlen(str);
    do {
        iov.iov_len = left;
        iov.iov_base = str + (total - left);
        len = sock_sendmsg(sock, &msg, left);
        if (len < 0) {
            pr_err("%s: sock_sendmsg error!\n", __func__);
            return NULL;
        }
        if (len == 0)
            break;
        left -= len;
    } while (left);
    if (left) {
        pr_err("%s: sock_sendmsg error2!\n", __func__);
        return NULL;
    }

    left = total = MAX_RESPONSE_SIZE;
    response = kmalloc(MAX_RESPONSE_SIZE, GFP_KERNEL);
    do {
        iov.iov_len = left;
        iov.iov_base = response + (total - left);
        len = sock_recvmsg(sock, &msg, left, 0);
        pr_debug("%s: sock_recvmsg %d bytes\n", __func__, len);
        if (len < 0) {
            pr_err("%s: sock_recvmsg error!\n", __func__);
            return NULL;
        }
        if (len == 0)
            break;
        left -= len;
    } while (left);
    if (left == 0)
        pr_err("%s: File too large, only first %d bytes.", __func__, total);
    response[total - left] = '\0';

    set_fs(oldmm);
    return response;
}

/*
 * Get content of an url.
 */
char *http_open_url(char *url)
{
    struct sockaddr_in addr;
    struct socket *sock= NULL;
    char *request, *response, *res = NULL, *host, *path, *hostname, *body;
    time_t timeout = 10;
    int hostname_length;

    hostname = kmalloc(256, GFP_KERNEL);
    request = kmalloc(MAX_BUF_SIZE, GFP_KERNEL);

    path = strchr(url, '/');
    if (path) {
        hostname_length = path - url;
    } else {
        path = "/";
        hostname_length = strlen(url);
    }
    strncpy(hostname, url, hostname_length);
    hostname[hostname_length] = '\0';

    errno = dns_query(NULL, hostname, hostname_length, NULL, &host, &timeout);
    if (errno < 0) {
        pr_err("%s: dns_query error! errno = %d!\n", __func__, errno);
        if (errno == -2)
            pr_err("Please `apt-get install keyutils`.\n");
        goto out2;
    }

    errno = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
    if (errno < 0) {
        pr_err("%s: sock_create error! errno = %d\n", __func__, errno);
        goto out2;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = in_aton(host);
    kfree(host);

    errno = sock->ops->connect(sock, (struct sockaddr *) &addr, 
            sizeof(addr), O_RDWR);
    if (errno < 0) {
        pr_err("%s: connect error! errno = %d\n", __func__, errno);
        goto out;
    }
    snprintf(request, MAX_BUF_SIZE,
            "GET %s HTTP/1.0\r\n"
            "User-Agent: httpfs/0.0.1\r\n"
            "Host: %s\r\n"
            "\r\n"
        , path, hostname);
    pr_debug("%s: sending request...\n%s", __func__, request);

    response = http_send(sock, request);
    if (response == NULL)
        goto out;
    body = strstr(response, "\r\n\r\n");
    if (body) {
        body += 4;
        res = kmalloc(strlen(body) + 1, GFP_KERNEL);
        strcpy(res, body);
    } else {
        pr_err("%s: invalid HTML!\n", __func__);
    }
    kfree(response);
out:
    sock_release(sock);
out2:
    kfree(hostname);
    kfree(request);
    return res;
}

int http_get_errno() {
    return errno;
}
