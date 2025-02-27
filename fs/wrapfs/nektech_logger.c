#ifdef NEKTECH_LOGGER
#include "nektech_logger.h"
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/bitops.h>
#include <linux/rcupdate.h>
#include <net/sock.h>
#include <linux/netfilter.h>
#include <linux/socket.h>

extern char *nektech_lower_path;
struct file_path {
        int     length;
        char    *filePathName;
};


/*
 * Function Name: inet_ntop
 * Description: Function copies and the Internet address of remote
 * System to a bufffer provide by the caller.
 * Return Type: return the filled buffers Pointer.
 */
static const char *
inet_ntop(const struct in_addr *addr, char *buf, int len)
{    
	const u_int8_t *ap = (const u_int8_t *)&addr->s_addr;
	char tmp[15]; /* max length of ipv4 addr string */
	int fulllen;
	
	/*
	 * snprintf returns number of bytes printed (not including NULL) or
	 * number of bytes that would have been printed if more than would
	 * fit
	 */
	fulllen = snprintf(tmp, sizeof(tmp), "%d.%d.%d.%d",
					   ap[0], ap[1], ap[2], ap[3]);
	if (fulllen >= (int)len) {
		return NULL;
	}
	strncpy(buf,tmp,fulllen);
	buf[fulllen] = '\0';

	return buf;
}
/*
 * Function Name: getfilepath()
 * Description: It will create a path by traversing through the parent dentry
 * objects and crete a path till mounted location.
 */
