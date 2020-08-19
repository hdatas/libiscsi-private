/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/* 
   Copyright (C) 2013 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"

void
test_unmap_simple(void)
{
        int i;
        int max_nr_bdc = 256;
        struct unmap_list list[257];

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test basic UNMAP");

        CHECK_FOR_DATALOSS;
        CHECK_FOR_THIN_PROVISIONING;
        CHECK_FOR_SBC;


        logging(LOG_VERBOSE, "Test UNMAP of 1-256 blocks at the start of the "
                "LUN as a single descriptor");

        logging(LOG_VERBOSE, "Write 'a' to the first 256 LBAs");
        memset(scratch, 'a', 256 * block_size);
        WRITE10(sd, 0, 256 * block_size,
                block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "inquiry->max_unmap_bdc=%d, max_nr_bdc=%d, lbprz=%d",
                inq_bl->max_unmap_bdc, max_nr_bdc, (rc16 ? rc16->lbprz : 0));

          for (i = 0; i < 256; i += 1) {
                logging(LOG_VERBOSE, "UNMAP blocks 0-%d", i);
                list[0].lba = 0;
                list[0].num = i;
                UNMAP(sd, 0, list, 1,
                      EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Read blocks 0-%d", i);
                READ10(sd, NULL, 0, i * block_size,
                       block_size, 0, 0, 0, 0, 0, scratch,
                       EXPECT_STATUS_GOOD);

                if (rc16 && rc16->lbprz) {
                        logging(LOG_VERBOSE, "LBPRZ==1 All UNMAPPED blocks "
                                "should read back as 0");
                        ALL_ZERO(scratch, i * block_size);
                }
        }

        logging(LOG_VERBOSE, "device max allowed unmap_bdc = %d\n", inq_bl->max_unmap_bdc);
        if (inq_bl->max_unmap_bdc > 0 && max_nr_bdc > (int)inq_bl->max_unmap_bdc) {
          max_nr_bdc = (int)inq_bl->max_unmap_bdc;
        }
        if (max_nr_bdc < 0 || max_nr_bdc > 256) {
                logging(LOG_VERBOSE, "Clamp max unmapped blocks to 256");
                max_nr_bdc = 256;
        }

        logging(LOG_VERBOSE, "Test UNMAP of 1-%d blocks at the start of the "
                "LUN with one descriptor per page", max_nr_bdc);

        logging(LOG_VERBOSE, "Write 'b' to the first %d LBAs", max_nr_bdc);
        memset(scratch, 'b', max_nr_bdc * block_size);
        WRITE10(sd, 0, max_nr_bdc * block_size,
                block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_STATUS_GOOD);

        int num_descs = 0;
        int per_op_blks = 13;
        // "unmap" offset should be 4K aligned, size be 4K aligned.
        for (i = 0; i < 256 && num_descs < max_nr_bdc; i += per_op_blks, num_descs++) {
        // for (i = 0; i < max_nr_bdc; i++) {
                list[num_descs].lba = i;
                list[num_descs].num = per_op_blks;
                UNMAP(sd, 0, list, num_descs + 1, EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Read blocks 0-%d", (num_descs + 1) * per_op_blks);
                READ10(sd, NULL, 0, (num_descs + 1) * per_op_blks * block_size,
                       block_size, 0, 0, 0, 0, 0, scratch,
                       EXPECT_STATUS_GOOD);

                if (rc16 && rc16->lbprz) {
                        logging(LOG_VERBOSE, "LBPRZ==1 All UNMAPPED blocks "
                                "should read back as 0");
                        ALL_ZERO(scratch, (num_descs + 1) * per_op_blks * block_size);
                }
        }
}
