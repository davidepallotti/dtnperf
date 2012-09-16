/*
 * csv_tools.c
 *
 *  Created on: 29/ago/2012
 *      Author: michele
 */

#include "utils.h"
#include "csv_tools.h"

void csv_print_rx_time(FILE * file, struct timeval time, struct timeval start_time)
{
	struct timeval * result = malloc(sizeof(struct timeval));
	char buf[50];
	sub_time(time, start_time, result);
	sprintf(buf, "%ld.%ld;", result->tv_sec, result->tv_usec);
	fwrite(buf, strlen(buf), 1, file);
}

void csv_print_eid(FILE * file, bp_endpoint_id_t eid)
{
	char buf[256];
	sprintf(buf, "%s;", eid.uri);
	fwrite(buf, strlen(buf), 1, file);
}

void csv_print_timestamp(FILE * file, bp_timestamp_t timestamp)
{
	char buf[50];
	sprintf(buf, "%lu;%lu;", timestamp.secs, timestamp.seqno);
	fwrite(buf, strlen(buf), 1, file);
}

void csv_print_status_report_timestamps_header(FILE * file)
{
	char buf[300];
	memset(buf, 0, 300);
	strcat(buf, "STATUS_RECEIVED_TIMESTAMP;SEQNO;");
	strcat(buf, "STATUS_CUSTODY_ACCEPTED_TIMESTAMP;SEQNO;");
	strcat(buf, "STATUS_FORWARDED_TIMESTAMP;SEQNO;");
	strcat(buf, "STATUS_DELETED_TIMESTAMP;SEQNO;");
	strcat(buf, "STATUS_DELIVERED_TIMESTAMP;SEQNO;");
	strcat(buf, "STATUS_ACKED_BY_APP_TIMESTAMP;SEQNO;");
	fwrite(buf, strlen(buf), 1, file);
}
void csv_print_status_report_timestamps(FILE * file, bp_bundle_status_report_t status_report)
{
	char buf1[256];
	char buf2[50];
	memset(buf1, 0, 256);
	if (status_report.flags & BP_STATUS_RECEIVED)
		sprintf(buf2, "%lu;%lu;", status_report.receipt_ts.secs, status_report.receipt_ts.seqno);
	else
		sprintf(buf2, " ; ;");
	strcat(buf1, buf2);

	if (status_report.flags & BP_STATUS_CUSTODY_ACCEPTED)
		sprintf(buf2, "%lu;%lu;", status_report.custody_ts.secs, status_report.custody_ts.seqno);
	else
		sprintf(buf2, " ; ;");
	strcat(buf1, buf2);

	if (status_report.flags & BP_STATUS_FORWARDED)
		sprintf(buf2, "%lu;%lu;", status_report.forwarding_ts.secs, status_report.forwarding_ts.seqno);
	else
		sprintf(buf2, " ; ;");
	strcat(buf1, buf2);

	if (status_report.flags & BP_STATUS_DELETED)
		sprintf(buf2, "%lu;%lu;", status_report.deletion_ts.secs, status_report.deletion_ts.seqno);
	else
		sprintf(buf2, " ; ;");
	strcat(buf1, buf2);

	if (status_report.flags & BP_STATUS_DELIVERED)
		sprintf(buf2, "%lu;%lu;", status_report.delivery_ts.secs, status_report.delivery_ts.seqno);
	else
		sprintf(buf2, " ; ;");
	strcat(buf1, buf2);

	if (status_report.flags & BP_STATUS_ACKED_BY_APP)
		sprintf(buf2, "%lu;%lu;", status_report.ack_by_app_ts.secs, status_report.ack_by_app_ts.seqno);
	else
		sprintf(buf2, " ; ;");
	strcat(buf1, buf2);

	fwrite(buf1, strlen(buf1), 1, file);
}

void csv_print_long(FILE * file, long num)
{
	char buf[50];
	sprintf(buf, "%ld", num);
	csv_print(file, buf);
}

void csv_print_ulong(FILE * file, unsigned long num)
{
	char buf[50];
	sprintf(buf, "%lu", num);
	csv_print(file, buf);
}

void csv_print(FILE * file, char * string)
{
	char buf[256];
	memset(buf, 0, 256);
	strcat(buf, string);
	if (buf[strlen(buf) -1] != ';')
		strcat(buf, ";");
	fwrite(buf, strlen(buf), 1, file);
}

void csv_end_line(FILE * file)
{
	char c = '\n';
	fwrite(&c, 1, 1, file);
}
