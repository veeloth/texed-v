#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct tex
  {
  size_t size;
  size_t cur;
  char data[];
  };

struct tex* shtex_create(char name[256], size_t size)
  {
  struct tex* ret = NULL;
  struct stat fstats;
  size_t length = sizeof(struct tex) + size;
  char fpath[265] = "/dev/shm/"; strcat(fpath, name);
  int fexists;
  int fd;

  fexists = !access(fpath, F_OK);/* record whether file already existed before running shm_open,
                                    this way we know if someone else already create it, so we 
                                    know to not set it's length and instead query it with fstat
                                  */


  if ((fd = shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) == -1)
    {
    perror("file opening failed");
    return ret;
    }

  if (!fexists)//only if file DIDN'T exist already (that is, if someone else created it already)
    {//set file's length
    if (ftruncate(fd, length) == -1)
      {
      perror("size setting failed");
      return ret;
      }
    if ((ret = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
      {
      perror("mapping failed");
      return ret;
      }
    ret->cur = 0;
    ret->data[ret->cur] = 0;
    ret->size = size;
    return ret;
    }

  //if file already existed, get the size
  stat(fpath, &fstats);
  length = fstats.st_size;
  if ((ret = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
    {
    perror("mapping failed");
    return ret;
    }
  //IMPORTANT WARNING: do not set any of the values if it already exists, as they're already set
  if (ret->size < ret->cur) ret->cur = 0;
  //ret->data[ret->cur] = 0;
  return ret;
  }
