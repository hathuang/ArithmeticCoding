/*
http://dungenessbin.diandian.com/post/2012-05-23/21949784
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "huffman.h"
#include "syslog.h"

#ifdef Debug
#define print_debug(x)     printf(x)
#else
#define print_debug(x)
#endif

static int huffman_line(struct huffman_node **head, char *str, unsigned int length)
{
        unsigned int i;
        char ch;
        char *src = str;
        struct huffman_node *node = *head;
        struct huffman_node *pnode = NULL;
        struct huffman_node *prenode= NULL;
        
        if (!node || !src || !length) return -1;

        // first node
        node->data = *(src + 0);
        node->bits = 0;
        node->newcode = 0;
        node->priority = 1;
        node->lnext = NULL;
        node->rnext = NULL;
        node->next = NULL;

        i = 1;
        while (i < length) {
                ch = *(src + i++);
                pnode = node;
                do {
                        if (pnode->data == ch) {
                                pnode->priority++;
                                break;
                        }
                        prenode = pnode;
                } while (pnode = pnode->next);
                if (!pnode) { // new
                        pnode = (struct huffman_node *) malloc(sizeof(struct huffman_node)); 
                        if (!(prenode->next = pnode))return -1;
                        pnode->data = ch;
                        pnode->bits = 0;
                        pnode->newcode = 0;
                        pnode->priority = 1;
                        pnode->lnext = NULL;
                        pnode->rnext = NULL;
                        pnode->next = NULL;
                }
        }
        return 0;
}

static int huffman_sort(struct huffman_node **head, int flag)
{
        struct huffman_node *p = *head;
        struct huffman_node *q = NULL;
        
        if (!p) return -1;
        if (p->next == NULL) return 0; // one element ONLY

        // bubble sort
        do {
                q = p;
                while (q = q->next) {
                        if (p->priority > q->priority) {
                                p->data = (p->data) ^ (q->data);
                                q->data = (p->data) ^ (q->data);
                                p->data = (p->data) ^ (q->data);
                               
                                p->bits = (p->bits) ^ (q->bits);
                                q->bits = (p->bits) ^ (q->bits);
                                p->bits = (p->bits) ^ (q->bits);
                                
                                p->newcode = (p->newcode) ^ (q->newcode);
                                q->newcode = (p->newcode) ^ (q->newcode);
                                p->newcode = (p->newcode) ^ (q->newcode);

                                p->priority = (p->priority) ^ (q->priority);
                                q->priority = (p->priority) ^ (q->priority);
                                p->priority = (p->priority) ^ (q->priority);
                        }
                }
        } while (p = p->next);

        return 0;
}

static int huffman_code(struct huffman_node *root)
{
        struct huffman_node *p = root;
        
        if (p == NULL) return -1;
        if (p->rnext) {
                if ((p->bits & 0x7f) >= 0x0f) {
                        syslog(LOG_USER | LOG_ERR, "%s : bits=%d, more than 16 is not support !", p->bits);
                        return -1;       // it is < 16, bigger than 16 is not support
                }
                p->rnext->bits &= 0x80; 
                p->rnext->bits |= (p->bits & 0x0f) + 1;         // it is < 16 
                p->rnext->newcode = (p->newcode << 1) | 0x01;   // right : 1
                if (huffman_code(p->rnext)) return -1;
        }
        if (p->lnext) {
                if ((p->bits & 0x7f) >= 0x0f) {
                        syslog(LOG_USER | LOG_ERR, "%s : bits=%d, more than 16 is not support !", p->bits);
                        return -1;       // it is < 16, bigger than 16 is not support
                }
                p->lnext->bits &= 0x80;
                p->lnext->bits |= (p->bits & 0xf) + 1;         
                p->lnext->newcode = p->newcode << 1;           // left  : 0 
                if (huffman_code(p->lnext)) return -1;
        }
        return 0;
}

static int huffman_insert_tree(struct huffman_node **head, struct huffman_node *new)
{
        struct huffman_node *q = *head;
        struct huffman_node *p = NULL;

        if (!q || !new) return -1;
        if (q->priority >= new->priority) {
                new->next = q;
                *head = new;
                return 0;
        }
        p = q;
        while (q = p->next) {
                if (p->priority <= new->priority && new->priority <= q->priority) {
                        p->next = new;
                        new->next = q;
                        return 0;
                }
                p = q;
        }
        p->next = new;
        new->next = NULL;
        
        return 0;
}

static int huffman_tree(struct huffman_node **root)
{
        struct huffman_node *node = NULL;
        struct huffman_node *p = NULL;
        
        if ((*root) == NULL) return -1;
        if ((*root)->next == NULL) {
                return 0;
        } else {
                if (!(node = (struct huffman_node *)malloc(sizeof(struct huffman_node)))) {
                        return -1;
                }
                node->next = NULL;
                node->data = '\0'; 
                node->bits = 0x80; // means not the code form the file or str 
                node->newcode = 0; 
                node->priority = (*root)->priority + (*root)->next->priority;
                
                if ((*root)->priority <= (*root)->next->priority) { // bigger right 
                        node->lnext = *root;
                        node->rnext = (*root)->next;
                } else {
                        return -1;
                }
                if ((*root)->next->next == NULL) { // two
                        (*root)->next->next = node;
                        (*root)->next = NULL;
                        *root = node;
                        return 0;
                } else { // three or more
                        p = (*root)->next->next;
                        (*root)->next->next = NULL;
                        (*root)->next = NULL;
                        *root = p;
                        if (!(huffman_insert_tree(root, node))) {
                                return huffman_tree(root);
                        }
                }
        }
        
        return -1;
}

static int node_distory(struct huffman_node **head)
{
        struct huffman_node *p = *head;
        struct huffman_node *q = NULL;

        if (!p) return -1;
        q = p;
        while (q = p->next) {
                free(p);
                p = q;
        }
        free(p);
        *head = NULL;
        return 0;
}

static int huffman_root_distory(struct huffman_node *root)
{
        struct huffman_node *p = root;

        if (!p) return 0;
        if (p->rnext) {
                huffman_root_distory(p->rnext);
        }
        if (p->lnext) {
                huffman_root_distory(p->lnext);
        }
        free(p);

        return 0;
}

#if 0
static int print_newcode(struct huffman_node **head, int flag)
{
        struct huffman_node *p = *head;
        if (!p) {
                printf("Ugly head\n");
                return -1;
        }
        printf("\nplace : %p\n", p);
        if (flag != 0 && flag != 1) {
                return 0;
        } 
        
        do {
                switch (flag) {
                case 0:
                        printf("Huffman : char = %c = 0x%02x, bits = %d, newcode = 0x%04x, priority = %d\n",
                                p->data, p->data, p->bits, p->newcode, p->priority);
                        break;
                case 1:
                        syslog(LOG_USER | LOG_DEBUG, "%s : char=%c=0x%02x,bits=%d,newcode=0x%04x,priority=%d,my=%p,next=%p,lnext=%p,rnext=%p",
                                __func__, p->data, p->data, p->bits, p->newcode, p->priority, p, p->next, p->lnext, p->rnext);
                        break;
                default:
                        break;
                }
        } while (p = p->next);
        
        return 0;
}
#endif

static int huffman_fill_newcode(int fd, struct huffman_node *head, struct huffman_tags *tags, unsigned short (*arr)[2])
{
        struct huffman_node *q = head;
        char buf[4];
        
        if (fd < 0 || !q || !tags) return -1;

        if (q->rnext) huffman_fill_newcode(fd, q->rnext, tags, arr);
        if (q->lnext) huffman_fill_newcode(fd, q->lnext, tags, arr);
        if (q->bits & 0x80) return 0;

        buf[0] = q->data;
        buf[1] = q->bits;
        buf[2] = (q->newcode >> 8) & 0xff;
        buf[3] = q->newcode & 0xff;
        arr[q->data & 0xff][0] = q->bits & 0xff;
        arr[q->data & 0xff][1] = q->newcode;
#ifdef Debug
        syslog(LOG_USER | LOG_DEBUG, "%s : buf -%c-0x%x-0x%x-0x%x-0x%x-",
                __func__, buf[0], buf[0]&0xff, buf[1]&0xff, buf[2]&0xff, buf[3]&0xff);
#endif
        if (4 != write(fd, buf, 4)) return -1;
        if (q->bits <= ONE_CHAR) {
                tags->bytes1++;
        } else if (ONE_CHAR < q->bits && q->bits <= ONE_SHORT) {
                tags->bytes2++;
        } else {
                syslog(LOG_USER | LOG_ERR, "%s : too large bits : %d", __func__, q->bits);
                return -1;
        }
        
        return 0;
}

static int huffman_fill_file(const char *file, struct huffman_node *head, struct huffman_tags *tags, char *str, unsigned int length)
{
        unsigned char ch, newchar;
        unsigned int i;
        char newstr[4];
        int n, fd;
        unsigned char bits, flag;
        struct huffman_node *p = NULL;
        struct huffman_node *q = NULL;
        unsigned short arr[256][2];

        if (!file || !head || !tags || !str || !length) return -1;
        for (i = 0; i < 256; i++) {
                arr[i][0] = 0;
                arr[i][1] = 0;
        }
        if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0
                || write(fd, (char *)tags, HUFFMAN_TAGS_SIZE) != HUFFMAN_TAGS_SIZE
                || huffman_fill_newcode(fd, head, tags, arr)) {
                return -1;
        }
        if (tags->bytes1 == 0 && tags->bytes2 == 0 && tags->bytes4 == 0) {
                p = head;
                while (q = p->rnext);
                if (p == head || p->newcode == 0 || p->bits <= 0) return -1; 
                if (p->bits <= ONE_CHAR) {
                        tags->magic |= ALL_CHAR;
                } else if (ONE_CHAR < p->bits && p->bits <= ONE_SHORT) {
                        tags->magic |= ALL_SHORT;
                } else {
                        return -1; // not support right now !
                }
        }
        // write file body
        i = 0;
        flag = 0;
        newchar = 0;
        while (i < length) {
                ch = *(str + i++);
                n = ch & 0xff;
                bits = arr[n][0];
                flag += bits;
                if (flag < ONE_CHAR) {
                        newchar = newchar << bits;
                        newchar |= arr[n][1] & ((1 << bits) - 1);
                        continue;
                } else if (flag >= ONE_CHAR && flag < ONE_SHORT) { // FIXME
#ifdef Debug
                        if (bits > ONE_CHAR) {
                                syslog(LOG_SYSTEM | LOG_ERR, "%s : == bits=%d, flag=%d, newchar=0x%x", __func__, bits, flag, newchar);
                        }
#endif
                        newchar = newchar << (ONE_CHAR - (flag - bits));
                        //newchar |= (arr[n][1] >> (flag - ((bits > ONE_CHAR) ? bits : ONE_CHAR))) & ((1 << (ONE_CHAR - (flag - bits))) - 1);
                        newchar |= (arr[n][1] >> (flag - ONE_CHAR)) & ((1 << (ONE_CHAR - (flag - bits))) - 1);
                        newstr[0] = newchar;
                        if (1 != write(fd, newstr, 1)) {
                                close(fd);
                                return -1;
                        }
#ifdef Debug
                        syslog(LOG_SYSTEM | LOG_WARNING, "%s : out Compression 1 : 0x%02x", __func__, newstr[0] & 0xff);
#endif
                        flag -= ONE_CHAR;
                        newchar = arr[n][1] & ((1 << flag) - 1);
#ifdef Debug
                        if (bits > ONE_CHAR) {
                                syslog(LOG_SYSTEM | LOG_ERR, "%s : == bits=%d, flag=%d, newchar=0x%x, newcode=%c=0x%x",
                                        __func__, bits, flag, newchar, arr[n][1], arr[n][1]&0xffff);
                        }
#endif
                } else { // >= ONE_SHORT
                        newchar = newchar << (ONE_CHAR - (flag - bits));
                        newchar |= (arr[n][1] >> (flag - ONE_CHAR)) & ((1 << (ONE_CHAR - (flag - bits))) - 1);
                        newstr[0] = newchar;

                        flag -= ONE_SHORT;
                        newstr[1] = (arr[n][1] >> flag) & 0xff;
                        if (2 != write(fd, newstr, 2)) {
                                close(fd);
                                return -1;
                        }
                        newchar = arr[n][1] & ((1 << flag) - 1);
                }
        }
        if (flag) {
                if (flag >= ONE_CHAR) {
                        syslog(LOG_USER | LOG_ERR, "%s : unkown flag = %d", __func__, flag);
                        close(fd);
                        return -1;
                }
                newstr[0] = newchar;// << (ONE_CHAR - flag);
                if (1 != write(fd, newstr, 1)) {
                        close(fd);
                        return -1;
                }
                tags->magic &= 0x0f;
                tags->magic |= (flag << 4) & 0xf0;
#ifdef Debug
                syslog(LOG_SYSTEM | LOG_WARNING, "%s : out Compression ~ : 0x%02x, flag = %d", __func__, newstr[0] & 0xff, flag);
#endif
        }
        if (lseek(fd, 0, SEEK_SET) < 0 || write(fd, (char *)tags, HUFFMAN_TAGS_SIZE) != HUFFMAN_TAGS_SIZE) {
                close(fd);
                return -1;
        }
        close(fd);
#ifdef Debug
        printf("\n=========== HUFFMAN TAGS: ============\n");
        printf("tags : fillbits %c=0x%x\n", tags->fillbits[0], tags->fillbits[0]);
        printf("tags : fillbits %c=0x%x\n", tags->fillbits[1], tags->fillbits[1]);
        printf("tags : fillbits %c=0x%x\n", tags->fillbits[2], tags->fillbits[2]);
        printf("tags : fillbits %c=0x%x\n", tags->fillbits[3], tags->fillbits[3]);

        printf("tags : magic %d=0x%x\n", tags->magic, tags->magic);
        printf("tags : bytes1 %d=0x%x\n", tags->bytes1, tags->bytes1);
        printf("tags : bytes2 %d=0x%x\n", tags->bytes2, tags->bytes2);
        printf("tags : bytes4 %d=0x%x\n", tags->bytes4, tags->bytes4);
        printf("=========== HUFFMAN TAGS: ============\n");
#endif
        return 0;
}

static int get_root(char *buf, int n, struct tree *root)
{
        int i, bits, k;
        char oldcode, onebit;
        unsigned short newcode;
        struct tree *p = NULL;;
        struct tree *q = NULL;;
/*
        newstr[0] = i & 0xff;                // oldcode 1 byte
        newstr[1] = arr[i][0];               // bits    1 byte
        newstr[2] = (arr[i][1] >> 8) & 0xff; // newcode 2 bytes
        newstr[3] = arr[i][1] & 0xff;        // newcode 2 bytes
*/
        if (!buf || !n || !root) return -1;
        i = 0;
        while (i < n) {
                k = (i << 2) & (~0x03);
                oldcode = buf[k++];
                bits = buf[k++];
                newcode = buf[k++] << 8;
                newcode |= buf[k] & 0xff;
                p = root;
                while (bits--) {
                        onebit = (newcode >> bits) & 0x01;
                        if (onebit) {
                                if (!(q = p->rchild)) {
                                        if (p->oldcode) {
#ifdef Debug
                                                syslog(LOG_USER | LOG_INFO, "%s : +++ oldcode=0x%x, i=%d,k=%d,buf[0]=0x%x=0x%x=0x%x=0x%x bits=%d", 
                                                                __func__, p->oldcode & 0xff, i,k,buf[k-3]&0xff, buf[k-2]&0xff, buf[k-1]&0xff, buf[k]&0xff, bits);
#endif
                                                return -1;
                                        }
                                        if (!(q = (struct tree *)malloc(sizeof(struct tree)))) return -1;
                                        q->rchild = NULL;
                                        q->lchild = NULL;
                                        q->oldcode = 0;
                                        p->rchild = q;
                                }
                                p = q;
                        } else {
                                if (!(q = p->lchild)) {
                                        if (p->oldcode) {
#ifdef Debug
                                                syslog(LOG_USER | LOG_INFO, "%s : +++ oldcode=0x%x, i=%d,k=%d,buf[0]=0x%x=0x%x=0x%x=0x%x bits=%d", 
                                                                __func__, p->oldcode & 0xff, i,k,buf[k-3]&0xff, buf[k-2]&0xff, buf[k-1]&0xff, buf[k]&0xff, bits);
#endif
                                                return -1;
                                        } 
                                        if (!(q = (struct tree *)malloc(sizeof(struct tree)))) return -1;
                                        q->rchild = NULL;
                                        q->lchild = NULL;
                                        q->oldcode = 0;
                                        p->lchild = q;
                                }
                                p = q;
                        }
                }
                if (!q) return -1; 
                q->oldcode = oldcode;
                ++i;
        }

        return 0;
}

