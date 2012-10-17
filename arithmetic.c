#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static int element_init(unsigned int *arr, const char *src, unsigned int length)
{
        unsigned int i, n;

        if (!src || !length) return -1;

        i = 0;
        while (i < 256) arr[i++] = 0;
        i = 0;
        n = 0;
        while (i < length) {
                if (0 == arr[0xff & *(src+i++)]) {
                        ++n;
                        arr[0xff & *(src+i-1)] = 1;
                }
        }

        return n;
}

unsigned int priority_offset(unsigned int *arr, const unsigned int ch)
{
        unsigned int i, offset;

        if (!arr || ch > 255) return ~0;
        i = 0;
        offset = 0;
        while (i < ch) {
                offset += arr[i++];
        }

        return offset;
}

#define CHAR_BITS               8
#define HIGHEST_BIT(x)          ((x) & (1<<((sizeof(unsigned int)<<3)-1)))
#define LOWEST_BIT(x)           ((x) & 0x01)
/* arithmetic_compression */
int compression(const char *outfile, char *src, unsigned int length, struct tags *tags)
{
        unsigned int priority[256];
        unsigned int high, low, high_t, low_t;
        unsigned int element_no;
        int fd, i;
        unsigned char outch, outbit;
        unsigned int offset, all_priority;

        if (!outfile || !tags || !src || !length) return -1;
        if ((element_no = element_init(priority, src, length)) <= 0) return -1;
        all_priority = element_no;
        low = 0;
        high = ~0; // 0xfffffff > 4290000000
        if ((fd = open(outfile, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
                return -1;
        }
        // write tags
        // wirte char element
        i = 0;
        outbit = 0;
        while (i < length) {
                if (HIGHEST_BIT(high) ^ HIGHEST_BIT(low)) {
                // chk the second hihgest bit
                } else {
                        outch <<= 1;
                        if (HIGHEST_BIT(high)) {
                                outch |= 0x01;
                        } else {
                                // do nothing
                        }
                        if (++outbit == CHAR_BITS && 1 != wirte(fd, &outch, 1)) {
                                fprintf(stderr, "file to write %s : %s", outfile, strerror(errno));        
                                return -1;
                        }
                        low <<= 1;
                        high <<= 1;
                        ++high;
                }
                // get priority_offset
                ++(priority[*(src+i) & 0xff]);
                ++all_priority;
                if ((offset = priority_offset(priority, *(src+i))) >= (length + element_no)) return -1;
                low_t = (high - low) * offset / all_priority + low;
                high_t = (high - low) * (offset + (priority[*(src+i) & 0xff])) / all_priority + low;
                if (high_t <= low_t) {
                       // TODO  
                }
                ++i;
        }

        return 0;
}

/* arithmetic_decompression */
int decompression(const char *outfile, const char *infile)
{
        if (!outfile || !infile) return -1;


        return 0;
}
