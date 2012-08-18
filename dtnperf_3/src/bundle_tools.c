#include <stdio.h>
#include "bundle_tools.h"
#include "definitions.h"
#include <bp_abstraction_api.h>


// static variables for stream operations
static char * buffer;
static u32_t buffer_len;


/* ----------------------------------------------
 * bundles_needed
 * ---------------------------------------------- */
long bundles_needed (long data, long pl)
{
    long res = 0;
    ldiv_t r;

    r = ldiv(data, pl);
    res = r.quot;
    if (r.rem > 0)
        res += 1;

    return res;
} // end bundles_needed


/* ----------------------------
 * print_eid
 * ---------------------------- */
void print_eid(char * label, bp_endpoint_id_t * eid)
{
    printf("%s [%s]\n", label, eid->uri);
} // end print_eid




void init_info(send_information_t *send_info, int window)
{
    int i;

    for (i = 0; i < window; i++)
    {
        send_info[i].bundle_id.creation_ts.secs = 0;
        send_info[i].bundle_id.creation_ts.seqno = 0;
    }
} // end init_info



long add_info(send_information_t* send_info, bp_bundle_id_t bundle_id, struct timeval p_start, int window)
{
    int i;

    static u_int id = 0;
    static int last_inserted = -1;
    for (i = (last_inserted + 1); i < window ; i++)
    {
        if ((send_info[i].bundle_id.creation_ts.secs == 0) && (send_info[i].bundle_id.creation_ts.seqno == 0))
        {
            send_info[i].bundle_id.creation_ts.secs = bundle_id.creation_ts.secs;
            send_info[i].bundle_id.creation_ts.seqno = bundle_id.creation_ts.seqno;
            send_info[i].send_time.tv_sec = p_start.tv_sec;
            send_info[i].send_time.tv_usec = p_start.tv_usec;
            send_info[i].relative_id = id;
            last_inserted = i;
            return id++;
        }
    }
    for (i = 0; i <= last_inserted ; i++)
    {
        if ((send_info[i].bundle_id.creation_ts.secs == 0) && (send_info[i].bundle_id.creation_ts.seqno == 0))
        {
            send_info[i].bundle_id.creation_ts.secs = bundle_id.creation_ts.secs;
            send_info[i].bundle_id.creation_ts.seqno = bundle_id.creation_ts.seqno;
            send_info[i].send_time.tv_sec = p_start.tv_sec;
            send_info[i].send_time.tv_usec = p_start.tv_usec;
            send_info[i].relative_id = id;
            last_inserted = i;
            return id++;
        }
    }
    return -1;
} // end add_info


int is_in_info(send_information_t* send_info, bp_timestamp_t bundle_timestamp, int window)
{
    int i;

    static int last_collected = -1;
    for (i = (last_collected + 1); i < window; i++)
    {
        if ((send_info[i].bundle_id.creation_ts.secs == bundle_timestamp.secs) && (send_info[i].bundle_id.creation_ts.seqno == bundle_timestamp.seqno))
        {
            last_collected = i;
            return i;
        }
    }
    for (i = 0; i <= last_collected; i++)
    {
        if ((send_info[i].bundle_id.creation_ts.secs == bundle_timestamp.secs) && (send_info[i].bundle_id.creation_ts.seqno == bundle_timestamp.seqno))
        {
            last_collected = i;
            return i;
        }

    }
    return -1;
} // end is_in_info

int count_info(send_information_t* send_info, int window)
{
	int i, count = 0;
	for (i = 0; i < window; i++)
	{
		if (send_info[i].bundle_id.creation_ts.secs != 0)
		{
			count++;
		}
	}
	return count;
}

void remove_from_info(send_information_t* send_info, int position)
{
    send_info[position].bundle_id.creation_ts.secs = 0;
    send_info[position].bundle_id.creation_ts.seqno = 0;
} // end remove_from_info


