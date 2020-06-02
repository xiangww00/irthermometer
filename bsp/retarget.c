


extern int __io_putchar(int ch);

#define UNUSED(x) ((void)(x))

void retarget_init()
{
    // Initialize UART
}

int _write(int fd, char *ptr, int len)
{

    UNUSED(fd);
    int i = 0;
    while (*ptr && (i < len)) {
        __io_putchar(*ptr);
        if (*ptr == '\n') {
            __io_putchar('\r');
        }
        i++;
        ptr++;
    }
    return i;
}

int _read(int fd, char *ptr, int len)
{
    UNUSED(fd);
    UNUSED(ptr);

    /* Read "len" of char to "ptr" from file id "fd"
     * Return number of char read.
     * Need implementing with UART here. */
    return len;
}

void _ttywrch(int ch)
{
    __io_putchar(ch);
    return;
}


