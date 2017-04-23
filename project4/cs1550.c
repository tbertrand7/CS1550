/* 
 * Tom Bertrand
 * 4/14/2017
 * COE 1550
 * Misurda T, Th 11:00
 * Project 4
 * FUSE filesystem implementation
 */

/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;

typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

struct cs1550_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

#define MAX_FAT_ENTRIES (BLOCK_SIZE/sizeof(short))

struct cs1550_file_alloc_table_block {
	short table[MAX_FAT_ENTRIES];
};

typedef struct cs1550_file_alloc_table_block cs1550_fat_block;

/*********************Helper methods and macros*********************/

//EOF Sentinel
#define CS1550_EOF -2

//Macro for last block in memory where FAT is located
#define FAT_LOCATION (MAX_DIRS_IN_ROOT + MAX_FAT_ENTRIES + 1)

//Offset for blocks indexed by FAT
#define FAT_OFFSET (MAX_DIRS_IN_ROOT + 1)

/* Loads root and FAT into vars */
static void load_root_FAT(cs1550_root_directory* root, cs1550_fat_block* FAT)
{
	FILE* disk = fopen(".disk", "r+b");

	//Root in first block in .disk
	fread(root, BLOCK_SIZE, 1, disk);

	//FAT in last block in .disk
	fseek(disk, (BLOCK_SIZE*FAT_LOCATION), SEEK_SET); //Seek to last block
	fread(FAT, BLOCK_SIZE, 1, disk);

	fclose(disk);
}

/* Reads disk block into buffer */
static int read_disk_block(int block, void* buffer)
{
	int res;
	FILE* disk = fopen(".disk", "r+b");
	fseek(disk, (BLOCK_SIZE*block), SEEK_SET);
	res = fread(buffer, BLOCK_SIZE, 1, disk);
	fclose(disk);
	return res;
}

/* Writes buffer to disk block */
static int write_disk_block(int block, void* buffer)
{
	int res;
	FILE* disk = fopen(".disk", "r+b");
	fseek(disk, (BLOCK_SIZE*block), SEEK_SET);
	res = fwrite(buffer, BLOCK_SIZE, 1, disk);
	fclose(disk);
	return res;
}

/* Locate next free block in FAT */
static int get_next_free_block(cs1550_fat_block* FAT)
{
	int i;
	for (i=0; i < MAX_FAT_ENTRIES; i++)
	{
		if (FAT->table[i] == 0)
		{
			return (i + FAT_OFFSET); //Added offset for root and subdir blocks
		}
	}
	return -1; //FAT is full
}

/*******************************************************************/

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not. 
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];

	cs1550_directory_entry* subdir = malloc(sizeof(cs1550_directory_entry));
	int file_location;
	size_t filesize;

	//Load in root and FAT
	cs1550_root_directory* root = malloc(sizeof(cs1550_root_directory));
	cs1550_fat_block* FAT = malloc(sizeof(cs1550_fat_block));
	load_root_FAT(root, FAT);

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	memset(stbuf, 0, sizeof(struct stat));
   
	//is path the root dir?
	if (strcmp(path, "/") == 0)
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else if (root->nDirectories != 0)
	{
		//Check to see if subdirectory exists
		int i, exists = 0;
		for (i=0; i < root->nDirectories; i++)
		{
			if(!strcmp(directory, root->directories[i].dname))
			{
				exists = 1;
				read_disk_block(root->directories[i].nStartBlock, subdir);
				break;
			}
		}

		if (exists)
		{
			//Check if name is subdirectory
			if (path[strlen(directory)+1] != '/')
			{
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				res = 0; //no error
			}
			else //Check if name is a regular file
			{
				exists = 0;

				for (i=0; i < subdir->nFiles; i++)
				{
					if (!strcmp(filename, subdir->files[i].fname) && !strcmp(extension, subdir->files[i].fext))
					{
						exists = 1;
						file_location = i;
						filesize = subdir->files[i].fsize;
						break;
					}
				}

				if (exists)
				{
					//regular file, probably want to be read and write
					stbuf->st_mode = S_IFREG | 0666; 
					stbuf->st_nlink = 1; //file links
					stbuf->st_size = filesize; //file size - make sure you replace with real size!
					res = 0; // no error
				}
				else
				{
					res = -ENOENT;
				}
			}
		}
		else
		{
			res = -ENOENT;
		}
	} 
	else
	{
		//Else return that path doesn't exist
		res = -ENOENT;
	}

	free(subdir);
	free(root);
	free(FAT);

	return res;
}