static int huffman_decomp_tree(const char *filename, struct tree **tree, char **filebuf, unsigned int *_filelen, char *last_ch_bits)
{
        char buf[1024];
        int fd, ret, len, codenum;
        unsigned int filelen;
        struct huffman_tags *tags = NULL;
        struct tree *root = NULL;

        if (!filename || !tree || !_filelen || !last_ch_bits) {
                return -1;
        }
        fd = open(filename, O_RDONLY); 
        if (fd < 0) {
                perror("Fail to open TMP_FILE");
                return -1;
        }
        if ((filelen = lseek(fd, 0, SEEK_END)) <= 0 || lseek(fd, 0, SEEK_SET) < 0) {
                close(fd);
                return -1;
        }
        len = read(fd, buf, HUFFMAN_TAGS_SIZE);
        if (len != HUFFMAN_TAGS_SIZE) return -1;
#ifdef Debug
        ret = 0;
        printf("==============\n");
        while (ret < HUFFMAN_TAGS_SIZE) {
                printf("+ tags : buf[%d] = 0x%x\n", ret, buf[ret]&0xff);
                ret++;
        }
        printf("==============\n");
#endif

        tags = (struct huffman_tags *)buf;
        // header check
        if (strncmp((char *)(tags->fillbits), HUFFMAN_FILE_HEADER, 4)) {
                close(fd);
                fprintf(stderr, "Not a huffman file !\n");
                fflush(stderr);
                return -1;
        }
        switch (tags->magic & MAGIC_MAC) {
        case ALL_INT :
        case ALL_CHAR :
        case ALL_SHORT :
                len = 256;
                break;
        default :
                len = tags->bytes1 + tags->bytes2 + tags->bytes4;
                break;
        }
        *last_ch_bits = (tags->magic >> 4) & MAGIC_MAC;
        codenum = len;
        len = (len << 2) & (~0x03); // * 4
        filelen -= HUFFMAN_TAGS_SIZE;
        if (len >= filelen || len > 1024 || len < 4 || len != read(fd, buf, len)) {
#ifdef Debug
                printf("%s : Ugly len = %d, bytes1=%d, bytes2=%d, bytes4=%d", __func__, len, tags->bytes1, tags->bytes2, tags->bytes4);
#endif
                close(fd);
                return -1;
        }
        tags = NULL;
        filelen -= len;
        if (!(*filebuf = (char *)malloc(sizeof(char) * (filelen + 1)))) {
                close(fd);
                return -1;
        }
        len = 0;
        while (len < filelen) {
                if ((ret = read(fd, *filebuf + len, filelen - len)) <= 0) {
                        free(*filebuf);
                        *filebuf = NULL;
                        close(fd);
                        return -1;
                }
                len += ret;
        }
        close(fd);
#ifdef Debug
        ret = 0;
        while (ret < filelen) {
                syslog(LOG_USER | LOG_INFO, "%s : file buf %04d %c=0x%x", __func__, ret, *(*filebuf + ret), *(*filebuf + ret) & 0xff);
                ++ret;
        }
#endif
        // get the tree;
        if (!(root = (struct tree *)malloc(sizeof(struct tree)))) {
                free(*filebuf);
                *filebuf = NULL;
                return -1;
        }
        root->oldcode = 0;
        root->rchild = NULL;
        root->lchild = NULL;
        *tree = root;
        *_filelen = filelen;
#ifdef Debug
        printf("%s start to get_root\n", __func__);
        ret = 0;
        while (ret < codenum) {
                printf("-0x%x-0x%x-0x%x-0x%x-\n", buf[(ret<<2)+0]&0xff,buf[(ret<<2)+1], buf[(ret<<2)+2]&0xff,buf[(ret<<2)+3]&0xff);
                ret++;
        }
#endif
        return get_root(buf, codenum, root); 
}

