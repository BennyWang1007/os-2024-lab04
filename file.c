#include <linux/fs.h>
#include <linux/uaccess.h>
#include "osfs.h"

/**
 * Function: osfs_read
 * Description: Reads data from a file.
 * Inputs:
 *   - filp: The file pointer representing the file to read from.
 *   - buf: The user-space buffer to copy the data into.
 *   - len: The number of bytes to read.
 *   - ppos: The file position pointer.
 * Returns:
 *   - The number of bytes read on success.
 *   - 0 if the end of the file is reached.
 *   - -EFAULT if copying data to user space fails.
 */
static ssize_t osfs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(filp);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    void *data_block;
    ssize_t bytes_read;

    // If the file has not been allocated a data block, it indicates the file is empty
    if (osfs_inode->i_blocks == 0)
        return 0;

    if (*ppos >= osfs_inode->i_size)
        return 0;

    if (*ppos + len > osfs_inode->i_size)
        len = osfs_inode->i_size - *ppos;

    // data_block = sb_info->data_blocks + osfs_inode->i_block * BLOCK_SIZE + *ppos;
    // if (copy_to_user(buf, data_block, len))
    //     return -EFAULT;

    // MODED
    int32_t idx_of_block = *ppos / BLOCK_SIZE;
    int32_t offset_in_block = *ppos % BLOCK_SIZE;
    int32_t idx_of_iblock = 0;
    while(idx_of_block > 0) {
        if (idx_of_block >= osfs_inode->i_blocks_length[idx_of_iblock]) {
            idx_of_block -= osfs_inode->i_blocks_length[idx_of_iblock];
            idx_of_iblock++;
        } else {
            break;
        }
    }
    data_block = sb_info->data_blocks + osfs_inode->i_blocks_ptr[idx_of_iblock] * BLOCK_SIZE + idx_of_block * BLOCK_SIZE + offset_in_block;
    if (copy_to_user(buf, data_block, len))
        return -EFAULT;

    *ppos += len;
    bytes_read = len;

    return bytes_read;
}


/**
 * Function: osfs_write
 * Description: Writes data to a file.
 * Inputs:
 *   - filp: The file pointer representing the file to write to.
 *   - buf: The user-space buffer containing the data to write.
 *   - len: The number of bytes to write.
 *   - ppos: The file position pointer.
 * Returns:
 *   - The number of bytes written on success.
 *   - -EFAULT if copying data from user space fails.
 *   - Adjusted length if the write exceeds the block size.
 */