void set_bp_options(bp_bundle_object_t *bundle, dtnperf_connection_options_t *opt)
{
	bp_bundle_delivery_opts_t dopts = BP_DOPTS_NONE;

	// Bundle expiration
	bp_bundle_set_expiration(bundle, opt->expiration);

	// Bundle priority
	bp_bundle_set_priority(bundle, opt->priority);

	// Delivery receipt option
	if (opt->delivery_receipts)
		dopts |= BP_DOPTS_DELIVERY_RCPT;

	// Forward receipt option
	if (opt->forwarding_receipts)
		dopts |= BP_DOPTS_FORWARD_RCPT;

	// Custody transfer
	if (opt->custody_transfer)
		dopts |= BP_DOPTS_CUSTODY;


	// Custody receipts
	if (opt->custody_receipts)
		dopts |= BP_DOPTS_CUSTODY_RCPT;

	// Receive receipt
	if (opt->receive_receipts)
		dopts |= BP_DOPTS_RECEIVE_RCPT;

	//Disable bundle fragmentation

	if (opt->disable_fragmentation)
		dopts |= BP_DOPTS_DO_NOT_FRAGMENT;

	//Set options
	bp_bundle_set_delivery_opts(bundle, dopts);

} // end set_bp_options

int open_payload_stream_read(bp_bundle_object_t bundle, FILE ** f)
{
	bp_bundle_payload_location_t pl_location;
	char * buffer;
	u32_t buffer_len;

	bp_bundle_get_payload_location(bundle, &pl_location);

	if (pl_location == BP_PAYLOAD_MEM)
	{
		bp_bundle_get_payload_mem(bundle, &buffer, &buffer_len);
		*f = fmemopen(buffer, buffer_len, "rb");
		if (*f == NULL)
			return -1;
	}
	else
	{
		bp_bundle_get_payload_file(bundle, &buffer, &buffer_len);
		*f = fopen(buffer, "rb");
		perror("open");
		if (*f == NULL)
			return -1;
	}
	return 0;
}

int close_payload_stream_read(FILE * f)
{
	return fclose(f);
}

int open_payload_stream_write(bp_bundle_object_t bundle, FILE ** f)
{
	bp_bundle_payload_location_t pl_location;

	bp_bundle_get_payload_location(bundle, &pl_location);

	if (pl_location == BP_PAYLOAD_MEM)
	{
		bp_bundle_get_payload_mem(bundle, &buffer, &buffer_len);
		*f= open_memstream(&buffer, &buffer_len);
		if (*f == NULL)
			return -1;
	}
	else
	{
		bp_bundle_get_payload_file(bundle, &buffer, &buffer_len);
		*f = fopen(buffer, "wb");
		if (*f == NULL)
			return -1;
	}
	return 0;
}

int close_payload_stream_write(bp_bundle_object_t * bundle, FILE *f)
{
	bp_bundle_payload_location_t pl_location;
	bp_bundle_get_payload_location(*bundle, &pl_location);

	fclose(f);

	if (pl_location == BP_PAYLOAD_MEM)
	{
		bp_bundle_set_payload_mem(bundle, buffer, buffer_len);
	}
	else
	{
		bp_bundle_set_payload_file(bundle, buffer, buffer_len);
	}
	return 0;
}

bp_error_t prepare_payload_header(dtnperf_options_t *opt, FILE * f, boolean_t file_transfer_first)
{
	if (f == NULL)
		return BP_ENULLPNTR;

	char header[HEADER_SIZE];
	char congestion_control = opt->congestion_ctrl;
	switch(opt->op_mode)
	{
	case 'T':
		strncpy(header, TIME_HEADER, HEADER_SIZE);
		break;
	case 'D':
		strncpy(header, DATA_HEADER, HEADER_SIZE);
		break;
	case 'F':
		if(file_transfer_first)
			strncpy(header, FILE_FIRST_HEADER, HEADER_SIZE);
		else
			strncpy(header, FILE_HEADER, HEADER_SIZE);
		break;
	default:
		return BP_EINVAL;
	}
	fwrite(header, HEADER_SIZE, 1, f);
	fwrite(&congestion_control, 1, 1, f);

	return BP_SUCCESS;
}