static int free_tree(struct tree *root)
{
        struct tree *p = root;

        if (!p) return 0;
        if (p->rchild) free_tree(p->rchild);
        if (p->lchild) free_tree(p->lchild);
        free(p);
        return 0;
}

int huffman_decompression(const char *infile, const char *outfile)
{
        struct tree *root = NULL;
        struct tree *p = NULL;
        unsigned int i, file_len;
        char *buf = NULL;
        char preflag, last_ch_bits, abit;
        int fd;

        if (!outfile || !infile) return -1;
#ifdef Debug
        printf("%s : Before huffman_decomp_tree\n", __func__);
#endif
        if (huffman_decomp_tree(infile, &root, &buf, &file_len, &last_ch_bits)) {
                syslog(LOG_USER | LOG_ERR, "%s : fail to huffman_decomp_tree", __func__);
                // distory root
                if (buf) free(buf);
                if (root) free_tree(root); 
                return -1;
        }
#ifdef Debug
        printf("%s : After huffman_decomp_tree\n", __func__);
#endif
        if ((fd = open(outfile, O_RDWR | O_TRUNC | O_CREAT, 0644)) < 0) {
                if (buf) free(buf);
                if (root) free_tree(root); 
                return -1;
        }
        i = 0;
        preflag = ONE_CHAR; // current buf[i] remain to use.
        do {
                p = root;
                while (p->lchild) {
                        if (!(p->rchild)) {
#ifdef Debug
                                syslog(LOG_USER | LOG_INFO, "%s : ER p : %p, p->rchild : %p, p->oldcode=%c=0x%x",
                                        __func__, p, p->rchild, p->oldcode,p->oldcode & 0xff);
#endif
                                return -1;
                        } 
                        abit = (buf[i] >> (--preflag)) & 0x01;
                        if (abit) {
#ifdef Debug
                                syslog(LOG_USER | LOG_INFO, "%s : RR p : %p, p->rchild : %p, p->oldcode=%c=0x%x",
                                        __func__, p, p->rchild, p->oldcode,p->oldcode & 0xff);
                                syslog(LOG_USER | LOG_INFO, "%s : RR p : %p, p->lchild : %p", __func__, p, p->lchild);
                                if (p->oldcode == 0x0d) {
                                        while (p->lchild) {
                                                syslog(LOG_USER | LOG_INFO, "%s : xx p : %p, p->lchild : %p, p->rchild : %p, oldcode=0x%x",
                                                        __func__, p, p->lchild, p->rchild, p->oldcode&0xff);
                                                if (p->lchild) {
                                                        p=p->lchild;
                                                } else {
                                                        p=p->rchild;
                                                }
                                        }
                                        while (p->rchild) {
                                                syslog(LOG_USER | LOG_INFO, "%s : yy p : %p, p->lchild : %p, p->rchild : %p, oldcode=0x%x",
                                                        __func__, p, p->lchild, p->rchild, p->oldcode&0xff);
                                                p=p->rchild;
                                        }
                                        if (buf) free(buf);
                                        if (root) free_tree(root); 
                                        return -1;
                                }
#endif
                                p = p->rchild;
                        } else {
#ifdef Debug
                                syslog(LOG_USER | LOG_INFO, "%s : LL p : %p, p->rchild : %p, p->oldcode=%c=0x%x",
                                        __func__, p, p->rchild, p->oldcode,p->oldcode & 0xff);
                                syslog(LOG_USER | LOG_INFO, "%s : LL p : %p, p->lchild : %p", __func__, p, p->lchild);
                                if (p->oldcode == 0x0d) {
                                        while (p->lchild) {
                                                syslog(LOG_USER | LOG_INFO, "%s : xx p : %p, p->lchild : %p, p->rchild : %p, oldcode=0x%x",
                                                        __func__, p, p->lchild, p->rchild, p->oldcode&0xff);
                                                if (p->lchild) {
                                                        p=p->lchild;
                                                } else {
                                                        p=p->rchild;
                                                }
                                        }
                                        while (p->rchild) {
                                                syslog(LOG_USER | LOG_INFO, "%s : yy p : %p, p->lchild : %p, p->rchild : %p, oldcode=0x%x",
                                                        __func__, p, p->lchild, p->rchild, p->oldcode&0xff);
                                                p=p->rchild;
                                        }
                                        if (buf) free(buf);
                                        if (root) free_tree(root); 
                                        return -1;
                                }
#endif
                                p = p->lchild;
                        }
                        if (!preflag) {
                                ++i;
                                if (i < (file_len - 1)) {
                                        preflag = ONE_CHAR;
                                } else if (i == (file_len - 1)) {
                                        preflag = last_ch_bits;
                                } else {
                                        break;
                                }
                        }
                }
                if (p->rchild) {
                        syslog(LOG_USER | LOG_INFO, "%s : EL p : %p, p->rchild : %p, p->oldcode=%c=0x%x",
                                __func__, p, p->rchild, p->oldcode,p->oldcode & 0xff);
                                return -1;
                }
                if (!(p->lchild) && 1 != write(fd, &(p->oldcode), 1)) return -1;
        } while (i < file_len || preflag > 0);
        close(fd);
        if (buf) free(buf);
        if (root) free_tree(root); 

        return 0;
}

