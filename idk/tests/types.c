
/* Enums */

enum Alpha {
    EINS = 1,
    ZWEI = 2,
    DREI = 3,
};

typedef enum Alpha Alpha;

typedef enum {
    SIEBEN = 7,
    ACHT = 8,
    NEUN = 9,
} Bravo;

Alpha alpha;
Bravo bravo;


/* Pointers to pointers with const and volatile */

typedef const int * volatile * Charlie;
typedef int * const * volatile Delta;
typedef int * volatile * const Echo;

Charlie charlie;
Delta delta;
Echo echo;


/* Pointer to array and array of pointers */

typedef int (* Foxtrot)[20];
typedef int * Golf[20];

Foxtrot foxtrot;
Golf golf;


/* Structs */

struct Hotel {
    int a;
    int b;
    int c;
};

typedef struct Hotel Hotel;

struct Hotel hotel1;
Hotel hotel2;

typedef struct India {
    int d;
    int e;
    int f;
} India;

struct India india1;
India india2;

typedef struct {
    int g;
    int h;
    int i;
} Juliet;

Juliet juliet;


/* Unions */

union Kilo {
    int a;
    char b[4];
};

typedef union Kilo Kilo;

union Kilo kilo1;
Kilo kilo2;

typedef union Lima {
    int c;
    char d[4];
} Lima;

union Lima lima1;
Lima lima2;

typedef union {
    int c;
    char d[4];
} Mike;

Mike mike;


/* Nested */

struct November {

    union {
    
        struct {
            int i1;
            int i2;
        };
        
        char str[8];
        
        enum {
            ANTON = 12,
            BERTA,
            CAESAR,
            DORA,
            EMIL,
        } e;
    };
    
};

struct November november;

/* Aligned struct */

struct Oscar {
    char anton;
    int berta;
    int zurich __attribute__((aligned(32)));
};

struct Oscar oscar;

/* Bitfields */

struct Papa {
    int victor: 2;
    signed int wilhelm: 3;
    unsigned int xaver: 4;
};

struct Papa papa;

/* Anonymous structs */
struct QuebecInner;

struct Quebec {
    struct {
        struct Oscar oscar;
        const struct November * november;
    };
    
    struct {
        union Kilo kilo;
        const Lima * lima;
    } quebuec_nested;
    
    struct QuebecInner * inner;
    
    int q[];
};

struct QuebecInner {
    union Kilo kilo;
    const Lima * lima;
} quebuec_inner;

struct Quebec quebec;

int main() {
#define printsize(t) printf("sizeof(%s) == %i\n", # t, sizeof(t))

    printsize(Alpha);
    printsize(Bravo);
    printsize(Charlie);
    printsize(Delta);
    printsize(Golf);
    printsize(Echo);
    printsize(Foxtrot);
    printsize(Golf);
    printsize(Hotel);
    printsize(India);
    printsize(Juliet);
    printsize(Kilo);
    printsize(Lima);
    printsize(Mike);
    printsize(struct November);
    printsize(struct Papa);
    printsize(struct Quebec);
    
    return 0;
}
