/*
 * Copyright (C) 2012 Sebastian Pipping <sebastian@pipping.org>
 * Licensed under GPL v3 or later
 */

#define _POSIX_C_SOURCE 200809L  /* for pread, pwrite */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#define BLOCK_SIZE 4096

#define TODO_ASSERT assert  /* i.e. needs proper runtime handling */


void notify_skipped(off_t first_skipped_byte, off_t last_skipped_byte, off_t blocks_affected) {
	printf("Bytes %ld to %ld identical already (%ld blocks)\n", first_skipped_byte, last_skipped_byte, blocks_affected);
}


void notify_written(off_t first_written_byte, off_t last_written_byte, off_t blocks_affected) {
	printf("Bytes %ld to %ld copied (%ld blocks)\n", first_written_byte, last_written_byte, blocks_affected);
}


int main(int argc, char ** argv) {
	if (argc != 3) {
		fprintf(stderr, "USAGE:\n  %s SOURCE DESTINATION\n", argv[0]);
		return 1;
	}

	const char * const src = argv[1];
	const char * const dst = argv[2];

	const int fd_src = open(src, O_RDONLY);
	const int fd_dst = open(dst, O_RDWR);
	TODO_ASSERT(fd_src != -1);
	TODO_ASSERT(fd_dst != -1);

	char buf_src[BLOCK_SIZE];
	char buf_dst[BLOCK_SIZE];

	struct stat st_src;
	struct stat st_dst;
	const int fstat_res_src = fstat(fd_src, &st_src);
	const int fstat_res_dst = fstat(fd_dst, &st_dst);
	TODO_ASSERT(fstat_res_src != -1);
	TODO_ASSERT(fstat_res_dst != -1);

	TODO_ASSERT(st_src.st_size == st_dst.st_size);

	const int block_count = st_src.st_size / BLOCK_SIZE + (st_src.st_size % BLOCK_SIZE != 0);

	int blocks_written = 0;

	int skip_first_index = -1;
	int skip_last_index = -1;

	int write_first_index = -1;
	int write_last_index = -1;

	for (int block_index = 0; block_index < block_count; block_index++) {
		const size_t count = (block_index == block_count - 1)
				? st_src.st_size - BLOCK_SIZE * (st_src.st_size / BLOCK_SIZE)
				: BLOCK_SIZE;
		const off_t offset = BLOCK_SIZE * (off_t)block_index;
		const ssize_t bytes_read_src = pread(fd_src, (void *)buf_src, count, offset);
		const ssize_t bytes_read_dst = pread(fd_dst, (void *)buf_dst, count, offset);
		TODO_ASSERT(bytes_read_src == count);
		TODO_ASSERT(bytes_read_dst == count);

		if (! memcmp(buf_src, buf_dst, BLOCK_SIZE) == 0) {
			if (skip_first_index != -1) {
				const off_t first_skipped_byte = BLOCK_SIZE * (off_t)skip_first_index;
				const off_t last_skipped_byte = BLOCK_SIZE * (off_t)(skip_last_index + 1) - 1;
				const off_t blocks_affected = skip_last_index - skip_first_index + 1;
				notify_skipped(first_skipped_byte, last_skipped_byte, blocks_affected);
				skip_first_index = -1;
			}

			const ssize_t bytes_written_dst = pwrite(fd_dst, buf_src, count, offset);
			TODO_ASSERT(bytes_written_dst == count);
			blocks_written++;

			if (write_first_index == -1) {
				write_first_index = block_index;
				write_last_index = block_index;
			} else {
				write_last_index++;
			}
		} else {
			if (write_first_index != -1) {
				const off_t first_written_byte = BLOCK_SIZE * (off_t)write_first_index;
				const off_t last_written_byte = BLOCK_SIZE * (off_t)(write_last_index + 1) - 1;
				const off_t blocks_affected = write_last_index - write_first_index + 1;
				notify_written(first_written_byte, last_written_byte, blocks_affected);
				write_first_index = -1;
			}

			if (skip_first_index == -1) {
				skip_first_index = block_index;
				skip_last_index = block_index;
			} else {
				skip_last_index++;
			}
		}

		if (block_index == block_count - 1) {
			assert(! ((skip_first_index != -1) && (write_first_index != -1)));

			if (skip_first_index != -1) {
				const off_t first_skipped_byte = BLOCK_SIZE * (off_t)skip_first_index;
				const off_t last_skipped_byte = offset + count - 1;
				const off_t blocks_affected = block_index - skip_first_index + 1;
				notify_skipped(first_skipped_byte, last_skipped_byte, blocks_affected);
				skip_first_index = -1;
			} else if (write_first_index != -1) {
				const off_t first_written_byte = BLOCK_SIZE * (off_t)write_first_index;
				const off_t last_written_byte = offset + count - 1;
				const off_t blocks_affected = block_index - write_first_index + 1;
				notify_written(first_written_byte, last_written_byte, blocks_affected);
				write_first_index = -1;
			}
		}
	}

	close(fd_src);
	close(fd_dst);

	const double percent_saved = (blocks_written == block_count)
			? 0.0
			: 100.0 - (blocks_written * 100.0 / block_count);

	printf("\n");
	printf("Blocks read: %d/%d  (%.2f%% wasted)\n", block_count * 2, block_count, 100.0);
	printf("Blocks written: %d/%d  (%.2f%% saved)\n", blocks_written, block_count, percent_saved);

	return 0;
}
