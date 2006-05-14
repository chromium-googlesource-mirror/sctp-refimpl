/*-
 * Copyright (C) 2002-2006 Cisco Systems Inc,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __SCTP_SACK_H__
#define __SCTP_SACK_H__

#define SCTP_MAX_GAPS_INARRAY 4
struct sctp_gap_ack_block {
	u_int16_t start;		/* Gap Ack block start */
	u_int16_t end;			/* Gap Ack block end */
};

struct sack_track {
	u_int8_t right_edge;	/* mergable on the right edge */
	u_int8_t left_edge;     /* mergable on the left edge */
        u_int8_t num_entries;
        u_int8_t spare;
	struct sctp_gap_ack_block gaps[SCTP_MAX_GAPS_INARRAY];
};

struct sack_track sack_array[256] = {
{0, 0, 0, 0,     /* 0x00 */
 { { 0, 0},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 1, 0,     /* 0x01 */
 { { 0, 0},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x02 */
 { { 1, 1},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 1, 0,     /* 0x03 */
 { { 0, 1},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x04 */
 { { 2, 2},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x05 */
 { { 0, 0},
   { 2, 2},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x06 */
 { { 1, 2},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 1, 0,     /* 0x07 */
 { { 0, 2},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x08 */
 { { 3, 3},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x09 */
 { { 0, 0},
   { 3, 3},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x0a */
 { { 1, 1},
   { 3, 3},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x0b */
 { { 0, 1},
   { 3, 3},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x0c */
 { { 2, 3},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x0d */
 { { 0, 0},
   { 2, 3},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x0e */
 { { 1, 3},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 1, 0,     /* 0x0f */
 { { 0, 3},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x10 */
 { { 4, 4},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x11 */
 { { 0, 0},
   { 4, 4},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x12 */
 { { 1, 1},
   { 4, 4},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x13 */
 { { 0, 1},
   { 4, 4},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x14 */
 { { 2, 2},
   { 4, 4},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x15 */
 { { 0, 0},
   { 2, 2},
   { 4, 4},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x16 */
 { { 1, 2},
   { 4, 4},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x17 */
 { { 0, 2},
   { 4, 4},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x18 */
 { { 3, 4},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x19 */
 { { 0, 0},
   { 3, 4},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x1a */
 { { 1, 1},
   { 3, 4},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x1b */
 { { 0, 1},
   { 3, 4},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x1c */
 { { 2, 4},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x1d */
 { { 0, 0},
   { 2, 4},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x1e */
 { { 1, 4},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 1, 0,     /* 0x1f */
 { { 0, 4},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x20 */
 { { 5, 5},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x21 */
 { { 0, 0},
   { 5, 5},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x22 */
 { { 1, 1},
   { 5, 5},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x23 */
 { { 0, 1},
   { 5, 5},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x24 */
 { { 2, 2},
   { 5, 5},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x25 */
 { { 0, 0},
   { 2, 2},
   { 5, 5},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x26 */
 { { 1, 2},
   { 5, 5},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x27 */
 { { 0, 2},
   { 5, 5},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x28 */
 { { 3, 3},
   { 5, 5},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x29 */
 { { 0, 0},
   { 3, 3},
   { 5, 5},
   { 0, 0}
 }
},
{0, 0, 3, 0,     /* 0x2a */
 { { 1, 1},
   { 3, 3},
   { 5, 5},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x2b */
 { { 0, 1},
   { 3, 3},
   { 5, 5},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x2c */
 { { 2, 3},
   { 5, 5},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x2d */
 { { 0, 0},
   { 2, 3},
   { 5, 5},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x2e */
 { { 1, 3},
   { 5, 5},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x2f */
 { { 0, 3},
   { 5, 5},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x30 */
 { { 4, 5},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x31 */
 { { 0, 0},
   { 4, 5},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x32 */
 { { 1, 1},
   { 4, 5},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x33 */
 { { 0, 1},
   { 4, 5},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x34 */
 { { 2, 2},
   { 4, 5},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x35 */
 { { 0, 0},
   { 2, 2},
   { 4, 5},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x36 */
 { { 1, 2},
   { 4, 5},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x37 */
 { { 0, 2},
   { 4, 5},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x38 */
 { { 3, 5},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x39 */
 { { 0, 0},
   { 3, 5},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x3a */
 { { 1, 1},
   { 3, 5},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x3b */
 { { 0, 1},
   { 3, 5},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x3c */
 { { 2, 5},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x3d */
 { { 0, 0},
   { 2, 5},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x3e */
 { { 1, 5},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 1, 0,     /* 0x3f */
 { { 0, 5},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x40 */
 { { 6, 6},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x41 */
 { { 0, 0},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x42 */
 { { 1, 1},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x43 */
 { { 0, 1},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x44 */
 { { 2, 2},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x45 */
 { { 0, 0},
   { 2, 2},
   { 6, 6},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x46 */
 { { 1, 2},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x47 */
 { { 0, 2},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x48 */
 { { 3, 3},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x49 */
 { { 0, 0},
   { 3, 3},
   { 6, 6},
   { 0, 0}
 }
},
{0, 0, 3, 0,     /* 0x4a */
 { { 1, 1},
   { 3, 3},
   { 6, 6},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x4b */
 { { 0, 1},
   { 3, 3},
   { 6, 6},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x4c */
 { { 2, 3},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x4d */
 { { 0, 0},
   { 2, 3},
   { 6, 6},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x4e */
 { { 1, 3},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x4f */
 { { 0, 3},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x50 */
 { { 4, 4},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x51 */
 { { 0, 0},
   { 4, 4},
   { 6, 6},
   { 0, 0}
 }
},
{0, 0, 3, 0,     /* 0x52 */
 { { 1, 1},
   { 4, 4},
   { 6, 6},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x53 */
 { { 0, 1},
   { 4, 4},
   { 6, 6},
   { 0, 0}
 }
},
{0, 0, 3, 0,     /* 0x54 */
 { { 2, 2},
   { 4, 4},
   { 6, 6},
   { 0, 0}
 }
},
{1, 0, 4, 0,     /* 0x55 */
 { { 0, 0},
   { 2, 2},
   { 4, 4},
   { 6, 6}
 }
},
{0, 0, 3, 0,     /* 0x56 */
 { { 1, 2},
   { 4, 4},
   { 6, 6},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x57 */
 { { 0, 2},
   { 4, 4},
   { 6, 6},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x58 */
 { { 3, 4},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x59 */
 { { 0, 0},
   { 3, 4},
   { 6, 6},
   { 0, 0}
 }
},
{0, 0, 3, 0,     /* 0x5a */
 { { 1, 1},
   { 3, 4},
   { 6, 6},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x5b */
 { { 0, 1},
   { 3, 4},
   { 6, 6},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x5c */
 { { 2, 4},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x5d */
 { { 0, 0},
   { 2, 4},
   { 6, 6},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x5e */
 { { 1, 4},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x5f */
 { { 0, 4},
   { 6, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x60 */
 { { 5, 6},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x61 */
 { { 0, 0},
   { 5, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x62 */
 { { 1, 1},
   { 5, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x63 */
 { { 0, 1},
   { 5, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x64 */
 { { 2, 2},
   { 5, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x65 */
 { { 0, 0},
   { 2, 2},
   { 5, 6},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x66 */
 { { 1, 2},
   { 5, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x67 */
 { { 0, 2},
   { 5, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x68 */
 { { 3, 3},
   { 5, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x69 */
 { { 0, 0},
   { 3, 3},
   { 5, 6},
   { 0, 0}
 }
},
{0, 0, 3, 0,     /* 0x6a */
 { { 1, 1},
   { 3, 3},
   { 5, 6},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x6b */
 { { 0, 1},
   { 3, 3},
   { 5, 6},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x6c */
 { { 2, 3},
   { 5, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x6d */
 { { 0, 0},
   { 2, 3},
   { 5, 6},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x6e */
 { { 1, 3},
   { 5, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x6f */
 { { 0, 3},
   { 5, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x70 */
 { { 4, 6},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x71 */
 { { 0, 0},
   { 4, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x72 */
 { { 1, 1},
   { 4, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x73 */
 { { 0, 1},
   { 4, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x74 */
 { { 2, 2},
   { 4, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 3, 0,     /* 0x75 */
 { { 0, 0},
   { 2, 2},
   { 4, 6},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x76 */
 { { 1, 2},
   { 4, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x77 */
 { { 0, 2},
   { 4, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x78 */
 { { 3, 6},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x79 */
 { { 0, 0},
   { 3, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 2, 0,     /* 0x7a */
 { { 1, 1},
   { 3, 6},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x7b */
 { { 0, 1},
   { 3, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x7c */
 { { 2, 6},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 2, 0,     /* 0x7d */
 { { 0, 0},
   { 2, 6},
   { 0, 0},
   { 0, 0}
 }
},
{0, 0, 1, 0,     /* 0x7e */
 { { 1, 6},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 0, 1, 0,     /* 0x7f */
 { { 0, 6},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 1, 0,     /* 0x80 */
 { { 7, 7},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0x81 */
 { { 0, 0},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0x82 */
 { { 1, 1},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0x83 */
 { { 0, 1},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0x84 */
 { { 2, 2},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0x85 */
 { { 0, 0},
   { 2, 2},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0x86 */
 { { 1, 2},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0x87 */
 { { 0, 2},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0x88 */
 { { 3, 3},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0x89 */
 { { 0, 0},
   { 3, 3},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0x8a */
 { { 1, 1},
   { 3, 3},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0x8b */
 { { 0, 1},
   { 3, 3},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0x8c */
 { { 2, 3},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0x8d */
 { { 0, 0},
   { 2, 3},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0x8e */
 { { 1, 3},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0x8f */
 { { 0, 3},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0x90 */
 { { 4, 4},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0x91 */
 { { 0, 0},
   { 4, 4},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0x92 */
 { { 1, 1},
   { 4, 4},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0x93 */
 { { 0, 1},
   { 4, 4},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0x94 */
 { { 2, 2},
   { 4, 4},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 4, 0,     /* 0x95 */
 { { 0, 0},
   { 2, 2},
   { 4, 4},
   { 7, 7}
 }
},
{0, 1, 3, 0,     /* 0x96 */
 { { 1, 2},
   { 4, 4},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0x97 */
 { { 0, 2},
   { 4, 4},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0x98 */
 { { 3, 4},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0x99 */
 { { 0, 0},
   { 3, 4},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0x9a */
 { { 1, 1},
   { 3, 4},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0x9b */
 { { 0, 1},
   { 3, 4},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0x9c */
 { { 2, 4},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0x9d */
 { { 0, 0},
   { 2, 4},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0x9e */
 { { 1, 4},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0x9f */
 { { 0, 4},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xa0 */
 { { 5, 5},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xa1 */
 { { 0, 0},
   { 5, 5},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xa2 */
 { { 1, 1},
   { 5, 5},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xa3 */
 { { 0, 1},
   { 5, 5},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xa4 */
 { { 2, 2},
   { 5, 5},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 4, 0,     /* 0xa5 */
 { { 0, 0},
   { 2, 2},
   { 5, 5},
   { 7, 7}
 }
},
{0, 1, 3, 0,     /* 0xa6 */
 { { 1, 2},
   { 5, 5},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xa7 */
 { { 0, 2},
   { 5, 5},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xa8 */
 { { 3, 3},
   { 5, 5},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 4, 0,     /* 0xa9 */
 { { 0, 0},
   { 3, 3},
   { 5, 5},
   { 7, 7}
 }
},
{0, 1, 4, 0,     /* 0xaa */
 { { 1, 1},
   { 3, 3},
   { 5, 5},
   { 7, 7}
 }
},
{1, 1, 4, 0,     /* 0xab */
 { { 0, 1},
   { 3, 3},
   { 5, 5},
   { 7, 7}
 }
},
{0, 1, 3, 0,     /* 0xac */
 { { 2, 3},
   { 5, 5},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 4, 0,     /* 0xad */
 { { 0, 0},
   { 2, 3},
   { 5, 5},
   { 7, 7}
 }
},
{0, 1, 3, 0,     /* 0xae */
 { { 1, 3},
   { 5, 5},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xaf */
 { { 0, 3},
   { 5, 5},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xb0 */
 { { 4, 5},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xb1 */
 { { 0, 0},
   { 4, 5},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xb2 */
 { { 1, 1},
   { 4, 5},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xb3 */
 { { 0, 1},
   { 4, 5},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xb4 */
 { { 2, 2},
   { 4, 5},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 4, 0,     /* 0xb5 */
 { { 0, 0},
   { 2, 2},
   { 4, 5},
   { 7, 7}
 }
},
{0, 1, 3, 0,     /* 0xb6 */
 { { 1, 2},
   { 4, 5},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xb7 */
 { { 0, 2},
   { 4, 5},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xb8 */
 { { 3, 5},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xb9 */
 { { 0, 0},
   { 3, 5},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xba */
 { { 1, 1},
   { 3, 5},
   { 7, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xbb */
 { { 0, 1},
   { 3, 5},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xbc */
 { { 2, 5},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xbd */
 { { 0, 0},
   { 2, 5},
   { 7, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xbe */
 { { 1, 5},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xbf */
 { { 0, 5},
   { 7, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 1, 0,     /* 0xc0 */
 { { 6, 7},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xc1 */
 { { 0, 0},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xc2 */
 { { 1, 1},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xc3 */
 { { 0, 1},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xc4 */
 { { 2, 2},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xc5 */
 { { 0, 0},
   { 2, 2},
   { 6, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xc6 */
 { { 1, 2},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xc7 */
 { { 0, 2},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xc8 */
 { { 3, 3},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xc9 */
 { { 0, 0},
   { 3, 3},
   { 6, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xca */
 { { 1, 1},
   { 3, 3},
   { 6, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xcb */
 { { 0, 1},
   { 3, 3},
   { 6, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xcc */
 { { 2, 3},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xcd */
 { { 0, 0},
   { 2, 3},
   { 6, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xce */
 { { 1, 3},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xcf */
 { { 0, 3},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xd0 */
 { { 4, 4},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xd1 */
 { { 0, 0},
   { 4, 4},
   { 6, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xd2 */
 { { 1, 1},
   { 4, 4},
   { 6, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xd3 */
 { { 0, 1},
   { 4, 4},
   { 6, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xd4 */
 { { 2, 2},
   { 4, 4},
   { 6, 7},
   { 0, 0}
 }
},
{1, 1, 4, 0,     /* 0xd5 */
 { { 0, 0},
   { 2, 2},
   { 4, 4},
   { 6, 7}
 }
},
{0, 1, 3, 0,     /* 0xd6 */
 { { 1, 2},
   { 4, 4},
   { 6, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xd7 */
 { { 0, 2},
   { 4, 4},
   { 6, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xd8 */
 { { 3, 4},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xd9 */
 { { 0, 0},
   { 3, 4},
   { 6, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xda */
 { { 1, 1},
   { 3, 4},
   { 6, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xdb */
 { { 0, 1},
   { 3, 4},
   { 6, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xdc */
 { { 2, 4},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xdd */
 { { 0, 0},
   { 2, 4},
   { 6, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xde */
 { { 1, 4},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xdf */
 { { 0, 4},
   { 6, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 1, 0,     /* 0xe0 */
 { { 5, 7},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xe1 */
 { { 0, 0},
   { 5, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xe2 */
 { { 1, 1},
   { 5, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xe3 */
 { { 0, 1},
   { 5, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xe4 */
 { { 2, 2},
   { 5, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xe5 */
 { { 0, 0},
   { 2, 2},
   { 5, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xe6 */
 { { 1, 2},
   { 5, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xe7 */
 { { 0, 2},
   { 5, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xe8 */
 { { 3, 3},
   { 5, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xe9 */
 { { 0, 0},
   { 3, 3},
   { 5, 7},
   { 0, 0}
 }
},
{0, 1, 3, 0,     /* 0xea */
 { { 1, 1},
   { 3, 3},
   { 5, 7},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xeb */
 { { 0, 1},
   { 3, 3},
   { 5, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xec */
 { { 2, 3},
   { 5, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xed */
 { { 0, 0},
   { 2, 3},
   { 5, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xee */
 { { 1, 3},
   { 5, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xef */
 { { 0, 3},
   { 5, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 1, 0,     /* 0xf0 */
 { { 4, 7},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xf1 */
 { { 0, 0},
   { 4, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xf2 */
 { { 1, 1},
   { 4, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xf3 */
 { { 0, 1},
   { 4, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xf4 */
 { { 2, 2},
   { 4, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 3, 0,     /* 0xf5 */
 { { 0, 0},
   { 2, 2},
   { 4, 7},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xf6 */
 { { 1, 2},
   { 4, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xf7 */
 { { 0, 2},
   { 4, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 1, 0,     /* 0xf8 */
 { { 3, 7},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xf9 */
 { { 0, 0},
   { 3, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 2, 0,     /* 0xfa */
 { { 1, 1},
   { 3, 7},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xfb */
 { { 0, 1},
   { 3, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 1, 0,     /* 0xfc */
 { { 2, 7},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 2, 0,     /* 0xfd */
 { { 0, 0},
   { 2, 7},
   { 0, 0},
   { 0, 0}
 }
},
{0, 1, 1, 0,     /* 0xfe */
 { { 1, 7},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
},
{1, 1, 1, 0,     /* 0xff */
 { { 0, 7},
   { 0, 0},
   { 0, 0},
   { 0, 0}
 }
}
};

#endif
