#include "klib.h"
#include "file.h"
#include "proc.h"
#include "vme.h"

#define TOTAL_FILE 128

file_t files[TOTAL_FILE];

#define TOTAL_PIPE 32

static pipe_t pipes[TOTAL_PIPE];
static pipe_t *pipe_alloc()
{
    // 找到一个空的管道并返回
    for (int i = 0; i < TOTAL_PIPE; ++i)
    {
        if (!pipes[i].write_open && !pipes[i].read_open)
        {
            return &pipes[i];
        }
    }
    assert(false);
    return NULL;
}

void pipe_init(pipe_t *pipe)
{
    // 初始化分配的管道
    pipe->read_open = 1;
    pipe->write_open = 1;
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->empty = PIPE_SIZE;
    pipe->full = 0;
    sem_init(&pipe->mutex, 1);
    sem_init(&pipe->cv_buf, 0);
    pipe->no = 0;
}
static file_t *falloc()
{
    // Lab3-1: find a file whose ref==0, init it, inc ref and return it, return NULL if none
    for (int i = 0; i < TOTAL_FILE; ++i)
    {
        if (files[i].ref == 0)
        {
            files[i].type = TYPE_NONE;
            ++files[i].ref;
            return &files[i];
        }
    }

    return NULL;
}

// 通过磁盘文件系统的dinode，返回一个用户可见的file_t*回去
file_t *fopen(const char *path, int mode, int depth)
{
    // printf("path = %s\n", path);
    file_t *fp = falloc();
    inode_t *ip = NULL;
    if (!fp)
        goto bad;
    // TODO: Lab3-2, determine type according to mode
    // iopen in Lab3-2: if file exist, open and return it
    //       if file not exist and type==TYPE_NONE, return NULL
    //       if file not exist and type!=TYPE_NONE, create the file as type
    // you can ignore this in Lab3-1
    int open_type = 0;
    if (!(mode & O_CREATE))
        open_type = TYPE_NONE;
    else if (mode & O_DIR)
        open_type = TYPE_DIR;
    else
        open_type = TYPE_FILE;
    ip = iopen(path, open_type);
    if (!ip)
        goto bad;
    int type = itype(ip);
    if (type == TYPE_FILE || type == TYPE_DIR)
    {
        // TODO: Lab3-2, if type is not DIR, go bad if mode&O_DIR
        if (type != TYPE_DIR && (mode & O_DIR))
            goto bad;

        // TODO: Lab3-2, if type is DIR, go bad if mode WRITE or TRUNC
        if (type == TYPE_DIR && (mode & (O_WRONLY | O_RDWR | O_TRUNC)))
            goto bad;
        // TODO: Lab3-2, if mode&O_TRUNC, trunc the file
        if (mode & O_TRUNC)
            itrunc(ip);

        fp->type = TYPE_FILE; // file_t don't and needn't distingush between file and dir
        fp->inode = ip;
        fp->offset = 0;
    }
    else if (type == TYPE_DEV)
    {
        fp->type = TYPE_DEV;
        fp->dev_op = dev_get(idevid(ip));
        iclose(ip);
        ip = NULL;
    }
    else if (type == TYPE_SYMLINK)
    {
        char buf[MAX_NAME + 1];
        iread(ip, 0, buf, MAX_NAME + 1);
        if (depth >= 40)
        {
            return NULL;
        }
        return fopen(buf, mode, depth + 1);
    }
    else if (type == TYPE_FIFO)
    {
        pipe_t *pipe = (pipe_t *)ififoaddr(ip);
        if (pipe->no != ino(ip))
        {
            pipe = pipe_alloc();
            pipe_init(pipe);
            isetfifo(ip, pipe);
            pipe->no = ino(ip);
        }

        fp->pipe = pipe;
        fp->type = TYPE_FIFO;
        fp->inode = ip;
    }
    else
    {
        // printf("type = %d, path = %s\n", type, path);
        assert(0);
    }
    fp->readable = !(mode & O_WRONLY);
    fp->writable = (mode & O_WRONLY) || (mode & O_RDWR);
    return fp;
bad:
    if (fp)
        fclose(fp);
    if (ip)
        iclose(ip);
    return NULL;
}

