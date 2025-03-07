/**
* @file sio/aux/fs.h
* @brief Simple I/O (SIO) - Cross-platform I/O library for high-performance systems programming
* 
* A simple way to interact with the file system.
* Allows simple access to files, directories and recursive access, looping and retrieval.
* 
* Makes use of POSIX file system functionality and Windows API file system functionality
*
* This API performs no internal allocation which isn't required by the operating system
* all memory buffers should be provided by the user.
*
* Callbacks are implemented to help with the cross compatible design
*
* This header file implements no file functionality like reading, writing or even opening files
* this is auxiliary to file functionality only focusing on the filesystem manipulation and information gathering
*
* @author zczxy
* @version 0.1.0
*/

#ifndef SIO_AUX_FS_H
#define SIO_AUX_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sio/platform.h>
#include <sio/err.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#if defined(SIO_OS_WINDOWS)
  #include <windows.h>
#elif defined(SIO_OS_POSIX)
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <dirent.h>
  #include <unistd.h>
  #include <fcntl.h>
#else
  #error "fs.h - Unsupported operating system"
#endif


/**
* @brief Maximum filename length supported
* 
* This value is set to accommodate common filesystem limitations:
* - POSIX NAME_MAX is typically 255
* - Windows MAX_PATH is 260 (including path and null terminator)
*/
#if defined(SIO_OS_WINDOWS)
  #define SIO_MAX_FILENAME_LEN 260
#else
  #define SIO_MAX_FILENAME_LEN 256
#endif

/**
* @brief File type enumeration
*/
typedef enum sio_file_type_e {
  SIO_FILE_TYPE_UNKNOWN = 0,  /**< Unknown file type */
  SIO_FILE_TYPE_REGULAR,      /**< Regular file */
  SIO_FILE_TYPE_DIRECTORY,    /**< Directory */
  SIO_FILE_TYPE_SYMLINK,      /**< Symbolic link */
  SIO_FILE_TYPE_PIPE,         /**< Named pipe or FIFO */
  SIO_FILE_TYPE_SOCKET,       /**< Socket */
  SIO_FILE_TYPE_CHAR_DEVICE,  /**< Character device */
  SIO_FILE_TYPE_BLOCK_DEVICE, /**< Block device */
} sio_file_type_t;

/**
* @brief File information structure
*/
typedef struct sio_file_info_s {
  sio_file_type_t type;                     /**< File type */
  uint64_t        size;                     /**< File size in bytes */
  time_t          access_time;              /**< Last access time */
  time_t          modify_time;              /**< Last modification time */
  time_t          create_time;              /**< Creation time */
  uint32_t        permissions;              /**< File permissions */
  char            name[SIO_MAX_FILENAME_LEN]; /**< File name */
} sio_file_info_t;

/**
* @brief Directory entry callback function type
* 
* @param path Full path to the entry
* @param info Information about the entry
* @param user_data User data passed to the callback
* @return int - 0 to continue, non-zero to stop iteration
*/
typedef int (*sio_dir_entry_callback_t)(const char* path, const sio_file_info_t* info, void* user_data);

// Paths (string manipulation functions to help with cross compatible pathing)

/**
* @brief Normalizes a path according to the current platform
* 
* Converts backslashes to forward slashes on POSIX, and vice versa on Windows.
* Collapses redundant separators and resolves "." and ".." components.
* 
* @param path Input path
* @param normalized_path Output buffer for the normalized path
* @param size Size of the output buffer
* @return sio_error_t Error code
*/
sio_error_t sio_path_normalize(const char* path, char* normalized_path, size_t size);

/**
* @brief Joins two path components
* 
* @param base Base path
* @param component Path component to append
* @param result Output buffer for the joined path
* @param size Size of the output buffer
* @return sio_error_t Error code
*/
sio_error_t sio_path_join(const char* base, const char* component, char* result, size_t size);

/**
* @brief Extracts the directory name from a path
* 
* @param path Input path
* @param dirname Output buffer for the directory name
* @param size Size of the output buffer
* @return sio_error_t Error code
*/
sio_error_t sio_path_dirname(const char* path, char* dirname, size_t size);

/**
* @brief Extracts the base name from a path
* 
* @param path Input path
* @param basename Output buffer for the base name
* @param size Size of the output buffer
* @return sio_error_t Error code
*/
sio_error_t sio_path_basename(const char* path, char* basename, size_t size);

/**
* @brief Extracts the file extension from a path
* 
* @param path Input path
* @param extension Output buffer for the extension
* @param size Size of the output buffer
* @return sio_error_t Error code
*/
sio_error_t sio_path_extension(const char* path, char* extension, size_t size);

/**
* @brief Returns the absolute path of a relative path
* 
* @param path Input relative path
* @param absolute_path Output buffer for the absolute path
* @param size Size of the output buffer
* @return sio_error_t Error code
*/
sio_error_t sio_path_absolute(const char* path, char* absolute_path, size_t size);

// File (including temporary, symbolic links and anything which isn't a directory)

/**
* @brief Checks if a file exists
* 
* @param path Path to the file
* @return int 1 if exists, 0 if not, negative on error
*/
int sio_file_exists(const char* path);

/**
* @brief Gets information about a file
* 
* @param path Path to the file
* @param info Output buffer for file information
* @return sio_error_t Error code
*/
sio_error_t sio_file_info(const char* path, sio_file_info_t* info);

/**
* @brief Copies a file
* 
* @param src Source file path
* @param dst Destination file path
* @param overwrite Whether to overwrite the destination if it exists
* @return sio_error_t Error code
*/
sio_error_t sio_file_copy(const char* src, const char* dst, int overwrite);

