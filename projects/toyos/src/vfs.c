#include "toyos.h"

#if CONFIG_MILESTONE >= 9

#define RAMFS_CAPACITY 256U
#define VFS_FILE_BASE 5U

struct inode {
    const char *path;
    uint8_t data[RAMFS_CAPACITY];
    size_t size;
    bool writable;
};

struct file {
    struct inode *inode;
    size_t offset;
    bool open;
};

static struct inode inodes[2];
static struct file files[4];

static bool strings_equal(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) {
            return false;
        }
    }
    return *left == *right;
}

static void set_contents(struct inode *inode, const char *text)
{
    inode->size = 0;
    while (text[inode->size] != '\0' && inode->size < RAMFS_CAPACITY) {
        inode->data[inode->size] = (uint8_t)text[inode->size];
        ++inode->size;
    }
}

int64_t vfs_open(const char *path)
{
    struct inode *inode = NULL;
    for (size_t i = 0; i < ARRAY_LEN(inodes); ++i) {
        if (strings_equal(path, inodes[i].path)) {
            inode = &inodes[i];
            break;
        }
    }
    if (inode == NULL) {
        return -2;
    }
    for (size_t i = 0; i < ARRAY_LEN(files); ++i) {
        if (!files[i].open) {
            files[i].inode = inode;
            files[i].offset = 0;
            files[i].open = true;
            return (int64_t)(VFS_FILE_BASE + i);
        }
    }
    return -24;
}

static struct file *file_for(unsigned fd)
{
    if (fd < VFS_FILE_BASE || fd >= VFS_FILE_BASE + ARRAY_LEN(files)) {
        return NULL;
    }
    struct file *const file = &files[fd - VFS_FILE_BASE];
    return file->open ? file : NULL;
}

int64_t vfs_read(unsigned fd, void *buffer, size_t length)
{
    struct file *const file = file_for(fd);
    if (file == NULL || buffer == NULL) {
        return -9;
    }
    const size_t available = file->inode->size - file->offset;
    const size_t count = length < available ? length : available;
    uint8_t *const bytes = buffer;
    for (size_t i = 0; i < count; ++i) {
        bytes[i] = file->inode->data[file->offset + i];
    }
    file->offset += count;
    return (int64_t)count;
}

int64_t vfs_write(unsigned fd, const void *buffer, size_t length)
{
    struct file *const file = file_for(fd);
    if (file == NULL || buffer == NULL || !file->inode->writable) {
        return -9;
    }
    if (length > RAMFS_CAPACITY - file->offset) {
        return -28;
    }
    const uint8_t *const bytes = buffer;
    for (size_t i = 0; i < length; ++i) {
        file->inode->data[file->offset + i] = bytes[i];
    }
    file->offset += length;
    if (file->offset > file->inode->size) {
        file->inode->size = file->offset;
    }
    return (int64_t)length;
}

void vfs_init(void)
{
    inodes[0].path = "/hello";
    inodes[0].writable = false;
    set_contents(&inodes[0], "USER: ramfs open/read through fd\n");
    inodes[1].path = "/note";
    inodes[1].writable = true;
    set_contents(&inodes[1], "");

    const int64_t descriptor = vfs_open("/note");
    ASSERT(descriptor >= 0);
    const char probe[] = "vfs";
    ASSERT(vfs_write((unsigned)descriptor, probe, sizeof(probe)) == 4);
    serial_write("VFS: ramfs inode/path/open/read/write PASS\n");
#if EXPECT_FAULT == 8
    ASSERT(vfs_open("/missing") == -2);
    serial_write("EXPECTED-FAULT: filesystem PASS\n");
    qemu_exit(0x17);
#endif
}

#endif
