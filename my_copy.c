#include <unistd.h>     // read, write, close
#include <fcntl.h>      // open, flags כמו O_RDONLY, O_CREAT
#include <sys/stat.h>   // fstat, struct stat
#include <errno.h>      // errno עבור קוד שגיאה אחרון
#include <stddef.h>     // size_t

#define BUF_SIZE 8192   // גודל הבאפר להעתקה במקטעים

/*
  my_strlen
  מחשב אורך של מחרוזת C רגילה עד תו סיום \0
  לא משתמשים ב strlen מספריית C הסטנדרטית
*/
static size_t my_strlen(const char *s) {
    size_t n = 0;
    while (s && s[n] != '\0') n++;
    return n;
}

/*
  my_streq
  בדיקה האם שתי מחרוזות זהות תו תו
  שימושי כדי לזהות מצב שבו source ו destination הם אותו נתיב
*/
static int my_streq(const char *a, const char *b) {
    size_t i = 0;
    if (!a || !b) return 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

/*
  write_all
  כתיבה בטוחה שמבטיחה לכתוב את כל len הבתים
  חשוב כי write יכול לכתוב פחות מהמבוקש ואז צריך להמשיך לכתוב את השאר
*/
static void write_all(int fd, const char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, buf + off, len - off);
        if (w <= 0) return;  // במקרה כשל או עצירה, יוצאים
        off += (size_t)w;
    }
}

/*
  write_str
  כתיבה של מחרוזת ל fd נתון
  משתמש ב write_all כדי לוודא כתיבה מלאה
*/
static void write_str(int fd, const char *s) {
    write_all(fd, s, my_strlen(s));
}

/*
  write_uint
  הדפסה של מספר לא שלילי ל fd
  ממיר ידנית למחרוזת ספרות ואז כותב עם write_all
*/
static void write_uint(int fd, unsigned int x) {
    char tmp[16];
    int i = 0;

    if (x == 0) {
        write_all(fd, "0", 1);
        return;
    }

    // בונים את הספרות בסדר הפוך
    while (x > 0 && i < (int)sizeof(tmp)) {
        tmp[i++] = (char)('0' + (x % 10));
        x /= 10;
    }

    // כותבים את הספרות בסדר הנכון
    while (i > 0) {
        i--;
        write_all(fd, &tmp[i], 1);
    }
}

/*
  err_msg
  הדפסת הודעת שגיאה ל stderr
  מציג טקסט כללי, את הנתיב אם יש, ואת errno
  לא משתמשים ב perror או fprintf
*/
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

/*
  prompt_overwrite
  אם קובץ היעד כבר קיים, חובה לשאול את המשתמש האם להמשיך
  מחזיר 1 אם המשתמש אישר y
  מחזיר 0 אם המשתמש ביטל n או אם קריאת קלט נכשלה
  כל תו אחר גורם לשאלה להופיע שוב
*/
static int prompt_overwrite(const char *dst) {
    for (;;) {
        write_str(STDOUT_FILENO, "קובץ היעד '");
        write_str(STDOUT_FILENO, dst);
        write_str(STDOUT_FILENO, "' כבר קיים. העתקה תמחק את תוכנו.\nהאם ברצונך להמשיך? (n/y)\n");

        // קוראים תו ראשון בלבד
        char c = 0;
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r <= 0) {
            return 0;
        }

        /*
          ניקוי שאר השורה עד Enter
          כדי שאם המשתמש כתב יותר מתו אחד,
          לא ישארו תווים בבאפר שישפיעו על הקריאה הבאה
        */
        char dump = 0;
        while (c != '\n') {
            ssize_t rr = read(STDIN_FILENO, &dump, 1);
            if (rr <= 0) break;
            if (dump == '\n') break;
        }

        if (c == 'y' || c == 'Y') return 1;
        if (c == 'n' || c == 'N') return 0;

        // כל דבר אחר, ממשיכים לסיבוב נוסף ושואלים שוב
    }
}

