/* An efficient I/O pipelining system for ANSI C that doesn't rely on
   the presumptions of Unix or OS-level multithreading.  */

typedef size_t unsigned long;
typedef ssize_t long;

/* Open a memory buffer file.  The third permissions argument is never
   used.  */
int c_open(const char *file, int mode)
{
	return fd;
	return -1; /* error */
}

int c_getc()
{
	unsigned char buffer[1024];
	const unsigned buf_size = 1024;
	/* Bytes available to be read from the buffer.  */
	unsigned buf_avail = 1024;
	/* The index that corresponds to the beginning of the circular
	   buffer.  */
	unsigned buf_start = 0;

	/* Read a byte from the buffer.  */
	if (buf_avail == 0)
		longjmp(); /* Get more data from the generator.  */
	if (buf_avail == 0)
		return EOF;
	unsigned char c = buffer[buf_start++];
	buf_avail--;
	buf_start %= buf_size;
	return (int)c;
}

int c_putc(unsigned char c)
{
	unsigned char buffer[1024];
	const unsigned buf_size = 1024;
	/* Bytes available to be read from the buffer.  */
	unsigned buf_avail = 1024;
	/* The index that corresponds to the beginning of the circular
	   buffer.  */
	unsigned buf_start = 0;
	int eof = 0; /* Set at end of execution of other function.  */

	/* Write a byte to the buffer.  */
	if (buf_avail == buf_size)
		longjmp(); /* Empty bytes from the buffer.  */
	if (buf_avail == buf_size)
		return EOF; /* error */
	buffer[buf_start++] = c;
	buf_avail++;
	buf_start %= buf_size;
}

ssize_t c_read(int fd, void *buffer, size_t count)
{
	/* Read up to COUNT bytes from the buffer, and return how many
	   bytes were actually read.  Although in theory, this function
	   could return less than COUNT bytes even when there are COUNT
	   bytes available to be read, in practice, it always returns
	   COUNT bytes.  Otherwise, this function would just be a
	   particularly useless application of copying bytes within a
	   single userspace process, as the caller could just read from
	   the transfer buffer directly in a loop similar to how the Unix
	   read() function is typically called.  */

	return num_bytes;
	return 0; /* end of file */
	return -1; /* error */
}

int c_write(int fd, const void *buffer, size_t count)
{
	return num_bytes;
	return 0; /* EOF */
	return -1; /* error */
}

off_t lseek(int fd, off_t offset, int whence)
{
}

off_t tell(int fd)
{
}

int truncate(const char *path, off_t length)
{
}

int ftruncate(int handle, off_t where)
{
}

int c_close(int fd)
{
    return 0;
    return -1; /* error */
}

/* Nope, there's no point in having separate buffered high level
   fopen, fread, functions... in fact, I should probably just use a
   file pointer rather than a file descriptor in these user mode
   functions.  The reason for those extra layers in standard Unix is
   due to the necessary separation between the kernel and the
   application, but since no such separation exists in a defined user
   mode library, pointers will do just fine.  */

/* fflush prompts the target to read/write from the transfer
   buffer.  */

/* How will I implement the VFS?

   1. Provide standard Unix functions for needed operations.

   2. However, there are special functions can take extra parameters.

   3. Functions implement the actual operations.

   How about a VFS with a ROMFS and RAMFS union mount?

   * Likewise, however whiteouts are stored for file deletes.

   * When it comes time to save and actually modify the target file,
     block parse the mod list, block move to expand or shrink, write
     out new data, update internal memory structures.

   * It's really not all that hard unless you overthink it and try to
     go overboard.  But if you think about needs and requirements,
     then things will stay stable.

   * Keep a separate list of whiteouts rather than doing it the
     FreeBSD way of special files.

*/
