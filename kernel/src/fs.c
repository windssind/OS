
#include "klib.h"
#include "fs.h"
#include "disk.h"
#include "proc.h"

#ifdef EASY_FS

#define MAX_FILE (SECTSIZE / sizeof(dinode_t))
#define MAX_DEV 16
#define MAX_INODE (MAX_FILE + MAX_DEV)
#define NDIRECT 12

// On disk inode
typedef struct dinode
{
    uint16_t type;       // file type
    uint16_t link_count; // link count for hard link
    union
    {
        uint32_t device; // dev_id for device files
        uint32_t pipe;   // pipe addr for FIFO files
    };
    uint32_t size;               // file size
    uint32_t addrs[NDIRECT + 1]; // data block addresses, 12 direct and 1 first indirect and 1 second indirect

} dinode_t;

#define DINODE_NUM 128
typedef struct dinode_no
{
    dinode_t dinode;
    uint32_t no;
} dinode_no_t;

static dinode_no_t open_dinodes[DINODE_NUM];

// On OS inode, dinode with special info
struct inode
{
    int valid;
    int type;
    int dev; // dev_id if type==TYPE_DEV
    dinode_t *dinode;
};

static inode_t inodes[MAX_INODE];

void init_fs()
{
    dinode_t buf[MAX_FILE];
    read_disk(buf, 256);
    for (int i = 0; i < MAX_FILE; ++i)
    {
        inodes[i].valid = 1;
        inodes[i].type = TYPE_FILE;
        inodes[i].dinode = &buf[i];
    }
}

inode_t *iopen(const char *path, int type)
{
    for (int i = 0; i < MAX_INODE; ++i)
    {
        if (!inodes[i].valid)
            continue;
        if (strcmp(path, inodes[i].dinode->addrs) == 0)
        {
            return &inodes[i];
        }
    }
    return NULL;
}

int iread(inode_t *inode, uint32_t off, void *buf, uint32_t len)
{
    assert(inode);
    char *cbuf = buf;
    char dbuf[SECTSIZE];
    uint32_t curr = -1;
    uint32_t total_len = inode->dinode->size;

    // ?? WTF
    uint32_t st_sect = 0;
    int i;
    for (i = 0; i < len && off < total_len; ++i, ++off)
    {
        if (curr != off / SECTSIZE)
        {
            read_disk(dbuf, st_sect + off / SECTSIZE);
            curr = off / SECTSIZE;
        }
        *cbuf++ = dbuf[off % SECTSIZE];
    }
    return i;
}

void iadddev(const char *name, int id)
{
    assert(id < MAX_DEV);
    inode_t *inode = &inodes[MAX_FILE + id];
    inode->valid = 1;
    inode->type = TYPE_DEV;
    inode->dev = id;
    strcpy(inode->dinode->addrs, name);
}

uint32_t isize(inode_t *inode)
{
    return inode->dinode->size;
}

int itype(inode_t *inode)
{
    return inode->type;
}

uint32_t ino(inode_t *inode)
{
    return inode - inodes;
}

int idevid(inode_t *inode)
{
    return inode->type == TYPE_DEV ? inode->dev : -1;
}

int iwrite(inode_t *inode, uint32_t off, const void *buf, uint32_t len)
{
    panic("write doesn't support");
}

void itrunc(inode_t *inode)
{
    panic("trunc doesn't support");
}

inode_t *idup(inode_t *inode)
{
    return inode;
}

void iclose(inode_t *inode) { /* do nothing */ }

int iremove(const char *path)
{
    panic("remove doesn't support");
}

#else

#define DISK_SIZE (128 * 1024 * 1024)
#define BLK_NUM (DISK_SIZE / BLK_SIZE)

#define NDIRECT 12
#define NINDIRECT (BLK_SIZE / sizeof(uint32_t))

#define IPERBLK (BLK_SIZE / sizeof(dinode_t)) // inode num per blk