int fread(file_t *file, void *buf, uint32_t size)
{

    // Lab3-1, distribute read operation by file's type
    // remember to add offset if type is FILE (check if iread return value >= 0!)
    if (!file->readable)
        return -1;
    // check [buf, buf + size] write or read prot
    if (file->type == TYPE_PIPE || file->type == TYPE_FIFO) // 在iopen的时候，inode已经绑定了pipe了
    {
        return pipe_read(file, buf, size);
    }
    for (uint32_t addr = (uint32_t)buf; addr <= (uint32_t)buf + size; addr += PGSIZE)
    {
        PTE *pte = vm_walkpte(vm_curr(), addr, 0);
        assert(pte);
        if (pte->cow == 0)
            vm_pgfault(addr, 2);
    }

    // read
    int read_size = -1;
    if (file->type == TYPE_FILE)
    {
        // memset(buf, 0, size);
        read_size = iread(file->inode, file->offset, buf, size);
        if (read_size != -1)
            file->offset += read_size;
    }
    else if (file->type == TYPE_DEV)
        read_size = file->dev_op->read(buf, size);

    return read_size;
}

int fwrite(file_t *file, const void *buf, uint32_t size)
{

    // Lab3-1, distribute write operation by file's type
    // remember to add offset if type is FILE (check if iwrite return value >= 0!)
    if (!file->writable)
        return -1;
    // TODO();
    if (file->type == TYPE_PIPE || file->type == TYPE_FIFO)
    {
        return pipe_write(file, buf, size);
    }
    int write_size = -1;
    if (file->type == TYPE_FILE)
    {
        write_size = iwrite(file->inode, file->offset, buf, size);
        if (write_size != -1)
            file->offset += write_size;
    }
    else if (file->type == TYPE_DEV)
        write_size = file->dev_op->write(buf, size);

    return write_size;
}

uint32_t fseek(file_t *file, uint32_t off, int whence)
{
    // Lab3-1, change file's offset, do not let it cross file's size
    if (file->type == TYPE_FILE)
    {
        // TODO();
        switch (whence)
        {
        case SEEK_SET:
            file->offset = off;
            return file->offset;
            break;
        case SEEK_CUR:
            file->offset += off;
            return file->offset;
            break;
        case SEEK_END:
            int file_size = isize(file->inode);
            file->offset = (file_size + off) > file_size ? file_size : (file_size + off);
            return file->offset;
        default:
            break;
        }
    }
    return -1;
}

file_t *fdup(file_t *file)
{
    // Lab3-1, inc file's ref, then return itself
    // TODO();
    ++file->ref;
    return file;
}

void fclose(file_t *file)
{
    // Lab3-1, dec file's ref, if ref==0 and it's a file, call iclose
    // TODO();

    // printf("enter fclose\n");
    // printf("file->type = %d\n", file->type);
    printf("child_ : pid = %d   before close, file->ref = %d file->addr = %p\n",proc_curr()->pid,file->ref,file);
    --file->ref;
    if (file->ref == 0)
    {
        if (file->type == TYPE_FILE || file->type == TYPE_FIFO)
            iclose(file->inode);
        else if (file->type == TYPE_PIPE)
        {
                        printf("child_ : enter pipe_close_1\n");
            pipe_t *pipe = file->pipe;
            sem_p(&pipe->mutex);

            if (file->readable)
                pipe->read_open = 0;
            if (file->writable)
                pipe->write_open = 0;
            

            if (!pipe->write_open && !pipe->read_open){
                            printf("child_ : enter pipe_close_2\n");
                pipe_close(pipe);
            }

            cv_signal(&pipe->cv_buf);
            sem_v(&pipe->mutex);
        }
    }
}

