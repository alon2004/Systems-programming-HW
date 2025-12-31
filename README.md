README

שם התוכנית
c.copy_my

מטרה
התוכנית מעתיקה את תוכן קובץ המקור לקובץ היעד תוך שימוש בקריאות מערכת של Linux בלבד.
אם קובץ היעד כבר קיים, התוכנית תשאל האם לבצע Overwrite.

קבצים בפרויקט
1. c.copy_my.c  קוד המקור
2. c.copy_my    קובץ ההרצה שייווצר לאחר קומפילציה

דרישות
1. סביבת Linux או WSL
2. קומפיילר gcc

קומפילציה
פתח טרמינל בתיקיית הקבצים והריץ

gcc c.copy_my.c -o c.copy_my

הערה
הדגל o יוצר קובץ הרצה בשם c.copy_my כפי שנדרש.

הרצה
הסינטקס

./c.copy_my <source_file> <destination_file>

דוגמאות

./c.copy_my input.txt output.txt
./c.copy_my /home/user/a.bin /home/user/b.bin

התנהגות במקרה שקובץ היעד קיים
אם קובץ היעד כבר קיים, תופיע הודעה ושאלה
האם ברצונך להמשיך? (n/y)

1. אם מכניסים n התוכנית תדפיס
ההעתקה בוטלה לבקשת המשתמש.
ותסתיים ללא שינוי הקובץ

2. אם מכניסים y התוכנית תבצע Overwrite ותעתיק מחדש

3. כל קלט אחר יגרום לשאלה להופיע שוב עד שמתקבל y או n

קודי יציאה
0 הצלחה או ביטול לפי בחירת המשתמש
1 שגיאה כללית כמו ארגומנטים לא תקינים או כשל בקריאת מערכת

טיפים לבדיקה
1. בדיקה מהירה שההעתקה הצליחה
diff <source_file> <destination_file>

2. בדיקה עם קובץ גדול כדי לראות שזה עובד עם Buffer
dd if=/dev/urandom of=bigfile.bin bs=1M count=5
./c.copy_my bigfile.bin bigfile_copy.bin
diff bigfile.bin bigfile_copy.bin
