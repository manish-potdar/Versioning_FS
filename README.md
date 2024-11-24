### Versioning_FS
Versioning File System - Implementing Snapshots, Rollbacks and Difference Visualization using FUSE


### Requirements:
    1. FUSE library : Visit the github link of libfuse to know the steps for installing 
                      it in your machine.
                      Here is the link : https://github.com/libfuse/libfuse/tree/master
    2. Familarity with how file systems works.
    
### How to run the Compiled File (versioning_fs):
    Run following command : ./versioning_fs testfs/    ## where testfs/ is a directory 
                                                          created for mounting new User-Space file system.


### Output : 


    (base) manish@ubuntu:~/Desktop/versioning_fs$ ./versioning_fs testfs/
    Creating files...
    Created files file1, file2...
    
    Writing to files...
    Written 'Hello, World!' to file1 and
    'FUSE File System' to file2..
    
    Taking snapshot 0...
    Snapshot 0 taken
    
    Modifying file1...
    Modified content of file1 to 'Modified File1'...
    
    Modifying file2...
    Modified content of file2 to 'Modified File2'...
    
    Taking snapshot 1...
    Snapshot 1 taken
    
    Visualizing diff between snapshots 0 and 1...
    Diff between snapshots 0 and 1:
    File: file1
    - Snapshot 0: Hello, World!
    + Snapshot 1: Modified File1
    File: file2
    - Snapshot 0: FUSE File System
    + Snapshot 1: Modified File2
    
    Displaying file system state before rollback...
    Current file system state:
    File: file1, Content: "Modified File1"
    File: file2, Content: "Modified File2"
    
    Rolling back to snapshot 0...
    Rolled back to snapshot 0
    
    Displaying file system state after rollback...
    Current file system state:
    File: file1, Content: "Hello, World!"
    File: file2, Content: "FUSE File System"
    