// super block
typedef struct super_block
{
    uint32_t bitmap; // block num of bitmap
    uint32_t istart; // start block no of inode blocks
    uint32_t inum;   // total inode num
    uint32_t root;   // inode no of root dir
} sb_t;

// On disk inode
typedef struct dinode
{
    uint16_t type;       // file type
    uint16_t link_count; // link count for hard link
    union
    {
        uint32_t device; // dev_id for device files
        uint32_t pipe;   // pipe addr for FIFO files
    };
    uint32_t size;               // file size
    uint32_t addrs[NDIRECT + 1]; // data block addresses, 12 direct and 1 first indirect and 1 second indirect
} dinode_t;

#define DINODE_NUM 128
typedef struct dinode_no
{
    dinode_t dinode;
    uint32_t no;
} dinode_no_t;

static dinode_no_t open_dinodes[DINODE_NUM];

struct inode
{
    int no;
    int ref;
    int del;
    dinode_t *dinode;
};

#define SUPER_BLOCK 32
static sb_t sb;

void init_fs()
{
    bread(&sb, sizeof(sb), SUPER_BLOCK, 0);
}

#define I2BLKNO(no) (sb.istart + no / IPERBLK)
#define I2BLKOFF(no) ((no % IPERBLK) * sizeof(dinode_t))

static void diread(dinode_t *di, uint32_t no)
{
    bread(di, sizeof(dinode_t), I2BLKNO(no), I2BLKOFF(no));
}

static void diwrite(const dinode_t *di, uint32_t no)
{
    bwrite(di, sizeof(dinode_t), I2BLKNO(no), I2BLKOFF(no));
}

static uint32_t dialloc(int type)
{
    // Lab3-2: iterate all dinode, find a empty one (type==TYPE_NONE)
    // set type, clean other infos and return its no (remember to write back)
    // if no empty one, just abort
    // note that first (0th) inode always unused, because dirent's inode 0 mark invalid
    dinode_t dinode;
    for (uint32_t i = 1; i < sb.inum; ++i)
    {
        diread(&dinode, i);
        if (dinode.type == TYPE_NONE)
        {
            // memset(&dinode.addrs, 0, NDIRECT + 1); // 这一步其实是多余的，因为我们回收的时候已经free掉这些内容了
            dinode.type = type;
            dinode.link_count = 1;
            // dinode.device = -1;
            diwrite(&dinode, i);
            return i;
        }
        memset(&dinode, 0, sizeof dinode);
    }
    assert(0);
}

static void difree(uint32_t no)
{
    dinode_t dinode;
    memset(&dinode, 0, sizeof dinode);
    diwrite(&dinode, no);
}

static uint32_t balloc()
{
    // Lab3-2: iterate bitmap, find one free block
    // set the bit, clean the blk (can call bzero) and return its no
    // if no free block, just abort
    uint32_t byte;
    for (int i = 0; i < BLK_NUM / 32; ++i)
    {
        bread(&byte, 4, sb.bitmap, i * 4);
        if (byte != 0xffffffff)
        {
            for (int j = 0; j < 32; ++j)
            {
                if ((byte & (1u << j)) == 0)
                {
                    bzero(32 * i + j);
                    byte |= (1u << j);
                    bwrite(&byte, 4, sb.bitmap, i * 4);
                    return i * 32 + j;
                }
            }
        }
    }
    assert(0);
}

static void bfree(uint32_t blkno)
{
    // Lab3-2: clean the bit of blkno in bitmap
    assert(blkno >= 64); // cannot free first 64 block
    uint32_t byte;
    bread(&byte, 4, sb.bitmap, (blkno / 32) * 4);
    byte &= ~(1u << (blkno % 32));
    bwrite(&byte, 4, sb.bitmap, (blkno / 32) * 4);
}

#define INODE_NUM 128
static inode_t inodes[INODE_NUM]; // active inode -> inodes we can open

