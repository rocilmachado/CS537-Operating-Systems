#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <limits.h>
#include <string.h>

#define stat xv6_stat  // avoid clash with host struct stat
#define dirent xv6_dirent  // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#undef stat
#undef dirent


void print_error(char* msg){
    fprintf(stderr,"%s\n", msg);
    exit(1);
}

int num_bitmap_blocks(int numblocks)
{
  int bitmap_blocks = numblocks / BPB;
  if(numblocks % BPB > 0)
    bitmap_blocks++;
  return bitmap_blocks; 

}


int main(int argc, char* argv[])
{
    int fd;
    // Usage is something like <my prog> <fs.img>
	if (argc == 2) {
		fd = open(argv[1], O_RDONLY);
	} else {
        print_error("Usage: program fs.img");
  }

  if (fd < 0) {
    print_error("image not found.");
  }

  struct stat sbuf;
  fstat(fd, &sbuf);

    //Maps the fs image onto the virtual memory
  void *img_ptr = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0); 

  //Check if image pointer is not -1
  if((*(int*)img_ptr) == -1)
  {
      print_error("mmap failed");
  }

  struct superblock *sb = (struct superblock*)(img_ptr + BSIZE);
  //printf("size %d, numblocks %d, numinodes %d\n", sb->size, sb->nblocks, sb->ninodes);

  // img_ptr is pointing to byte 0 inside the file
  // FS layout:
  // unused           | superblock              | inodes
  // first 512 bytes  | next 512 is super block |

  int bitmap_blocks = num_bitmap_blocks(sb->nblocks);

  //Array to kepp track of data blocks marked used in inode. If data block i is marked used in inode, entry at index i is 1
  int blocks_marked_used[sb->size];
  int inodes_marked_used[sb->ninodes];
  int directory_inode_reference[sb->ninodes];
  memset(blocks_marked_used, 0, sb->size * sizeof(int));
  memset(inodes_marked_used, 0, sb->ninodes * sizeof(int));
  memset(directory_inode_reference, 0, sb->ninodes* sizeof(int));

  //TODO: Check the condition
  //Check 1: For the metadata in the super block, the file-system size 
  //is larger than the number of blocks used by the super-block, inodes, bitmaps and data
  if(sb->size < (sb->nblocks) + (sb->ninodes/IPB) + bitmap_blocks + 3)
  {
    print_error("ERROR: superblock is corrupted.");
  }

  //Check 2: Each inode is either unallocated or one of the valid types (T_DIR = 1, T_FILE = 2, T_DEV = 3).
  //inode table starts from block 2 (block 0 is unused, block 1 is super block)
  struct dinode *dip = (struct dinode *) (img_ptr + 2 * BSIZE);

  for(int i = 0; i < sb->ninodes; i++)
  {
    if(dip[i].type < 0 || dip[i].type > T_DEV)   //type 0 is for unused or unallocated
    {
      print_error("ERROR: bad inode.");
    }
  }

  //Check 3:For in-use inodes, each address that is used by inode is valid 
  //(points to a valid datablock address within the image). 

  //void* datablock_start_addr = (void*)(img_ptr + 3 * BSIZE + (sb->ninodes/IPB)*BSIZE + bitmap_blocks * BSIZE);
  // The first data block number (0 is unused, 1 is super block, inode blocks, 1 free block, bitmap blocks, data blocks) 
  int datablock_start =  3 + (sb->ninodes/IPB) + bitmap_blocks;

  for(int i = 0; i < sb->ninodes; i++)
  {
    if((dip[i].type != 0) && (dip[i].type <= 3)) //T_FILE, T_DIR, T_DEV
    {
      for(int j = 0; j < NDIRECT; j++)
      {
        if(dip[i].addrs[j] != 0 && (dip[i].addrs[j] < datablock_start || dip[i].addrs[j] >= datablock_start + sb->nblocks))
        {
          print_error("ERROR: bad direct address in inode.");
        }

        //Check 7 : For in-use inodes, each direct address in use is only used once.
        if((dip[i].addrs[j] != 0) && (blocks_marked_used[dip[i].addrs[j]] == 1))
        {
          print_error("ERROR: direct address used more than once.");
        }

        blocks_marked_used[dip[i].addrs[j]] = 1; //Mark these direct blocks as in use
      }

      if(dip[i].addrs[NDIRECT] != 0 && (dip[i].addrs[NDIRECT] < datablock_start || dip[i].addrs[NDIRECT] >= datablock_start + sb->nblocks))
      {
        print_error("ERROR: bad indirect address in inode.");
      }
      blocks_marked_used[dip[i].addrs[NDIRECT]] = 1;  //Mark the indirect address block as used

      //Check if indirect block addresses(block addresses in the indirect block) are correct for every inode indirect block
      uint* indirect_blk_ptr = (uint*)(img_ptr + dip[i].addrs[NDIRECT]* BSIZE);
      for(int j = 0; j < NINDIRECT; j++)
      {
        //The block number entries in the indirect address block should be a valid data block i.e should belong in (datablock_start, datablock_start + total number of blocks) 
        if(indirect_blk_ptr[j] != 0 && (indirect_blk_ptr[j] < datablock_start || indirect_blk_ptr[j] >= datablock_start + sb->nblocks))
        {
          print_error("ERROR: bad indirect address in inode.");
        }

        blocks_marked_used[indirect_blk_ptr[j]] = 1;  //Mark these indirect data blocks as used
      }

    }
  }

  //Check 4: Each directory contains . and .. entries, and the . entry points to the directory itself.
  for(int i = 0 ; i < sb->ninodes; i++)
  {
    if(i == 1 && dip[i].type != T_DIR)
      print_error("ERROR: directory not properly formatted.");

    if(dip[i].type == T_DIR)
    {
      struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + dip[i].addrs[0] * BSIZE);
      if(strcmp(entry[0].name, ".") != 0 || strcmp(entry[1].name, "..") != 0 || entry[0].inum != i)
      {
        print_error("ERROR: directory not properly formatted.");
      }

      // parent of root .. points to itself
      if((i == 1) && (entry[1].inum != 1))
        print_error("ERROR: directory not properly formatted.");

    }
  }

  /*
    //Check 4: Each directory contains . and .. entries, and the . entry points to the directory itself.
  for(int i = 0 ; i < sb->ninodes; i++)
  {
    if(dip[i].type == T_DIR)
    {
      for(int j = 0; j < NDIRECT; j++)
      {
        struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + dip[i].addrs[j] * BSIZE);
        if(strcmp(entry[0].name, ".") != 0 || strcmp(entry[1].name, "..") != 0 || entry[0].inum != i)
        {
          print_error("ERROR: directory not properly formatted.");
        }

        if(i == 1 && entry[1].inum != i)
        {
          print_error("ERROR: directory not properly formatted.");
        }
      }
    }
  }
 */
  

  //Check 5: For in-use inodes, each address in use is also marked in use in the bitmap.
  char* bitmap_start_addr = (char *) (img_ptr + 3 * BSIZE + ((sb->ninodes/IPB) * BSIZE));
  for (int i = 0; i < sb->ninodes; i++) 
  {
    if (dip[i].type > 0 && dip[i].type <= 3)
    {
    	for(int j = 0; j < NDIRECT+1; j++) 
      {
    		if(dip[i].addrs[j] != 0) //Check only the blocks marked used in the inode
        { 
    		  uint bitmap_byte_offset = (dip[i].addrs[j]) / 8;
    		  uint bit_offset_in_bitmap_byte = (dip[i].addrs[j]) % 8;
    		  if(((bitmap_start_addr[bitmap_byte_offset] >> bit_offset_in_bitmap_byte) & 1) == 0)
            print_error("ERROR: address used by inode but marked free in bitmap.");
        }
    	}
    	uint* indirect_blk_ptr = (uint*) (img_ptr + dip[i].addrs[NDIRECT] * BSIZE);
    	for (int k = 0; k < NINDIRECT; k++)
      {
    	  if(indirect_blk_ptr[k] != 0)
        {
    		  uint bitmap_byte_offset = (indirect_blk_ptr[k]) / 8;
    			uint bit_offset_in_bitmap_byte = (indirect_blk_ptr[k]) % 8;
    			if(((bitmap_start_addr[bitmap_byte_offset] >> bit_offset_in_bitmap_byte) & 1) == 0) 
            print_error("ERROR: address used by inode but marked free in bitmap.");
    	  }
      }
    }
  }

    //Check 6 : For blocks marked in-use in bitmap, the block should actually be in-use in an inode or indirect block somewhere
    for(int i = 0; i < sb->nblocks; i++)
    {
      uint bitmap_byte_offset = (datablock_start + i) / 8;
    	uint bit_offset_in_bitmap_byte = (datablock_start + i) % 8;
      if(((bitmap_start_addr[bitmap_byte_offset] >> bit_offset_in_bitmap_byte) & 1) == 1)
      {
        if(blocks_marked_used[datablock_start + i] != 1)
          print_error("ERROR: bitmap marks block in use but it is not in use.");
      }   
    }

    //Check 8 : For in-use inodes, the file size stored must be within the actual number of blocks used for storage.
    for(int i = 0; i < sb->ninodes ; i++)
    {
      if (dip[i].type == T_FILE)
      {
        int num_blks_used = 0;
        //Count the number of direct data blocks used by this inode     
        for(int j = 0 ; j < NDIRECT; j++)
        {
          if(dip[i].addrs[j] != 0)
            num_blks_used++;
        }
        if(dip[i].addrs[NDIRECT] != 0) //Check if indirect block address is used, this means indirect blocks are also used by the file
        {
          //Count the number of indirect data blocks used by this inode
          uint* indirect_blk_ptr = (uint*) (img_ptr + dip[i].addrs[NDIRECT] * BSIZE);
          for(int k = 0; k < NINDIRECT; k++)
          {
            if(indirect_blk_ptr[k] != 0)
              num_blks_used++;
          }

        }
        //fprintf(stdout, "inode %d uses %d data blocks and file size is %d\n", i, num_blks_used, dip[i].size);
        if(num_blks_used > 0)
        {
          if(!((dip[i].size > (num_blks_used - 1) * BSIZE) && (dip[i].size <= num_blks_used * BSIZE)))
          {
            print_error("ERROR: incorrect file size in inode.");
          }
        }
      }
    }

    //Form arrays of inodes marked used and directory refrences to inodes.
  for (int i = 0; i < sb->ninodes; i++) 
  {
    if (dip[i].type == T_DIR)  //For every directory inode
    {
    	for (int j = 0; j < NDIRECT; j++)  //For every used block of directory entries
      {
    		if(dip[i].addrs[j] != 0)
        {
	        struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + dip[i].addrs[j] * BSIZE);
				  for (int k = 0; k < (BSIZE / sizeof(struct xv6_dirent)); k++) //For every directory entry in a block
          {
					  if(/*j > 0 ||*/ k > 1)
            {
              directory_inode_reference[entry[k].inum]++;  //Number of times the inode is referenced by the directories
				    }
	          inodes_marked_used[entry[k].inum]++;
	        }
        }
	    }
	    uint* indirect_ptr_addr = (uint*) (img_ptr + BSIZE * dip[i].addrs[NDIRECT]);
	    for (int j = 0; j < NINDIRECT; j++) //For every indirect address block
      {
	      if(indirect_ptr_addr[j] != 0)
        {
	        struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + indirect_ptr_addr[j] * BSIZE); 
				  for (int k = 0; k < (BSIZE / sizeof(struct xv6_dirent)); k++)  //For every directory entry in the indirect block
          {
	          inodes_marked_used[entry[k].inum]++;
	          directory_inode_reference[entry[k].inum]++;
	        }
        }
	    }
    }
	}

	for(int i = 1; i < sb->ninodes; i++) 
  {
    // Check 9 : For all inodes marked in use, each must be referred to in at least one directory.
		if(dip[i].type > 0 && dip[i].type <= 3) 
    {
			if(inodes_marked_used[i] == 0) 
      {
        print_error("ERROR: inode marked used but not found in a directory.");
			}
		}

    // Check 10 : For each inode number that is referred to in a valid directory, it is actually marked in use.
    if(inodes_marked_used[i] != 0 && dip[i].type == 0) 
    {
			print_error("ERROR: inode referred to in directory but marked free.");
		}

    // Check 11 : Reference counts (number of links) for regular files match the 
    //number of times file is referred to in directories (i.e., hard links work correctly).
    if((dip[i].type == T_FILE) && (inodes_marked_used[i] != dip[i].nlink))
    {
      print_error("ERROR: bad reference count for file.");
		}

    // Check 12 : No extra links allowed for directories (each directory only appears in one other directory). 
    if((dip[i].type == T_DIR) && (directory_inode_reference[i] > 1))
    {
			print_error("ERROR: directory appears more than once in file system.");
		}
	}

  exit(0);
}
