/** @file buf.c
 * @brief Dynamic buffer.
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "faux/faux.h"
#include "faux/str.h"
#include "faux/buf.h"

// Default chunk size
#define DATA_CHUNK 4096

struct faux_buf_s {
	faux_list_t *list; // List of chunks
	faux_list_node_t *wchunk; // Chunk to write to
	size_t rpos; // Read position within first chunk
	size_t wpos; // Write position within wchunk (can be non-last chunk)
	size_t chunk_size; // Size of chunk
	size_t len; // Whole data length
	size_t limit; // Overflow limit
	size_t rblocked;
	size_t wblocked;
};


/** @brief Create new dynamic buffer object.
 *
 * @param [in] chunk_size Chunk size. If "0" then default size will be used.
 * @return Allocated object or NULL on error.
 */
faux_buf_t *faux_buf_new(size_t chunk_size)
{
	faux_buf_t *buf = NULL;

	buf = faux_zmalloc(sizeof(*buf));
	assert(buf);
	if (!buf)
		return NULL;

	// Init
	buf->chunk_size = (chunk_size != 0) ? chunk_size : DATA_CHUNK;
	buf->limit = FAUX_BUF_UNLIMITED;
	buf->list = faux_list_new(FAUX_LIST_UNSORTED, FAUX_LIST_NONUNIQUE,
		NULL, NULL, faux_free);
	buf->rpos = 0;
	buf->wpos = buf->chunk_size;
	buf->len = 0;
	buf->wchunk = NULL;
	buf->rblocked = 0; // Unblocked
	buf->wblocked = 0; // Unblocked

	return buf;
}


/** @brief Free dynamic buffer object.
 *
 * @param [in] buf Buffer object.
 */
void faux_buf_free(faux_buf_t *buf)
{
	if (!buf)
		return;

	faux_list_free(buf->list);

	faux_free(buf);
}


ssize_t faux_buf_len(const faux_buf_t *buf)
{
	assert(buf);
	if (!buf)
		return -1;

	return buf->len;
}


static ssize_t faux_buf_chunk_num(const faux_buf_t *buf)
{
	assert(buf);
	if (!buf)
		return -1;
	assert(buf->list);
	if (!buf->list)
		return -1;

	return faux_list_len(buf->list);
}


ssize_t faux_buf_limit(const faux_buf_t *buf)
{
	assert(buf);
	if (!buf)
		return -1;

	return buf->limit;
}


/** @brief Set size limit.
 *
 * Read limits define conditions when the read callback will be executed.
 * Buffer must contain data amount greater or equal to "min" value. Callback
 * will not get data amount greater than "max" value. If min == max then
 * callback will be executed with fixed data size. The "max" value can be "0".
 * It means indefinite i.e. data transferred to callback can be really large.
 *
 * @param [in] buf Allocated and initialized buf I/O object.
 * @param [in] min Minimal data amount.
 * @param [in] max Maximal data amount. The "0" means indefinite.
 * @return BOOL_TRUE - success, BOOL_FALSE - error.
 */
bool_t faux_buf_set_limit(faux_buf_t *buf, size_t limit)
{
	assert(buf);
	if (!buf)
		return BOOL_FALSE;

	buf->limit = limit;

	return BOOL_TRUE;
}


/** @brief Get amount of unused space within current data chunk.
 *
 * Inernal static function.
 *
 * @param [in] list Internal buffer (list of chunks) to inspect.
 * @param [in] pos Current write position within last chunk
 * @return Size of unused space or < 0 on error.
 */
static ssize_t faux_buf_wavail(faux_buf_t *buf)
{
	assert(buf);
	if (!buf)
		return -1;

	if (faux_buf_chunk_num(buf) == 0)
		return 0; // Empty list

	return (buf->chunk_size - buf->wpos);
}


/*
static ssize_t faux_buf_ravail(faux_buf_t *buf)
{
	ssize_t num = 0;

	assert(buf);
	if (!buf)
		return -1;

	num = faux_buf_chunk_num(buf);
	if (num == 0)
		return 0; // Empty list
	if (num > 1)
		return (buf->chunk_size - buf->rpos);

	// Single chunk
	return (buf->wpos - buf->rpos);
}
*/

bool_t faux_buf_is_wblocked(const faux_buf_t *buf)
{
	assert(buf);
	if (!buf)
		return BOOL_FALSE;

	if (buf->wblocked != 0)
		return BOOL_TRUE;

	return BOOL_FALSE;
}


bool_t faux_buf_is_rblocked(const faux_buf_t *buf)
{
	assert(buf);
	if (!buf)
		return BOOL_FALSE;

	if (buf->rblocked != 0)
		return BOOL_TRUE;

	return BOOL_FALSE;
}


static faux_list_node_t *faux_buf_alloc_chunk(faux_buf_t *buf)
{
	char *chunk = NULL;

	assert(buf);
	if (!buf)
		return NULL;
	assert(buf->list);
	if (!buf->list)
		return NULL;

	chunk = faux_malloc(buf->chunk_size);
	assert(chunk);
	if (!chunk)
		return NULL;

	return faux_list_add(buf->list, chunk);
}