long getfilepath(struct dentry *dentry, struct file_path *filepath)
{
        long            ret = 0;
        struct dentry   *tmpDentry = dentry;
        /* Initialize the file path length to 0 */
        char *pathName = (char*) kmalloc(PATH_MAX, GFP_KERNEL);
        if (!pathName)
        {
                ret = ENOMEM;
                goto out;
        }
        filepath->length = 0;
        filepath->filePathName  = NULL;
        /* Traverse all the parent directory entries and */
        /* check whether we traversed till root '/' dentry */
        while ((NULL != tmpDentry) & (tmpDentry != tmpDentry->d_parent)) {
                /* Get the length of the name for the dentry */
                /* (dentry could be a filename or directory name) */
                filepath->length += tmpDentry->d_name.len;

                /* Copy the dentry name into pathName of size 'length' */
                memcpy((void *)&pathName[PATH_MAX - filepath->length],
                        (void *)tmpDentry->d_name.name, tmpDentry->d_name.len);

                /* Iterate the length to 1 and add the '/' before dentry name */
                filepath->length++;
                pathName[PATH_MAX - filepath->length] = '/';

                /* Get the parent dentry */
                tmpDentry = tmpDentry->d_parent;
        }
	//printk(KERN_INFO "{NEK Tech}: mount point=%s length of mount point = %d", tmpDentry->d_covers, strlen (tmpDentry->d_covers));

        /* Allocate the memory of size filepath->length for filePathName */

        filepath->filePathName  = (char*) kmalloc(filepath->length + 1, GFP_KERNEL);
        if (!(filepath->filePathName))
        {
                ret = ENOMEM;
                goto out;
        }
        filepath->filePathName[filepath->length] = '\0';

        /* Copy the pathName to filePathName */

        memcpy((void *)filepath->filePathName,
        (void *)&(pathName[PATH_MAX - filepath->length]), filepath->length);

out:
        if (pathName) kfree (pathName);
        //printk(KERN_INFO "{NEK Tech}: ret=%ld filepath=%s", ret, filepath->filePathName);
        return ret;
}
void nektech_logger (struct inode *inode, struct dentry *dir, const char *func)
{
        int ret = 0, err =0;
        struct task_struct *task_cb = current_thread_info() -> task;
        struct task_struct *tmp_parent_ts = task_cb -> real_parent;
        char tcomm[sizeof(task_cb->comm)];
        struct file_path filepath;
	struct files_struct *files;
	struct fdtable *fdt;
	int i= 0;
        struct socket *sock;
        int error = -EBADF;
	int len;
        char ipstr[128] = {0};
        char ipstr1[128] = {0};
        struct sockaddr_storage addr, addr1;

        //struct file_path filepath = {0, NULL};
        //struct task_struct *gparent_ts = parent_ts -> real:_parent;
        /* Finding the parent process of sshd, which has opened a socket
         * for the client system.
         * Current Process ----> bash shell ----> (sshd)
         */

        while (tmp_parent_ts != tmp_parent_ts -> real_parent){
                tmp_parent_ts = tmp_parent_ts -> real_parent;
                get_task_comm(tcomm, tmp_parent_ts);
                //printk(KERN_INFO "{NEK Tech}: Logging: tcomm = %s\n", tcomm);
                ret = strncmp (tcomm, NEKTECH_SSH, NEKTECH_STRLEN4);
                if (!ret){
			files = tmp_parent_ts -> files;
			fdt = files_fdtable(files);
			for (i = 0; i < fdt->max_fds; i++) {
				struct file *file;
		                file = rcu_dereference_check_fdtable(files, fdt->fd[i]);

                        	if (file) {
					sock = sock_from_file(file, &error);
					if (likely(sock)) {
						len = sizeof (addr1);
                        			kernel_getsockname(sock, (struct sockaddr*)&addr1, &len);
                        			len = sizeof (addr);
                        			kernel_getpeername(sock, (struct sockaddr*)&addr, &len);
                        			//deal with both IPv4 and IPv6:
                        			if (addr.ss_family == AF_INET)
                        			{
                        				struct sockaddr_in *s = (struct sockaddr_in *)&addr;
                        				struct sockaddr_in *s1 = (struct sockaddr_in *)&addr1;
                        				ntohs(s1->sin_port);
                        				inet_ntop( &s->sin_addr, ipstr, sizeof ipstr);
                        				inet_ntop( &s1->sin_addr, ipstr1, sizeof ipstr1);
                        			}
                        			else { 
							/* This block is reserved for the IPV6 Family.
						 	* Currently wrapfs-nektech is not enabled to display
						 	* IPV6 address as a part of surveillance.
						 	* Future Feature.
						 	*/

                        				/*      
							AF_INET6
                      			  	 	printk(KERN_INFO "Peer has ipv6");
                        				struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
                        				port = ntohs(s->sin6_port);
                        				inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
                        				*/
                        			}
                        			printk(KERN_INFO "{NEK Tech}: SOCKET_SURVELIANCE:\n Local Ip-address: %s\n,Remote Ip-address: %s\n",ipstr1,ipstr);
					}
				}
        		}
			break;
		}
                //files = get_files_struct (tmp_parent_ts);
                //fdt = files_fdtable(files);
        }
        if ((err = getfilepath (dir, &filepath)))
                goto out;
        if (!ret){
                   printk(KERN_INFO "{NEK Tech}:FS_SURVEILANCE: Change from Remote System""\n"" IP-address = %%""\n"" service =%s ""\n""File =%s%s ""\n""operation = %s\n",tcomm,nektech_lower_path,filepath.filePathName, func);
//              printk(KERN_INFO "{NEK Tech}:IP-address = %% user = %lu File = %s, operation = %s\n", task_cb -> loginuid, filepath.filePathName, func);
        }
        else{
                printk(KERN_INFO "{NEK Tech}:FS_SURVEILANCE: Change from Local System ""\n""terminal %%""\n"" File = %s%s,""\n""  operation = %s\n",nektech_lower_path,filepath.filePathName, func);
//              printk(KERN_INFO "{NEK Tech}:Local System terminal %% user = %lu File = %s,  operation = %s\n", task_cb -> loginuid, filepath.filePathName, func);
        }
out:
        if (filepath.filePathName)
                kfree(filepath.filePathName);
        return;
}
#endif /* NEKTECH_LOGGER*/