static inode_t *iget(uint32_t no)
{
    // Lab3-2
    // if there exist one inode whose no is just no, inc its ref and return it
    // otherwise, find a empty inode slot, init it and return it
    // if no empty inode slot, just abort
    for (int i = 0; i < INODE_NUM; ++i)
    {
        if (inodes[i].no == no && inodes[i].ref > 0)
        {
            inodes[i].ref += 1;
            return &inodes[i];
        }
    }

    for (int i = 0; i < INODE_NUM; ++i)
    {
        if (inodes[i].ref == 0)
        {
            inodes[i].ref = 1;
            inodes[i].del = 0;
            inodes[i].no = no;

            // 从open_dinodes找到一个空闲的dinode
            for (int j = 0; j < DINODE_NUM; ++j)
            {
                if (open_dinodes[j].no == no)
                {
                    inodes[i].dinode = &(open_dinodes[j].dinode);
                    diread(&(open_dinodes[j].dinode), no);
                    return &inodes[i];
                }
            }

            // 没找到，分配一个slot，然后再读取
            for (int j = 0; j < DINODE_NUM; ++j)
            {
                if (open_dinodes[j].no == 0)
                {
                    dinode_no_t *dinode_no = &open_dinodes[j];
                    dinode_no->no = no;
                    inodes[i].dinode = &(dinode_no->dinode);
                    diread(&(open_dinodes[j].dinode), no);
                    return &inodes[i];
                }
            }
        }
    }
    return NULL;
}

