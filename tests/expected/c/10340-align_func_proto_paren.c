// Test: align_func_proto_paren with mixed empty/comment/PP lines in C

VeryLongReturnType complex_func_one  (void);
unsigned int       complex_func_two  (int a, int b);
#ifdef PLATFORM_A
// Platform specific function
double *           complex_func_three(const char *name);

// Another comment
short              complex_func_four (struct point *p,
                                      size_t count);
#endif
#ifdef PLATFORM_B
// Multiple lines here
long int complex_func_five(void);
// Comment line 1
// Comment line 2
// Comment line 3
unsigned long complex_func_six(int x);


// Two empty + comment
static int         complex_func_seven    (int x, ...);
#endif

void               final_func_eight      (void);

extern const char *another_func_nine     (void);


int                short_ten             (int);

VeryLongReturnType long_named_func_eleven(void (*callback)(int), void *data);
#if 1
#if 1
#if 1
int twelve(void);
#endif
#endif
#endif

VeryLongReturnType extra_long_func_thirteen(void);
// cmt 1
// cmt 2
// cmt 3
// cmt 4
int fn_fourteen(void);

/* Open parenthesis on the next line: not aligned */
int
     fifteen
    (int a);
char sixteen(int a);

/* Function pointers are variables, not prototypes */
struct ops
{
    int (*open)(const char *path);
    void (*close)(int fd);
};
int               seventeen(struct ops *o);
static inline int eighteen (int a)
{
    return a;
}
static inline unsigned int nineteen(int a)
{
    return a + 1;
}
int   twenty            (void);

/* Several functions on a line: only the first one is aligned */
int   twenty_one        (void); unsigned twenty_two(void);
long  twenty_three      (void);

/* Spaces before the open parenthesis, and a comment before it */
int   spaced            (int a);
long  more_spaced       (int a);
short commented /* c */ (int a);

/* Comments between a name and its open parenthesis */
int   a /* c */         (int);
int   abcdefghijkl      (int);
int   abcdefghijklmno   (int);
long  k /* multi
           line */(int);
int   verylongname      (int);
