/*
 *  Sequencer Interface - main file
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@perex.cz>
 *                        Abramo Bagnara <abramo@alsa-project.org>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "seq_local.h"
#include <fcntl.h>
#include <sys/ioctl.h>

#ifndef PIC
/* entry for static linking */
const char *_snd_module_seq_hw = "";
#endif

#ifndef DOC_HIDDEN
#define SNDRV_FILE_SEQ		ALSA_DEVICE_DIRECTORY "seq"
#define SNDRV_FILE_ALOADSEQ	ALOAD_DEVICE_DIRECTORY "aloadSEQ"

typedef struct {
	int fd;
	int version;
} snd_seq_hw_t;
#endif /* DOC_HIDDEN */

static int snd_seq_hw_close(snd_seq_t *seq)
{
	snd_seq_hw_t *hw = seq->private_data;
	int err = 0;

	if (close(hw->fd)) {
		err = -errno;
		SYSERR("close failed\n");
	}
	free(hw);
	return err;
}

static int snd_seq_hw_nonblock(snd_seq_t *seq, int nonblock)
{
	snd_seq_hw_t *hw = seq->private_data;
	long flags;

	if ((flags = fcntl(hw->fd, F_GETFL)) < 0) {
		SYSERR("F_GETFL failed");
		return -errno;
	}
	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	if (fcntl(hw->fd, F_SETFL, flags) < 0) {
		SYSERR("F_SETFL for O_NONBLOCK failed");
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_client_id(snd_seq_t *seq)
{
	snd_seq_hw_t *hw = seq->private_data;
	int client;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_CLIENT_ID, &client) < 0) {
		SYSERR("SNDRV_SEQ_IOCTL_CLIENT_ID failed");
		return -errno;
	}
	return client;
}

static int snd_seq_hw_system_info(snd_seq_t *seq, snd_seq_system_info_t * info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_SYSTEM_INFO, info) < 0) {
		SYSERR("SNDRV_SEQ_IOCTL_SYSTEM_INFO failed");
		return -errno;
	}
	return 0;
}

static void update_midi_version(snd_seq_t *seq, snd_seq_client_info_t *info)
{
	snd_seq_hw_t *hw = seq->private_data;

	if (SNDRV_PROTOCOL_VERSION(1, 0, 3) <= hw->version &&
	    seq->midi_version != (int)info->midi_version) {
		seq->midi_version = info->midi_version;
		if (info->midi_version > 0)
			seq->packet_size = sizeof(snd_seq_ump_event_t);
		else
			seq->packet_size = sizeof(snd_seq_event_t);
	}
}

static int snd_seq_hw_get_client_info(snd_seq_t *seq, snd_seq_client_info_t * info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_CLIENT_INFO, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_GET_CLIENT_INFO failed");*/
		return -errno;
	}
	if (hw->version < SNDRV_PROTOCOL_VERSION(1, 0, 2)) {
		info->card = -1;
		info->pid = -1;
	}
	return 0;
}

static int snd_seq_hw_set_client_info(snd_seq_t *seq, snd_seq_client_info_t * info)
{
	snd_seq_hw_t *hw = seq->private_data;

	/* added fields are not checked on older kernels */
	if (SNDRV_PROTOCOL_VERSION(1, 0, 3) > hw->version) {
		if (info->midi_version > 0)
			return -EINVAL;
		if (info->filter & SNDRV_SEQ_FILTER_NO_CONVERT)
			return -EINVAL;
		if (info->group_filter != 0)
			return -EINVAL;
	}
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_SET_CLIENT_INFO, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_SET_CLIENT_INFO failed");*/
		return -errno;
	}
	update_midi_version(seq, info);
	return 0;
}

static int snd_seq_hw_get_ump_info(snd_seq_t *seq, int client, int type, void *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	struct snd_seq_client_ump_info buf;
	size_t size;

	if (type < 0 || type >= SNDRV_SEQ_CLIENT_UMP_INFO_BLOCK + 32)
		return -EINVAL;
	if (hw->version < SNDRV_PROTOCOL_VERSION(1, 0, 3))
		return -ENOTTY;
	if (type == SNDRV_SEQ_CLIENT_UMP_INFO_ENDPOINT)
		size = sizeof(struct snd_ump_endpoint_info);
	else
		size = sizeof(struct snd_ump_block_info);
	buf.client = client;
	buf.type = type;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_CLIENT_UMP_INFO, &buf) < 0)
		return -errno;
	memcpy(info, buf.info, size);
	return 0;
}

