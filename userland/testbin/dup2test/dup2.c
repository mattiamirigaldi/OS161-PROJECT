#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main(void)
{
  //const char *file = "logfile";
	//int logfd;
	int rv;
        int fd ;
	int new_fd = 5;
	const char *f_name = "dup";
	static char writebuf[41] = "Twiddle dee dee, Twiddle dum dum.......\n";
	static char readbuf[41];

	fd = open(f_name, O_RDWR|O_CREAT|O_TRUNC, 0666);	
	if (fd<0) {
		err(1, "%s: open for write", f_name);
	}
	rv = write(fd, writebuf, 40);
	if (rv<0) {
		err(1, "%s: write", f_name);
	}
	printf("Written to file with fd = %d :", fd);
	printf("%s", writebuf);
	rv = dup2(fd, new_fd);
	if (rv<0) {
		err(1, "%s: ", f_name);
	}
	rv = close(fd);
	if (rv<0) {
		err(1, "%s: close (2nd time)", f_name);
	}
	printf("After dup2 even if different file descriptors they point to same file\nfd = %d, new_fd = %d\n",fd, new_fd);
	lseek(new_fd, 0, SEEK_SET);
	rv = read(new_fd, readbuf, 40);
	if (rv<0) {
		err(1, "%s: read", f_name);
	}
	printf("Read from file with fd = %d: \n%s \n", new_fd, readbuf);
	rv = close(new_fd);
	if (rv<0) {
		err(1, "%s: close (2nd time)", f_name);
	}
	//printf("Testing dup2 with STDOUT\n");
	//logfd = open(file, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	//if (logfd<0) {
	//	err(1, "%s: open for write", file);
	//}
	//
	//dup2(logfd, STDOUT_FILENO);
	//close(logfd);
	//
	//printf("Hello there\n");
	//
	//rv = read(STDOUT_FILENO, readbuf, 12);
	///* ensure null termination */
	//readbuf[12] = 0;
	//if (rv<0) {
	//	err(1, "%s: read", file);
	//}
	//printf("read %s \n", readbuf);
	//printf("Passed dup2 test.\n");
	return 0;
}
