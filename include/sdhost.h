#ifndef _SD_HOST_H_
#define _SD_HOST_H_

#define BLOCK_SIZE 512

void sd_init();
void write_block(int block_idx, void *buf);
void read_block(int block_idx, void *buf);

#endif /* _SD_HOST_H_ */