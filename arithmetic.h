#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#define HALF_CHAR_BITS          (4)
#define CHAR_BITS               (8)
#define SHORT_BITS              (CHAR_BITS << (sizeof(short)>>1))
#define INT_BITS                (CHAR_BITS << (sizeof(int)>>1))

#define HIGHEST_BIT(x)          ((x) & (1<<((sizeof(unsigned int)<<3)-1)))
#define SECOND_HIGHEST_BIT(x)   ((x) & (1<<((sizeof(unsigned int)<<3)-2)))
#define LOWEST_BIT(x)           ((x) & 0x01)

#define TAGS_TYPE_STATIC        (1<<1)
#define TAGS_TYPE_DYNAMIC       (1<<2)
#define TAGS_TAG                'A'

#define max(x, y)               ((x) > (y) ? (x) : (y))
#define min(x, y)               ((x) < (y) ? (x) : (y))
#define bool2int(x)             ((x) ? 1 : 0)
#define edge(x, y)              ((x)/(y) + bool2int((x)%(y)))
/*#define edge(x, y)              ((x)/(y) + (((x)%(y)) ? 1 : 0))*/

struct tags {
        unsigned char tag; // A
        unsigned char type;
        unsigned char lastoutbits;
        unsigned char elements;
        unsigned int magic;
};

extern int compression(const char *outfile, char *src, unsigned int length);
extern int decompression(const char *outfile, const char *infile);


#endif // arithmetic.h
