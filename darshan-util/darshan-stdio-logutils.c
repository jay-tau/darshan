/*
 * Copyright (C) 2015 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#define _GNU_SOURCE
#include "darshan-util-config.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "darshan-logutils.h"

/* integer counter name strings for the STDIO module */
#define X(a) #a,
char *stdio_counter_names[] = {
    STDIO_COUNTERS
};

/* floating point counter name strings for the STDIO module */
char *stdio_f_counter_names[] = {
    STDIO_F_COUNTERS
};
#undef X

/* prototypes for each of the STDIO module's logutil functions */
static int darshan_log_get_stdio_record(darshan_fd fd, void** stdio_buf_p);
static int darshan_log_put_stdio_record(darshan_fd fd, void* stdio_buf, int ver);
static void darshan_log_print_stdio_record(void *file_rec,
    char *file_name, char *mnt_pt, char *fs_type, int ver);
static void darshan_log_print_stdio_description(void);
static void darshan_log_print_stdio_record_diff(void *file_rec1, char *file_name1,
    void *file_rec2, char *file_name2);

/* structure storing each function needed for implementing the darshan
 * logutil interface. these functions are used for reading, writing, and
 * printing module data in a consistent manner.
 */
struct darshan_mod_logutil_funcs stdio_logutils =
{
    .log_get_record = &darshan_log_get_stdio_record,
    .log_put_record = &darshan_log_put_stdio_record,
    .log_print_record = &darshan_log_print_stdio_record,
    .log_print_description = &darshan_log_print_stdio_description,
    .log_print_diff = &darshan_log_print_stdio_record_diff
};

/* retrieve a STDIO record from log file descriptor 'fd', storing the
 * buffer in 'stdio_buf'. Return 1 on successful record read, 0 on no 
 * more data, and -1 on error.
 */
static int darshan_log_get_stdio_record(darshan_fd fd, void** stdio_buf_p)
{
    struct darshan_stdio_file *file = *((struct darshan_stdio_file **)stdio_buf_p);
    int i;
    int ret;

    if(fd->mod_map[DARSHAN_STDIO_MOD].len == 0)
        return(0);

    if(*stdio_buf_p == NULL)
    {
        file = malloc(sizeof(*file));
        if(!file)
            return(-1);
    }

    /* read a STDIO module record from the darshan log file */
    ret = darshan_log_get_mod(fd, DARSHAN_STDIO_MOD, file,
        sizeof(struct darshan_stdio_file));

    if(*stdio_buf_p == NULL)
    {
        if(ret == sizeof(struct darshan_stdio_file))
            *stdio_buf_p = file;
        else
            free(file);
    }

    if(ret < 0)
        return(-1);
    else if(ret < sizeof(struct darshan_stdio_file))
        return(0);
    else
    {
        /* if the read was successful, do any necessary byte-swapping */
        if(fd->swap_flag)
        {
            DARSHAN_BSWAP64(&file->base_rec.id);
            DARSHAN_BSWAP64(&file->base_rec.rank);
            for(i=0; i<STDIO_NUM_INDICES; i++)
                DARSHAN_BSWAP64(&file->counters[i]);
            for(i=0; i<STDIO_F_NUM_INDICES; i++)
                DARSHAN_BSWAP64(&file->fcounters[i]);
        }

        return(1);
    }
}

/* write the STDIO record stored in 'stdio_buf' to log file descriptor 'fd'.
 * Return 0 on success, -1 on failure
 */
static int darshan_log_put_stdio_record(darshan_fd fd, void* stdio_buf, int ver)
{
    struct darshan_stdio_file *rec = (struct darshan_stdio_file *)stdio_buf;
    int ret;

    /* append STDIO record to darshan log file */
    ret = darshan_log_put_mod(fd, DARSHAN_STDIO_MOD, rec,
        sizeof(struct darshan_stdio_file), DARSHAN_STDIO_VER);
    if(ret < 0)
        return(-1);

    return(0);
}

/* print all I/O data record statistics for the given STDIO record */
static void darshan_log_print_stdio_record(void *file_rec, char *file_name,
    char *mnt_pt, char *fs_type, int ver)
{
    int i;
    struct darshan_stdio_file *stdio_rec =
        (struct darshan_stdio_file *)file_rec;

    /* print each of the integer and floating point counters for the STDIO module */
    for(i=0; i<STDIO_NUM_INDICES; i++)
    {
        /* macro defined in darshan-logutils.h */
        DARSHAN_COUNTER_PRINT(darshan_module_names[DARSHAN_STDIO_MOD],
            stdio_rec->base_rec.rank, stdio_rec->base_rec.id, stdio_counter_names[i],
            stdio_rec->counters[i], file_name, mnt_pt, fs_type);
    }

    for(i=0; i<STDIO_F_NUM_INDICES; i++)
    {
        /* macro defined in darshan-logutils.h */
        DARSHAN_F_COUNTER_PRINT(darshan_module_names[DARSHAN_STDIO_MOD],
            stdio_rec->base_rec.rank, stdio_rec->base_rec.id, stdio_f_counter_names[i],
            stdio_rec->fcounters[i], file_name, mnt_pt, fs_type);
    }

    return;
}