int huffman_compression(const char *outfile, char *src, unsigned int length, struct huffman_tags *tags)
{
        struct huffman_node *head = NULL;
        struct huffman_node *node = NULL;
        
        head = (struct huffman_node *)malloc(sizeof(struct huffman_node));

        if (!(node = head)) {
                syslog(LOG_SYSTEM | LOG_ERR , "%s : Fail to malloc for head", __func__);
                return -1;
        } else if (!src || !length) {
                syslog(LOG_USER | LOG_ERR , "%s : Ugly params. of file src", __func__);
                node_distory(&node);
                return -1;
        }
        // make the src in the huffman line
#ifdef Debug
        print_debug("start to huffman_line\n");
        syslog(LOG_USER | LOG_INFO, "%s : hello huffman line", __func__);
#endif
        if (huffman_line(&head, src, length)) {
                syslog(LOG_USER | LOG_ERR , "%s : Fail to huffman_line", __func__);
                node_distory(&node);
                return -1;
        }
        // sort the line
#ifdef Debug
        print_debug("start to huffman_sort, small first\n");
#endif
        if (huffman_sort(&head, HUFFMAN_SORT_SMALL_FIRST)) {
                syslog(LOG_USER | LOG_ERR , "%s : Fail to huffman_sort small first", __func__);
                node_distory(&node);
                return -1;
        }

#ifdef Debug
        printf("start to huffman_tree node : %p, head : %p\n", node, head);
        syslog(LOG_USER | LOG_INFO, "%s : Before huffman_tree", __func__);
#endif
        if (huffman_tree(&head)) {
                syslog(LOG_USER | LOG_ERR , "%s : Fail to huffman_tree", __func__);
                node_distory(&node);
                return -1;
        }
#ifdef Debug
        print_debug("After huffman_tree\n");
        syslog(LOG_USER | LOG_INFO, "%s : After huffman_tree", __func__);
        print_debug("start to huffman_code\n");
#endif
        if (huffman_code(head)) {
                syslog(LOG_USER | LOG_ERR , "%s : Fail to huffman_code", __func__);
                huffman_root_distory(head);
                return -1;
        }
        
#ifdef Debug
        print_debug("start to huffman_fill_file");
#endif
        if (huffman_fill_file(outfile, head, tags, src, length)) {
                syslog(LOG_USER | LOG_ERR , "%s : Fail to huffman_fill_file", __func__);
                return -1;
        } 
        // distory the node, no need any more. 
        huffman_root_distory(head);
#ifdef Debug
        print_debug("All goes well...\n");
#endif

        return 0;
}