int flink(const char *oldpath, const char *newpath)
{
    inode_t *old_node = iopen(oldpath, TYPE_NONE);
    if (old_node == NULL)
        return -1;
    inode_t *new_inode = ilink(newpath, old_node);
    return new_inode == NULL ? 1 : 0;
}

int fsymlink(const char *oldpath, const char *newpath)
{
    // assert(oldpath);
    // file_t *file = fopen(newpath, TYPE_NONE);
    // if (file)
    // {
    //     fclose(file);
    //     return -1;
    // }
    // assert(!file);

    // // 将path写到文件中
    // char name[MAX_NAME + 1];
    // strncpy(name, oldpath, MAX_NAME + 1);
    // name[MAX_NAME] = '\0';
    // fwrite(file, name, MAX_NAME + 1);

    // return 1;

    // // 调用api错误，应该直接修改inode了
    // inode_t *new_node = iopen(newpath, TYPE_NONE);
    // // 如果原来就有，直接返回null
    // if (new_node)
    // {
    //     return -1;
    // }
    // new_node = iopen(newpath, TYPE_SYMLINK);
    // assert(new_node);
    // char name[MAX_NAME + 1];
    // memset(name, 0, MAX_NAME + 1);
    // strncpy(name, oldpath, MAX_NAME + 1);
    // iwrite(new_node, 0, name, MAX_NAME + 1);
    // return 0;
    if (iopen(newpath, TYPE_NONE))
        return -1;
    inode_t *new_inode = iopen(newpath, TYPE_SYMLINK);
    assert(new_inode);
    char name[MAX_NAME + 1];
    memset(name, 0, MAX_NAME + 1);

    strcpy(name, oldpath);
    iwrite(new_inode, 0, name, MAX_NAME + 1);

    return 0;
}

int pipe_open(file_t *pipe_files[2])
{
    pipe_t *pipe = pipe_alloc();
    if (!pipe)
        return -1;

    // alloc read_side and write_side
    file_t *read_side = falloc();
    file_t *write_side = falloc();
    if (!read_side || !write_side)
    {
        if (read_side)
            fclose(read_side);
        if (write_side)
            fclose(write_side);
        return -1;
    }
    assert(read_side && write_side);

    // 设置文件结构
    read_side->type = TYPE_PIPE;
    read_side->pipe = pipe;
    read_side->inode = NULL;
    read_side->dev_op = NULL;
    read_side->readable = 1;
    read_side->writable = 0;
    read_side->offset = 0;
    read_side->ref = 1;

    write_side->type = TYPE_PIPE;
    write_side->pipe = pipe;
    write_side->inode = NULL;
    write_side->dev_op = NULL;
    write_side->readable = 0;
    write_side->writable = 1;
    write_side->offset = 0;
    write_side->ref = 1;

    // 初始化管道结构
    pipe_init(pipe);

    pipe_files[0] = read_side;
    pipe_files[1] = write_side;

    assert(pipe_files[0] && pipe_files[1]);

    return 0;
}

void pipe_close(pipe_t *pipe)
{
    // TODO: WEEK11-link-pipe
    // printf("pipe_close\n",pipe->no);
    assert(pipe);
    pipe->read_open = 0;
    pipe->write_open = 0;
    memset(pipe->buffer, 0, PIPE_SIZE);
}

