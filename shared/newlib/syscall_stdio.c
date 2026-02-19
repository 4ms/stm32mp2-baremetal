int _open(const char *filename, int flags, int mode)
{
	return -1;
}

int _close(int fd)
{
	return -1;
}

int _lseek(int fd, int offset, int whenc)
{
	return -1;
}

int _read(int fd, char *ptr, int len)
{
	return -1;
}

struct stat;

int _fstat(int fd, struct stat *st)
{
	return -1;
}

int _isatty(int fd)
{
	return -1;
}

int _stat(const char *filename, struct stat *st)
{
	return -1;
}

extern void putchar_s(char);

int _write(int fd, const char *ptr, int len)
{
	int l = len;
	// Do we stop at null char if len != 0?
	// while (*ptr && l) {
	while (l) {
		l--;
		putchar_s(*ptr++);
	}
	return len - l;
}