/**
* @brief Moves/renames a file
* 
* @param src Source file path
* @param dst Destination file path
* @return sio_error_t Error code
*/
sio_error_t sio_file_move(const char* src, const char* dst);

/**
* @brief Deletes a file
* 
* @param path Path to the file
* @return sio_error_t Error code
*/
sio_error_t sio_file_delete(const char* path);

/**
* @brief Changes file permissions
* 
* @param path Path to the file
* @param permissions New permissions
* @return sio_error_t Error code
*/
sio_error_t sio_file_chmod(const char* path, uint32_t permissions);

/**
* @brief Creates a symbolic link
* 
* @param target Target path
* @param link Path to the link to create
* @return sio_error_t Error code
*/
sio_error_t sio_file_symlink(const char* target, const char* link);

/**
* @brief Reads the target of a symbolic link
* 
* @param link Path to the symbolic link
* @param target Output buffer for the target path
* @param size Size of the output buffer
* @return sio_error_t Error code
*/
sio_error_t sio_file_readlink(const char* link, char* target, size_t size);

/**
* @brief Creates a temporary file with a unique name
* 
* @param prefix Prefix for the file name
* @param path Output buffer for the path to the created file
* @param size Size of the output buffer
* @return sio_error_t Error code
*/
sio_error_t sio_file_temp(const char* prefix, char* path, size_t size);

// Directory

/**
* @brief Creates a directory
* 
* @param path Path to the directory to create
* @param permissions Directory permissions
* @return sio_error_t Error code
*/
sio_error_t sio_dir_create(const char* path, uint32_t permissions);

/**
* @brief Creates a directory and all parent directories if they don't exist
* 
* @param path Path to the directory to create
* @param permissions Directory permissions
* @return sio_error_t Error code
*/
sio_error_t sio_dir_create_recursive(const char* path, uint32_t permissions);

/**
* @brief Opens a directory for reading
* 
* @param path Path to the directory
* @param dir_handle Output handle to the opened directory
* @return sio_error_t Error code
*/
sio_error_t sio_dir_open(const char* path, void** dir_handle);

/**
* @brief Reads the next entry in a directory
* 
* @param dir_handle Directory handle
* @param info Output buffer for entry information
* @return sio_error_t Error code
*/
sio_error_t sio_dir_read(void* dir_handle, sio_file_info_t* info);

/**
* @brief Closes a directory handle
* 
* @param dir_handle Directory handle
* @return sio_error_t Error code
*/
sio_error_t sio_dir_close(void* dir_handle);

/**
* @brief Deletes a directory (must be empty)
* 
* @param path Path to the directory
* @return sio_error_t Error code
*/
sio_error_t sio_dir_delete(const char* path);

/**
* @brief Deletes a directory recursively, including all contents
* 
* @param path Path to the directory
* @return sio_error_t Error code
*/
sio_error_t sio_dir_delete_recursive(const char* path);

/**
* @brief Enumerates all entries in a directory
* 
* @param path Path to the directory
* @param callback Callback function to call for each entry
* @param user_data User data to pass to the callback
* @return sio_error_t Error code
*/
sio_error_t sio_dir_enumerate(const char* path, sio_dir_entry_callback_t callback, void* user_data);

/**
* @brief Recursively enumerates all entries in a directory and its subdirectories
* 
* @param path Path to the directory
* @param callback Callback function to call for each entry
* @param user_data User data to pass to the callback
* @return sio_error_t Error code
*/
sio_error_t sio_dir_enumerate_recursive(const char* path, sio_dir_entry_callback_t callback, void* user_data);

/**
* @brief Gets the current working directory
* 
* @param path Output buffer for the current working directory
* @param size Size of the output buffer
* @return sio_error_t Error code
*/
sio_error_t sio_dir_getcwd(char* path, size_t size);

/**
* @brief Changes the current working directory
* 
* @param path New working directory
* @return sio_error_t Error code
*/
sio_error_t sio_dir_chdir(const char* path);

// Disk (Drives, Partitions and Volumes)

/**
* @brief Disk space information structure
*/
typedef struct sio_disk_space_s {
  uint64_t total_bytes;     /**< Total disk space in bytes */
  uint64_t free_bytes;      /**< Free disk space in bytes */
  uint64_t available_bytes; /**< Available disk space in bytes (may differ from free) */
} sio_disk_space_t;

/**
* @brief Gets disk space information for a path
* 
* @param path Path to get disk space information for
* @param space Output buffer for disk space information
* @return sio_error_t Error code
*/
sio_error_t sio_disk_space(const char* path, sio_disk_space_t* space);

/**
* @brief Disk drive information structure
*/
typedef struct sio_drive_info_s {
  char name[SIO_MAX_FILENAME_LEN]; /**< Drive name/identifier */
  char type[32];                   /**< Drive type (e.g., "fixed", "cdrom", "removable") */
  char filesystem[32];             /**< Filesystem type (e.g., "ntfs", "fat32", "ext4") */
  char mount_point[SIO_MAX_FILENAME_LEN]; /**< Mount point or drive letter */
} sio_drive_info_t;

/**
* @brief Drive enumeration callback function type
* 
* @param info Drive information
* @param user_data User data passed to the callback
* @return int - 0 to continue, non-zero to stop iteration
*/
typedef int (*sio_drive_enum_callback_t)(const sio_drive_info_t* info, void* user_data);

/**
* @brief Enumerates available drives/volumes
* 
* @param callback Callback function to call for each drive
* @param user_data User data to pass to the callback
* @return sio_error_t Error code
*/
sio_error_t sio_drive_enumerate(sio_drive_enum_callback_t callback, void* user_data);

// END


#ifdef __cplusplus
}
#endif

#endif /* SIO_AUX_FS_H */