static ssize_t osfs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{   
    //Step1: Retrieve the inode and filesystem information
    struct inode *inode = file_inode(filp);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    void *data_block;
    ssize_t bytes_written;
    // int ret;

    // Step2: Check if a data block has been allocated; if not, allocate one
    // if (osfs_inode->i_blocks == 0) {
    //     osfs_inode->i_block = osfs_alloc_data_block(sb_info, &osfs_inode->i_block);
    //     if (osfs_inode->i_block < 0)
    //         return -ENOSPC; // No space left on device
    //     osfs_inode->i_blocks = 1;
    // }

    
    // MODED
    int32_t block_needed = (*ppos + len) / BLOCK_SIZE + ((*ppos + len) % BLOCK_SIZE == 0 ? 0 : 1);

    if (osfs_inode->i_blocks < block_needed) {
        if (osfs_inode->i_blocks == 0) {
            // alloc data blocks
            osfs_inode->i_blocks_ptr = (uint32_t *)kmalloc(sizeof(uint32_t) * block_needed, GFP_KERNEL);
            osfs_inode->i_blocks_length = (int32_t *)kmalloc(sizeof(uint32_t) * block_needed, GFP_KERNEL);
            for (int i = 0; i < block_needed; i++) {
                osfs_inode->i_blocks_ptr[i] = -1;
                osfs_inode->i_blocks_length[i] = -1;
            }
        } else {
            // realloc data blocks
            int old_block_idx = osfs_inode->i_blocks - 1;
            osfs_inode->i_blocks_ptr = (uint32_t *)krealloc(osfs_inode->i_blocks_ptr, sizeof(uint32_t) * block_needed, GFP_KERNEL);
            osfs_inode->i_blocks_length = (int32_t *)krealloc(osfs_inode->i_blocks_length, sizeof(uint32_t) * block_needed, GFP_KERNEL);
            for (int i = old_block_idx+1; i < block_needed; i++) {
                osfs_inode->i_blocks_ptr[i] = -1;
                osfs_inode->i_blocks_length[i] = -1;
            }
        }
        if (osfs_realloc_multiple_data_blocks(sb_info, osfs_inode->i_blocks_ptr, osfs_inode->i_blocks_length, block_needed) < 0) {
            return -ENOSPC;
        }
        osfs_inode->i_blocks = block_needed;
    }
        

    // Step3: Limit the write length to fit within one data block
    // if (*ppos + len > BLOCK_SIZE) {
    //     len = BLOCK_SIZE - *ppos;
    // }


    // Step4: Write data from user space to the data block
    // data_block = sb_info->data_blocks + osfs_inode->i_block * BLOCK_SIZE + *ppos;
    // if (copy_from_user(data_block, buf, len)) {
    //     return -EFAULT;
    // }


    // MODED
    osfs_inode->i_size = *ppos + len;

    bytes_written = len;

    // MODED
    while (len > 0) {
        int32_t idx_of_block = *ppos / BLOCK_SIZE;
        int32_t offset_in_block = *ppos % BLOCK_SIZE;
        int32_t idx_of_iblock = 0;
        while(idx_of_block > 0) {
            if (idx_of_block >= osfs_inode->i_blocks_length[idx_of_iblock]) {
                idx_of_block -= osfs_inode->i_blocks_length[idx_of_iblock];
                idx_of_iblock++;
            } else {
                break;
            }
        }
        data_block = sb_info->data_blocks + osfs_inode->i_blocks_ptr[idx_of_iblock] * BLOCK_SIZE + idx_of_block * BLOCK_SIZE + offset_in_block;
        int32_t write_len = len > BLOCK_SIZE - offset_in_block ? BLOCK_SIZE - offset_in_block : len;
        if (copy_from_user(data_block, buf, write_len)) {
            return -EFAULT;
        }
        len -= write_len;
        *ppos += write_len;
        buf += write_len;
    }



    // Step5: Update inode & osfs_inode attribute
    // osfs_inode->i_size = *ppos + len;
    // *ppos += len;
    // bytes_written = len;
    // if (*ppos + len > osfs_inode->i_size) {
    //     osfs_inode->i_size = *ppos + len;
    // }
    // *ppos += len;
    // bytes_written = len;

    

    // Step6: Return the number of bytes written
    return bytes_written;
}

/**
 * Function: osfs_unlink
 * Description: Unlinks (deletes) a file.
 * Inputs:
 *   - dir: The directory inode containing the file to be unlinked.
 *   - dentry: The dentry representing the file to be unlinked.
 * Returns:
 *   - 0 on success.
 *   - -ENOENT if the file does not exist.
 */
static int osfs_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = d_inode(dentry);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;

    // Step1: Check if the file exists
    if (!inode)
        return -ENOENT;

    // Step2: Free the data block if allocated
    // if (osfs_inode->i_blocks > 0) {
    //     osfs_free_data_block(sb_info, osfs_inode->i_block);
    //     osfs_inode->i_blocks = 0;
    // }

    // MODED
    for (int i = 0; i < osfs_inode->i_blocks; i++) {
        for (int j = 0; j < osfs_inode->i_blocks_length[i]; j++) {
            osfs_free_data_block(sb_info, osfs_inode->i_blocks_ptr[i] + j);
        }
        osfs_inode->i_blocks_ptr[i] = -1;
        osfs_inode->i_blocks_length[i] = -1;
    }

    // Step3: Remove the dentry from the directory
    d_drop(dentry);

    // Step4: Update the inode attributes
    clear_nlink(inode);
    mark_inode_dirty(inode);

    return 0;
}


/**
 * Struct: osfs_file_operations
 * Description: Defines the file operations for regular files in osfs.
 */
const struct file_operations osfs_file_operations = {
    .open = generic_file_open, // Use generic open or implement osfs_open if needed
    .read = osfs_read,
    .write = osfs_write,
    .llseek = default_llseek,
    // Add other operations as needed
};


/**
 * Struct: osfs_file_inode_operations
 * Description: Defines the inode operations for regular files in osfs.
 * Note: Add additional operations such as getattr as needed.
 */
const struct inode_operations osfs_file_inode_operations = {
    // Add inode operations here, e.g., .getattr = osfs_getattr,
    .unlink = osfs_unlink,
};