/*
static bool_t faux_buf_rm_trailing_empty_chunks(faux_buf_t *buf)
{
	faux_list_node_t *node = NULL;

	assert(buf);
	if (!buf)
		return BOOL_FALSE;
	assert(buf->list);
	if (!buf->list)
		return BOOL_FALSE;

	if (faux_buf_chunk_num(buf) == 0)
		return BOOL_TRUE; // Empty list


	while ((node = faux_list_tail(buf->list)) != buf->wchunk)
		faux_list_del(buf->list, node);
	if (buf->wchunk &&
		((buf->wpos == 0) || // Empty chunk
		((faux_list_chunk_num(buf) == 1) && (buf->rpos == buf->wpos)))
		) {
		faux_list_del(buf->list, buf->wchunk);
		buf->wchunk = NULL;
		buf->wpos = buf->chunk_size;
	}

	return BOOL_TRUE;
}
*/

static bool_t faux_buf_will_be_overflow(const faux_buf_t *buf, size_t add_len)
{
	assert(buf);
	if (!buf)
		return BOOL_FALSE;

	if (FAUX_BUF_UNLIMITED == buf->limit)
		return BOOL_FALSE;

	if ((buf->len + add_len) > buf->limit)
		return BOOL_TRUE;

	return BOOL_FALSE;
}


bool_t faux_buf_is_overflow(const faux_buf_t *buf)
{
	return faux_buf_will_be_overflow(buf, 0);
}


/** @brief buf data write.
 *
 * All given data will be stored to internal buffer (list of data chunks).
 * Then function will try to write stored data to file descriptor in
 * non-blocking mode. Note some data can be left within buffer. In this case
 * the "stall" callback will be executed to inform about it. To try to write
 * the rest of the data user can be call faux_buf_out() function. Both
 * functions will not block.
 *
 * @param [in] buf Allocated and initialized buf I/O object.
 * @param [in] data Data buffer to write.
 * @param [in] len Data length to write.
 * @return Length of stored/writed data or < 0 on error.
 */
ssize_t faux_buf_write(faux_buf_t *buf, const void *data, size_t len)
{
	size_t data_left = len;

	assert(buf);
	if (!buf)
		return -1;
	assert(data);
	if (!data)
		return -1;

	// It will be overflow after writing
	if (faux_buf_will_be_overflow(buf, len))
		return -1;

	// Don't write to the space reserved for direct write
	if (faux_buf_is_wblocked(buf))
		return -1;

	while (data_left > 0) {
		ssize_t bytes_free = 0;
		size_t copy_len = 0;
		char *chunk_ptr = NULL;

		// Allocate new chunk if necessary
		bytes_free = faux_buf_wavail(buf);
		if (bytes_free < 0)
			return -1;
		if (0 == bytes_free) {
			faux_list_node_t *node = faux_buf_alloc_chunk(buf);
			assert(node);
			if (!node) // Something went wrong. Strange.
				return -1;
			buf->wpos = 0;
			buf->wchunk = node;
			bytes_free = faux_buf_wavail(buf);
		}

		// Copy data
		chunk_ptr = faux_list_data(faux_list_tail(buf->list));
		copy_len = (data_left < (size_t)bytes_free) ? data_left : (size_t)bytes_free;
		memcpy(chunk_ptr + buf->wpos, data + len - data_left, copy_len);
		buf->wpos += copy_len;
		data_left -= copy_len;
		buf->len += copy_len;
	}

	return len;
}

#if 0

/** @brief Write output buffer to fd in non-blocking mode.
 *
 * Previously data must be written to internal buffer by faux_buf_write()
 * function. But some data can be left within internal buffer because can't be
 * written to fd in non-blocking mode. This function tries to write the rest of
 * data to fd in non-blocking mode. So function doesn't block. It can be called
 * after select() or poll() if fd is ready to be written to. If function can't
 * to write all buffer to fd it executes "stall" callback to inform about it.
 *
 * @param [in] buf Allocated and initialized buf I/O object.
 * @return Length of data actually written or < 0 on error.
 */
ssize_t faux_buf_read(faux_buf_t *buf, void *data, size_t len)
{
	ssize_t total_written = 0;
	size_t must_be_read = 0;

	assert(buf);
	if (!buf)
		return -1;

	// Don't read from the space reserved for direct read
	if (faux_buf_is_rblocked(buf))
		return -1;

	must_be_read = (len < buf->len) ? len : buf->len;
	while (must_be_read > 0) {
		faux_list_node_t *node = NULL;
		char *chunk_ptr = NULL;
		ssize_t data_to_write = 0;
		ssize_t bytes_written = 0;
		bool_t postpone = BOOL_FALSE;

		node = faux_list_head(buf->o_list);
		if (!node) // List is empty while o_size > 0
			return -1;
		chunk_ptr = faux_list_data(node);
		data_to_write = data_avail(buf->o_list,
			buf->o_rpos, buf->o_wpos);
		if (data_to_write <= 0) // Strange case
			return -1;

		bytes_written = write(buf->fd, chunk_ptr + buf->o_rpos,
			data_to_write);
		if (bytes_written > 0) {
			buf->o_size -= bytes_written;
			total_written += bytes_written;
		}

		if (bytes_written < 0) {
			if ( // Something went wrong
				(errno != EINTR) &&
				(errno != EAGAIN) &&
				(errno != EWOULDBLOCK)
			)
				return -1;
			// Postpone next read
			postpone = BOOL_TRUE;

		// Not whole data block was written
		} else if (bytes_written != data_to_write) {
			buf->o_rpos += bytes_written;
			// Postpone next read
			postpone = BOOL_TRUE;
		}

		// Postponed
		if (postpone) {
			// Execute callback
			if (buf->stall_cb)
				buf->stall_cb(buf, buf->o_size,
					buf->stall_udata);
			break;
		}

		// Not postponed. Current chunk was fully written. So
		// remove it from list.
		buf->o_rpos = 0;
		faux_list_del(buf->o_list, node);
	}

	return total_written;
}
#endif
