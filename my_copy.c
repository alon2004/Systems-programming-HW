#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stddef.h>

#define BUF_SIZE 8192

static size_t my_strlen(const char *s) {
    size_t n = 0;
    while (s && s[n] != '\0') n++;
    return n;
}

static int my_streq(const char *a, const char *b) {
    size_t i = 0;
    if (!a || !b) return 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static void write_all(int fd, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, buf + off, len - off);
        if (w <= 0) return;
        off += (size_t)w;
    }
}

static void write_str(int fd, const char *s) {
    write_all(fd, s, my_strlen(s));
}

static void write_uint(int fd, unsigned int x) {
    char tmp[16];
    int i = 0;
    if (x == 0) {
        write_all(fd, "0", 1);
        return;
    }
    while (x > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = (char)('0' + (x % 10));
        x /= 10;
    }
    while (i > 0) {
        i--;
        write_all(fd, &tmp[i], 1);
    }
}

static void err_msg(const char *op, const char *path) {
    write_str(STDERR_FILENO, "Error: ");
    write_str(STDERR_FILENO, op);
    if (path) {
        write_str(STDERR_FILENO, " (");
        write_str(STDERR_FILENO, path);
        write_str(STDERR_FILENO, ")");
    }
    write_str(STDERR_FILENO, ". errno=");
    write_uint(STDERR_FILENO, (unsigned int)errno);
    write_str(STDERR_FILENO, "\n");
}

static int prompt_overwrite(const char *dst) {
    for (;;) {
        write_str(STDOUT_FILENO, "קובץ היעד '");
        write_str(STDOUT_FILENO, dst);
        write_str(STDOUT_FILENO, "' כבר קיים. העתקה תמחק את תוכנו.\nהאם ברצונך להמשיך? (n/y)\n");

        char c = 0;
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r <= 0) {
            return 0;
        }

        char dump = 0;
        while (c != '\n') {
            ssize_t rr = read(STDIN_FILENO, &dump, 1);
            if (rr <= 0) break;
            if (dump == '\n') break;
        }

        if (c == 'y' || c == 'Y') return 1;
        if (c == 'n' || c == 'N') return 0;
    }
}

static int copy_stream(int src_fd, int dst_fd) {
    char buf[BUF_SIZE];

    for (;;) {
        ssize_t rd = read(src_fd, buf, (size_t)BUF_SIZE);
        if (rd == 0) return 0;
        if (rd < 0) return -1;

        size_t off = 0;
        while (off < (size_t)rd) {
            ssize_t wr = write(dst_fd, buf + off, (size_t)rd - off);
            if (wr < 0) return -2;
            off += (size_t)wr;
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        write_str(STDERR_FILENO, "Usage: c.copy_my <source_file> <destination_file>\n");
        return 1;
    }

    const char *src_path = argv[1];
    const char *dst_path = argv[2];

    if (my_streq(src_path, dst_path)) {
        write_str(STDERR_FILENO, "Error: source and destination are the same path\n");
        return 1;
    }

    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        err_msg("open source failed", src_path);
        return 1;
    }

    struct stat st;
    if (fstat(src_fd, &st) < 0) {
        err_msg("fstat source failed", src_path);
        close(src_fd);
        return 1;
    }

    int dst_exists = 0;
    if (access(dst_path, F_OK) == 0) dst_exists = 1;

    if (dst_exists) {
        int ok = prompt_overwrite(dst_path);
        if (!ok) {
            write_str(STDOUT_FILENO, "ההעתקה בוטלה לבקשת המשתמש.\n");
            close(src_fd);
            return 0;
        }
    }

    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, (mode_t)(st.st_mode & 0777));
    if (dst_fd < 0) {
        err_msg("open destination failed", dst_path);
        close(src_fd);
        return 1;
    }

    int rc = copy_stream(src_fd, dst_fd);
    if (rc != 0) {
        if (rc == -1) err_msg("read failed", src_path);
        if (rc == -2) err_msg("write failed", dst_path);
        close(dst_fd);
        close(src_fd);
        return 1;
    }

    if (close(dst_fd) < 0) {
        err_msg("close destination failed", dst_path);
        close(src_fd);
        return 1;
    }
    if (close(src_fd) < 0) {
        err_msg("close source failed", src_path);
        return 1;
    }

    return 0;
}