bp_error_t prepare_generic_payload(dtnperf_options_t *opt, FILE * f)
{
	if (f == NULL)
		return BP_ENULLPNTR;

	char * pattern = PL_PATTERN;
	long remaining;
	int i;
	bp_error_t result;

	// prepare header and congestion control
	result = prepare_payload_header(opt, f, FALSE);

	// remaining = bundle_payload - HEADER_SIZE - congestion control char
	remaining = opt->bundle_payload - HEADER_SIZE - 1;

	// fill remainig payload with a pattern
	for (i = remaining; i > strlen(pattern); i -= strlen(pattern))
	{
		fwrite(pattern, strlen(pattern), 1, f);
	}
	fwrite(pattern, remaining % strlen(pattern), 1, f);

	return result;
}

/**
 *
 */
bp_error_t prepare_server_ack_payload(dtnperf_server_ack_payload_t ack, char ** payload, size_t * payload_size)
{
	FILE * buf_stream;
	char * buf;
	size_t buf_size;
	buf_stream = open_memstream(& buf, &buf_size);
	fwrite(ack.header, 1, HEADER_SIZE, buf_stream);
	fwrite(&(ack.bundle_source), 1, sizeof(ack.bundle_source), buf_stream);
	fwrite(&(ack.bundle_creation_ts), 1, sizeof(ack.bundle_creation_ts), buf_stream);
	fclose(buf_stream);
	*payload = (char*)malloc(buf_size);
	memcpy(*payload, buf, buf_size);
	*payload_size = buf_size;
	free(buf);
	return BP_SUCCESS;
}

bp_error_t get_info_from_ack(bp_bundle_object_t * ack, bp_timestamp_t * reported_timestamp)
{
	char* buf;
	u32_t buf_len;
	bp_error_t error;
	bp_endpoint_id_t reported_eid;
	bp_endpoint_id_t ack_dest;
	error = bp_bundle_get_dest(*ack, &ack_dest);
	bp_bundle_get_payload_size(*ack, &buf_len);
	buf = malloc(buf_len);
	if (error < 0)
		return error;
	error = bp_bundle_get_payload_mem(*ack, &buf, &buf_len);
	if (error != BP_SUCCESS)
		return error;
	if (strncmp(DSA_HEADER, buf, HEADER_SIZE) == 0)
	{
		buf += HEADER_SIZE;
		memcpy(&reported_eid, buf, sizeof(reported_eid));
		if (strcmp(reported_eid.uri, ack_dest.uri) == 0)
		{
			buf += sizeof(reported_eid);
			memcpy(reported_timestamp, buf, sizeof(bp_timestamp_t));
			return BP_SUCCESS;
		}
	}
	return BP_ERRBASE;
}

boolean_t is_header(bp_bundle_object_t * bundle, const char * header_string)
{
	if (bundle == NULL)
		return FALSE;
	FILE * pl_stream = NULL;
	open_payload_stream_read(*bundle, &pl_stream);
	char header[HEADER_SIZE];

	fread(header, HEADER_SIZE, 1, pl_stream);
	fclose(pl_stream);

	if (strncmp(header, header_string, HEADER_SIZE) == 0)
		return TRUE;
	return FALSE;
}

boolean_t is_congestion_ctrl(bp_bundle_object_t * bundle, char mode)
{
	if (bundle == NULL)
		return FALSE;
	FILE * pl_stream = NULL;
	open_payload_stream_read(*bundle, &pl_stream);
	char header[HEADER_SIZE + 1];

	fread(header, HEADER_SIZE + 1, 1, pl_stream);
	fclose(pl_stream);

	if (header[HEADER_SIZE] == mode)
		return TRUE;
	return FALSE;
}