/* 
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	//the filler function allows us to add entries to the listing
	//read the fuse.h file for a description (in the ../include dir)
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//Load in root and FAT
	cs1550_root_directory* root = malloc(sizeof(cs1550_root_directory));
	cs1550_fat_block* FAT = malloc(sizeof(cs1550_fat_block));
	load_root_FAT(root, FAT);

	/*
	//add the user stuff (subdirs or files)
	//the +1 skips the leading '/' on the filenames
	filler(buf, newpath + 1, NULL, 0);
	*/
	int i;
	//In root so print subdirectories
	if (!strcmp(path, "/"))
	{
		for (i=0; i < root->nDirectories; i++)
		{
			filler(buf, root->directories[i].dname, NULL, 0);
		}
	}
	else //In subdirectory so print filenames
	{
		int exists = 0;
		cs1550_directory_entry* subdir = malloc(sizeof(cs1550_directory_entry));
		char directory[MAX_FILENAME+1];
		char filename[MAX_FILENAME+1];
		char extension[MAX_EXTENSION+1];

		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

		//Ensure path is not a file or invalid
		if (strchr(filename, '/'))
		{
			free(subdir);
			free(root);
			free(FAT);
			return -ENOENT;
		}

		//Find subdirectory
		for (i=0; i < root->nDirectories; i++)
		{
			if(!strcmp(directory, root->directories[i].dname))
			{
				read_disk_block(root->directories[i].nStartBlock, subdir);
				exists = 1;
				break;
			}
		}
		if (!exists)
		{
			free(subdir);
			free(root);
			free(FAT);
			return -ENOENT;
		}

		char* str[MAX_FILENAME+MAX_EXTENSION+1];
		//Print files in subdirectory
		for (i=0; i < subdir->nFiles; i++)
		{
			strcpy(str, subdir->files[i].fname);
			strcat(str, ".");
			strcat(str, subdir->files[i].fext);
			filler(buf, str, NULL, 0);
		}

		free(subdir);
	}

	free(root);
	free(FAT);

	return 0;
}

