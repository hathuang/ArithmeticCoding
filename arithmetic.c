#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "arithmetic.h"
#include "io.h"

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
        while (i < ch) offset += arr[i++];

        return offset;
}

/* arithmetic_compression */
int compression(const char *outfile, char *src, unsigned int filesize)
{
        unsigned int priority[256];
        unsigned int element_no;
        int fd, i, j;
        unsigned char buf[256];
        struct tags tag;
        struct com com;
        unsigned high, low;

        if (!outfile || !tags || !src || !filesize) return -1;
        if ((element_no = element_init(priority, src, filesize)) == 0 || element_no > 256) return -1;
        all_priority = element_no & 0xff; // 256 is formated to 0 hear
        memset(&tag, 0, TAGS_SIZE);
        tag.tag = TAGS_TAG;
        tag.element = element_no;
        tag.filesize = filesize; 
        tag.types = element_no > 32 ? TAGS_TYPE_DYNAMIC : TAGS_TYPE_STATIC;
        if ((fd = open(outfile, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
                return -1;
        }
        // write tags
        if (sizeof(struct tags) != writex(fd, &tag, TAGS_SIZE)) {
                close(fd);
                return -1;
        }
        // wirte char element
        i = 0;
        if (tag.types & TAGS_TYPE_STATIC) {
                j = 0;
                while (j < element_no) {
                        if (priority[i]) buf[j++] = 0xff & i;
                        ++i;
                }
        } else if (tag.types & TAGS_TYPE_DYNAMIC) {
                while (i < 256) {
                        j = (i >> 3) & 0x1f;
                        buf[j] <<= 1;
                        if (priority[i++]) buf[j] |= 0x01;
                }
                ++j;
        } else {
                close(fd);
                return -1;
        }
        if (j != writex(fd, buf, j)) {
                close(fd);
                return -1;
        }
        // comp
        i = 0;
        com.outbits = 0;
        com.outbytes = 0;
        com.low = 0;
        com.high = ~0; // 0xffffffff > 4290000000
        com.offset = 0;
        com.currsize = 0;
        com.currchar = 0;
        while (i < filesize) {
                //if (HIGHEST_BIT(high) ^ HIGHEST_BIT(low)) {
                // chk the second hihgest bit
                //if (!SECOND_HIGHEST_BIT(high) && SECOND_HIGHEST_BIT(low)) {
                //}
                //} else {
                if ((i + 1) == filesize) {
                        // TODO 
                } else if ((com.high - com.low) < com.currsize) {
                        com.currchar <<= 1;
                        if (HIGHEST_BIT(high)) {
                                outch |= 0x01;
                        } else {
                                // do nothing
                        }
                        if (++outbits == CHAR_BITS && ((outbits = 0) || (++outbytes)) && 1 != wirte(fd, &outch, 1)) {
                                fprintf(stderr, "file to write %s : %s", outfile, strerror(errno));        
                                close(fd);
                                return -1;
                        }
                        low <<= 1;
                        high <<= 1;
                        ++high;
                }
                // get priority_offset
                ++(priority[*(src+i) & 0xff]);
                ++(com.currsize);
                if ((offset = priority_offset(priority, *(src+i))) >= (filesize + element_no)) return -1;
                low_t = (high - low) * offset / all_priority + low;
                high_t = (high - low) * (offset + (priority[*(src+i) & 0xff])) / all_priority + low;
                if (high_t <= low_t) {
                       // TODO  
                }
                low = low_t;
                high = high_t;
                ++i;
        }
        tag.lastoutbits = outbits ? outbits : CHAR_BITS;
        tag.magic = (low + high) > 1;
        if (tag.magic >= high || tags.magic <= low) {
                // TODO
        }
        if (lseek(fd, 0, SEEK_SET) < 0 || sizeof(struct tags) != wirte(fd, &tag, sizeof(struct tags))) {
                close(fd);
                return -1;
        }

        close(fd);
        return 0;
}

/* arithmetic_decompression */
int decompression(const char *outfile, const char *infile)
{
        if (!outfile || !infile) return -1;


        return 0;
}