static int snd_seq_hw_set_ump_info(snd_seq_t *seq, int type, const void *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	struct snd_seq_client_ump_info buf;
	size_t size;

	if (type < 0 || type >= SNDRV_SEQ_CLIENT_UMP_INFO_BLOCK + 32)
		return -EINVAL;
	if (hw->version < SNDRV_PROTOCOL_VERSION(1, 0, 3))
		return -ENOTTY;
	if (type == SNDRV_SEQ_CLIENT_UMP_INFO_ENDPOINT)
		size = sizeof(struct snd_ump_endpoint_info);
	else
		size = sizeof(struct snd_ump_block_info);
	buf.client = seq->client;
	buf.type = type;
	memcpy(buf.info, info, size);
	*(int *)buf.info = -1; /* invalidate the card number */
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_SET_CLIENT_UMP_INFO, &buf) < 0)
		return -errno;
	return 0;
}

static int snd_seq_hw_create_port(snd_seq_t *seq, snd_seq_port_info_t * port)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_CREATE_PORT, port) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_CREATE_PORT failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_delete_port(snd_seq_t *seq, snd_seq_port_info_t * port)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_DELETE_PORT, port) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_DELETE_PORT failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_get_port_info(snd_seq_t *seq, snd_seq_port_info_t * info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_PORT_INFO, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_GET_PORT_INFO failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_set_port_info(snd_seq_t *seq, snd_seq_port_info_t * info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_SET_PORT_INFO, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_SET_PORT_INFO failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_get_port_subscription(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_SUBSCRIPTION, sub) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_GET_SUBSCRIPTION failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_subscribe_port(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_SUBSCRIBE_PORT, sub) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_SUBSCRIBE_PORT failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_unsubscribe_port(snd_seq_t *seq, snd_seq_port_subscribe_t * sub)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_UNSUBSCRIBE_PORT, sub) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_UNSUBSCRIBE_PORT failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_query_port_subscribers(snd_seq_t *seq, snd_seq_query_subscribe_t * subs)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_QUERY_SUBS, subs) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_QUERY_SUBS failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_get_queue_status(snd_seq_t *seq, snd_seq_queue_status_t * status)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_QUEUE_STATUS, status) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_GET_QUEUE_STATUS failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_get_queue_tempo(snd_seq_t *seq, snd_seq_queue_tempo_t * tempo)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_QUEUE_TEMPO, tempo) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_GET_QUEUE_TEMPO failed");*/
		return -errno;
	}
	if (!seq->has_queue_tempo_base)
		tempo->tempo_base = 1000;
	return 0;
}

