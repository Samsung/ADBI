
/* Further declarations heavily depend on features available only in GCC. */
#ifndef __GNUC__
#error "Incompatible compiler."
#endif


/* Make sure the sizes of integer types match the values defined by the GNU C standard. */
#if __SIZEOF_SHORT__ != 2
#error "sizeof(short) != 2"
#endif

#if __SIZEOF_INT__ != 4
#error "sizeof(int) != 4"
#endif

#if __SIZEOF_LONG_LONG__ != 8
#error "sizeof(long long) != 8"
#endif


/* Define the right floating point type macros. */
#if   __SIZEOF_FLOAT__ == 4
#undef  __adbi_fp32_t
#define __adbi_fp32_t   float
#elif __SIZEOF_FLOAT__ == 8
#undef  __adbi_fp64_t
#define __adbi_fp64_t   float
#elif __SIZEOF_FLOAT__ == 16
#undef  __adbi_fp128_t
#define __adbi_fp128_t  float
#endif

#if   __SIZEOF_DOUBLE__ == 4
#undef  __adbi_fp32_t
#define __adbi_fp32_t   double
#elif __SIZEOF_DOUBLE__ == 8
#undef  __adbi_fp64_t
#define __adbi_fp64_t   double
#elif __SIZEOF_DOUBLE__ == 16
#undef  __adbi_fp128_t
#define __adbi_fp128_t  double
#endif

#if   __SIZEOF_LONG_DOUBLE__ == 4
#undef  __adbi_fp32_t
#define __adbi_fp32_t   long double
#elif __SIZEOF_LONG_DOUBLE__ == 8
#undef  __adbi_fp64_t
#define __adbi_fp64_t   long double
#elif __SIZEOF_LONG_DOUBLE__ == 16
#undef  __adbi_fp128_t
#define __adbi_fp128_t  long double
#endif


/* Warn about missing types. */
#ifndef __adbi_fp32_t
#warning "Architecture does not support 32 bit floating point values."
#endif

#ifndef __adbi_fp64_t
#warning "Architecture does not support 64 bit floating point values."
#endif

#ifndef __adbi_fp128_t
#warning Architecture does not support 128 bit floating point values.
#endif

#define report_size(t) printf("sizeof(%s) == %i\n", # t, (int) sizeof(t))

#define __concat_impl( x, y ) x##y
#define __concat( x, y ) __concat_impl( x, y )

#define test_var(type) type __concat(test_var_, __COUNTER__) = 0

test_var(float);
test_var(double);
test_var(long double);

test_var(__INT8_TYPE__);
test_var(__INT16_TYPE__);
test_var(__INT32_TYPE__);
test_var(__INT64_TYPE__);

test_var(char);
test_var(short);
test_var(int);
test_var(long int);
test_var(long long int);

test_var(int long long);

test_var(__adbi_fp32_t);
test_var(__adbi_fp64_t);
// test_var(__adbi_fp128_t);

int long unsigned long  xxx;

typedef double my_float_t;

my_float_t my_float = 0.12;

struct MyStructure {
    int i;
    void * ptr;
    char str[10];
} my_structure;


struct __attribute__((packed)) WeirdStructure {

    char __adbi_padding_01[7];
    int a;
    
    char __adbi_padding_02[3];
    int b;
    
} weird_structure;

struct __attribute__((packed)) BitStructure {
    int a : 2;
    unsigned b : 2;
    signed c : 3;
} bit_struct;

union MyUnion {
    int a;
    char b;
    struct MyStructure s;
} my_union;

enum MyEnum {
    E_EINS = 1,
    E_ZWEI = 2,
    E_DREI = 3,
    E_VIER = 4,
    E_TAUSEND = 1000,
} my_enum;

/* Anonymous enum? */
enum {
    E_ANON_FOO = 1,
    E_ANON_BAR = 0,
};

struct StructWithAnons {

    struct {
        int a;
        int b;
    };
    
} swa;

int tab3d[10][20][30 ];

int main() {
    swa.a = 2;
    return E_ANON_BAR;
}