/*
  copy_stream
  מעתיק את כל תוכן src_fd אל dst_fd בעזרת באפר קבוע
  לולאה חיצונית קוראת מקטעים עם read
  לולאה פנימית כותבת את כל המקטע עם write גם אם write כתב חלקית
  מחזיר 0 בהצלחה
  מחזיר מינוס 1 אם read נכשל
  מחזיר מינוס 2 אם write נכשל
*/
static int copy_stream(int src_fd, int dst_fd) {
    char buf[BUF_SIZE];

    for (;;) {
        // קריאה של עד BUF_SIZE בתים
        ssize_t rd = read(src_fd, buf, (size_t)BUF_SIZE);

        if (rd == 0) return 0;   // סוף קובץ
        if (rd < 0) return -1;   // שגיאת read

        // כתיבה של כל מה שנקרא, כולל טיפול ב partial write
        size_t off = 0;
        while (off < (size_t)rd) {
            ssize_t wr = write(dst_fd, buf + off, (size_t)rd - off);
            if (wr < 0) return -2;   // שגיאת write
            off += (size_t)wr;
        }
    }
}

int main(int argc, char **argv) {

    /*
      אימות מספר ארגומנטים
      argc כולל גם את שם התוכנית, לכן צריך argc להיות 3
    */
    if (argc != 3) {
        write_str(STDERR_FILENO, "Usage: c.copy_my <source_file> <destination_file>\n");
        return 1;
    }

    const char *src_path = argv[1];
    const char *dst_path = argv[2];

    // מניעת מצב שבו מנסים להעתיק קובץ לעצמו
    if (my_streq(src_path, dst_path)) {
        write_str(STDERR_FILENO, "Error: source and destination are the same path\n");
        return 1;
    }

    /*
      פתיחת קובץ המקור לקריאה בלבד
      אם נכשל, אין טעם להמשיך
    */
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        err_msg("open source failed", src_path);
        return 1;
    }

    /*
      קריאת מאפייני קובץ המקור
      משתמשים בזה כדי להעתיק הרשאות לקובץ היעד במקרה של יצירה
    */
    struct stat st;
    if (fstat(src_fd, &st) < 0) {
        err_msg("fstat source failed", src_path);
        close(src_fd);
        return 1;
    }

    /*
      בדיקת קיום קובץ יעד
      access עם F_OK מחזיר 0 אם הקובץ קיים
    */
    int dst_exists = 0;
    if (access(dst_path, F_OK) == 0) dst_exists = 1;

    /*
      אם היעד קיים, שואלים את המשתמש האם לבצע overwrite
      אם המשתמש בחר n, יוצאים עם הודעת ביטול
    */
    if (dst_exists) {
        int ok = prompt_overwrite(dst_path);
        if (!ok) {
            write_str(STDOUT_FILENO, "ההעתקה בוטלה לבקשת המשתמש.\n");
            close(src_fd);
            return 0;
        }
    }

    /*
      פתיחת קובץ יעד
      O_WRONLY כתיבה בלבד
      O_CREAT יצירה אם לא קיים
      O_TRUNC מחיקת תוכן קודם אם קיים
      הרשאות נוצרות לפי הרשאות המקור
    */
    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, (mode_t)(st.st_mode & 0777));
    if (dst_fd < 0) {
        err_msg("open destination failed", dst_path);
        close(src_fd);
        return 1;
    }

    // העתקת הנתונים בפועל
    int rc = copy_stream(src_fd, dst_fd);
    if (rc != 0) {
        if (rc == -1) err_msg("read failed", src_path);
        if (rc == -2) err_msg("write failed", dst_path);
        close(dst_fd);
        close(src_fd);
        return 1;
    }

    /*
      סגירת הקבצים
      גם close יכולה להיכשל ולכן בודקים
    */
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
