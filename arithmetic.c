#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
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

int expand(unsigned int *_high, unsigned int *_low, unsigned int size, unsigned int *_outch)
{
        unsigned int high = *_high;
        unsigned int low = *_low;
        unsigned int outch = 0;
        int bits = 0;

        if (!outch) return -1; 
        if ((high - low) > size) return 0;
        if (high == 0x80000000 && low == 0x7fffffff) {
                high = ~0;
                low = 0;
        } else {
                while ((high - low) <= size) {
                        if (++bits > 32) return -1;
                        outch <<= 1;
                        outch |= 0x01 & (high >> 31);
                        high <<= 1;
                        low <<= 1;
                }
        }
        *_high = high;
        *_low = low;
        *_outch = outch;

        return bits;
}


//if (outformate(outstr, outch, com.outbits, bit)) {
int outformate(unsigned char *currstr, unsigned int outch, unsigned char currbits, unsigned int bit)
{
        unsigned int byte, i, usebits, ret;

        if (!currstr || currbits > CHAR_BITS || bit >= INT_BITS) return -1;

        byte = (bit + currbits) / CHAR_BITS;
        ret = byte;

        i = 0;
        do {
                if (byte) {
                        usebits = CHAR_BITS - currbits;
                        *(currstr + i) <<= usebits;
                        *(currstr + i) |= (outch >> (bit - usebits)) & ((1 << usebits) - 1);
                        currbits = 0;
                        bit -= usebits;
                        --byte;
                        ++i;
                } else {
                        *(currstr + i) <<= bit;
                        *(currstr + i) |= outch & ((1 << bit) - 1);
                        bit = 0;
                }
        } while (byte > 0 || bit > 0);

        return ret;
}

/* arithmetic_compression */
int compression(const char *outfile, char *src, unsigned int filesize)
{
        unsigned int priority[256];
        unsigned int element_no;
        int fd, i, j;
        unsigned char buf[256], outstr[8];
        struct tags tag;
        struct com com;
        unsigned int high, low, bit, outbyte, outch = 0;
        unsigned long long tmp;

        if (!outfile || !src || !filesize) return -1;
        if ((element_no = element_init(priority, src, filesize)) == 0 || element_no > 256) return -1;
        //memset(&tag, 0, TAGS_SIZE);
        tag.tag = TAGS_TAG;
        tag.elements = element_no & 0xff; // 256 is formated to 0 hear
        tag.filesize = filesize; 
        tag.type = element_no > 32 ? TAGS_TYPE_DYNAMIC : TAGS_TYPE_STATIC;
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
        if (tag.type & TAGS_TYPE_STATIC) {
                j = 0;
                while (j < element_no) {
                        if (priority[i]) buf[j++] = 0xff & i;
                        ++i;
                }
        } else if (tag.type & TAGS_TYPE_DYNAMIC) {
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
        //com.currsize = 0;
        com.currsize = element_no & 0xffff;
        //com.currchar = 0;
        //com.elements = element_no & 0xffff;
        while (i < filesize) {
                if ((com.high - com.low) < com.currsize) {
                        if ((bit = expand(&com.high, &com.low, com.currsize, &outch)) < 0) {
                                return -1;
                        } else if (bit > 0) {
                                if ((outbyte = outformate(outstr, outch, com.outbits, bit)) < 0) {
                                        close(fd);
                                        return -1;
                                }
                                com.outbits += bit;
                                //outbyte = com.outbits / CHAR_BITS;
                                //com.outbits = com.outbits % CHAR_BITS;
                                com.outbits = com.outbits - (outbyte << 3);
                                if (outbyte > 0) {
                                        if (outbyte != writex(fd, &outstr, outbyte)) {
                                                fprintf(stderr, "file to write %s : %s", outfile, strerror(errno));        
                                                close(fd);
                                                return -1;
                                        }
                                        com.outbytes += outbyte;
                                        outstr[0] = outstr[outbyte];
                                }
                        }
                }
                // get priority_offset
                if ((com.offset = priority_offset(priority, *(src+i))) >= (filesize + element_no)) return -1;
                tmp = (com.high - com.low) * ((unsigned long long)com.offset);
                low = edge(tmp , com.currsize) + com.low;
                tmp = (com.high - com.low) * ((unsigned long long)com.offset + priority[*(src+i) & 0xff]);
                high = edge(tmp , com.currsize) + com.low;
                ++(priority[*(src+i) & 0xff]);
                ++(com.currsize);
                com.low = low;
                com.high = high;
                ++i;
        }
        if (com.outbits > 0) {
                if (com.outbits >= CHAR_BITS) return -1;
                outstr[0] <<= CHAR_BITS - com.outbits;
                if (1 != writex(fd, &outstr, 1)) {
                        fprintf(stderr, "file to write %s : %s", outfile, strerror(errno));        
                        close(fd);
                        return -1;
                }
                tag.lastoutbits = com.outbits;
        } else {
                tag.lastoutbits = CHAR_BITS;
        }
        tmp = (com.low + com.high);
        tag.magic = tmp >> 1;
        if (lseek(fd, 0, SEEK_SET) < 0 || TAGS_SIZE != writex(fd, &tag, TAGS_SIZE)) {
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
