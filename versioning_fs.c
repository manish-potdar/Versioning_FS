#define FUSE_USE_VERSION 31

#include <fuse.h> // FUSE library
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#define MAX_FILES 100 //maximum number of files the system can handle
#define MAX_SNAPSHOTS 100 //maximum number of snapshots that can be stored
#define MAX_CONTENT_SIZE 65536 // maximum size of the file content (#Bytes)

typedef struct {
    char name[256];
    char content[MAX_CONTENT_SIZE];
    size_t size;
} file_t; // represents a file in the file system (metadata about a file)

typedef struct {
    file_t files[MAX_FILES];
    int num_files;
} snapshot_t; // represents snapshot of file system 

static struct {
    file_t files[MAX_FILES];
    int num_files;
    snapshot_t snapshots[MAX_SNAPSHOTS];
    int num_snapshots;
} fs_state; // Global file system state (contains whole contents of file system of an instance)

// Utility Function: Find a file by name
static file_t *find_file(const char *name) {
    for (int i = 0; i < fs_state.num_files; i++) {
        if (strcmp(fs_state.files[i].name, name) == 0)
            return &fs_state.files[i];
    }
    return NULL;
}

// Take a snapshot of the file system
static void take_snapshot() {
    if (fs_state.num_snapshots >= MAX_SNAPSHOTS) {
        fprintf(stderr, "Max snapshots reached!\n");
        return;
    }
    snapshot_t *snap = &fs_state.snapshots[fs_state.num_snapshots++];
    memcpy(snap->files, fs_state.files, sizeof(fs_state.files));
    snap->num_files = fs_state.num_files;
    printf("Snapshot %d taken\n\n", fs_state.num_snapshots - 1);
}

// Rollback to a specific snapshot
static int rollback(int snapshot_id) {
    if (snapshot_id < 0 || snapshot_id >= fs_state.num_snapshots)
        return -EINVAL;

    snapshot_t *snap = &fs_state.snapshots[snapshot_id];
    memcpy(fs_state.files, snap->files, sizeof(snap->files));
    fs_state.num_files = snap->num_files;
    printf("Rolled back to snapshot %d\n\n", snapshot_id);
    return 0;
}

// Visualize differences between snapshots
static void visualize_diff(int snap1, int snap2) {
    if (snap1 < 0 || snap1 >= fs_state.num_snapshots || snap2 < 0 || snap2 >= fs_state.num_snapshots) {
        printf("Invalid snapshot IDs\n");
        return;
    }

    snapshot_t *s1 = &fs_state.snapshots[snap1];
    snapshot_t *s2 = &fs_state.snapshots[snap2];
    printf("Diff between snapshots %d and %d:\n", snap1, snap2);

    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(s1->files[i].name, s2->files[i].name) != 0 ||
            strcmp(s1->files[i].content, s2->files[i].content) != 0) {
            printf("File: %s\n", s1->files[i].name);
            printf("- Snapshot %d: %s\n", snap1, s1->files[i].content);
            printf("+ Snapshot %d: %s\n", snap2, s2->files[i].content);
        }
    }
    printf("\n");
}

// Display the current file system state
static void display_file_system() {
    printf("\nCurrent file system state:\n");
    for (int i = 0; i < fs_state.num_files; i++) {
        printf("File: %s, Content: \"%s\"\n", fs_state.files[i].name, fs_state.files[i].content);
    }
    printf("\n");
}

// FUSE: Get file attributes
static int vfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        file_t *file = find_file(path + 1); // Skip leading '/'
        if (!file)
            return -ENOENT;
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = file->size;
    }
    return 0;
}

// FUSE: Read directory
static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    for (int i = 0; i < fs_state.num_files; i++) {
        filler(buf, fs_state.files[i].name, NULL, 0, 0);
    }
    return 0;
}

// FUSE: Create a file
static int vfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    (void) mode;
    (void) rdev;

    if (fs_state.num_files >= MAX_FILES)
        return -ENOSPC;

    file_t *file = find_file(path + 1);
    if (file)
        return -EEXIST;

    file = &fs_state.files[fs_state.num_files++];
    strncpy(file->name, path + 1, sizeof(file->name) - 1);
    file->size = 0;
    memset(file->content, 0, sizeof(file->content));
    return 0;
}

// FUSE: Write to a file
static int vfs_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    (void) fi;
    file_t *file = find_file(path + 1);
    if (!file)
        return -ENOENT;

    if (offset + size > MAX_CONTENT_SIZE)
        return -EFBIG;

    memcpy(file->content + offset, buf, size);
    file->size = offset + size;
    return size;
}

// FUSE: Read from a file
static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    (void) fi;
    file_t *file = find_file(path + 1);
    if (!file)
        return -ENOENT;

    if (offset >= file->size)
        return 0;

    if (offset + size > file->size)
        size = file->size - offset;

    memcpy(buf, file->content + offset, size);
    return size;
}

static int vfs_unlink(const char *path) {
    // Skip the leading '/' in the file path
    const char *filename = path + 1;

    // Find the file in the file system state
    for (int i = 0; i < fs_state.num_files; i++) {
        if (strcmp(fs_state.files[i].name, filename) == 0) {
            // File found: remove it by shifting subsequent entries
            for (int j = i; j < fs_state.num_files - 1; j++) {
                fs_state.files[j] = fs_state.files[j + 1];
            }

            // Decrement the file count
            fs_state.num_files--;

            printf("File '%s' deleted successfully.\n", filename);
            return 0; // Success
        }
    }

    // File not found
    return -ENOENT;
}


static const struct fuse_operations vfs_oper = {
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .mknod = vfs_mknod,
    .read = vfs_read,
    .write = vfs_write,
    .unlink = vfs_unlink,
};

int main(int argc, char *argv[]) {
    fs_state.num_files = 0;
    fs_state.num_snapshots = 0;

    // Code to Demonstrate Functionality
    printf("Creating files...\n");
    vfs_mknod("/file1", 0, 0);
    vfs_mknod("/file2", 0, 0);
    printf("Created files file1, file2...\n\n");

    printf("Writing to files...\n");
    vfs_write("/file1", "Hello, World!", 13, 0, NULL);
    vfs_write("/file2", "FUSE File System", 16, 0, NULL);
    printf("Written 'Hello, World!' to file1 and\n'FUSE File System' to file2..\n\n");

    printf("Taking snapshot 0...\n");
    take_snapshot();

    printf("Modifying file1...\n");
    vfs_write("/file1", "Modified File1", 16, 0, NULL);
    printf("Modified content of file1 to 'Modified File1'...\n\n");


    printf("Modifying file2...\n");
    vfs_write("/file2", "Modified File2", 16, 0, NULL);
    printf("Modified content of file2 to 'Modified File2'...\n\n");

    printf("Taking snapshot 1...\n");
    take_snapshot();

    printf("Visualizing diff between snapshots 0 and 1...\n");
    visualize_diff(0, 1);

    printf("Displaying file system state before rollback...");
    display_file_system();

    printf("Rolling back to snapshot 0...\n");
    rollback(0);

    printf("Displaying file system state after rollback...");
    display_file_system();

    return fuse_main(argc, argv, &vfs_oper, NULL);
}
