#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <libgen.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((noreturn)) void panic(const char *msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

#define TODO() panic("implement me")

// Disk layout:
//         [ boot.img | kernel.img |                      user.img                      ]
//         [   mbr    |   kernel   | super block | bit map | inode blocks | data blocks ]
// sect    0          1          256           264       272            512        262144
// block   0                      32            33        34             64         32768
// YOUR TASK: build user.img

#define DISK_SIZE (128 * 1024 * 1024)             // disk is 128 MiB
#define BLK_SIZE 4096                             // combine 8 sects to 1 block
#define BLK_OFF 32                                // user img start from 256th sect, i.e. 32th block
#define IMG_SIZE (DISK_SIZE - BLK_OFF * BLK_SIZE) // size of user.img
#define IMG_BLK (IMG_SIZE / BLK_SIZE)
#define BLK_NUM (DISK_SIZE / BLK_SIZE)

#define SUPER_BLK BLK_OFF         // block no of super block
#define BITMAP_BLK (BLK_OFF + 1)  // block no of bitmap
#define INODE_START (BLK_OFF + 2) // start block no of inode blocks
#define DATA_START (BLK_OFF + 32) // start block no of data blocks

#define IPERBLK (BLK_SIZE / sizeof(dinode_t)) // inode num per blk
#define INODE_NUM ((DATA_START - INODE_START) * IPERBLK)

#define NDIRECT 12
#define NINDIRECT (BLK_SIZE / sizeof(uint32_t))

#define TYPE_NONE 0
#define TYPE_FILE 1
#define TYPE_DIR 2
#define TYPE_DEV 3

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

// block
typedef union
{
  uint8_t u8buf[BLK_SIZE];
  uint32_t u32buf[BLK_SIZE / 4];
} blk_t;

// super block
typedef struct
{
  uint32_t bitmap; // block num of bitmap
  uint32_t istart; // start block no of inode blocks
  uint32_t inum;   // total inode num
  uint32_t root;   // inode no of root dir
} sb_t;

// on-disk inode
typedef struct
{
  uint32_t type;               // file type
  uint32_t device;             // if it is a dev, its dev_id
  uint32_t size;               // file size
  uint32_t addrs[NDIRECT + 1]; // data block addresses, 12 direct and 1 indirect
} dinode_t;

// directory is a file containing a sequence of dirent structures

#define MAX_NAME (31 - sizeof(uint32_t))
typedef struct
{
  uint32_t inode;          // inode no of the file
  char name[MAX_NAME + 1]; // name of the file
} dirent_t;

struct
{
  blk_t blocks[IMG_BLK];
} *img;         // pointor to the img mapped memory
sb_t *sb;       // pointor to the super block
blk_t *bitmap;  // pointor to the bitmap block
dinode_t *root; // pointor to the root dir's inode

// get the pointer to the memory of block no
static inline blk_t *bget(uint32_t no)
{
  assert(no >= BLK_OFF);
  return &(img->blocks[no - BLK_OFF]);
}

// get the pointer to the memory of inode no
static inline dinode_t *iget(uint32_t no)
{
  return (dinode_t *)&(bget(no / IPERBLK + INODE_START)->u8buf[(no % IPERBLK) * sizeof(dinode_t)]);
}

void init_disk();
uint32_t balloc();
uint32_t ialloc(int type);
blk_t *iwalk(dinode_t *file, uint32_t blk_no);
void iappend(dinode_t *file, const void *buf, uint32_t size);
void add_file(char *path);

int main(int argc, char *argv[])
{
  // argv[1] is target user.img, argv[2..argc-1] are files you need to add
  assert(argc > 2);
  static_assert(BLK_SIZE % sizeof(dinode_t) == 0, "sizeof inode should divide BLK_SIZE");
  char *target = argv[1];
  int tfd = open(target, O_RDWR | O_CREAT | O_TRUNC, 0777);
  if (tfd < 0)
    panic("open target error");
  if (ftruncate(tfd, IMG_SIZE) < 0)
    panic("truncate error");
  // map the img to memory, you can edit file by edit memory
  img = mmap(NULL, IMG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, tfd, 0);
  assert(img != (void *)-1);
  init_disk();
  for (int i = 2; i < argc; ++i)
  {
    add_file(argv[i]);
  }
  munmap(img, IMG_SIZE);
  close(tfd);
  return 0;
}