/* 
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

	int length, free_block, subdir_loc;

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];

	//Load in root and FAT
	cs1550_root_directory* root = malloc(sizeof(cs1550_root_directory));
	cs1550_fat_block* FAT = malloc(sizeof(cs1550_fat_block));
	load_root_FAT(root, FAT);

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	//Directory not in root
	if (path[strlen(directory)+1] == '/')
	{
		free(root);
		free(FAT);
		return -EPERM;
	}

	struct stat* stbuf = malloc(sizeof(struct stat));
	memset(stbuf, 0, sizeof(struct stat));

	//Check directory name length
	length = strlen(directory);
	if (length > MAX_FILENAME)
	{
		free(root);
		free(FAT);
		free(stbuf);
		return -ENAMETOOLONG;
	}

	//Check if directory exists
	int i;
	for (i=0; i < root->nDirectories; i++)
	{
		if (!strcmp(directory, root->directories[i].dname))
		{
			free(root);
			free(FAT);
			free(stbuf);
			return -EEXIST;
		}
	}

	//Root can't hold any more directories
	if (root->nDirectories >= MAX_DIRS_IN_ROOT)
	{
		free(root);
		free(FAT);
		free(stbuf);
		return -ENOMEM;
	}

	subdir_loc = root->nDirectories;
	strncpy(root->directories[subdir_loc].dname, directory, length);
	root->directories[subdir_loc].nStartBlock = 1 + subdir_loc;
	root->nDirectories = root->nDirectories + 1;

	write_disk_block(0, root);

	cs1550_directory_entry* new_dir = malloc(sizeof(cs1550_directory_entry));
	write_disk_block(root->directories[subdir_loc].nStartBlock, new_dir);

	free(root);
	free(FAT);
	free(stbuf);
	free(new_dir);

	return 0;
}

/* 
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/* 
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	
	int fnamelength, fextlength;
	
	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];

	//Load in root and FAT
	cs1550_root_directory* root = malloc(sizeof(cs1550_root_directory));
	cs1550_fat_block* FAT = malloc(sizeof(cs1550_fat_block));
	load_root_FAT(root, FAT);

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	
	//Prevent files from being created in root
	if (path[strlen(directory)+1] != '/')
	{
		free(root);
		free(FAT);
		return -EPERM;
	}
	
	//Check if name and extension are too long
	fnamelength = strlen(filename);
	fextlength = strlen(extension);
	if (fnamelength > MAX_FILENAME || fextlength > MAX_EXTENSION)
	{
		free(root);
		free(FAT);
		return -ENAMETOOLONG;
	}

	//Check if subdirectory exists
	int i, exists=0, subdir_loc;
	cs1550_directory_entry* subdir = malloc(sizeof(cs1550_directory_entry));
	for (i=0; i < root->nDirectories; i++)
	{
		if (!strcmp(directory, root->directories[i].dname))
		{
			exists = 1;
			subdir_loc = root->directories[i].nStartBlock; //Save block of subdir
			read_disk_block(subdir_loc, subdir);
			break;
		}
	}

	//Check if file already exists
	if (exists)
	{
		for (i=0; i < subdir->nFiles; i++)
		{
			if (!strcmp(filename, subdir->files[i].fname) && !strcmp(extension, subdir->files[i].fext))
			{
				free(root);
				free(FAT);
				free(subdir);
				return -EEXIST;
			}
		}

		//Check for room in directory
		if (subdir->nFiles >= MAX_FILES_IN_DIR)
		{
			free(root);
			free(FAT);
			free(subdir);
			return -ENOMEM;
		}

		//Check for room in disk
		if (get_next_free_block(FAT) == -1)
		{
			free(root);
			free(FAT);
			free(subdir);
			return -ENOMEM;
		}

		/* File doesn't exist, create new file */

		//Update subdirectory entry
		int file_loc = subdir->nFiles;
		strncpy(subdir->files[file_loc].fname, filename, fnamelength);
		strncpy(subdir->files[file_loc].fext, extension, fextlength);
		subdir->files[file_loc].fsize = 0; //Initialize to empty file
		subdir->files[file_loc].nStartBlock = get_next_free_block(FAT);
		subdir->nFiles = subdir->nFiles + 1;
		write_disk_block(subdir_loc, subdir);

		//Update FAT
		int FAT_index = subdir->files[file_loc].nStartBlock - FAT_OFFSET; //Convert to FAT index
		FAT->table[FAT_index] = CS1550_EOF; //Set allocated block to EOF sentinel
		write_disk_block(FAT_LOCATION, FAT); //write out FAT to disk
	}
	else 
	{
		free(root);
		free(FAT);
		free(subdir);
		return -ENOENT;
	}

	free(root);
	free(FAT);
	free(subdir);

	return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/* 
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) path;

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];

	//Load in root and FAT
	cs1550_root_directory* root = malloc(sizeof(cs1550_root_directory));
	cs1550_fat_block* FAT = malloc(sizeof(cs1550_fat_block));
	load_root_FAT(root, FAT);

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	int i, exists = 0, subdir_loc, file_loc;
	cs1550_directory_entry* subdir = malloc(sizeof(cs1550_directory_entry));

	//check to make sure path exists
	for (i=0; i < root->nDirectories; i++)
	{
		if (!strcmp(root->directories[i].dname, directory))
		{
			exists = 1;
			subdir_loc = i;
			read_disk_block(root->directories[i].nStartBlock, subdir);
			break;
		}
	}

	if (exists)
	{
		//Check if path is directory
		if (path[strlen(directory)+1] != '/')
		{
			free(root);
			free(FAT);
			free(subdir);
			return -EISDIR;
		}

		//Check to make sure file exists
		exists = 0;
		for (i=0; i < subdir->nFiles; i++)
		{
			if (!strcmp(subdir->files[i].fname, filename) && !strcmp(subdir->files[i].fext, extension))
			{
				exists = 1;
				file_loc = i;
				break;
			}
		}
		if (!exists)
		{
			free(root);
			free(FAT);
			free(subdir);
			return -ENOENT;
		}
	}
	else
	{
		free(root);
		free(FAT);
		free(subdir);
		return -ENOENT;
	}

	//check that size is > 0
	if (size <= 0)
	{
		free(root);
		free(FAT);
		free(subdir);
		return size;
	}

	//check that offset is <= to the file size
	if (offset > subdir->files[i].fsize)
	{
		free(root);
		free(FAT);
		free(subdir);
		return -EFBIG;
	}

	//read in data
	int j;
	int num_blocks = (subdir->files[file_loc].fsize / BLOCK_SIZE) + 1; //Number of blocks to be read
	int* blocks_to_read = malloc(sizeof(int)*num_blocks); //Array containing block numbers of all of the blocks for the file

	blocks_to_read[0] = subdir->files[file_loc].nStartBlock; //Get starting address block
	int FAT_index = blocks_to_read[0] - FAT_OFFSET; //Get first FAT index to begin traversal
	//Traverse FAT to get block numbers of all blocks for file
	for (j=1; j < num_blocks; j++)
	{
		blocks_to_read[j] = FAT->table[FAT_index];
		FAT_index = blocks_to_read[j] - FAT_OFFSET;
	}

	//Read each disk block and copy it's contents to the buffer
	char* tempbuf = malloc(sizeof(char) * BLOCK_SIZE);
	for (j=0; j < num_blocks; j++)
	{
		read_disk_block(blocks_to_read[j], tempbuf);
		strncpy(buf+(j*BLOCK_SIZE), tempbuf, BLOCK_SIZE);
	}

	//set size and return, or error
	size = subdir->files[file_loc].fsize;

	free(root);
	free(FAT);
	free(subdir);
	free(blocks_to_read);
	free(tempbuf);

	return size;
}

/* 
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size, 
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];

	//Load in root and FAT
	cs1550_root_directory* root = malloc(sizeof(cs1550_root_directory));
	cs1550_fat_block* FAT = malloc(sizeof(cs1550_fat_block));
	load_root_FAT(root, FAT);

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	int i, exists = 0, subdir_loc, file_loc;
	cs1550_directory_entry* subdir = malloc(sizeof(cs1550_directory_entry));

	//check to make sure path exists
	for (i=0; i < root->nDirectories; i++)
	{
		if (!strcmp(root->directories[i].dname, directory))
		{
			exists = 1;
			subdir_loc = i;
			read_disk_block(root->directories[i].nStartBlock, subdir);
			break;
		}
	}

	if (exists)
	{
		//Check that file exists
		exists = 0;
		for (i=0; i < subdir->nFiles; i++)
		{
			if (!strcmp(subdir->files[i].fname, filename) && !strcmp(subdir->files[i].fext, extension))
			{
				exists = 1;
				file_loc = i;
				break;
			}
		}
		if (!exists)
		{
			free(root);
			free(FAT);
			free(subdir);
			return -ENOENT;
		}
	}
	else
	{
		free(root);
		free(FAT);
		free(subdir);
		return -ENOENT;
	}

	//check that size is > 0
	if (size <= 0)
	{
		free(root);
		free(FAT);
		free(subdir);
		return size;
	}

	//check that offset is <= to the file size
	if (offset > subdir->files[i].fsize)
	{
		free(root);
		free(FAT);
		free(subdir);
		return -EFBIG;
	}

	//write data
	int j;
	int num_blocks_needed = ((offset + size) / BLOCK_SIZE) + 1; //Get number of needed blocks
	int curr_blocks = (subdir->files[file_loc].fsize / BLOCK_SIZE) + 1; //Get number of blocks file currently has

	int* block_list = malloc(sizeof(int) * num_blocks_needed); //Addresses of all blocks needed
	memset(block_list, 0, sizeof(int) * num_blocks_needed);

	int FAT_index = subdir->files[file_loc].nStartBlock - FAT_OFFSET; //Get first location in FAT to look
	block_list[0] = subdir->files[file_loc].nStartBlock; //Store starting block in block_list

	//Traverse FAT to get block numbers for all blocks of the file
	for (j=1; j < curr_blocks; j++)
	{
		FAT_index = FAT->table[FAT_index] - FAT_OFFSET;
		block_list[j] = FAT_index + FAT_OFFSET;
	}

	//Allocate new blocks if file will require them
	for (j=0; j < num_blocks_needed; j++)
	{
		if(block_list[j] == 0)
		{
			FAT_index = block_list[j-1] - FAT_OFFSET;
			FAT->table[FAT_index] = get_next_free_block(FAT);
			block_list[j] = FAT->table[FAT_index];
			FAT->table[(block_list[j] - FAT_OFFSET)] = CS1550_EOF;
		}
	}
	write_disk_block(FAT_LOCATION, FAT); //Write out modified FAT

	//Write out each blocks data to disk
	char* tempbuf= malloc(sizeof(char)*BLOCK_SIZE);
	int start_block = (offset / BLOCK_SIZE); //Start at offset
	for (j=start_block; j < num_blocks_needed; j++)
	{
		read_disk_block(block_list[j], tempbuf); //Read in blocks current data

		//Write out data to block buffer, three corner cases handled
		if (j == start_block && ((offset%BLOCK_SIZE)+size < BLOCK_SIZE)) //First block to write and appending to middle of a block
		{
			strncpy((tempbuf+(offset%BLOCK_SIZE)), buf, size);
		}
		else if (j == start_block) //First block and writing whole block
		{
			strncpy((tempbuf+(offset%BLOCK_SIZE)), buf, BLOCK_SIZE-(offset%BLOCK_SIZE));
		}
		else if (j == num_blocks_needed - 1) //Last block
		{
			strncpy(tempbuf, (buf+((j-start_block) * BLOCK_SIZE)), size % BLOCK_SIZE);
		}
		else
		{
			strncpy(tempbuf, (buf+((j-start_block) * BLOCK_SIZE)), BLOCK_SIZE);
		}
		
		write_disk_block(block_list[j], tempbuf); //Write out updated block
	}

	//set size (should be same as input) and return, or error
	subdir->files[file_loc].fsize = (size + offset);
	write_disk_block(root->directories[subdir_loc].nStartBlock, subdir);

	free(root);
	free(FAT);
	free(subdir);
	free(block_list);
	free(tempbuf);

	return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or 
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/* 
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but 
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file 
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
