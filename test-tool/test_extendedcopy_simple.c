/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (c) 2015 SanDisk Corp.

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
#include <string.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

// We only support sector size 512 or 4KB.
// Given copy-source's in-page offset in terms of sectors, decide the copy-destination's
// offset in terms of sectors starting from end of disk.
uint32_t GetDestLbaOffset(uint32_t src_lba_offset) {
  if (block_size == 512) {
    if (src_lba_offset == 0) {
      return 0;
    } else if (src_lba_offset >= 8) {
      logging(LOG_NORMAL, "sector size=512 doesn't support lba offset=%d", src_lba_offset);
      abort();
    } else {
      return 8 - src_lba_offset;
    }
  } else if (block_size == 4096) {
    if (src_lba_offset == 0) {
      return 0;
    } else if (src_lba_offset >= 4) {
      logging(LOG_NORMAL, "sector size=4K doesn't support lba offset=%d", src_lba_offset);
      abort();
    } else {
      return 4 - src_lba_offset;
    }
  } else {
    logging(LOG_NORMAL, "sector size=%ld not supported", block_size);
    abort();
  }
}

void
test_extendedcopy_simple(void)
{
        int tgt_desc_len = 0, seg_desc_len = 0, offset = XCOPY_DESC_OFFSET;
        struct iscsi_data data;
        unsigned char *xcopybuf;
        unsigned int copied_blocks;
        unsigned char *buf1;
        unsigned char *buf2;
        uint32_t src_lba;  // starting partial page offset by 1 sectors.

        copied_blocks = num_blocks / 2;
        if (copied_blocks > 2048 * 10)
                copied_blocks = 2048 * 10;
        buf1 = malloc(copied_blocks * block_size);
        buf2 = malloc(copied_blocks * block_size);

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE,
                "Test EXTENDED COPY of %u blocks from start of LUN to end of LUN",
                copied_blocks);

        CHECK_FOR_DATALOSS;

        uint32_t src_lbas[8] = {0, 1, 2, 3, 4, 5, 6, 7};
        char contents[8] = {'a', 'b', 'c', 'd', 'e', 'x', 'y', 'z'};
        uint32_t num_offsets;
        // Daelaam backend page size=4K, if sector-size=512, there are 8 possible in-page offset,
        // if sector size=4K, there is no in-page alignment issue.
        if (block_size == 512) {
          num_offsets = 8;
        } else if (block_size == 4096) {
          num_offsets = 1;
        } else {
          logging(LOG_NORMAL, "sector size=%ld is not supported", block_size);
          abort();
        }

        for (uint32_t i = 0; i < num_offsets; i++) {
          src_lba = src_lbas[i];
          char c= contents[i];
          offset = XCOPY_DESC_OFFSET;

          logging(LOG_VERBOSE, "\n\n**** write %u blocks of '%c' at LBA:0, sector offset=%d",
                  copied_blocks, c, src_lba);

          memset(buf1, c, copied_blocks * block_size);
          WRITE16(sd, src_lba, copied_blocks * block_size, block_size, 0, 0, 0, 0, 0,
                  buf1, EXPECT_STATUS_GOOD);

          data.size = XCOPY_DESC_OFFSET +
            get_desc_len(IDENT_DESCR_TGT_DESCR) +
            get_desc_len(BLK_TO_BLK_SEG_DESCR);
          data.data = alloca(data.size);
          xcopybuf = data.data;
          memset(xcopybuf, 0, data.size);

          /* Initialize target descriptor list with one target descriptor */
          offset += populate_tgt_desc(xcopybuf+offset, IDENT_DESCR_TGT_DESCR,
                                      LU_ID_TYPE_LUN, 0, 0, 0, 0, sd);
          tgt_desc_len = offset - XCOPY_DESC_OFFSET;

          /* Initialize segment descriptor list with one segment descriptor */
          offset += populate_seg_desc_b2b(xcopybuf+offset, 0, 0, 0, 0,
                                          copied_blocks, src_lba, num_blocks - copied_blocks - GetDestLbaOffset(src_lba));
          seg_desc_len = offset - XCOPY_DESC_OFFSET - tgt_desc_len;

          /* Initialize the parameter list header */
          populate_param_header(xcopybuf, 1, 0, LIST_ID_USAGE_DISCARD, 0,
                                tgt_desc_len, seg_desc_len, 0);

          EXTENDEDCOPY(sd, &data, EXPECT_STATUS_GOOD);

          logging(LOG_VERBOSE, "Read %u blocks from end of the LUN",
                  copied_blocks);
          READ16(sd, NULL, num_blocks - copied_blocks - GetDestLbaOffset(src_lba),
                 copied_blocks * block_size, block_size, 0, 0, 0, 0, 0, buf2, EXPECT_STATUS_GOOD);

          if (memcmp(buf1, buf2, copied_blocks * block_size)) {
            CU_FAIL("Blocks were not copied correctly");
          }
        }
        free(buf1);
        free(buf2);
}