void init_disk()
{
  sb = (sb_t *)bget(SUPER_BLK);
  sb->bitmap = BITMAP_BLK;
  sb->istart = INODE_START;
  sb->inum = INODE_NUM;
  bitmap = bget(BITMAP_BLK);
  // mark first 64 blocks used
  bitmap->u32buf[0] = bitmap->u32buf[1] = 0xffffffff;
  // alloc and init root inode
  sb->root = ialloc(TYPE_DIR);
  root = iget(sb->root);
  // set root's . and ..
  dirent_t dirent;
  dirent.inode = sb->root;
  strcpy(dirent.name, ".");
  iappend(root, &dirent, sizeof dirent);
  strcpy(dirent.name, "..");
  iappend(root, &dirent, sizeof dirent);
}

uint32_t balloc()
{
  // alloc a unused block, mark it on bitmap, then return its no
  static uint32_t next_blk = 64;
  if (next_blk >= BLK_NUM)
    panic("no more block");
  bitmap->u8buf[next_blk / 8] |= (1 << (next_blk % 8));
  return next_blk++;
}

uint32_t ialloc(int type)
{
  // alloc a unused inode, return its no
  // first inode always unused, because dirent's inode 0 mark invalid
  static uint32_t next_inode = 1;
  if (next_inode >= INODE_NUM)
    panic("no more inode");
  iget(next_inode)->type = type;
  return next_inode++;
}

// 给出文件，以及要查阅的文件的第几个索引
blk_t *iwalk(dinode_t *file, uint32_t blk_no)
{
  // return the pointer to the file's data's blk_no th block, if no, alloc it
  if (blk_no < NDIRECT)
  {
    // direct address
    if (file->addrs[blk_no] != 0)
    {
      return bget(file->addrs[blk_no]);
    }
    int d_blk_no = balloc();
    file->addrs[blk_no] = d_blk_no;
    return bget(d_blk_no);
  }
  blk_no -= NDIRECT;
  if (blk_no < NINDIRECT)
  {
    // indirect address
    // 先找到第一个索引块
    // if (file->addrs[NDIRECT] == 0){
    //   // 还没有间接索引,就添加一块逻辑块
    //   int d_blk_no = balloc();
    //   file->addrs[NDIRECT] = d_blk_no;
    //   return bget(d_blk_no);
    // }

    // // 已经有了
    // blk_t * i_dirent_blk = bget(blk_no);
    // return bget(i_dirent_blk->u32buf[blk_no]);
    // 画一个示意图就清晰了
    if (file->addrs[NINDIRECT] == 0)
      file->addrs[NDIRECT] = balloc();

    blk_t *i_blk = bget(file->addrs[NDIRECT]);
    int d_blk_no = i_blk->u32buf[blk_no];
    if (d_blk_no == 0)
    {
      d_blk_no = balloc();
      i_blk->u32buf[blk_no] = d_blk_no;
    }
    return bget(d_blk_no);
  }
  panic("file too big");
}

void iappend(dinode_t *file, const void *buf, uint32_t size)
{
  // append buf to file's data, remember to add file->size
  // you can append block by block
  int left_size = size;
  while (left_size > 0){
    int w_blk_no = file->size / BLK_SIZE;
    int offset = file->size % BLK_SIZE;
    blk_t * blk = iwalk(file,w_blk_no);
    int w_size = MIN(left_size,BLK_SIZE - offset);
    memcpy(&blk->u8buf[offset],buf,w_size);

    // 修改文件数据
    file->size += w_size;
    left_size -= w_size;
    buf += w_size;
  }
}

// 添加文件到磁盘根目录里面
void add_file(char *path)
{
  static uint8_t buf[BLK_SIZE];
  FILE *fp = fopen(path, "rb");
  if (!fp)
    panic("file not exist");
  // alloc a inode
  uint32_t inode_blk = ialloc(TYPE_FILE);
  dinode_t *inode = iget(inode_blk);
  // append dirent to root dir
  dirent_t dirent;
  dirent.inode = inode_blk;
  strcpy(dirent.name, basename(path));
  iappend(root, &dirent, sizeof dirent); // 将文件名写入根目录
  // write the file's data, first read it to buf then call iappend
  
  int read_bytes = 0;
  while ((read_bytes = fread(buf,1,BLK_SIZE,fp))){
    iappend(inode,buf,read_bytes);
  }
  fclose(fp);
}


