/*
 * file_transfer_tools.c
 *
 *  Created on: 08/ago/2012
 *      Author: michele
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <bp_abstraction_api.h>
#include "file_transfer_tools.h"
#include "bundle_tools.h"

file_transfer_info_list_t file_transfer_info_list_create()
{
	file_transfer_info_list_t * list;
	list = (file_transfer_info_list_t *) malloc(sizeof(file_transfer_info_list_t));
	list->first = NULL;
	list->last = NULL;
	list->count = 0;
	return * list;
}

void file_transfer_info_list_destroy(file_transfer_info_list_t * list)
{
	free(list);
}

file_transfer_info_t * file_transfer_info_create(bp_endpoint_id_t client_eid,
		int filename_len,
		char * filename,
		char * full_dir,
		u32_t file_dim,
		u32_t bundle_time,
		u32_t expiration)
{
	file_transfer_info_t * info;
	info = (file_transfer_info_t *) malloc(sizeof(file_transfer_info_t));
	bp_copy_eid(&(info->client_eid), &client_eid);
	info->filename_len = filename_len;
	info->full_dir = (char*) malloc(strlen(full_dir) + 1);
	strncpy(info->full_dir, full_dir, strlen(full_dir) + 1);
 	info->filename = (char*) malloc(filename_len + 1);
	strncpy(info->filename, filename, filename_len +1);
	info->file_dim = file_dim;
	info->bytes_recvd = 0;
	info->first_bundle_time = bundle_time;
	info->last_bundle_time = bundle_time;
	info->expiration = expiration;
	return info;
}

void file_transfer_info_destroy(file_transfer_info_t * info)
{
	free(info->filename);
	free(info->full_dir);
	free(info);
}

void file_transfer_info_put(file_transfer_info_list_t * list, file_transfer_info_t * info)
{
	file_transfer_info_list_item_t * new;
	new = (file_transfer_info_list_item_t *) malloc(sizeof(file_transfer_info_list_item_t));
	new->info = info;
	new->next = NULL;
	if (list->first == NULL) // empty list
	{
		new->previous = NULL;
		list->first = new;
		list->last = new;
	}
	else
	{
		new->previous = list->last;
		list->last->next = new;
		list->last = new;
	}
	list->count ++;
}

file_transfer_info_list_item_t *  file_transfer_info_get_list_item(file_transfer_info_list_t * list, bp_endpoint_id_t client)
{
	file_transfer_info_list_item_t * item = list->first;
	while (item != NULL)
	{
		if (strcmp(item->info->client_eid.uri, client.uri) == 0)
		{
			return item;
		}
		item = item->next;
	}
	return NULL;
}

file_transfer_info_t *  file_transfer_info_get(file_transfer_info_list_t * list, bp_endpoint_id_t client)
{
	file_transfer_info_list_item_t * item;
	item = file_transfer_info_get_list_item(list, client);
	if (item != NULL)
	{
		return item->info;
	}
	return NULL;
}

void file_transfer_info_del(file_transfer_info_list_t * list, bp_endpoint_id_t client)
{
	file_transfer_info_list_item_t * item;
	item = file_transfer_info_get_list_item(list, client);
	if (item != NULL)
	{
		if (item->next == NULL && item->previous == NULL) // unique element of the list
		{
			list->first = NULL;
			list->last = NULL;
		}
		else if (item->next == NULL)  // last element of list
		{
			item->previous->next = NULL;
			list->last = item->previous;
		}
		else if (item->previous == NULL) // first element of list
		{
			item->next->previous = NULL;
			list->first = item->next;
		}
		else // generic element of list
		{
			item->next->previous = item->previous;
			item->previous->next = item->next;
		}
		file_transfer_info_destroy(item->info);
		free(item);
		list->count --;
	}
}



int assemble_file(file_transfer_info_t * info, FILE * pl_stream,
		u32_t pl_size, u32_t timestamp_secs, u32_t expiration)
{
	char * transfer;
	u32_t transfer_len;
	int fd;
	u32_t offset;


	// transfer length is total payload length without header,
	// congestion control char and file fragment offset
	transfer_len = get_file_fragment_size(pl_size, info->filename_len);

	// read file fragment offset
	fread(&offset, sizeof(u32_t), 1, pl_stream);

	// read remaining file fragment
	transfer = (char*) malloc(transfer_len);
	if (fread(transfer, transfer_len, 1, pl_stream) != 1)
		return -1;

	// open or create destination file
	char* filename = (char*) malloc(info->filename_len + strlen(info->full_dir) +1);
	strcpy(filename, info->full_dir);
	strcat(filename, info->filename);
	fd = open(filename, O_WRONLY | O_CREAT, 0755);
	if (fd < 0)
	{
		return -1;
	}


	// write fragment
	lseek(fd, offset, SEEK_SET);
	if (write(fd, transfer, transfer_len) < 0)
		return -1;
	close(fd);

	// deallocate resources
	free(filename);
	free(transfer);

	// update info
	info->bytes_recvd += transfer_len;
	info->expiration = expiration;
	info->last_bundle_time = timestamp_secs;

	// if transfer completed return 1
	if (info->bytes_recvd >= info->file_dim)
		return 1;

	return 0;

}

int process_incoming_file_transfer_bundle(file_transfer_info_list_t *info_list,
		pending_bundle_list_t * pending_list,
		bp_bundle_object_t * bundle,
		char * dir)
{
	bp_endpoint_id_t client_eid;
	bp_timestamp_t timestamp;
	bp_timeval_t expiration;
	file_transfer_info_t * info;
	FILE * pl_stream;
	int result, filename_len;
	char * filename;
	u32_t file_dim, pl_size;
	char * eid, temp[256];
	char * full_dir;

	// get info from bundle
	bp_bundle_get_source(*bundle, &client_eid);
	bp_bundle_get_creation_timestamp(*bundle, &timestamp);
	bp_bundle_get_expiration(*bundle, &expiration);
	bp_bundle_get_payload_size(*bundle, &pl_size);

	// create stream from incoming bundle payload
	if (open_payload_stream_read(*bundle, &pl_stream) < 0)
		return -1;

	// skip header and congestion control char
	fseek(pl_stream, HEADER_SIZE + 1, SEEK_SET);

	info = file_transfer_info_get(info_list, client_eid);
	if (info == NULL) // this is the first bundle
	{
		// get filename len
		result = fread(&filename_len, sizeof(int), 1, pl_stream);

		// get filename
		filename = (char *) malloc(filename_len + 1);
		memset(filename, 0, filename_len + 1);
		result = fread(filename, filename_len, 1, pl_stream);
		if(result < 1 )
			perror("fread");
		filename[filename_len] = '\0';

		//get file size
		fread(&file_dim, sizeof(u32_t), 1, pl_stream);

		// create destination dir for file
		strncpy(temp, client_eid.uri, strlen(client_eid.uri) + 1);
		strtok(temp, "/");
		eid = strtok(NULL, "/");
		full_dir = (char*) malloc(strlen(dir) + strlen(eid) + 20);
		sprintf(full_dir, "%s/%s/%lu/", dir, eid, timestamp.secs);
		sprintf(temp, "mkdir -p %s", full_dir);
		system(temp);

		// create file transfer info object
		info = file_transfer_info_create(client_eid, filename_len, filename, full_dir, file_dim, timestamp.secs, expiration);

		// insert info into info list
		file_transfer_info_put(info_list, info);

		free(full_dir);

	}
	else  // first bundle of transfer already received
	{
		// skip filename_len and filename
		fseek(pl_stream, sizeof(int) + strlen(info->filename), SEEK_CUR);

		// skip file_size
		fseek(pl_stream, sizeof(u32_t), SEEK_CUR);


	}
	// assemble file
	result = assemble_file(info, pl_stream, pl_size, timestamp.secs, expiration);
	close_payload_stream_read(pl_stream);

	if (result < 0) // error
		return result;
	if (result == 1) // transfer completed
	{
		// remove info from list
		file_transfer_info_del(info_list, client_eid);
		return 1;
	}
	return 0;


}

u32_t get_file_fragment_size(long payload_size, int filename_len)
{
	u32_t result;
	// file fragment size is payload without header, congestion ctrl char and offset
	result = payload_size - (HEADER_SIZE + 1 + sizeof(u32_t));
	// ... without filename_len, filename, file_size
	result -= (filename_len + sizeof(filename_len) + sizeof(u32_t));
	return result;
}

bp_error_t prepare_file_transfer_payload(dtnperf_options_t *opt, FILE * f, int fd,
		char * filename, u32_t file_dim, boolean_t * eof)
{
	if (f == NULL)
		return BP_ENULLPNTR;

	bp_error_t result;
	u32_t fragment_len;
	char * fragment;
	u32_t offset;
	long bytes_read;
	int filename_len = strlen(filename);

	// prepare header and congestion control
	result = prepare_payload_header(opt, f);

	// write filename length
	fwrite(&filename_len, sizeof(int), 1, f);

	// write filename
	fwrite(filename, filename_len, 1, f);

	//write file size
	fwrite(&file_dim, sizeof(file_dim), 1, f);

	// get size of fragment and allocate fragment
	fragment_len = get_file_fragment_size(opt->bundle_payload, filename_len);
	fragment = (char *) malloc(fragment_len);

	// get offset of fragment
	offset = lseek(fd, 0, SEEK_CUR);
	// write offset in the bundle
	fwrite(&offset, sizeof(offset), 1, f);

	// read fragment from file
	bytes_read = read(fd, fragment, fragment_len);
	if (bytes_read < fragment_len) // reached EOF
		*eof = TRUE;
	else
		*eof = FALSE;

	// write fragment in the bundle
	fwrite(fragment, bytes_read, 1, f);

	return result;
}