static void iupdate(inode_t *inode)
{
    // Lab3-2: sync the inode->dinode to disk
    // call me EVERYTIME after you edit inode->dinode
    diwrite(inode->dinode, inode->no);
}

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return NULL.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = NULL
//
static const char *skipelem(const char *path, char *name)
{
    const char *s;
    int len;
    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = path - s;
    if (len >= MAX_NAME)
    {
        memcpy(name, s, MAX_NAME);
        name[MAX_NAME] = 0;
    }
    else
    {
        memcpy(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

static void idirinit(inode_t *inode, inode_t *parent)
{
    // Lab3-2: init the dir inode, i.e. create . and .. dirent
    assert(inode->dinode->type == TYPE_DIR);
    assert(parent->dinode->type == TYPE_DIR); // both should be dir
    assert(inode->dinode->size == 0);         // inode shoule be empty
    dirent_t dirent;
    // set .
    dirent.inode = inode->no;
    strcpy(dirent.name, ".");
    iwrite(inode, 0, &dirent, sizeof dirent);
    // set ..
    dirent.inode = parent->no;
    strcpy(dirent.name, "..");
    iwrite(inode, sizeof dirent, &dirent, sizeof dirent);
}

static inode_t *ilookup(inode_t *parent, const char *name, uint32_t *off, int type)
{
    // Lab3-2: iterate the parent dir, find a file whose name is name
    // if off is not NULL, store the offset of the dirent_t to it
    // if no such file and type == TYPE_NONE, return NULL
    // if no such file and type != TYPE_NONE, create the file with the type
    assert(parent->dinode->type == TYPE_DIR); // parent must be a dir
    dirent_t dirent;
    uint32_t size = parent->dinode->size, empty = size;
    for (uint32_t i = 0; i < size; i += sizeof dirent)
    {
        // directory is a file containing a sequence of dirent structures
        iread(parent, i, &dirent, sizeof dirent);
        if (dirent.inode == 0)
        {
            // a invalid dirent, record the offset (used in create file), then skip
            if (empty == size)
                empty = i;
            continue;
        }
        // a valid dirent, compare the name
        // TODO();

        // find name
        if (strcmp(dirent.name, name) == 0)
        {
            if (off)
                *off = i;
            return iget(dirent.inode);
        }
        memset(&dirent, 0, sizeof dirent);
    }
    // not found
    if (type == TYPE_NONE)
        return NULL;
    // need to create the file, first alloc inode, then init dirent, write it to parent
    // if you create a dir, remember to init it's . and ..
    // TODO();

    uint32_t inode_no = dialloc(type);
    inode_t *new_inode = iget(inode_no);
    if (type == TYPE_DIR)
        idirinit(new_inode, parent);

    dirent.inode = inode_no;
    strncpy(dirent.name, name, MAX_NAME);

    iwrite(parent, empty, &dirent, sizeof dirent);
    if (off)
        *off = empty;

    return new_inode;
}

static inode_t *iopen_parent(const char *path, char *name)
{
    // Lab3-2: open the parent dir of path, store the basename to name
    // if no such parent, return NULL
    inode_t *ip, *next;
    // set search starting inode
    if (path[0] == '/')
    {
        ip = iget(sb.root);
    }
    else
    {
        ip = idup(proc_curr()->group_leader->cwd);
    }
    assert(ip);
    while ((path = skipelem(path, name)))
    {
        // curr round: need to search name in ip
        if (ip->dinode->type != TYPE_DIR)
        {
            // not dir, cannot search
            iclose(ip);
            return NULL;
        }
        if (*path == 0)
        {
            // last search, return ip because we just need parent
            return ip;
        }
        // not last search, need to continue to find parent
        next = ilookup(ip, name, NULL, 0);
        if (next == NULL)
        {
            // name not exist
            iclose(ip);
            return NULL;
        }
        iclose(ip);
        ip = next;
    }
    iclose(ip);
    return NULL;
}

inode_t *iopen(const char *path, int type)
{
    // Lab3-2: if file exist, open and return it
    // if file not exist and type==TYPE_NONE, return NULL
    // if file not exist and type!=TYPE_NONE, create the file as type
    char name[MAX_NAME + 1];
    memset(name, 0, MAX_NAME);
    if (skipelem(path, name) == NULL)
    {
        // no parent dir for path, path is "" or "/"
        // "" is an invalid path, "/" is root dir
        return path[0] == '/' ? iget(sb.root) : NULL;
    }
    // path do have parent, use iopen_parent and ilookup to open it
    // remember to close the parent inode after you ilookup it
    // TODO();
    inode_t *parent_inode = iopen_parent(path, name);
    if (!parent_inode)
        return NULL;
    inode_t *curr_inode = ilookup(parent_inode, name, NULL, type);
    iclose(parent_inode);
    return curr_inode;
}

static uint32_t iwalk(inode_t *inode, uint32_t no)
{
    // return the blkno of the file's data's no th block, if no, alloc it
    if (no < NDIRECT)
    {
        int blk_no = inode->dinode->addrs[no];
        if (blk_no == 0)
        {
            blk_no = balloc();
            inode->dinode->addrs[no] = blk_no;
            iupdate(inode);
        }
        // direct address
        return blk_no;
    }
    no -= NDIRECT;
    if (no < NINDIRECT)
    {
        // indirect address
        // 获得间接索引的第一个
        int head_no = inode->dinode->addrs[NDIRECT];
        if (head_no == 0)
        {
            // 说明索引还没有创建
            int blk_no = balloc();
            inode->dinode->addrs[NDIRECT] = blk_no;
            iupdate(inode);
        }

        int indirect_blk_no = inode->dinode->addrs[NDIRECT];
        // 获取对应的逻辑块
        int blk_no; // 这个是要返回的最终值
        bread(&blk_no, 4, indirect_blk_no, no * sizeof blk_no);
        if (blk_no == 0)
        {
            blk_no = balloc();
            bwrite(&blk_no, 4, indirect_blk_no, no * 4);
            iupdate(inode);
        }
        return blk_no;
    }
    assert(0); // file too big, not need to handle this case
}

int iread(inode_t *inode, uint32_t off, void *buf, uint32_t len)
{
    // Lab3-2: read the inode's data [off, MIN(off+len, size)) to buf
    // use iwalk to get the blkno and read blk by blk
    // TODO();

    if (off > inode->dinode->size)
        return -1;
    uint32_t read = 0;
    len = MIN(len, inode->dinode->size - off);

    while (read < len)
    {
        uint32_t blk_index = off / BLK_SIZE;
        uint32_t blk_offset = off % BLK_SIZE;

        uint32_t blk_no = iwalk(inode, blk_index);

        uint32_t to_read = MIN(BLK_SIZE - blk_offset, len - read);
        bread(buf, to_read, blk_no, blk_offset);

        read += to_read;
        off += to_read;
        buf += to_read;
    }

    return read;
}

int iwrite(inode_t *inode, uint32_t off, const void *buf, uint32_t len)
{
    // Lab3-2: write buf to the inode's data [off, off+len)
    // if off>size, return -1 (can not cross size before write)
    // if off+len>size, update it as new size (but can cross size after write)
    // use iwalk to get the blkno and read blk by blk
    // TODO();
    uint32_t written = 0;

    while (written < len)
    {
        uint32_t blk_index = off / BLK_SIZE;
        uint32_t blk_offset = off % BLK_SIZE;
        uint32_t to_write = MIN(BLK_SIZE - blk_offset, len - written);

        uint32_t blk_no = iwalk(inode, blk_index);

        bwrite(buf, to_write, blk_no, blk_offset);

        written += to_write;
        buf += to_write;
        off += to_write;
    }

    if (off > inode->dinode->size)
    {
        inode->dinode->size = off;
        iupdate(inode);
    }

    return written;
}

void itrunc(inode_t *inode)
{
    // Lab3-2: free all data block used by inode (direct and indirect)
    // mark all address of inode 0 and mark its size 0
    for (int i = 0; i < NDIRECT; ++i)
    {
        if (inode->dinode->addrs[i] != 0)
        {
            bfree(inode->dinode->addrs[i]);
            inode->dinode->addrs[i] = 0;
        }
    }

    // 如果还有间接索引，也要清空
    if (inode->dinode->addrs[NDIRECT] != 0)
    {
        for (int i = 0; i < BLK_SIZE / sizeof(uint32_t); ++i)
        {
            uint32_t blk_no = 0;
            bread(&blk_no, sizeof blk_no, inode->dinode->addrs[NDIRECT], i * sizeof blk_no);
            if (blk_no != 0)
            {
                bfree(blk_no);
                uint32_t zero = 0;
                bwrite(&zero, sizeof(uint32_t), blk_no, i * sizeof blk_no);
            }
        }
    }

    inode->dinode->size = 0;
    iupdate(inode);
}

inode_t *idup(inode_t *inode)
{
    assert(inode);
    inode->ref += 1;
    return inode;
}

void iclose(inode_t *inode)
{

    assert(inode);
    if (inode->ref == 1 && inode->del)
    {
        if (inode->dinode->type == TYPE_FIFO)
            rmfifo(inode);
        else
            itrunc(inode);
        difree(inode->no);
        for (int i = 0; i < DINODE_NUM; i++)
        { // 在活动dinode数组中找到对应的项并释放它
            if (&(open_dinodes[i].dinode) == inode->dinode)
            {
                open_dinodes[i].no = 0;
                inode->dinode = NULL;
            }
        }
    }
    inode->ref -= 1;
}

uint32_t isize(inode_t *inode)
{
    return inode->dinode->size;
}

int itype(inode_t *inode)
{
    return inode->dinode->type;
}

uint32_t ino(inode_t *inode)
{
    return inode->no;
}

int idevid(inode_t *inode)
{
    return itype(inode) == TYPE_DEV ? inode->dinode->device : -1;
}

void iadddev(const char *name, int id)
{
    inode_t *ip = iopen(name, TYPE_DEV);
    assert(ip);
    ip->dinode->device = id;
    iupdate(ip);
    iclose(ip);
}

static int idirempty(inode_t *inode)
{
    // Lab3-2: return whether the dir of inode is empty
    // the first two dirent of dir must be . and ..
    // you just need to check whether other dirent are all invalid
    assert(inode->dinode->type == TYPE_DIR);
    // TODO();

    dirent_t dirent;

    for (uint32_t addr = sizeof dirent * 2; addr < inode->dinode->size; addr += sizeof dirent)
    {
        iread(inode, addr, &dirent, sizeof dirent);
        if (dirent.inode != 0)
            return 0;
    }

    return 1;
}

int iremove(const char *path)
{
    // Lab3-2: remove the file, return 0 on success, otherwise -1
    // first open its parent, if no parent, return -1
    // then find file in parent, if not exist, return -1
    // if the file need to remove is a dir, only remove it when it's empty
    // . and .. cannot be remove, so check name set by iopen_parent
    // remove a file just need to clean the dirent points to it and set its inode's del
    // the real remove will be done at iclose, after everyone close it
    // TODO();
    char name[MAX_NAME + 1];
    memset(name, 0, MAX_NAME);
    inode_t *parent_inode = iopen_parent(path, name);

    if (parent_inode == NULL)
        return -1;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        iclose(parent_inode);
        return -1;
    }

    uint32_t off = 0;
    inode_t *curr_inode = ilookup(parent_inode, name, &off, TYPE_NONE);
    if (curr_inode == NULL)
    {
        iclose(parent_inode);
        return -1;
    }

    if (curr_inode->dinode->type == TYPE_DIR)
    {
        if (!idirempty(curr_inode))
        {
            iclose(parent_inode);
            iclose(curr_inode);
            return -1;
        }
    }

    // 到这一步才是真的找到要删除的文件了
    curr_inode->del = 1;

    curr_inode->dinode->link_count -= 1;
    if (curr_inode->dinode->link_count == 0)
    {
        curr_inode->del = 1;
    }
    // 这个是操作系统项的清空，和dinode的link_count无关。iclose会在inode的ref为空的时候清理dinoe。
    dirent_t dirent;
    memset(&dirent, 0, sizeof dirent);

    iwrite(parent_inode, off, &dirent, sizeof dirent);

    iclose(parent_inode);
    iclose(curr_inode);
    return 0;
}

inode_t *ilink(const char *path, inode_t *old_inode)
{
    char name[MAX_NAME + 1];
    memset(name, 0, MAX_NAME);
    inode_t *parent = iopen_parent(path, name);

    dirent_t dirent;
    uint32_t size = parent->dinode->size, empty = size;

    // find a invalid dirent
    for (uint32_t i = 0; i < size; i += sizeof dirent)
    {
        // directory is a file containing a sequence of dirent structures
        iread(parent, i, &dirent, sizeof dirent);
        if (dirent.inode == 0)
        {
            // a invalid dirent, record the offset (used in create file), then skip
            if (empty == size)
            {

                empty = i;
            }
            continue;
        }
        // a valid dirent, compare the name
        // TODO();

        // find name
        if (strcmp(dirent.name, name) == 0)
        {
            return NULL;
        }
        memset(&dirent, 0, sizeof dirent);
    }
    // need to create the file, first alloc inode, then init dirent, write it to parent
    // if you create a dir, remember to init it's . and ..
    // TODO();

    // empty shows where the empty dirent is
    // shows no empty dirent
    // 确保到了这里都找到了一个empty的
    // 开一个inode，两个inode指向同一个dinode
    inode_t *new_inode = NULL;
    for (int i = 0; i < INODE_NUM; ++i)
    {
        if (inodes[i].ref == 0)
        {
            inodes[i].ref = 1;
            inodes[i].del = 0;
            inodes[i].no = old_inode->no;
            new_inode = &inodes[i];
            break;
        }
    }
    assert(new_inode);
    new_inode->dinode = old_inode->dinode;
    new_inode->dinode->link_count += 1;

    // 写回文件目录
    dirent.inode = new_inode->no;
    strncpy(dirent.name, name, MAX_NAME);
    iwrite(parent, empty, &dirent, sizeof dirent);
    return new_inode;
}

int ififoaddr(inode_t *inode)
{
    return (int)(inode->dinode->pipe);
}
void isetfifo(inode_t *ip, void *pipe)
{
    ip->dinode->pipe = (uint32_t)pipe; // 磁盘存储内存地址?
    iupdate(ip);
}

#endif