/* print out a description of the STDIO module record fields */
static void darshan_log_print_stdio_description()
{
    printf("\n# description of STDIO counters:\n");
    printf("#   STDIO_{OPENS|WRITES|READS|SEEKS|FLUSHES} are types of operations.\n");
    printf("#   STDIO_BYTES_*: total bytes read and written.\n");
    printf("#   STDIO_MAX_BYTE_*: highest offset byte read and written.\n");
    printf("#   STDIO_*_RANK: rank of the processes that were the fastest and slowest at I/O (for shared files).\n");
    printf("#   STDIO_*_RANK_BYTES: bytes transferred by the fastest and slowest ranks (for shared files).\n");
    printf("#   STDIO_F_*_START_TIMESTAMP: timestamp of the first call to that type of function.\n");
    printf("#   STDIO_F_*_END_TIMESTAMP: timestamp of the completion of the last call to that type of function.\n");
    printf("#   STDIO_F_*_TIME: cumulative time spent in different types of functions.\n");
    printf("#   STDIO_F_*_RANK_TIME: fastest and slowest I/O time for a single rank (for shared files).\n");
    printf("#   STDIO_F_VARIANCE_RANK_*: variance of total I/O time and bytes moved for all ranks (for shared files).\n");

    DARSHAN_PRINT_HEADER();

    return;
}

static void darshan_log_print_stdio_record_diff(void *file_rec1, char *file_name1,
    void *file_rec2, char *file_name2)
{
    struct darshan_stdio_file *file1 = (struct darshan_stdio_file *)file_rec1;
    struct darshan_stdio_file *file2 = (struct darshan_stdio_file *)file_rec2;
    int i;

    /* NOTE: we assume that both input records are the same module format version */

    for(i=0; i<STDIO_NUM_INDICES; i++)
    {
        if(!file2)
        {
            printf("- ");
            DARSHAN_COUNTER_PRINT(darshan_module_names[DARSHAN_STDIO_MOD],
                file1->base_rec.rank, file1->base_rec.id, stdio_counter_names[i],
                file1->counters[i], file_name1, "", "");

        }
        else if(!file1)
        {
            printf("+ ");
            DARSHAN_COUNTER_PRINT(darshan_module_names[DARSHAN_STDIO_MOD],
                file2->base_rec.rank, file2->base_rec.id, stdio_counter_names[i],
                file2->counters[i], file_name2, "", "");
        }
        else if(file1->counters[i] != file2->counters[i])
        {
            printf("- ");
            DARSHAN_COUNTER_PRINT(darshan_module_names[DARSHAN_STDIO_MOD],
                file1->base_rec.rank, file1->base_rec.id, stdio_counter_names[i],
                file1->counters[i], file_name1, "", "");
            printf("+ ");
            DARSHAN_COUNTER_PRINT(darshan_module_names[DARSHAN_STDIO_MOD],
                file2->base_rec.rank, file2->base_rec.id, stdio_counter_names[i],
                file2->counters[i], file_name2, "", "");
        }
    }

    for(i=0; i<STDIO_F_NUM_INDICES; i++)
    {
        if(!file2)
        {
            printf("- ");
            DARSHAN_F_COUNTER_PRINT(darshan_module_names[DARSHAN_STDIO_MOD],
                file1->base_rec.rank, file1->base_rec.id, stdio_f_counter_names[i],
                file1->fcounters[i], file_name1, "", "");

        }
        else if(!file1)
        {
            printf("+ ");
            DARSHAN_F_COUNTER_PRINT(darshan_module_names[DARSHAN_STDIO_MOD],
                file2->base_rec.rank, file2->base_rec.id, stdio_f_counter_names[i],
                file2->fcounters[i], file_name2, "", "");
        }
        else if(file1->fcounters[i] != file2->fcounters[i])
        {
            printf("- ");
            DARSHAN_F_COUNTER_PRINT(darshan_module_names[DARSHAN_STDIO_MOD],
                file1->base_rec.rank, file1->base_rec.id, stdio_f_counter_names[i],
                file1->fcounters[i], file_name1, "", "");
            printf("+ ");
            DARSHAN_F_COUNTER_PRINT(darshan_module_names[DARSHAN_STDIO_MOD],
                file2->base_rec.rank, file2->base_rec.id, stdio_f_counter_names[i],
                file2->fcounters[i], file_name2, "", "");
        }
    }

    return;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