int pipe_write(file_t *file, const void *buf, uint32_t size)
{
    int written = 0;
    while (written < size)
    {
        sem_p(&file->pipe->mutex);
        if (file->pipe->write_open == 0)
        {
            // 已经关闭
            sem_v(&file->pipe->mutex);
            return -1;
        }
        assert(file->pipe->write_open);

        // 这里可能是有人读让empty空出来了，但是读完就写口关闭了
        if (file->pipe->read_open == 0)
        {
            sem_v(&file->pipe->mutex);
            return written;
        }

        while (!file->pipe->empty)
        {
            cv_wait(&file->pipe->cv_buf, &file->pipe->mutex);
            sem_p(&file->pipe->mutex); // 在下次查看条件前再次获取锁
        }

        assert(file->pipe->empty > 0);

        int write_once = 0;
        /* 此时条件一定满足 */
        for (int i = written; written < size && file->pipe->empty > 0; ++i)
        {
            // printf("write pos = %d  write: %c\n",file->pipe->write_pos,((char*)buf)[written]);
            file->pipe->buffer[file->pipe->write_pos] = ((char *)buf)[written];
            file->pipe->write_pos = (file->pipe->write_pos + 1) % PIPE_SIZE;
            file->pipe->empty--;
            file->pipe->full++;
            write_once += 1;
            written += 1;
        }

        if (write_once > 0)
        {
            // printf("write bytes = %d\n", write_once);
            cv_signal(&file->pipe->cv_buf);
        }
        sem_v(&file->pipe->mutex);
    }
    return written;
}

int pipe_read(file_t *file, void *buf, uint32_t size)
{
    int readden = 0;
    while (readden < size)
    {
        sem_p(&file->pipe->mutex);
        if (file->pipe->read_open == 0)
        {
            // 已经关闭
            sem_v(&file->pipe->mutex);
            return -1;
        }
        assert(file->pipe->write_open);

        // 这里可能是有人读让empty空出来了，但是读完就写口关闭了。写口关闭直接返回
        if (file->pipe->write_open == 0)
        {
            sem_v(&file->pipe->mutex);
            return readden;
        }

        assert(file->pipe->write_open);
        // 如果是没有东西可以读取，直接返回空
        while (!file->pipe->full)
        {
            printf("id_file = %p read wating...\n",file->pipe);
            cv_wait(&file->pipe->cv_buf, &file->pipe->mutex);
            sem_p(&file->pipe->mutex); // 在下次查看条件前再次获取锁
        }

        // 不要再在这后面添加

        assert(file->pipe->full > 0);
        // printf("id_file = %p  file -> pipe ->full = %d\n",file->pipe,file->pipe->full);
        // 此时读写口都没有结束，并且管道里面有东西，则一次性读完管道里所有的数据(在size范围内)
        for (int i = 0; i < size && file->pipe->full > 0; ++i)
        {
            // printf("read pos = %d  read: %c\n",file->pipe->read_pos,file->pipe->buffer[file->pipe->read_pos]);
            ((char *)buf)[i] = file->pipe->buffer[file->pipe->read_pos];
            file->pipe->read_pos = (file->pipe->read_pos + 1) % PIPE_SIZE;
            file->pipe->empty++;
            file->pipe->full--;
            readden += 1;
        }

        cv_signal(&file->pipe->cv_buf);
        sem_v(&file->pipe->mutex);
        break;
    }
    return readden;
}

// 创建一个有名管道,这时候这个file需要持久化到磁盘中
file_t *mkfifo(const char *path, int mode)
{
    inode_t *inode = iopen(path, TYPE_NONE);
    if (inode)
    {
        // 已经存在了对应的文件了
        return NULL;
    }

    assert(!inode);

    file_t *fifo = falloc();
    pipe_t *pipe = pipe_alloc();
    pipe_init(pipe);

    inode_t *new_node = iopen(path, TYPE_FIFO);
    if (new_node == NULL)
    {
        return NULL;
    }

    isetfifo(new_node, pipe);

    fifo->inode = new_node;
    fifo->pipe = pipe;
    fifo->type = TYPE_FIFO;
    fifo->readable = !(mode & O_WRONLY);
    fifo->writable = (mode & O_WRONLY) || (mode & O_RDWR);
    fifo->offset = 0;

    pipe->no = ino(new_node);
    iclose(new_node);
    return fifo;
}

void rmfifo(inode_t *ip)
{
    pipe_t *pipe = (pipe_t *)ififoaddr(ip);
    pipe_close(pipe);
}
