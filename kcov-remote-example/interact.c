#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/types.h>

struct kcov_remote_arg {
    __u32           trace_mode;
    __u32           area_size;
    __u32           num_handles;
    __aligned_u64   common_handle;
    __aligned_u64   handles[0];
};

struct remote_arg {
    unsigned long size;
    unsigned char* buf;
};

#define KCOV_INIT_TRACE                	_IOR('c', 1, unsigned long)
#define KCOV_DISABLE               	_IO('c', 101)
#define KCOV_REMOTE_ENABLE          	_IOW('c', 102, struct kcov_remote_arg)

#define COVER_SIZE  (64 << 10)

#define KCOV_TRACE_PC       0

#define KCOV_SUBSYSTEM_COMMON       (0x00ull << 56)

#define KCOV_SUBSYSTEM_MASK (0xffull << 56)
#define KCOV_INSTANCE_MASK  (0xffffffffull)

static inline __u64 kcov_remote_handle(__u64 subsys, __u64 inst)
{
    if (subsys & ~KCOV_SUBSYSTEM_MASK || inst & ~KCOV_INSTANCE_MASK)
            return 0;
    return subsys | inst;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
 	printf("[!] Please input a string\n");
	exit(1);
    }

    int fd;
    unsigned long *cover, n, i;
    struct kcov_remote_arg *arg;

    fd = open("/sys/kernel/debug/kcov", O_RDWR);
    if (fd == -1)
            perror("open"), exit(1);
    if (ioctl(fd, KCOV_INIT_TRACE, COVER_SIZE))
            perror("ioctl"), exit(1);
    cover = (unsigned long*)mmap(NULL, COVER_SIZE * sizeof(unsigned long),
                                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if ((void*)cover == MAP_FAILED)
            perror("mmap"), exit(1);

    /* Enable coverage collection via common handle and from USB bus #1. */
    arg = calloc(1, sizeof(*arg) + sizeof(uint64_t));
    if (!arg)
            perror("calloc"), exit(1);

    arg->trace_mode = KCOV_TRACE_PC;
    arg->area_size = COVER_SIZE;
    arg->num_handles = 0;
    arg->common_handle = kcov_remote_handle(KCOV_SUBSYSTEM_COMMON,
                                                    0x42);

    if (ioctl(fd, KCOV_REMOTE_ENABLE, arg))
            perror("ioctl"), free(arg), exit(1);
    free(arg);

    /*
     * Here the user needs to trigger execution of a kernel code section
     * that is either annotated with the common handle, or to trigger some
     * activity on USB bus #1.
     */
    int rfd = open("/dev/remote_lkm", O_RDONLY);

    if (rfd < 0) 
	perror("open"), exit(1);
    
    struct remote_arg _arg;
    char   stackbuf[8] = "";
    strcpy(stackbuf, argv[1]);
    _arg.size = 8;
    _arg.buf  = stackbuf;
    if (ioctl(rfd, 0xdead, &_arg)) 
	 perror("ioctl"), exit(1);	
    
    close(rfd);

    n = __atomic_load_n(&cover[0], __ATOMIC_RELAXED);
    for (i = 0; i < n; i++)
            printf("0x%lx\n", cover[i + 1]);
    if (ioctl(fd, KCOV_DISABLE, 0))
            perror("ioctl"), exit(1);
    if (munmap(cover, COVER_SIZE * sizeof(unsigned long)))
            perror("munmap"), exit(1);
    if (close(fd))
            perror("close"), exit(1);
    return 0;
}
