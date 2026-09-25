#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <math.h>

// --- Pseudoc Runtime Core & Dynamic Memory Tracker ---
typedef struct PC_Node { void* ptr; struct PC_Node* next; } PC_Node;
static PC_Node* pc_gc_head = NULL;
static void* pc_track(void* p) {
    if (!p) return NULL;
    PC_Node* n = (PC_Node*)malloc(sizeof(PC_Node));
    n->ptr = p; n->next = pc_gc_head; pc_gc_head = n;
    return p;
}
// --- File I/O Runtime Tracker ---
typedef struct PC_File { char name[256]; FILE* fp; struct PC_File* next; } PC_File;
static PC_File* pc_files_head = NULL;
// --- Random File I/O Runtime ---
typedef struct PC_RandomFile {
    char name[256];
    char** lines;
    size_t count;
    size_t capacity;
    int64_t current_record;
    struct PC_RandomFile* next;
} PC_RandomFile;
static PC_RandomFile* pc_random_head = NULL;
static void pc_open_random(const char* name) {
    PC_RandomFile* rf = (PC_RandomFile*)calloc(1, sizeof(PC_RandomFile));
    strncpy(rf->name, name, sizeof(rf->name) - 1);
    rf->current_record = 1;
    rf->capacity = 16;
    rf->lines = (char**)malloc(rf->capacity * sizeof(char*));
    rf->count = 0;
    FILE* fp = fopen(name, "r");
    if (fp) {
        char buf[4096];
        while (fgets(buf, sizeof(buf), fp)) {
            size_t len = strlen(buf);
            while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = '\0';
            if (rf->count >= rf->capacity) {
                rf->capacity *= 2;
                rf->lines = (char**)realloc(rf->lines, rf->capacity * sizeof(char*));
            }
            rf->lines[rf->count++] = strdup(buf);
        }
        fclose(fp);
    }
    rf->next = pc_random_head;
    pc_random_head = rf;
}
static PC_RandomFile* pc_get_random(const char* name) {
    PC_RandomFile* cur = pc_random_head;
    while (cur) {
        if (strcmp(cur->name, name) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}
static void pc_seek_random(const char* name, int64_t addr) {
    PC_RandomFile* rf = pc_get_random(name);
    if (!rf) { fprintf(stderr, "Runtime Error: File '%s' not open for RANDOM\n", name); exit(1); }
    rf->current_record = addr < 1 ? 1 : addr;
}
static void pc_put_record_line(const char* name, const char* str) {
    PC_RandomFile* rf = pc_get_random(name);
    if (!rf) { fprintf(stderr, "Runtime Error: File '%s' not open for RANDOM\n", name); exit(1); }
    size_t target_idx = (size_t)((rf->current_record < 1 ? 1 : rf->current_record) - 1);
    while (rf->count <= target_idx) {
        if (rf->count >= rf->capacity) {
            rf->capacity = (rf->capacity == 0 ? 16 : rf->capacity * 2);
            rf->lines = (char**)realloc(rf->lines, rf->capacity * sizeof(char*));
        }
        rf->lines[rf->count++] = strdup("");
    }
    free(rf->lines[target_idx]);
    rf->lines[target_idx] = strdup(str);
    rf->current_record++;
}
static char* pc_get_record_line(const char* name) {
    PC_RandomFile* rf = pc_get_random(name);
    if (!rf) { fprintf(stderr, "Runtime Error: File '%s' not open for RANDOM\n", name); exit(1); }
    size_t target_idx = (size_t)((rf->current_record < 1 ? 1 : rf->current_record) - 1);
    char* res = "";
    if (target_idx < rf->count) {
        res = rf->lines[target_idx];
    }
    rf->current_record++;
    return res;
}
static void pc_close_random(const char* name) {
    PC_RandomFile** cur = &pc_random_head;
    while (*cur) {
        if (strcmp((*cur)->name, name) == 0) {
            PC_RandomFile* to_del = *cur;
            FILE* fp = fopen(to_del->name, "w");
            if (fp) {
                for (size_t i = 0; i < to_del->count; ++i) {
                    fprintf(fp, "%s\n", to_del->lines[i]);
                    free(to_del->lines[i]);
                }
                fclose(fp);
            }
            free(to_del->lines);
            *cur = to_del->next;
            free(to_del);
            return;
        }
        cur = &((*cur)->next);
    }
}
static char* pc_next_record_field(char** cursor) {
    if (!cursor || !*cursor) return "";
    char* start = *cursor;
    char* bar = strchr(start, '|');
    if (bar) {
        *bar = '\0';
        *cursor = bar + 1;
    } else {
        *cursor = NULL;
    }
    return start;
}
static void pc_open_file(const char* name, const char* mode) {
    if (strcmp(mode, "RANDOM") == 0) { pc_open_random(name); return; }
    const char* m = "r";
    if (strcmp(mode, "WRITE") == 0) m = "w";
    else if (strcmp(mode, "APPEND") == 0) m = "a";
    FILE* fp = fopen(name, m);
    if (!fp) { fprintf(stderr, "Runtime Error: Cannot open file '%s' for %s\n", name, mode); exit(1); }
    PC_File* f = (PC_File*)malloc(sizeof(PC_File));
    strncpy(f->name, name, sizeof(f->name) - 1); f->name[sizeof(f->name) - 1] = '\0';
    f->fp = fp; f->next = pc_files_head; pc_files_head = f;
}
static FILE* pc_get_file(const char* name) {
    PC_File* cur = pc_files_head;
    while (cur) {
        if (strcmp(cur->name, name) == 0) return cur->fp;
        cur = cur->next;
    }
    fprintf(stderr, "Runtime Error: File '%s' is not open\n", name); exit(1);
    return NULL;
}
static void pc_close_file(const char* name) {
    if (pc_get_random(name)) { pc_close_random(name); return; }
    PC_File** cur = &pc_files_head;
    while (*cur) {
        if (strcmp((*cur)->name, name) == 0) {
            PC_File* to_del = *cur;
            fclose(to_del->fp);
            *cur = to_del->next;
            free(to_del);
            return;
        }
        cur = &((*cur)->next);
    }
}
static char* pc_read_file_line(const char* name) {
    FILE* fp = pc_get_file(name);
    static char fbuf[4096];
    if (!fgets(fbuf, sizeof(fbuf), fp)) fbuf[0] = '\0';
    size_t len = strlen(fbuf);
    while (len > 0 && (fbuf[len - 1] == '\n' || fbuf[len - 1] == '\r')) fbuf[--len] = '\0';
    return (char*)pc_track(strdup(fbuf));
}
static void pc_write_file_line(const char* name, const char* str) {
    FILE* fp = pc_get_file(name);
    fprintf(fp, "%s\n", str);
}
static bool pc_eof(const char* name) {
    PC_RandomFile* rf = pc_get_random(name);
    if (rf) return (size_t)rf->current_record > rf->count;
    FILE* fp = pc_get_file(name);
    int c = fgetc(fp);
    if (c == EOF) return true;
    ungetc(c, fp);
    return false;
}
static void pc_cleanup(void) {
    while (pc_random_head) {
        pc_close_random(pc_random_head->name);
    }
    while (pc_files_head) {
        PC_File* next = pc_files_head->next;
        fclose(pc_files_head->fp);
        free(pc_files_head);
        pc_files_head = next;
    }
    while (pc_gc_head) {
        PC_Node* next = pc_gc_head->next;
        free(pc_gc_head->ptr);
        free(pc_gc_head);
        pc_gc_head = next;
    }
}
// --- Line-based Unified Input Runtime ---
static char pc_input_buf[4096];
static char* pc_read_line(void) {
    if (!fgets(pc_input_buf, sizeof(pc_input_buf), stdin)) {
        pc_input_buf[0] = '\0';
    } else {
        size_t len = strlen(pc_input_buf);
        while (len > 0 && (pc_input_buf[len - 1] == '\n' || pc_input_buf[len - 1] == '\r')) {
            pc_input_buf[--len] = '\0';
        }
    }
    return pc_input_buf;
}
static long long pc_read_int(void) {
    char* line = pc_read_line();
    char* end;
    long long val = strtoll(line, &end, 10);
    return (end == line) ? 0LL : val;
}
static double pc_read_real(void) {
    char* line = pc_read_line();
    char* end;
    double val = strtod(line, &end);
    return (end == line) ? 0.0 : val;
}
static bool pc_read_bool(void) {
    char* line = pc_read_line();
    while (*line && isspace((unsigned char)*line)) line++;
    if (strcasecmp(line, "TRUE") == 0 || strcmp(line, "1") == 0) return true;
    return false;
}
// --- Overflow-checked Integer Arithmetic ---
static inline long long pc_add(long long a, long long b) {
    long long res;
    if (__builtin_add_overflow(a, b, &res)) {
        fprintf(stderr, "Runtime Error: 64-bit integer addition overflow\n");
        pc_cleanup(); exit(1);
    }
    return res;
}
static inline long long pc_sub(long long a, long long b) {
    long long res;
    if (__builtin_sub_overflow(a, b, &res)) {
        fprintf(stderr, "Runtime Error: 64-bit integer subtraction overflow\n");
        pc_cleanup(); exit(1);
    }
    return res;
}
static inline long long pc_mul(long long a, long long b) {
    long long res;
    if (__builtin_mul_overflow(a, b, &res)) {
        fprintf(stderr, "Runtime Error: 64-bit integer multiplication overflow\n");
        pc_cleanup(); exit(1);
    }
    return res;
}
// --- Array Indexing, Bounds Checking & Mathematical Helpers ---
static inline void pc_bounds_check(long long val, long long low, long long high, const char* name) {
    if (val < low || val > high) {
        fprintf(stderr, "Runtime Error: Array index out of bounds on '%s': index %lld not in [%lld:%lld]\n", name, val, low, high);
        pc_cleanup(); exit(1);
    }
}
static inline long long pc_div(long long a, long long b) {
    if (b == 0) { fprintf(stderr, "Runtime Error: Division by zero\n"); pc_cleanup(); exit(1); }
    long long q = a / b, r = a % b;
    if ((r != 0) && ((r < 0) ^ (b < 0))) q--;
    return q;
}
static inline long long pc_mod(long long a, long long b) {
    if (b == 0) { fprintf(stderr, "Runtime Error: Modulo by zero\n"); pc_cleanup(); exit(1); }
    long long r = a % b;
    if ((r != 0) && ((r < 0) ^ (b < 0))) r += b;
    return r;
}
static char* pc_concat(const char* s1, const char* s2) {
    size_t l1 = strlen(s1), l2 = strlen(s2);
    char* res = (char*)malloc(l1 + l2 + 1);
    memcpy(res, s1, l1); memcpy(res + l1, s2, l2); res[l1 + l2] = '\0';
    return (char*)pc_track(res);
}
static char* pc_substring(const char* s, long long start, long long len) {
    long long slen = (long long)strlen(s);
    if (start < 1) start = 1;
    if (len < 0) len = 0;
    if (start > slen) return (char*)pc_track(strdup(""));
    if (start - 1 + len > slen) len = slen - (start - 1);
    char* sub = (char*)malloc(len + 1);
    memcpy(sub, s + (start - 1), len);
    sub[len] = '\0';
    return (char*)pc_track(sub);
}
static char* pc_ucase(const char* s) {
    size_t len = strlen(s);
    char* r = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; ++i) r[i] = toupper((unsigned char)s[i]);
    r[len] = '\0';
    return (char*)pc_track(r);
}
static char* pc_lcase(const char* s) {
    size_t len = strlen(s);
    char* r = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; ++i) r[i] = tolower((unsigned char)s[i]);
    r[len] = '\0';
    return (char*)pc_track(r);
}
static char* pc_num_to_str_int(long long n) {
    char buf[64]; snprintf(buf, sizeof(buf), "%lld", n);
    return (char*)pc_track(strdup(buf));
}
static char* pc_num_to_str_real(double d) {
    char buf[64]; snprintf(buf, sizeof(buf), "%.10g", d);
    return (char*)pc_track(strdup(buf));
}
static double pc_str_to_num(const char* s) { return atof(s); }
static char* pc_left(const char* s, long long len) {
    if (len <= 0) return (char*)pc_track(strdup(""));
    long long slen = (long long)strlen(s);
    if (len > slen) len = slen;
    char* sub = (char*)malloc(len + 1);
    memcpy(sub, s, len);
    sub[len] = '\0';
    return (char*)pc_track(sub);
}
static char* pc_right(const char* s, long long len) {
    if (len <= 0) return (char*)pc_track(strdup(""));
    long long slen = (long long)strlen(s);
    if (len > slen) len = slen;
    char* sub = (char*)malloc(len + 1);
    memcpy(sub, s + (slen - len), len);
    sub[len] = '\0';
    return (char*)pc_track(sub);
}
static char* pc_chr(long long code) {
    char* res = (char*)malloc(2);
    res[0] = (char)(code & 0xFF);
    res[1] = '\0';
    return (char*)pc_track(res);
}
static long long pc_asc(const char* s) {
    if (!s || s[0] == '\0') return 0;
    return (long long)(unsigned char)s[0];
}
static long long pc_int(double v) {
    return (long long)floor(v);
}
static double pc_round(double v, long long places) {
    double factor = pow(10.0, (double)places);
    return round(v * factor) / factor;
}
static inline double pc_rnd(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

typedef struct pc_type_Student {
    long long pc_ID;
    const char* pc_Name;
    long long pc_Marks[5];
    double pc_Average;
    const char* pc_Grade;
    bool pc_Passed;
} pc_type_Student;

static pc_type_Student pc_Students[10];
static long long pc_Count = 0;
static long long pc_i = 0;
static long long pc_j = 0;
static long long pc_k = 0;
static long long pc_Choice = 0;
static long long pc_SearchID = 0;
static bool pc_Found = false;
static long long pc_Total = 0;
static long long pc_Highest = 0;
static long long pc_Lowest = 0;
static long long pc_Temp = 0;
static const char* pc_TempName = "";
static double pc_TempAverage = 0.0;
static const char* pc_TempGrade = "";
static bool pc_TempPassed = false;
static double pc_AverageAll = 0.0;

void pc_DisplayStudent(pc_type_Student pc_StudentData);
long long pc_BinarySearch(const pc_type_Student* pc_Data_in, long long pc_Size, long long pc_TargetID);
void pc_SortStudents(pc_type_Student* pc_Data, long long pc_Size);
long long pc_RecursiveFactorial(long long pc_N);
void pc_SwapStudents(pc_type_Student* pc_A, pc_type_Student* pc_B);
bool pc_IsPrime(long long pc_N);
bool pc_HasPassed(const long long* pc_Marks_in);
void pc_CalculateStudentData(pc_type_Student* pc_StudentData);
const char* pc_CalculateGrade(double pc_Average);
double pc_CalculateAverage(const long long* pc_Marks_in);

double pc_CalculateAverage(const long long* pc_Marks_in) {
    const long long* pc_Marks = pc_Marks_in;
    long long pc_Sum = 0;
    long long pc_i = 0;
    pc_Sum = 0LL;
    {
        long long tmp_end_1 = 5LL;
        for (pc_i = 1LL; pc_i <= tmp_end_1; pc_i += 1) {
            pc_Sum = pc_add(pc_Sum, pc_Marks[(pc_i - 1)]);
        }
    }
    return ((double)pc_Sum / (double)5LL);
}

const char* pc_CalculateGrade(double pc_Average) {
    if ((pc_Average >= 80LL)) {
        return "A";
    } else {
        if ((pc_Average >= 70LL)) {
            return "B";
        } else {
            if ((pc_Average >= 60LL)) {
                return "C";
            } else {
                if ((pc_Average >= 50LL)) {
                    return "D";
                } else {
                    return "F";
                }
            }
        }
    }
}

bool pc_HasPassed(const long long* pc_Marks_in) {
    const long long* pc_Marks = pc_Marks_in;
    long long pc_i = 0;
    {
        long long tmp_end_2 = 5LL;
        for (pc_i = 1LL; pc_i <= tmp_end_2; pc_i += 1) {
            if ((pc_Marks[(pc_i - 1)] < 40LL)) {
                return false;
            }
        }
    }
    return true;
}

void pc_CalculateStudentData(pc_type_Student* pc_StudentData) {
    (*pc_StudentData).pc_Average = pc_CalculateAverage((*pc_StudentData).pc_Marks);
    (*pc_StudentData).pc_Grade = pc_CalculateGrade((*pc_StudentData).pc_Average);
    (*pc_StudentData).pc_Passed = pc_HasPassed((*pc_StudentData).pc_Marks);
}

void pc_SwapStudents(pc_type_Student* pc_A, pc_type_Student* pc_B) {
    pc_type_Student pc_TempStudent = {0};
    pc_TempStudent = (*pc_A);
    (*pc_A) = (*pc_B);
    (*pc_B) = pc_TempStudent;
}

void pc_SortStudents(pc_type_Student* pc_Data, long long pc_Size) {
    long long pc_i = 0;
    long long pc_j = 0;
    long long pc_MaxIndex = 0;
    {
        long long tmp_end_3 = pc_sub(pc_Size, 1LL);
        for (pc_i = 1LL; pc_i <= tmp_end_3; pc_i += 1) {
            pc_MaxIndex = pc_i;
            {
                long long tmp_end_4 = pc_Size;
                for (pc_j = pc_add(pc_i, 1LL); pc_j <= tmp_end_4; pc_j += 1) {
                    if ((pc_Data[(pc_j - 1)].pc_Average > pc_Data[(pc_MaxIndex - 1)].pc_Average)) {
                        pc_MaxIndex = pc_j;
                    } else {
                        if ((pc_Data[(pc_j - 1)].pc_Average == pc_Data[(pc_MaxIndex - 1)].pc_Average)) {
                            if ((pc_Data[(pc_j - 1)].pc_ID < pc_Data[(pc_MaxIndex - 1)].pc_ID)) {
                                pc_MaxIndex = pc_j;
                            }
                        }
                    }
                }
            }
            if ((pc_MaxIndex != pc_i)) {
                pc_SwapStudents(&pc_Data[(pc_i - 1)], &pc_Data[(pc_MaxIndex - 1)]);
            }
        }
    }
}

long long pc_BinarySearch(const pc_type_Student* pc_Data_in, long long pc_Size, long long pc_TargetID) {
    const pc_type_Student* pc_Data = pc_Data_in;
    long long pc_Low = 0;
    long long pc_High = 0;
    long long pc_Mid = 0;
    pc_Low = 1LL;
    pc_High = pc_Size;
    while ((pc_Low <= pc_High)) {
        pc_Mid = pc_div(pc_add(pc_Low, pc_High), 2LL);
        if ((pc_Data[(pc_Mid - 1)].pc_ID == pc_TargetID)) {
            return pc_Mid;
        } else {
            if ((pc_Data[(pc_Mid - 1)].pc_ID < pc_TargetID)) {
                pc_Low = pc_add(pc_Mid, 1LL);
            } else {
                pc_High = pc_sub(pc_Mid, 1LL);
            }
        }
    }
    return (-1LL);
}

long long pc_RecursiveFactorial(long long pc_N) {
    if ((pc_N <= 1LL)) {
        return 1LL;
    } else {
        return pc_mul(pc_N, pc_RecursiveFactorial(pc_sub(pc_N, 1LL)));
    }
}

bool pc_IsPrime(long long pc_N) {
    long long pc_i = 0;
    if ((pc_N < 2LL)) {
        return false;
    }
    {
        long long tmp_end_5 = pc_div(pc_N, 2LL);
        for (pc_i = 2LL; pc_i <= tmp_end_5; pc_i += 1) {
            if ((pc_mod(pc_N, pc_i) == 0LL)) {
                return false;
            }
        }
    }
    return true;
}

void pc_DisplayStudent(pc_type_Student pc_StudentData) {
    printf("%s\n", "-----------------------------");
    printf("%s%lld\n", "ID: ", pc_StudentData.pc_ID);
    printf("%s%s\n", "Name: ", pc_StudentData.pc_Name);
    printf("%s\n", "Marks:");
    {
        long long tmp_end_6 = 5LL;
        for (pc_k = 1LL; pc_k <= tmp_end_6; pc_k += 1) {
            printf("%s%lld%s%lld\n", "Subject ", pc_k, ": ", pc_StudentData.pc_Marks[(pc_k - 1)]);
        }
    }
    printf("%s%.10g\n", "Average: ", pc_StudentData.pc_Average);
    printf("%s%s\n", "Grade: ", pc_StudentData.pc_Grade);
    if ((pc_StudentData.pc_Passed == true)) {
        printf("%s\n", "Status: PASSED");
    } else {
        printf("%s\n", "Status: FAILED");
    }
}

int main(void) {
    pc_Count = 5LL;
    pc_Students[(pc_bounds_check(1LL, 1, 10, "Students"), (1LL - 1))].pc_ID = 104LL;
    pc_Students[(pc_bounds_check(1LL, 1, 10, "Students"), (1LL - 1))].pc_Name = "Alice";
    pc_Students[(pc_bounds_check(1LL, 1, 10, "Students"), (1LL - 1))].pc_Marks[(1LL - 1)] = 78LL;
    pc_Students[(pc_bounds_check(1LL, 1, 10, "Students"), (1LL - 1))].pc_Marks[(2LL - 1)] = 85LL;
    pc_Students[(pc_bounds_check(1LL, 1, 10, "Students"), (1LL - 1))].pc_Marks[(3LL - 1)] = 91LL;
    pc_Students[(pc_bounds_check(1LL, 1, 10, "Students"), (1LL - 1))].pc_Marks[(4LL - 1)] = 67LL;
    pc_Students[(pc_bounds_check(1LL, 1, 10, "Students"), (1LL - 1))].pc_Marks[(5LL - 1)] = 88LL;
    pc_Students[(pc_bounds_check(2LL, 1, 10, "Students"), (2LL - 1))].pc_ID = 101LL;
    pc_Students[(pc_bounds_check(2LL, 1, 10, "Students"), (2LL - 1))].pc_Name = "Bob";
    pc_Students[(pc_bounds_check(2LL, 1, 10, "Students"), (2LL - 1))].pc_Marks[(1LL - 1)] = 55LL;
    pc_Students[(pc_bounds_check(2LL, 1, 10, "Students"), (2LL - 1))].pc_Marks[(2LL - 1)] = 61LL;
    pc_Students[(pc_bounds_check(2LL, 1, 10, "Students"), (2LL - 1))].pc_Marks[(3LL - 1)] = 49LL;
    pc_Students[(pc_bounds_check(2LL, 1, 10, "Students"), (2LL - 1))].pc_Marks[(4LL - 1)] = 73LL;
    pc_Students[(pc_bounds_check(2LL, 1, 10, "Students"), (2LL - 1))].pc_Marks[(5LL - 1)] = 68LL;
    pc_Students[(pc_bounds_check(3LL, 1, 10, "Students"), (3LL - 1))].pc_ID = 109LL;
    pc_Students[(pc_bounds_check(3LL, 1, 10, "Students"), (3LL - 1))].pc_Name = "Charlie";
    pc_Students[(pc_bounds_check(3LL, 1, 10, "Students"), (3LL - 1))].pc_Marks[(1LL - 1)] = 92LL;
    pc_Students[(pc_bounds_check(3LL, 1, 10, "Students"), (3LL - 1))].pc_Marks[(2LL - 1)] = 95LL;
    pc_Students[(pc_bounds_check(3LL, 1, 10, "Students"), (3LL - 1))].pc_Marks[(3LL - 1)] = 89LL;
    pc_Students[(pc_bounds_check(3LL, 1, 10, "Students"), (3LL - 1))].pc_Marks[(4LL - 1)] = 94LL;
    pc_Students[(pc_bounds_check(3LL, 1, 10, "Students"), (3LL - 1))].pc_Marks[(5LL - 1)] = 90LL;
    pc_Students[(pc_bounds_check(4LL, 1, 10, "Students"), (4LL - 1))].pc_ID = 103LL;
    pc_Students[(pc_bounds_check(4LL, 1, 10, "Students"), (4LL - 1))].pc_Name = "Diana";
    pc_Students[(pc_bounds_check(4LL, 1, 10, "Students"), (4LL - 1))].pc_Marks[(1LL - 1)] = 35LL;
    pc_Students[(pc_bounds_check(4LL, 1, 10, "Students"), (4LL - 1))].pc_Marks[(2LL - 1)] = 76LL;
    pc_Students[(pc_bounds_check(4LL, 1, 10, "Students"), (4LL - 1))].pc_Marks[(3LL - 1)] = 82LL;
    pc_Students[(pc_bounds_check(4LL, 1, 10, "Students"), (4LL - 1))].pc_Marks[(4LL - 1)] = 64LL;
    pc_Students[(pc_bounds_check(4LL, 1, 10, "Students"), (4LL - 1))].pc_Marks[(5LL - 1)] = 71LL;
    pc_Students[(pc_bounds_check(5LL, 1, 10, "Students"), (5LL - 1))].pc_ID = 107LL;
    pc_Students[(pc_bounds_check(5LL, 1, 10, "Students"), (5LL - 1))].pc_Name = "Ethan";
    pc_Students[(pc_bounds_check(5LL, 1, 10, "Students"), (5LL - 1))].pc_Marks[(1LL - 1)] = 45LL;
    pc_Students[(pc_bounds_check(5LL, 1, 10, "Students"), (5LL - 1))].pc_Marks[(2LL - 1)] = 51LL;
    pc_Students[(pc_bounds_check(5LL, 1, 10, "Students"), (5LL - 1))].pc_Marks[(3LL - 1)] = 39LL;
    pc_Students[(pc_bounds_check(5LL, 1, 10, "Students"), (5LL - 1))].pc_Marks[(4LL - 1)] = 48LL;
    pc_Students[(pc_bounds_check(5LL, 1, 10, "Students"), (5LL - 1))].pc_Marks[(5LL - 1)] = 57LL;
    printf("%s\n", "CALCULATING STUDENT DATA...");
    printf("%s\n", "");
    {
        long long tmp_end_7 = pc_Count;
        for (pc_i = 1LL; pc_i <= tmp_end_7; pc_i += 1) {
            pc_CalculateStudentData(&pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))]);
        }
    }
    printf("%s\n", "ORIGINAL STUDENTS");
    {
        long long tmp_end_8 = pc_Count;
        for (pc_i = 1LL; pc_i <= tmp_end_8; pc_i += 1) {
            pc_DisplayStudent(pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))]);
        }
    }
    pc_Highest = pc_Students[(pc_bounds_check(1LL, 1, 10, "Students"), (1LL - 1))].pc_Marks[(1LL - 1)];
    pc_Lowest = pc_Students[(pc_bounds_check(1LL, 1, 10, "Students"), (1LL - 1))].pc_Marks[(1LL - 1)];
    pc_Total = 0LL;
    {
        long long tmp_end_9 = pc_Count;
        for (pc_i = 1LL; pc_i <= tmp_end_9; pc_i += 1) {
            {
                long long tmp_end_10 = 5LL;
                for (pc_j = 1LL; pc_j <= tmp_end_10; pc_j += 1) {
                    pc_Total = pc_add(pc_Total, pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))].pc_Marks[(pc_j - 1)]);
                    if ((pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))].pc_Marks[(pc_j - 1)] > pc_Highest)) {
                        pc_Highest = pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))].pc_Marks[(pc_j - 1)];
                    }
                    if ((pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))].pc_Marks[(pc_j - 1)] < pc_Lowest)) {
                        pc_Lowest = pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))].pc_Marks[(pc_j - 1)];
                    }
                }
            }
        }
    }
    pc_AverageAll = ((double)pc_Total / (double)pc_mul(pc_Count, 5LL));
    printf("%s\n", "");
    printf("%s\n", "CLASS STATISTICS");
    printf("%s%lld\n", "Total Marks: ", pc_Total);
    printf("%s%.10g\n", "Average Mark: ", pc_AverageAll);
    printf("%s%lld\n", "Highest Mark: ", pc_Highest);
    printf("%s%lld\n", "Lowest Mark: ", pc_Lowest);
    printf("%s\n", "");
    printf("%s\n", "SORTING STUDENTS...");
    pc_SortStudents(pc_Students, pc_Count);
    printf("%s\n", "");
    printf("%s\n", "STUDENTS SORTED BY AVERAGE");
    {
        long long tmp_end_11 = pc_Count;
        for (pc_i = 1LL; pc_i <= tmp_end_11; pc_i += 1) {
            printf("%lld%s%s%s%.10g%s%s\n", pc_i, ". ", pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))].pc_Name, " - ", pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))].pc_Average, " - ", pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))].pc_Grade);
        }
    }
    printf("%s\n", "");
    printf("%s\n", "SEARCH STUDENT BY ID");
    pc_SearchID = 103LL;
    pc_Found = false;
    {
        long long tmp_end_12 = pc_Count;
        for (pc_i = 1LL; pc_i <= tmp_end_12; pc_i += 1) {
            if ((pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))].pc_ID == pc_SearchID)) {
                pc_Found = true;
                printf("%s%lld\n", "Student found at position ", pc_i);
                pc_DisplayStudent(pc_Students[(pc_bounds_check(pc_i, 1, 10, "Students"), (pc_i - 1))]);
            }
        }
    }
    if ((pc_Found == false)) {
        printf("%s\n", "Student not found");
    }
    printf("%s\n", "");
    printf("%s\n", "PRIME NUMBER TEST");
    {
        long long tmp_end_13 = 20LL;
        for (pc_i = 1LL; pc_i <= tmp_end_13; pc_i += 1) {
            if (pc_IsPrime(pc_i)) {
                printf("%lld%s\n", pc_i, " is prime");
            }
        }
    }
    printf("%s\n", "");
    printf("%s\n", "RECURSIVE FACTORIAL TEST");
    {
        long long tmp_end_14 = 10LL;
        for (pc_i = 1LL; pc_i <= tmp_end_14; pc_i += 1) {
            printf("%lld%s%lld\n", pc_i, "! = ", pc_RecursiveFactorial(pc_i));
        }
    }
    printf("%s\n", "");
    printf("%s\n", "CASE TEST");
    pc_Choice = 3LL;
    switch (pc_Choice) {
    case 1LL:
        printf("%s\n", "Option One");
        break;
    case 2LL:
        printf("%s\n", "Option Two");
        break;
    case 3LL:
        printf("%s\n", "Option Three");
        break;
    case 4LL:
        printf("%s\n", "Option Four");
        break;
    default:
        printf("%s\n", "Invalid Option");
        break;
    }
    pc_cleanup();
    return 0;
}
