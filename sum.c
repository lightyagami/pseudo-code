#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

// --- Pseudoc Runtime Core & Dynamic Memory Tracker ---
typedef struct PC_Node { void* ptr; struct PC_Node* next; } PC_Node;
static PC_Node* pc_gc_head = NULL;
static void* pc_track(void* p) {
    if (!p) return NULL;
    PC_Node* n = (PC_Node*)malloc(sizeof(PC_Node));
    n->ptr = p; n->next = pc_gc_head; pc_gc_head = n;
    return p;
}
static void pc_cleanup(void) {
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

int main(void) {
    long long pc_n = 0;
    long long pc_total = 0;
    long long pc_i = 0;

    pc_n = pc_read_int();
    pc_total = 0LL;
    {
        long long tmp_end_1 = pc_n;
        for (pc_i = 1LL; pc_i <= tmp_end_1; pc_i += 1) {
            pc_total = pc_add(pc_total, pc_i);
        }
    }
    printf("%s%lld\n", "Sum is ", pc_total);
    pc_cleanup();
    return 0;
}