static int snd_seq_hw_set_queue_tempo(snd_seq_t *seq, snd_seq_queue_tempo_t * tempo)
{
	snd_seq_hw_t *hw = seq->private_data;

	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_SET_QUEUE_TEMPO, tempo) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_SET_QUEUE_TEMPO failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_get_queue_timer(snd_seq_t *seq, snd_seq_queue_timer_t * timer)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_QUEUE_TIMER, timer) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_GET_QUEUE_TIMER failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_set_queue_timer(snd_seq_t *seq, snd_seq_queue_timer_t * timer)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_SET_QUEUE_TIMER, timer) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_SET_QUEUE_TIMER failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_get_queue_client(snd_seq_t *seq, snd_seq_queue_client_t * info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_QUEUE_CLIENT, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_GET_QUEUE_CLIENT failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_set_queue_client(snd_seq_t *seq, snd_seq_queue_client_t * info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_SET_QUEUE_CLIENT, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_SET_QUEUE_CLIENT failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_create_queue(snd_seq_t *seq, snd_seq_queue_info_t *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_CREATE_QUEUE, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_CREATE_QUEUE failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_delete_queue(snd_seq_t *seq, snd_seq_queue_info_t *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_DELETE_QUEUE, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_DELETE_QUEUE failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_get_queue_info(snd_seq_t *seq, snd_seq_queue_info_t *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_QUEUE_INFO, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_GET_QUEUE_INFO failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_set_queue_info(snd_seq_t *seq, snd_seq_queue_info_t *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_SET_QUEUE_INFO, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_SET_QUEUE_INFO failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_get_named_queue(snd_seq_t *seq, snd_seq_queue_info_t *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_NAMED_QUEUE, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_GET_NAMED_QUEUE failed");*/
		return -errno;
	}
	return 0;
}

static ssize_t snd_seq_hw_write(snd_seq_t *seq, void *buf, size_t len)
{
	snd_seq_hw_t *hw = seq->private_data;
	ssize_t result = write(hw->fd, buf, len);
	if (result < 0)
		return -errno;
	return result;
}

static ssize_t snd_seq_hw_read(snd_seq_t *seq, void *buf, size_t len)
{
	snd_seq_hw_t *hw = seq->private_data;
	ssize_t result = read(hw->fd, buf, len);
	if (result < 0)
		return -errno;
	return result;
}

static int snd_seq_hw_remove_events(snd_seq_t *seq, snd_seq_remove_events_t *rmp)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_REMOVE_EVENTS, rmp) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_REMOVE_EVENTS failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_get_client_pool(snd_seq_t *seq, snd_seq_client_pool_t *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_GET_CLIENT_POOL, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_GET_CLIENT_POOL failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_set_client_pool(snd_seq_t *seq, snd_seq_client_pool_t *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_SET_CLIENT_POOL, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_SET_CLIENT_POOL failed");*/
		return -errno;
	}
	return 0;
}

static int snd_seq_hw_query_next_client(snd_seq_t *seq, snd_seq_client_info_t *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_QUERY_NEXT_CLIENT, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_QUERY_NEXT_CLIENT failed");*/
		return -errno;
	}
	if (hw->version < SNDRV_PROTOCOL_VERSION(1, 0, 2)) {
		info->card = -1;
		info->pid = -1;
	}
	return 0;
}

static int snd_seq_hw_query_next_port(snd_seq_t *seq, snd_seq_port_info_t *info)
{
	snd_seq_hw_t *hw = seq->private_data;
	if (ioctl(hw->fd, SNDRV_SEQ_IOCTL_QUERY_NEXT_PORT, info) < 0) {
		/*SYSERR("SNDRV_SEQ_IOCTL_QUERY_NEXT_PORT failed");*/
		return -errno;
	}
	return 0;
}

static const snd_seq_ops_t snd_seq_hw_ops = {
	.close = snd_seq_hw_close,
	.nonblock = snd_seq_hw_nonblock,
	.system_info = snd_seq_hw_system_info,
	.get_client_info = snd_seq_hw_get_client_info,
	.set_client_info = snd_seq_hw_set_client_info,
	.get_ump_info = snd_seq_hw_get_ump_info,
	.set_ump_info = snd_seq_hw_set_ump_info,
	.create_port = snd_seq_hw_create_port,
	.delete_port = snd_seq_hw_delete_port,
	.get_port_info = snd_seq_hw_get_port_info,
	.set_port_info = snd_seq_hw_set_port_info,
	.get_port_subscription = snd_seq_hw_get_port_subscription,
	.subscribe_port = snd_seq_hw_subscribe_port,
	.unsubscribe_port = snd_seq_hw_unsubscribe_port,
	.query_port_subscribers = snd_seq_hw_query_port_subscribers,
	.get_queue_status = snd_seq_hw_get_queue_status,
	.get_queue_tempo = snd_seq_hw_get_queue_tempo,
	.set_queue_tempo = snd_seq_hw_set_queue_tempo,
	.get_queue_timer = snd_seq_hw_get_queue_timer,
	.set_queue_timer = snd_seq_hw_set_queue_timer,
	.get_queue_client = snd_seq_hw_get_queue_client,
	.set_queue_client = snd_seq_hw_set_queue_client,
	.create_queue = snd_seq_hw_create_queue,
	.delete_queue = snd_seq_hw_delete_queue,
	.get_queue_info = snd_seq_hw_get_queue_info,
	.set_queue_info = snd_seq_hw_set_queue_info,
	.get_named_queue = snd_seq_hw_get_named_queue,
	.write = snd_seq_hw_write,
	.read = snd_seq_hw_read,
	.remove_events = snd_seq_hw_remove_events,
	.get_client_pool = snd_seq_hw_get_client_pool,
	.set_client_pool = snd_seq_hw_set_client_pool,
	.query_next_client = snd_seq_hw_query_next_client,
	.query_next_port = snd_seq_hw_query_next_port,
};

int snd_seq_hw_open(snd_seq_t **handle, const char *name, int streams, int mode)
{
	int fd, ver, client, fmode, ret;
	const char *filename;
	snd_seq_t *seq;
	snd_seq_hw_t *hw;

	*handle = NULL;

	switch (streams) {
	case SND_SEQ_OPEN_OUTPUT:
		fmode = O_WRONLY;
		break;
	case SND_SEQ_OPEN_INPUT:
		fmode = O_RDONLY;
		break;
	case SND_SEQ_OPEN_DUPLEX:
		fmode = O_RDWR;
		break;
	default:
		assert(0);
		return -EINVAL;
	}
	
	if (mode & SND_SEQ_NONBLOCK)
		fmode |= O_NONBLOCK;

	filename = SNDRV_FILE_SEQ;
	fd = snd_open_device(filename, fmode);
#ifdef SUPPORT_ALOAD
	if (fd < 0) {
		fd = snd_open_device(SNDRV_FILE_ALOADSEQ, fmode);
		if (fd >= 0)
			close(fd);
		fd = snd_open_device(filename, fmode);
	}
#endif
	if (fd < 0) {
		SYSERR("open %s failed", filename);
		return -errno;
	}
	if (ioctl(fd, SNDRV_SEQ_IOCTL_PVERSION, &ver) < 0) {
		SYSERR("SNDRV_SEQ_IOCTL_PVERSION failed");
		ret = -errno;
		close(fd);
		return ret;
	}
	if (SNDRV_PROTOCOL_INCOMPATIBLE(ver, SNDRV_SEQ_VERSION)) {
		close(fd);
		return -SND_ERROR_INCOMPATIBLE_VERSION;
	}
	if (SNDRV_PROTOCOL_VERSION(1, 0, 3) <= ver) {
		/* inform the protocol version we're supporting */
		unsigned int user_ver = SNDRV_SEQ_VERSION;
		ioctl(fd, SNDRV_SEQ_IOCTL_USER_PVERSION, &user_ver);
	}
	hw = calloc(1, sizeof(snd_seq_hw_t));
	if (hw == NULL) {
		close(fd);
		return -ENOMEM;
	}

	seq = calloc(1, sizeof(snd_seq_t));
	if (seq == NULL) {
		free(hw);
		close(fd);
		return -ENOMEM;
	}
	hw->fd = fd;
	hw->version = ver;
	if (streams & SND_SEQ_OPEN_OUTPUT) {
		seq->obuf = (char *) malloc(seq->obufsize = SND_SEQ_OBUF_SIZE);
		if (!seq->obuf) {
			free(hw);
			free(seq);
			close(fd);
			return -ENOMEM;
		}
	}
	if (streams & SND_SEQ_OPEN_INPUT) {
		seq->ibuf = (char *) calloc(seq->ibufsize = SND_SEQ_IBUF_SIZE, sizeof(snd_seq_ump_event_t));
		if (!seq->ibuf) {
			free(seq->obuf);
			free(hw);
			free(seq);
			close(fd);
			return -ENOMEM;
		}
	}
	if (name)
		seq->name = strdup(name);
	seq->type = SND_SEQ_TYPE_HW;
	seq->streams = streams;
	seq->mode = mode;
	seq->tmpbuf = NULL;
	seq->tmpbufsize = 0;
	seq->poll_fd = fd;
	seq->ops = &snd_seq_hw_ops;
	seq->private_data = hw;
	seq->packet_size = sizeof(snd_seq_event_t);
	seq->has_queue_tempo_base = ver >= SNDRV_PROTOCOL_VERSION(1, 0, 4);

	client = snd_seq_hw_client_id(seq);
	if (client < 0) {
		snd_seq_close(seq);
		return client;
	} else
		seq->client = client;

#ifdef SNDRV_SEQ_IOCTL_RUNNING_MODE
	{
		struct snd_seq_running_info run_mode;
		/* check running mode */
		memset(&run_mode, 0, sizeof(run_mode));
		run_mode.client = client;
#ifdef SNDRV_BIG_ENDIAN
		run_mode.big_endian = 1;
#else
		run_mode.big_endian = 0;
#endif
		run_mode.cpu_mode = sizeof(long);
		ioctl(fd, SNDRV_SEQ_IOCTL_RUNNING_MODE, &run_mode);
	}
#endif

	*handle = seq;
	return 0;
}

int _snd_seq_hw_open(snd_seq_t **handlep, char *name,
		     snd_config_t *root ATTRIBUTE_UNUSED, snd_config_t *conf,
		     int streams, int mode)
{
	snd_config_iterator_t i, next;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (_snd_conf_generic_id(id))
			continue;
		return -EINVAL;
	}
	return snd_seq_hw_open(handlep, name, streams, mode);
}
SND_DLSYM_BUILD_VERSION(_snd_seq_hw_open, SND_SEQ_DLSYM_VERSION);
