/*	$KAME: sctputil.c,v 1.37 2005/03/07 23:26:09 itojun Exp $	*/

/*
 * Copyright (c) 2001-2005 Cisco Systems, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Cisco Systems, Inc.
 * 4. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CISCO SYSTEMS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CISCO SYSTEMS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if !(defined(__OpenBSD__) || defined(__APPLE__))
#include "opt_ipsec.h"
#endif
#if defined(__FreeBSD__)
#include "opt_compat.h"
#include "opt_inet6.h"
#include "opt_inet.h"
#ifndef SCTP_BASE_FREEBSD
#include "opt_mpath.h"
#endif /* SCTP_BASE_FREEBSD */
#endif /* FreeBSD */
#if defined(__NetBSD__)
#include "opt_inet.h"
#endif
#ifdef __APPLE__
#include <sctp.h>
#elif !defined(__OpenBSD__)
#include "opt_sctp.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/file.h>			/* for struct knote */
#include <sys/kernel.h>
#include <sys/event.h>
#ifndef __APPLE__
#include <sys/poll.h>
#endif

#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/resourcevar.h>
#ifndef __APPLE__
#include <sys/signalvar.h>
#endif
#include <sys/sysctl.h>
#include <sys/uio.h>
#ifndef __APPLE__
#include <sys/jail.h>
#endif

#ifdef __FreeBSD__
#include <sys/callout.h>
#else
#include <netinet/sctp_callout.h>	/* for callout_active() */
#endif

#include <net/radix.h>
#include <net/route.h>

#ifdef INET6
#ifndef __OpenBSD__
#include <sys/domain.h>
#endif
#endif

#if (defined(__FreeBSD__) && __FreeBSD_version >= 500000)
#include <sys/limits.h>
#include <sys/mac.h>
#include <sys/mutex.h>
#include <vm/uma.h>
#else
#include <machine/limits.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <netinet6/in6_pcb.h>
#elif defined(__OpenBSD__)
#include <netinet/in_pcb.h>
#endif

#endif /* INET6 */

#include <netinet/sctp_pcb.h>

#ifdef IPSEC
#ifndef __OpenBSD__
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#else
#undef IPSEC
#endif
#endif /* IPSEC */

#include <netinet/sctputil.h>
#include <netinet/sctp_var.h>
#ifdef INET6
#include <netinet6/sctp6_var.h>
#endif
#include <netinet/sctp_header.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_hashdriver.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctp_crc32.h>
#include <netinet/sctp_indata.h>	/* for sctp_deliver_data() */
#define NUMBER_OF_MTU_SIZES 18

#ifdef SCTP_DEBUG
extern u_int32_t sctp_debug_on;
#endif

#ifdef SCTP_STAT_LOGGING
int sctp_cwnd_log_at=0;
int sctp_cwnd_log_rolled=0;
struct sctp_cwnd_log sctp_clog[SCTP_STAT_LOG_SIZE];

void sctp_clr_stat_log(void)
{
	sctp_cwnd_log_at=0;
	sctp_cwnd_log_rolled=0;
}

void
rto_logging(struct sctp_nets *net, int from)
{
	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_RTT;
	sctp_clog[sctp_cwnd_log_at].x.rto.net = (u_int32_t)net;
	sctp_clog[sctp_cwnd_log_at].x.rto.rtt = net->prev_rtt;
	sctp_clog[sctp_cwnd_log_at].x.rto.rttvar = net->rtt_variance;
	sctp_clog[sctp_cwnd_log_at].x.rto.direction = net->rto_variance_dir;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}

}

void
sctp_log_strm_del_alt(u_int32_t tsn, u_int16_t sseq, int from)
{

	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_STRM;
	sctp_clog[sctp_cwnd_log_at].x.strlog.n_tsn = tsn;
	sctp_clog[sctp_cwnd_log_at].x.strlog.n_sseq = sseq;
	sctp_clog[sctp_cwnd_log_at].x.strlog.e_tsn = 0;
	sctp_clog[sctp_cwnd_log_at].x.strlog.e_sseq = 0;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}

}

void
sctp_log_sack(u_int32_t old_cumack, u_int32_t cumack, u_int32_t tsn, u_int16_t gaps, u_int16_t dups, int from)
{
	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_SACK;
	sctp_clog[sctp_cwnd_log_at].x.sack.cumack = cumack;
	sctp_clog[sctp_cwnd_log_at].x.sack.oldcumack = old_cumack;
	sctp_clog[sctp_cwnd_log_at].x.sack.tsn = tsn;
	sctp_clog[sctp_cwnd_log_at].x.sack.numGaps = gaps;
	sctp_clog[sctp_cwnd_log_at].x.sack.numDups = dups;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}
}

void
sctp_log_map(uint32_t map, uint32_t cum, uint32_t high, int from)
{
	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_MAP;
	sctp_clog[sctp_cwnd_log_at].x.map.base = map;
	sctp_clog[sctp_cwnd_log_at].x.map.cum = cum;
	sctp_clog[sctp_cwnd_log_at].x.map.high = high;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}
}

void
sctp_log_fr(uint32_t biggest_tsn, uint32_t biggest_new_tsn, uint32_t tsn,
    int from)
{

	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_FR;
	sctp_clog[sctp_cwnd_log_at].x.fr.largest_tsn = biggest_tsn;
	sctp_clog[sctp_cwnd_log_at].x.fr.largest_new_tsn = biggest_new_tsn;
	sctp_clog[sctp_cwnd_log_at].x.fr.tsn = tsn;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}
}

void
sctp_log_strm_del(struct sctp_tmit_chunk *chk, struct sctp_tmit_chunk *poschk,
    int from)
{

	if (chk == NULL) {
		printf("Gak log of NULL?\n");
		return;
	}
	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_STRM;
	sctp_clog[sctp_cwnd_log_at].x.strlog.n_tsn = chk->rec.data.TSN_seq;
	sctp_clog[sctp_cwnd_log_at].x.strlog.n_sseq = chk->rec.data.stream_seq;
	if (poschk != NULL) {
		sctp_clog[sctp_cwnd_log_at].x.strlog.e_tsn =
		    poschk->rec.data.TSN_seq;
		sctp_clog[sctp_cwnd_log_at].x.strlog.e_sseq =
		    poschk->rec.data.stream_seq;
	} else {
		sctp_clog[sctp_cwnd_log_at].x.strlog.e_tsn = 0;
		sctp_clog[sctp_cwnd_log_at].x.strlog.e_sseq = 0;
	}
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}
}

void
sctp_log_cwnd(struct sctp_nets *net, int augment, uint8_t from)
{

	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_CWND;
	sctp_clog[sctp_cwnd_log_at].x.cwnd.net = net;
	sctp_clog[sctp_cwnd_log_at].x.cwnd.cwnd_new_value = net->cwnd;
	sctp_clog[sctp_cwnd_log_at].x.cwnd.inflight = net->flight_size;
	sctp_clog[sctp_cwnd_log_at].x.cwnd.cwnd_augment = augment;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}
}


void
sctp_log_lock(struct sctp_inpcb *inp, struct sctp_tcb *stcb, uint8_t from)
{
  sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
  sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_LOCK_EVENT;
  sctp_clog[sctp_cwnd_log_at].x.lock.sock = (u_int32_t)inp->sctp_socket;
  sctp_clog[sctp_cwnd_log_at].x.lock.inp = (u_int32_t)inp;
  if(stcb) {
    sctp_clog[sctp_cwnd_log_at].x.lock.tcb_lock = mtx_owned(&stcb->tcb_mtx);
  } else {
    sctp_clog[sctp_cwnd_log_at].x.lock.tcb_lock = SCTP_LOCK_UNKNOWN;
  }
  if(inp) {
	  sctp_clog[sctp_cwnd_log_at].x.lock.inp_lock = mtx_owned(&inp->inp_mtx);
	  sctp_clog[sctp_cwnd_log_at].x.lock.create_lock = mtx_owned(&inp->inp_create_mtx);
  } else {
	  sctp_clog[sctp_cwnd_log_at].x.lock.inp_lock = SCTP_LOCK_UNKNOWN;
	  sctp_clog[sctp_cwnd_log_at].x.lock.create_lock = SCTP_LOCK_UNKNOWN;
  }
  sctp_clog[sctp_cwnd_log_at].x.lock.info_lock = mtx_owned(&sctppcbinfo.ipi_ep_mtx);
  if(inp->sctp_socket) {
    sctp_clog[sctp_cwnd_log_at].x.lock.sock_lock = mtx_owned(&(inp->sctp_socket->so_rcv.sb_mtx));
    sctp_clog[sctp_cwnd_log_at].x.lock.sockrcvbuf_lock = mtx_owned(&(inp->sctp_socket->so_rcv.sb_mtx));
    sctp_clog[sctp_cwnd_log_at].x.lock.socksndbuf_lock = mtx_owned(&(inp->sctp_socket->so_snd.sb_mtx));
  } else {
    sctp_clog[sctp_cwnd_log_at].x.lock.sock_lock = SCTP_LOCK_UNKNOWN;
    sctp_clog[sctp_cwnd_log_at].x.lock.sockrcvbuf_lock = SCTP_LOCK_UNKNOWN;
    sctp_clog[sctp_cwnd_log_at].x.lock.socksndbuf_lock = SCTP_LOCK_UNKNOWN;
  }
  sctp_cwnd_log_at++;
  if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
    sctp_cwnd_log_at = 0;
    sctp_cwnd_log_rolled = 1;
  }

}

void
sctp_log_maxburst(struct sctp_nets *net, int error, int burst, uint8_t from)
{
	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_MAXBURST;
	sctp_clog[sctp_cwnd_log_at].x.cwnd.net = net;
	sctp_clog[sctp_cwnd_log_at].x.cwnd.cwnd_new_value = error;
	sctp_clog[sctp_cwnd_log_at].x.cwnd.inflight = net->flight_size;
	sctp_clog[sctp_cwnd_log_at].x.cwnd.cwnd_augment = burst;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}
}

void
sctp_log_rwnd(uint8_t from, u_int32_t peers_rwnd , u_int32_t snd_size, u_int32_t overhead)
{
	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_RWND;
	sctp_clog[sctp_cwnd_log_at].x.rwnd.rwnd = peers_rwnd;
	sctp_clog[sctp_cwnd_log_at].x.rwnd.send_size = snd_size;
	sctp_clog[sctp_cwnd_log_at].x.rwnd.overhead = overhead;
	sctp_clog[sctp_cwnd_log_at].x.rwnd.new_rwnd = 0;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}
}

void
sctp_log_rwnd_set(uint8_t from, u_int32_t peers_rwnd , u_int32_t flight_size, u_int32_t overhead, u_int32_t a_rwndval)
{
	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_RWND;
	sctp_clog[sctp_cwnd_log_at].x.rwnd.rwnd = peers_rwnd;
	sctp_clog[sctp_cwnd_log_at].x.rwnd.send_size = flight_size;
	sctp_clog[sctp_cwnd_log_at].x.rwnd.overhead = overhead;
	sctp_clog[sctp_cwnd_log_at].x.rwnd.new_rwnd = a_rwndval;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}
}

void
sctp_log_mbcnt(uint8_t from, u_int32_t total_oq , u_int32_t book, u_int32_t total_mbcnt_q, u_int32_t mbcnt)
{
	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_MBCNT;
	sctp_clog[sctp_cwnd_log_at].x.mbcnt.total_queue_size = total_oq;
	sctp_clog[sctp_cwnd_log_at].x.mbcnt.size_change  = book;
	sctp_clog[sctp_cwnd_log_at].x.mbcnt.total_queue_mb_size = total_mbcnt_q;
	sctp_clog[sctp_cwnd_log_at].x.mbcnt.mbcnt_change = mbcnt;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}
}

void
sctp_log_block(uint8_t from, struct socket *so, struct sctp_association *asoc)
{

	sctp_clog[sctp_cwnd_log_at].from = (u_int8_t)from;
	sctp_clog[sctp_cwnd_log_at].event_type = (u_int8_t)SCTP_LOG_EVENT_BLOCK;
	sctp_clog[sctp_cwnd_log_at].x.blk.maxmb = (u_int16_t)(so->so_snd.sb_mbmax/1024);
	sctp_clog[sctp_cwnd_log_at].x.blk.onmb = asoc->total_output_mbuf_queue_size;
	sctp_clog[sctp_cwnd_log_at].x.blk.maxsb = (u_int16_t)(so->so_snd.sb_hiwat/1024);
	sctp_clog[sctp_cwnd_log_at].x.blk.onsb = asoc->total_output_queue_size;
	sctp_clog[sctp_cwnd_log_at].x.blk.send_sent_qcnt = (u_int16_t)(asoc->send_queue_cnt + asoc->sent_queue_cnt);
	sctp_clog[sctp_cwnd_log_at].x.blk.stream_qcnt = (u_int16_t)asoc->stream_queue_cnt;
	sctp_cwnd_log_at++;
	if (sctp_cwnd_log_at >= SCTP_STAT_LOG_SIZE) {
		sctp_cwnd_log_at = 0;
		sctp_cwnd_log_rolled = 1;
	}
}

int
sctp_fill_stat_log(struct mbuf *m)
{
	struct sctp_cwnd_log_req *req;
	int size_limit, num, i, at, cnt_out=0;

	if (m == NULL)
		return (EINVAL);

	size_limit = (m->m_len - sizeof(struct sctp_cwnd_log_req));
	if (size_limit < sizeof(struct sctp_cwnd_log)) {
		return (EINVAL);
	}
	req = mtod(m, struct sctp_cwnd_log_req *);
	num = size_limit/sizeof(struct sctp_cwnd_log);
	if (sctp_cwnd_log_rolled) {
		req->num_in_log = SCTP_STAT_LOG_SIZE;
	} else {
		req->num_in_log = sctp_cwnd_log_at;
		/* if the log has not rolled, we don't
		 * let you have old data.
		 */
 		if (req->end_at > sctp_cwnd_log_at) {
			req->end_at = sctp_cwnd_log_at;
		}
	}
	if ((num < SCTP_STAT_LOG_SIZE) &&
	    ((sctp_cwnd_log_rolled) || (sctp_cwnd_log_at > num))) {
		/* we can't return all of it */
		if (((req->start_at == 0) && (req->end_at == 0)) ||
		    (req->start_at >= SCTP_STAT_LOG_SIZE) ||
		    (req->end_at >= SCTP_STAT_LOG_SIZE)) {
			/* No user request or user is wacked. */
			req->num_ret = num;
			req->end_at = sctp_cwnd_log_at - 1;
			if ((sctp_cwnd_log_at - num) < 0) {
				int cc;
				cc = num - sctp_cwnd_log_at;
				req->start_at = SCTP_STAT_LOG_SIZE - cc;
			} else {
				req->start_at = sctp_cwnd_log_at - num;
			}
		} else {
			/* a user request */
			int cc;
			if (req->start_at > req->end_at) {
				cc = (SCTP_STAT_LOG_SIZE - req->start_at) +
				    (req->end_at + 1);
			} else {

				cc = (req->end_at - req->start_at) + 1;
			}
			if (cc < num) {
				num = cc;
			}
			req->num_ret = num;
		}
	} else {
		/* We can return all  of it */
		req->start_at = 0;
		req->end_at = sctp_cwnd_log_at - 1;
		req->num_ret = sctp_cwnd_log_at;
	}
	for (i = 0, at = req->start_at; i < req->num_ret; i++) {
		req->log[i] = sctp_clog[at];
		cnt_out++;
		at++;
		if (at >= SCTP_STAT_LOG_SIZE)
			at = 0;
	}
	m->m_len = (cnt_out * sizeof(struct sctp_cwnd_log)) + sizeof(struct sctp_cwnd_log_req);
	return (0);
}

#endif

#ifdef SCTP_AUDITING_ENABLED
u_int8_t sctp_audit_data[SCTP_AUDIT_SIZE][2];
static int sctp_audit_indx = 0;

static
void sctp_print_audit_report(void)
{
	int i;
	int cnt;
	cnt = 0;
	for (i=sctp_audit_indx;i<SCTP_AUDIT_SIZE;i++) {
		if ((sctp_audit_data[i][0] == 0xe0) &&
		    (sctp_audit_data[i][1] == 0x01)) {
			cnt = 0;
			printf("\n");
		} else if (sctp_audit_data[i][0] == 0xf0) {
			cnt = 0;
			printf("\n");
		} else if ((sctp_audit_data[i][0] == 0xc0) &&
		    (sctp_audit_data[i][1] == 0x01)) {
			printf("\n");
			cnt = 0;
		}
		printf("%2.2x%2.2x ", (uint32_t)sctp_audit_data[i][0],
		    (uint32_t)sctp_audit_data[i][1]);
		cnt++;
		if ((cnt % 14) == 0)
			printf("\n");
	}
	for (i=0;i<sctp_audit_indx;i++) {
		if ((sctp_audit_data[i][0] == 0xe0) &&
		    (sctp_audit_data[i][1] == 0x01)) {
			cnt = 0;
			printf("\n");
		} else if (sctp_audit_data[i][0] == 0xf0) {
			cnt = 0;
			printf("\n");
		} else if ((sctp_audit_data[i][0] == 0xc0) &&
			 (sctp_audit_data[i][1] == 0x01)) {
			printf("\n");
			cnt = 0;
		}
		printf("%2.2x%2.2x ", (uint32_t)sctp_audit_data[i][0],
		    (uint32_t)sctp_audit_data[i][1]);
		cnt++;
		if ((cnt % 14) == 0)
			printf("\n");
	}
	printf("\n");
}

void sctp_auditing(int from, struct sctp_inpcb *inp, struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	int resend_cnt, tot_out, rep, tot_book_cnt;
	struct sctp_nets *lnet;
	struct sctp_tmit_chunk *chk;
	sctp_audit_data[sctp_audit_indx][0] = 0xAA;
	sctp_audit_data[sctp_audit_indx][1] = 0x000000ff & from;
	sctp_audit_indx++;
	if (sctp_audit_indx >= SCTP_AUDIT_SIZE) {
		sctp_audit_indx = 0;
	}
	if (inp == NULL) {
		sctp_audit_data[sctp_audit_indx][0] = 0xAF;
		sctp_audit_data[sctp_audit_indx][1] = 0x01;
		sctp_audit_indx++;
		if (sctp_audit_indx >= SCTP_AUDIT_SIZE) {
			sctp_audit_indx = 0;
		}
		return;
	}
	if (stcb == NULL) {
		sctp_audit_data[sctp_audit_indx][0] = 0xAF;
		sctp_audit_data[sctp_audit_indx][1] = 0x02;
		sctp_audit_indx++;
		if (sctp_audit_indx >= SCTP_AUDIT_SIZE) {
			sctp_audit_indx = 0;
		}
		return;
	}
	sctp_audit_data[sctp_audit_indx][0] = 0xA1;
	sctp_audit_data[sctp_audit_indx][1] =
	    (0x000000ff & stcb->asoc.sent_queue_retran_cnt);
	sctp_audit_indx++;
	if (sctp_audit_indx >= SCTP_AUDIT_SIZE) {
		sctp_audit_indx = 0;
	}
	rep = 0;
	tot_book_cnt = 0;
	resend_cnt = tot_out = 0;
	TAILQ_FOREACH(chk, &stcb->asoc.sent_queue, sctp_next) {
		if (chk->sent == SCTP_DATAGRAM_RESEND) {
			resend_cnt++;
		} else if (chk->sent < SCTP_DATAGRAM_RESEND) {
			tot_out += chk->book_size;
			tot_book_cnt++;
		}
	}
	if (resend_cnt != stcb->asoc.sent_queue_retran_cnt) {
		sctp_audit_data[sctp_audit_indx][0] = 0xAF;
		sctp_audit_data[sctp_audit_indx][1] = 0xA1;
		sctp_audit_indx++;
		if (sctp_audit_indx >= SCTP_AUDIT_SIZE) {
			sctp_audit_indx = 0;
		}
		printf("resend_cnt:%d asoc-tot:%d\n",
		    resend_cnt, stcb->asoc.sent_queue_retran_cnt);
		rep = 1;
		stcb->asoc.sent_queue_retran_cnt = resend_cnt;
		sctp_audit_data[sctp_audit_indx][0] = 0xA2;
		sctp_audit_data[sctp_audit_indx][1] =
		    (0x000000ff & stcb->asoc.sent_queue_retran_cnt);
		sctp_audit_indx++;
		if (sctp_audit_indx >= SCTP_AUDIT_SIZE) {
			sctp_audit_indx = 0;
		}
	}
	if (tot_out != stcb->asoc.total_flight) {
		sctp_audit_data[sctp_audit_indx][0] = 0xAF;
		sctp_audit_data[sctp_audit_indx][1] = 0xA2;
		sctp_audit_indx++;
		if (sctp_audit_indx >= SCTP_AUDIT_SIZE) {
			sctp_audit_indx = 0;
		}
		rep = 1;
		printf("tot_flt:%d asoc_tot:%d\n", tot_out,
		    (int)stcb->asoc.total_flight);
		stcb->asoc.total_flight = tot_out;
	}
	if (tot_book_cnt != stcb->asoc.total_flight_count) {
		sctp_audit_data[sctp_audit_indx][0] = 0xAF;
		sctp_audit_data[sctp_audit_indx][1] = 0xA5;
		sctp_audit_indx++;
		if (sctp_audit_indx >= SCTP_AUDIT_SIZE) {
			sctp_audit_indx = 0;
		}
		rep = 1;
		printf("tot_flt_book:%d\n", tot_book);

		stcb->asoc.total_flight_count = tot_book_cnt;
	}
	tot_out = 0;
	TAILQ_FOREACH(lnet, &stcb->asoc.nets, sctp_next) {
		tot_out += lnet->flight_size;
	}
	if (tot_out != stcb->asoc.total_flight) {
		sctp_audit_data[sctp_audit_indx][0] = 0xAF;
		sctp_audit_data[sctp_audit_indx][1] = 0xA3;
		sctp_audit_indx++;
		if (sctp_audit_indx >= SCTP_AUDIT_SIZE) {
			sctp_audit_indx = 0;
		}
		rep = 1;
		printf("real flight:%d net total was %d\n",
		    stcb->asoc.total_flight, tot_out);
		/* now corrective action */
		TAILQ_FOREACH(lnet, &stcb->asoc.nets, sctp_next) {

			tot_out = 0;
			TAILQ_FOREACH(chk, &stcb->asoc.sent_queue, sctp_next) {
				if ((chk->whoTo == lnet) &&
				    (chk->sent < SCTP_DATAGRAM_RESEND)) {
					tot_out += chk->book_size;
				}
			}
			if (lnet->flight_size != tot_out) {
				printf("net:%x flight was %d corrected to %d\n",
				    (uint32_t)lnet, lnet->flight_size, tot_out);
				lnet->flight_size = tot_out;
			}

		}
	}

	if (rep) {
		sctp_print_audit_report();
	}
}

void
sctp_audit_log(u_int8_t ev, u_int8_t fd)
{
	int s;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	sctp_audit_data[sctp_audit_indx][0] = ev;
	sctp_audit_data[sctp_audit_indx][1] = fd;
	sctp_audit_indx++;
	if (sctp_audit_indx >= SCTP_AUDIT_SIZE) {
		sctp_audit_indx = 0;
	}
	splx(s);
}

#endif

/*
 * a list of sizes based on typical mtu's, used only if next hop
 * size not returned.
 */
static int sctp_mtu_sizes[] = {
	68,
	296,
	508,
	512,
	544,
	576,
	1006,
	1492,
	1500,
	1536,
	2002,
	2048,
	4352,
	4464,
	8166,
	17914,
	32000,
	65535
};

int
find_next_best_mtu(int totsz)
{
	int i, perfer;
	/*
	 * if we are in here we must find the next best fit based on the
	 * size of the dg that failed to be sent.
	 */
	perfer = 0;
	for (i = 0; i < NUMBER_OF_MTU_SIZES; i++) {
		if (totsz < sctp_mtu_sizes[i]) {
			perfer = i - 1;
			if (perfer < 0)
				perfer = 0;
			break;
		}
	}
	return (sctp_mtu_sizes[perfer]);
}

void
sctp_fill_random_store(struct sctp_pcb *m)
{
	/*
	 * Here we use the MD5/SHA-1 to hash with our good randomNumbers
	 * and our counter. The result becomes our good random numbers and
	 * we then setup to give these out. Note that we do no lockig
	 * to protect this. This is ok, since if competing folks call
	 * this we will get more gobbled gook in the random store whic
	 * is what we want. There is a danger that two guys will use
	 * the same random numbers, but thats ok too since that
	 * is random as well :->
	 */
	m->store_at = 0;
	sctp_hash_digest((char *)m->random_numbers, sizeof(m->random_numbers),
			 (char *)&m->random_counter, sizeof(m->random_counter),
			 (char *)m->random_store);
	m->random_counter++;
}

uint32_t
sctp_select_initial_TSN(struct sctp_pcb *m)
{
	/*
	 * A true implementation should use random selection process to
	 * get the initial stream sequence number, using RFC1750 as a
	 * good guideline
	 */
	u_long x, *xp;
	uint8_t *p;

	if (m->initial_sequence_debug != 0) {
		u_int32_t ret;
		ret = m->initial_sequence_debug;
		m->initial_sequence_debug++;
		return (ret);
	}
	if ((m->store_at+sizeof(u_long)) > SCTP_SIGNATURE_SIZE) {
		/* Refill the random store */
		sctp_fill_random_store(m);
	}
	p = &m->random_store[(int)m->store_at];
	xp = (u_long *)p;
	x = *xp;
	m->store_at += sizeof(u_long);
	return (x);
}

u_int32_t sctp_select_a_tag(struct sctp_inpcb *m)
{
	u_long x, not_done;
	struct timeval now;

	SCTP_GETTIME_TIMEVAL(&now);
	not_done = 1;
	while (not_done) {
		x = sctp_select_initial_TSN(&m->sctp_ep);
		if (x == 0) {
			/* we never use 0 */
			continue;
		}
		if (sctp_is_vtag_good(m, x, &now)) {
			not_done = 0;
		}
	}
	return (x);
}


int
sctp_init_asoc(struct sctp_inpcb *m, struct sctp_association *asoc,
	       int for_a_init, uint32_t override_tag )
{
	/*
	 * Anything set to zero is taken care of by the allocation
	 * routine's bzero
	 */

	/*
	 * Up front select what scoping to apply on addresses I tell my peer
	 * Not sure what to do with these right now, we will need to come up
	 * with a way to set them. We may need to pass them through from the
	 * caller in the sctp_aloc_assoc() function.
	 */
	int i;
	/* init all variables to a known value.*/
	asoc->state = SCTP_STATE_INUSE;
	asoc->max_burst = m->sctp_ep.max_burst;
	asoc->heart_beat_delay = m->sctp_ep.sctp_timeoutticks[SCTP_TIMER_HEARTBEAT];
	asoc->cookie_life = m->sctp_ep.def_cookie_life;

	if (override_tag) {
		asoc->my_vtag = override_tag;
	} else {
		asoc->my_vtag = sctp_select_a_tag(m);
	}
	asoc->asconf_seq_out = asoc->str_reset_seq_out = asoc->init_seq_number = asoc->sending_seq =
		sctp_select_initial_TSN(&m->sctp_ep);
	asoc->t3timeout_highest_marked = asoc->asconf_seq_out;
	/* we are opptimisitic here */
	asoc->peer_supports_asconf = 1;
	asoc->peer_supports_asconf_setprim = 1;
	asoc->peer_supports_pktdrop = 1;

	asoc->sent_queue_retran_cnt = 0;
	/* This will need to be adjusted */
	asoc->last_cwr_tsn = asoc->init_seq_number - 1;
	asoc->last_acked_seq = asoc->init_seq_number - 1;
	asoc->advanced_peer_ack_point = asoc->last_acked_seq;
	asoc->asconf_seq_in = asoc->last_acked_seq;

	/* here we are different, we hold the next one we expect */
	asoc->str_reset_seq_in = asoc->last_acked_seq + 1;

	asoc->initial_init_rto_max = m->sctp_ep.initial_init_rto_max;
	asoc->initial_rto = m->sctp_ep.initial_rto;

	asoc->max_init_times = m->sctp_ep.max_init_times;
	asoc->max_send_times = m->sctp_ep.max_send_times;
	asoc->def_net_failure = m->sctp_ep.def_net_failure;

	/* ECN Nonce initialization */
	asoc->ecn_nonce_allowed = 0;
	asoc->receiver_nonce_sum = 1;
	asoc->nonce_sum_expect_base = 1;
	asoc->nonce_sum_check = 1;
	asoc->nonce_resync_tsn = 0;
	asoc->nonce_wait_for_ecne = 0;
	asoc->nonce_wait_tsn = 0;
	asoc->delayed_ack = TICKS_TO_MSEC(m->sctp_ep.sctp_timeoutticks[SCTP_TIMER_RECV]);

	if (m->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
		struct in6pcb *inp6;


		/* Its a V6 socket */
		inp6 = (struct in6pcb *)m;
		asoc->ipv6_addr_legal = 1;
		/* Now look at the binding flag to see if V4 will be legal */
	if (
#if defined(__OpenBSD__)
		(0) /* we always do dual bind */
#elif defined (__NetBSD__)
		(inp6->in6p_flags & IN6P_IPV6_V6ONLY)
#else
		(inp6->inp_flags & IN6P_IPV6_V6ONLY)
#endif
	     == 0) {
			asoc->ipv4_addr_legal = 1;
		} else {
			/* V4 addresses are NOT legal on the association */
			asoc->ipv4_addr_legal = 0;
		}
	} else {
		/* Its a V4 socket, no - V6 */
		asoc->ipv4_addr_legal = 1;
		asoc->ipv6_addr_legal = 0;
	}


	asoc->my_rwnd = max(m->sctp_socket->so_rcv.sb_hiwat, SCTP_MINIMAL_RWND);
	asoc->peers_rwnd = m->sctp_socket->so_rcv.sb_hiwat;

	asoc->smallest_mtu = m->sctp_frag_point;
	asoc->minrto = m->sctp_ep.sctp_minrto;
	asoc->maxrto = m->sctp_ep.sctp_maxrto;

	LIST_INIT(&asoc->sctp_local_addr_list);
	TAILQ_INIT(&asoc->nets);
	TAILQ_INIT(&asoc->pending_reply_queue);
	asoc->last_asconf_ack_sent = NULL;
	/* Setup to fill the hb random cache at first HB */
	asoc->hb_random_idx = 4;

	asoc->sctp_autoclose_ticks = m->sctp_ep.auto_close_time;

	/*
	 * Now the stream parameters, here we allocate space for all
	 * streams that we request by default.
	 */
	asoc->streamoutcnt = asoc->pre_open_streams =
	    m->sctp_ep.pre_open_stream_count;
	MALLOC(asoc->strmout, struct sctp_stream_out *, asoc->streamoutcnt *
	    sizeof(struct sctp_stream_out), M_PCB, M_NOWAIT);
	if (asoc->strmout == NULL) {
		/* big trouble no memory */
		return (ENOMEM);
	}
	for (i = 0; i < asoc->streamoutcnt; i++) {
		/*
		 * inbound side must be set to 0xffff,
		 * also NOTE when we get the INIT-ACK back (for INIT sender)
		 * we MUST reduce the count (streamoutcnt) but first check
		 * if we sent to any of the upper streams that were dropped
		 * (if some were). Those that were dropped must be notified
		 * to the upper layer as failed to send.
		 */
		asoc->strmout[i].next_sequence_sent = 0x0;
		TAILQ_INIT(&asoc->strmout[i].outqueue);
		asoc->strmout[i].stream_no = i;
		asoc->strmout[i].next_spoke.tqe_next = 0;
		asoc->strmout[i].next_spoke.tqe_prev = 0;
	}
	/* Now the mapping array */
	asoc->mapping_array_size = SCTP_INITIAL_MAPPING_ARRAY;
#ifdef __NetBSD__
	MALLOC(asoc->mapping_array, u_int8_t *, SCTP_INITIAL_MAPPING_ARRAY,
	       M_PCB, M_NOWAIT);
#else
	MALLOC(asoc->mapping_array, u_int8_t *, asoc->mapping_array_size,
	       M_PCB, M_NOWAIT);
#endif
	if (asoc->mapping_array == NULL) {
		FREE(asoc->strmout, M_PCB);
		return (ENOMEM);
	}
	memset(asoc->mapping_array, 0, asoc->mapping_array_size);
	/* Now the init of the other outqueues */
	TAILQ_INIT(&asoc->out_wheel);
	TAILQ_INIT(&asoc->control_send_queue);
	TAILQ_INIT(&asoc->send_queue);
	TAILQ_INIT(&asoc->sent_queue);
	TAILQ_INIT(&asoc->reasmqueue);
	TAILQ_INIT(&asoc->delivery_queue);
	asoc->max_inbound_streams = m->sctp_ep.max_open_streams_intome;

	TAILQ_INIT(&asoc->asconf_queue);
	return (0);
}

int
sctp_expand_mapping_array(struct sctp_association *asoc)
{
	/* mapping array needs to grow */
	u_int8_t *new_array;
	uint16_t new_size;

	new_size = asoc->mapping_array_size + SCTP_MAPPING_ARRAY_INCR;
#ifdef __NetBSD__
	MALLOC(new_array, u_int8_t *, asoc->mapping_array_size
		+ SCTP_MAPPING_ARRAY_INCR, M_PCB, M_NOWAIT);
#else
	MALLOC(new_array, u_int8_t *, new_size, M_PCB, M_NOWAIT);
#endif
	if (new_array == NULL) {
		/* can't get more, forget it */
		printf("No memory for expansion of SCTP mapping array %d\n",
		       new_size);
		return (-1);
	}
	memset(new_array, 0, new_size);
	memcpy(new_array, asoc->mapping_array, asoc->mapping_array_size);
	FREE(asoc->mapping_array, M_PCB);
	asoc->mapping_array = new_array;
	asoc->mapping_array_size = new_size;
	return (0);
}

static void
sctp_timeout_handler(void *t)
{
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;
	struct sctp_timer *tmr;
	int s, did_output, typ;
#if defined(__APPLE__) && defined(SCTP_APPLE_PANTHER)
	boolean_t funnel_state;

	/* get BSD kernel funnel/mutex */
	funnel_state = thread_funnel_set(network_flock, TRUE);
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	tmr = (struct sctp_timer *)t;
	inp = (struct sctp_inpcb *)tmr->ep;
	stcb = (struct sctp_tcb *)tmr->tcb;
	net = (struct sctp_nets *)tmr->net;
	did_output = 1;


#ifdef SCTP_AUDITING_ENABLED
	sctp_audit_log(0xF0, (u_int8_t)tmr->type);
	sctp_auditing(3, inp, stcb, net);
#endif
	sctp_pegs[SCTP_TIMERS_EXP]++;

	if (inp == NULL) {
		return;
	}

	SCTP_INP_WLOCK(inp);
	if (inp->sctp_socket == 0) {
		splx(s);
#if defined(__APPLE__) && defined(SCTP_APPLE_PANTHER)
		/* release BSD kernel funnel/mutex */
		(void) thread_funnel_set(network_flock, FALSE);
#endif
		SCTP_INP_WUNLOCK(inp);
		return;
	}
	if (stcb) {
		if (stcb->asoc.state == 0) {
			splx(s);
#if defined(__APPLE__) && defined(SCTP_APPLE_PANTHER)
			/* release BSD kernel funnel/mutex */
			(void) thread_funnel_set(network_flock, FALSE);
#endif
			SCTP_INP_WUNLOCK(inp);
			return;
		}
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_TIMER1) {
		printf("Timer type %d goes off\n", tmr->type);
	}
#endif /* SCTP_DEBUG */
#ifndef __NetBSD__
	if (!callout_active(&tmr->timer)) {
		splx(s);
#if defined(__APPLE__) && defined(SCTP_APPLE_PANTHER)
		/* release BSD kernel funnel/mutex */
		(void) thread_funnel_set(network_flock, FALSE);
#endif
		SCTP_INP_WUNLOCK(inp);
		return;
	}
#endif
#if defined(__APPLE__)
	/* clear the callout pending status here */
	callout_stop(&tmr->timer);
#endif
	if (stcb) {
		SCTP_TCB_LOCK(stcb);
	}
	SCTP_INP_INCR_REF(inp);
	SCTP_INP_WUNLOCK(inp);

	typ = tmr->type;
	switch (tmr->type) {
	case SCTP_TIMER_TYPE_ITERATOR:
	{
		struct sctp_iterator *it;
		it = (struct sctp_iterator *)inp;
		sctp_iterator_timer(it);
	}
	break;
	/* call the handler for the appropriate timer type */
	case SCTP_TIMER_TYPE_SEND:
		sctp_pegs[SCTP_TMIT_TIMER]++;
		stcb->asoc.num_send_timers_up--;
		if (stcb->asoc.num_send_timers_up < 0) {
			stcb->asoc.num_send_timers_up = 0;
		}
		if (sctp_t3rxt_timer(inp, stcb, net)) {
			/* no need to unlock on tcb its gone */

			goto out_decr;
		}
#ifdef SCTP_AUDITING_ENABLED
		sctp_auditing(4, inp, stcb, net);
#endif
		sctp_chunk_output(inp, stcb, 1);
		if ((stcb->asoc.num_send_timers_up == 0) &&
		    (stcb->asoc.sent_queue_cnt > 0)
			) {
			struct sctp_tmit_chunk *chk;
			/*
			 * safeguard. If there on some on the sent queue
			 * somewhere but no timers running something is
			 * wrong... so we start a timer on the first chunk
			 * on the send queue on whatever net it is sent to.
			 */
			sctp_pegs[SCTP_T3_SAFEGRD]++;
			chk = TAILQ_FIRST(&stcb->asoc.sent_queue);
			sctp_timer_start(SCTP_TIMER_TYPE_SEND, inp, stcb,
					 chk->whoTo);
		}
		break;
	case SCTP_TIMER_TYPE_INIT:
		if (sctp_t1init_timer(inp, stcb, net)) {
			/* no need to unlock on tcb its gone */
			goto out_decr;
		}
		/* We do output but not here */
		did_output = 0;
		break;
	case SCTP_TIMER_TYPE_RECV:
		sctp_pegs[SCTP_RECV_TIMER]++;
		sctp_send_sack(stcb);
#ifdef SCTP_AUDITING_ENABLED
		sctp_auditing(4, inp, stcb, net);
#endif
		sctp_chunk_output(inp, stcb, 4);
		break;
	case SCTP_TIMER_TYPE_SHUTDOWN:
		if (sctp_shutdown_timer(inp, stcb, net) ) {
			/* no need to unlock on tcb its gone */
			goto out_decr;
		}
#ifdef SCTP_AUDITING_ENABLED
		sctp_auditing(4, inp, stcb, net);
#endif
		sctp_chunk_output(inp, stcb, 5);
		break;
	case SCTP_TIMER_TYPE_HEARTBEAT:
		if (sctp_heartbeat_timer(inp, stcb, net)) {
			/* no need to unlock on tcb its gone */
			goto out_decr;
		}
#ifdef SCTP_AUDITING_ENABLED
		sctp_auditing(4, inp, stcb, net);
#endif
		sctp_chunk_output(inp, stcb, 6);
		break;
	case SCTP_TIMER_TYPE_COOKIE:
		if (sctp_cookie_timer(inp, stcb, net)) {
			/* no need to unlock on tcb its gone */
			goto out_decr;
		}
#ifdef SCTP_AUDITING_ENABLED
		sctp_auditing(4, inp, stcb, net);
#endif
		sctp_chunk_output(inp, stcb, 1);
		break;
	case SCTP_TIMER_TYPE_NEWCOOKIE:
	{
		struct timeval tv;
		int i, secret;
		SCTP_GETTIME_TIMEVAL(&tv);
		SCTP_INP_WLOCK(inp);
		inp->sctp_ep.time_of_secret_change = tv.tv_sec;
		inp->sctp_ep.last_secret_number =
			inp->sctp_ep.current_secret_number;
		inp->sctp_ep.current_secret_number++;
		if (inp->sctp_ep.current_secret_number >=
		    SCTP_HOW_MANY_SECRETS) {
			inp->sctp_ep.current_secret_number = 0;
		}
		secret = (int)inp->sctp_ep.current_secret_number;
		for (i = 0; i < SCTP_NUMBER_OF_SECRETS; i++) {
			inp->sctp_ep.secret_key[secret][i] =
				sctp_select_initial_TSN(&inp->sctp_ep);
		}
		SCTP_INP_WUNLOCK(inp);
		sctp_timer_start(SCTP_TIMER_TYPE_NEWCOOKIE, inp, stcb, net);
	}
	did_output = 0;
	break;
	case SCTP_TIMER_TYPE_PATHMTURAISE:
		sctp_pathmtu_timer(inp, stcb, net);
		did_output = 0;
		break;
	case SCTP_TIMER_TYPE_SHUTDOWNACK:
		if (sctp_shutdownack_timer(inp, stcb, net)) {
			/* no need to unlock on tcb its gone */
			goto out_decr;
		}
#ifdef SCTP_AUDITING_ENABLED
		sctp_auditing(4, inp, stcb, net);
#endif
		sctp_chunk_output(inp, stcb, 7);
		break;
	case SCTP_TIMER_TYPE_SHUTDOWNGUARD:
		sctp_abort_an_association(inp, stcb,
					  SCTP_SHUTDOWN_GUARD_EXPIRES, NULL);
		/* no need to unlock on tcb its gone */
		goto out_decr;
		break;

	case SCTP_TIMER_TYPE_STRRESET:
		if (sctp_strreset_timer(inp, stcb, net)) {
			/* no need to unlock on tcb its gone */
			goto out_decr;
		}
		sctp_chunk_output(inp, stcb, 9);
		break;
	case SCTP_TIMER_TYPE_EARLYFR:
	        /* Need to do FR of things for net */

	        break;
	case SCTP_TIMER_TYPE_ASCONF:
		if (sctp_asconf_timer(inp, stcb, net)) {
			/* no need to unlock on tcb its gone */
			goto out_decr;
		}
#ifdef SCTP_AUDITING_ENABLED
		sctp_auditing(4, inp, stcb, net);
#endif
		sctp_chunk_output(inp, stcb, 8);
		break;

	case SCTP_TIMER_TYPE_AUTOCLOSE:
		sctp_autoclose_timer(inp, stcb, net);
		sctp_chunk_output(inp, stcb, 10);
		did_output = 0;
		break;
	case SCTP_TIMER_TYPE_INPKILL:
		/* special case, take away our
		 * increment since WE are the killer
		 */
		SCTP_INP_WLOCK(inp);
		SCTP_INP_DECR_REF(inp);
		SCTP_INP_WUNLOCK(inp);
		sctp_timer_stop(SCTP_TIMER_TYPE_INPKILL, inp, NULL, NULL);
		sctp_inpcb_free(inp, 1);
		goto out_no_decr;
		break;
	default:
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_TIMER1) {
			printf("sctp_timeout_handler:unknown timer %d\n",
			       tmr->type);
		}
#endif /* SCTP_DEBUG */
		break;
	};
#ifdef SCTP_AUDITING_ENABLED
	sctp_audit_log(0xF1, (u_int8_t)tmr->type);
	sctp_auditing(5, inp, stcb, net);
#endif
	if (did_output) {
		/*
		 * Now we need to clean up the control chunk chain if an
		 * ECNE is on it. It must be marked as UNSENT again so next
		 * call will continue to send it until such time that we get
		 * a CWR, to remove it. It is, however, less likely that we
		 * will find a ecn echo on the chain though.
		 */
		sctp_fix_ecn_echo(&stcb->asoc);
	}
	if (stcb) {
		SCTP_TCB_UNLOCK(stcb);
	}
 out_decr:
	SCTP_INP_WLOCK(inp);
	SCTP_INP_DECR_REF(inp);
	SCTP_INP_WUNLOCK(inp);

 out_no_decr:

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_TIMER1) {
		printf("Timer now complete (type %d)\n", typ);
	}
#endif /* SCTP_DEBUG */

	splx(s);
#if defined(__APPLE__) && defined(SCTP_APPLE_PANTHER)
	/* release BSD kernel funnel/mutex */
	(void) thread_funnel_set(network_flock, FALSE);
#endif
}

int
sctp_timer_start(int t_type, struct sctp_inpcb *inp, struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	int to_ticks;
	struct sctp_timer *tmr;

	if (inp == NULL)
		return (EFAULT);

	to_ticks = 0;

	tmr = NULL;
	if(stcb) {
		STCB_TCB_LOCK_ASSERT(stcb);
	}
	switch (t_type) {
	case SCTP_TIMER_TYPE_ITERATOR:
	{
		struct sctp_iterator *it;
		it = (struct sctp_iterator *)inp;
		tmr = &it->tmr;
		to_ticks = SCTP_ITERATOR_TICKS;
	}
	break;
	case SCTP_TIMER_TYPE_SEND:
		/* Here we use the RTO timer */
	{
		int rto_val;
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		tmr = &net->rxt_timer;
		if (net->RTO == 0) {
			rto_val = stcb->asoc.initial_rto;
		} else {
			rto_val = net->RTO;
		}
		to_ticks = MSEC_TO_TICKS(rto_val);
	}
	break;
	case SCTP_TIMER_TYPE_INIT:
		/*
		 * Here we use the INIT timer default
		 * usually about 1 minute.
		 */
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		tmr = &net->rxt_timer;
		if (net->RTO == 0) {
			to_ticks = MSEC_TO_TICKS(stcb->asoc.initial_rto);
		} else {
			to_ticks = MSEC_TO_TICKS(net->RTO);
		}
		break;
	case SCTP_TIMER_TYPE_RECV:
		/*
		 * Here we use the Delayed-Ack timer value from the inp
		 * ususually about 200ms.
		 */
		if (stcb == NULL) {
			return (EFAULT);
		}
		tmr = &stcb->asoc.dack_timer;
		to_ticks = MSEC_TO_TICKS(stcb->asoc.delayed_ack);
		break;
	case SCTP_TIMER_TYPE_SHUTDOWN:
		/* Here we use the RTO of the destination. */
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}

		if (net->RTO == 0) {
			to_ticks = MSEC_TO_TICKS(stcb->asoc.initial_rto);
		} else {
			to_ticks = MSEC_TO_TICKS(net->RTO);
		}
		tmr = &net->rxt_timer;
		break;
	case SCTP_TIMER_TYPE_HEARTBEAT:
		/*
		 * the net is used here so that we can add in the RTO.
		 * Even though we use a different timer. We also add the
		 * HB timer PLUS a random jitter.
		 */
		if (stcb == NULL) {
			return (EFAULT);
		}
		{
			uint32_t rndval;
			uint8_t this_random;
			int cnt_of_unconf=0;
			struct sctp_nets *lnet;
			TAILQ_FOREACH(lnet, &stcb->asoc.nets, sctp_next) {
				if (lnet->dest_state & SCTP_ADDR_UNCONFIRMED) {
					cnt_of_unconf++;
				}
			}
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_TIMER1) {
				printf("HB timer to start unconfirmed:%d hb_delay:%d\n",
				       cnt_of_unconf, stcb->asoc.heart_beat_delay);
			}
#endif
			if (stcb->asoc.hb_random_idx > 3) {
				rndval = sctp_select_initial_TSN(&inp->sctp_ep);
				memcpy(stcb->asoc.hb_random_values, &rndval,
				       sizeof(stcb->asoc.hb_random_values));
				this_random = stcb->asoc.hb_random_values[0];
				stcb->asoc.hb_random_idx = 0;
				stcb->asoc.hb_ect_randombit = 0;
			} else {
				this_random = stcb->asoc.hb_random_values[stcb->asoc.hb_random_idx];
				stcb->asoc.hb_random_idx++;
				stcb->asoc.hb_ect_randombit = 0;
			}
			/*
			 * this_random will be 0 - 256 ms
			 * RTO is in ms.
			 */
			if ((stcb->asoc.heart_beat_delay == 0) &&
			    (cnt_of_unconf == 0)) {
				/* no HB on this inp after confirmations */
				return (0);
			}
			if (net) {
				struct sctp_nets *lnet;
				int delay;
				delay = stcb->asoc.heart_beat_delay;
				TAILQ_FOREACH(lnet, &stcb->asoc.nets, sctp_next) {
					if ((lnet->dest_state & SCTP_ADDR_UNCONFIRMED) &&
					    ((lnet->dest_state & SCTP_ADDR_OUT_OF_SCOPE) == 0) &&
					    (lnet->dest_state & SCTP_ADDR_REACHABLE)) {
					    delay = 0;
					}
				}
				if (net->RTO == 0) {
					/* Never been checked */
					to_ticks = this_random + stcb->asoc.initial_rto + delay;
				} else {
					/* set rto_val to the ms */
					to_ticks = delay + net->RTO + this_random;
				}
			} else {
				if (cnt_of_unconf) {
					to_ticks = this_random + stcb->asoc.initial_rto;
				} else {
					to_ticks = stcb->asoc.heart_beat_delay + this_random + stcb->asoc.initial_rto;
				}
			}
			/*
			 * Now we must convert the to_ticks that are now in
			 * ms to ticks.
			 */
			to_ticks *= hz;
			to_ticks /= 1000;
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_TIMER1) {
				printf("Timer to expire in %d ticks\n", to_ticks);
			}
#endif
			tmr = &stcb->asoc.hb_timer;
		}
		break;
	case SCTP_TIMER_TYPE_COOKIE:
		/*
		 * Here we can use the RTO timer from the network since
		 * one RTT was compelete. If a retran happened then we will
		 * be using the RTO initial value.
		 */
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		if (net->RTO == 0) {
			to_ticks = MSEC_TO_TICKS(stcb->asoc.initial_rto);
		} else {
			to_ticks = MSEC_TO_TICKS(net->RTO);
		}
		tmr = &net->rxt_timer;
		break;
	case SCTP_TIMER_TYPE_NEWCOOKIE:
		/*
		 * nothing needed but the endpoint here
		 * ususually about 60 minutes.
		 */
		tmr = &inp->sctp_ep.signature_change;
		to_ticks = inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_SIGNATURE];
		break;
	case SCTP_TIMER_TYPE_INPKILL:
		/*
		 * The inp is setup to die. We re-use the
		 * signature_chage timer since that has
		 * stopped and we are in the GONE state.
		 */
		tmr = &inp->sctp_ep.signature_change;
		to_ticks = (SCTP_INP_KILL_TIMEOUT * hz) / 1000;
		break;
	case SCTP_TIMER_TYPE_PATHMTURAISE:
		/*
		 * Here we use the value found in the EP for PMTU
		 * ususually about 10 minutes.
		 */
		if (stcb == NULL) {
			return (EFAULT);
		}
		if (net == NULL) {
			return (EFAULT);
		}
		to_ticks = inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_PMTU];
		tmr = &net->pmtu_timer;
		break;
	case SCTP_TIMER_TYPE_SHUTDOWNACK:
		/* Here we use the RTO of the destination */
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		if (net->RTO == 0) {
			to_ticks = MSEC_TO_TICKS(stcb->asoc.initial_rto);
		} else {
			to_ticks = MSEC_TO_TICKS(net->RTO);
		}
		tmr = &net->rxt_timer;
		break;
	case SCTP_TIMER_TYPE_SHUTDOWNGUARD:
		/*
		 * Here we use the endpoints shutdown guard timer
		 * usually about 3 minutes.
		 */
		if (stcb == NULL) {
			return (EFAULT);
		}
		to_ticks = inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_MAXSHUTDOWN];
		tmr = &stcb->asoc.shut_guard_timer;
		break;
	case SCTP_TIMER_TYPE_STRRESET:
		/*
		 * Here the timer comes from the inp
		 * but its value is from the RTO.
		 */
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		if (net->RTO == 0) {
			to_ticks = MSEC_TO_TICKS(stcb->asoc.initial_rto);
		} else {
			to_ticks = MSEC_TO_TICKS(net->RTO);
		}
		tmr = &stcb->asoc.strreset_timer;
		break;

	case SCTP_TIMER_TYPE_EARLYFR:
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		if(net->flight_size > net->cwnd) {
		  /* no need to start */
		  return (0);
		}
		if(net->lastsa == 0) {
		  /* Hmm no rtt estimate yet? */
		  to_ticks = MSEC_TO_TICKS((stcb->asoc.initial_rto >> 2));
		} else {
		  to_ticks = MSEC_TO_TICKS((net->lastsa +  (net->lastsa >> 1)));
		}
		tmr = &net->fr_timer;
	        break;
	case SCTP_TIMER_TYPE_ASCONF:
		/*
		 * Here the timer comes from the inp
		 * but its value is from the RTO.
		 */
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		if (net->RTO == 0) {
			to_ticks = MSEC_TO_TICKS(stcb->asoc.initial_rto);
		} else {
			to_ticks = MSEC_TO_TICKS(net->RTO);
		}
		tmr = &stcb->asoc.asconf_timer;
		break;
	case SCTP_TIMER_TYPE_AUTOCLOSE:
		if (stcb == NULL) {
			return (EFAULT);
		}
		if (stcb->asoc.sctp_autoclose_ticks == 0) {
			/* Really an error since stcb is NOT set to autoclose */
			return (0);
		}
		to_ticks = stcb->asoc.sctp_autoclose_ticks;
		tmr = &stcb->asoc.autoclose_timer;
		break;
	default:
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_TIMER1) {
			printf("sctp_timer_start:Unknown timer type %d\n",
			       t_type);
		}
#endif /* SCTP_DEBUG */
		return (EFAULT);
		break;
	};
	if ((to_ticks <= 0) || (tmr == NULL)) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_TIMER1) {
			printf("sctp_timer_start:%d:software error to_ticks:%d tmr:%p not set ??\n",
			       t_type, to_ticks, tmr);
		}
#endif /* SCTP_DEBUG */
		return (EFAULT);
	}
	if (callout_pending(&tmr->timer)) {
		/*
		 * we do NOT allow you to have it already running.
		 * if it is we leave the current one up unchanged
		 */
		return (EALREADY);
	}
	/* At this point we can proceed */
	if (t_type == SCTP_TIMER_TYPE_SEND) {
		stcb->asoc.num_send_timers_up++;
	}
	tmr->type = t_type;
	tmr->ep = (void *)inp;
	tmr->tcb = (void *)stcb;
	tmr->net = (void *)net;
	callout_reset(&tmr->timer, to_ticks, sctp_timeout_handler, tmr);
	return (0);
}

int
sctp_timer_stop(int t_type, struct sctp_inpcb *inp, struct sctp_tcb *stcb,
		struct sctp_nets *net)
{
	struct sctp_timer *tmr;

	if (inp == NULL)
		return (EFAULT);

	tmr = NULL;
	if(stcb) {
		STCB_TCB_LOCK_ASSERT(stcb);
	}
	switch (t_type) {
	case SCTP_TIMER_TYPE_EARLYFR:
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		tmr = &net->fr_timer;
		break;
	case SCTP_TIMER_TYPE_ITERATOR:
	{
		struct sctp_iterator *it;
		it = (struct sctp_iterator *)inp;
		tmr = &it->tmr;
	}
 	        break;
	case SCTP_TIMER_TYPE_SEND:
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		tmr = &net->rxt_timer;
		break;
	case SCTP_TIMER_TYPE_INIT:
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		tmr = &net->rxt_timer;
		break;
	case SCTP_TIMER_TYPE_RECV:
		if (stcb == NULL) {
			return (EFAULT);
		}
		tmr = &stcb->asoc.dack_timer;
		break;
	case SCTP_TIMER_TYPE_SHUTDOWN:
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		tmr = &net->rxt_timer;
		break;
	case SCTP_TIMER_TYPE_HEARTBEAT:
		if (stcb == NULL) {
			return (EFAULT);
		}
		tmr = &stcb->asoc.hb_timer;
		break;
	case SCTP_TIMER_TYPE_COOKIE:
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		tmr = &net->rxt_timer;
		break;
	case SCTP_TIMER_TYPE_NEWCOOKIE:
		/* nothing needed but the endpoint here */
		tmr = &inp->sctp_ep.signature_change;
		/* We re-use the newcookie timer for
		 * the INP kill timer. We must assure
		 * that we do not kill it by accident.
		 */
		break;
	case SCTP_TIMER_TYPE_INPKILL:
		/*
		 * The inp is setup to die. We re-use the
		 * signature_chage timer since that has
		 * stopped and we are in the GONE state.
		 */
		tmr = &inp->sctp_ep.signature_change;
		break;
	case SCTP_TIMER_TYPE_PATHMTURAISE:
		if (stcb == NULL) {
			return (EFAULT);
		}
		if (net == NULL) {
			return (EFAULT);
		}
		tmr = &net->pmtu_timer;
		break;
	case SCTP_TIMER_TYPE_SHUTDOWNACK:
		if ((stcb == NULL) || (net == NULL)) {
			return (EFAULT);
		}
		tmr = &net->rxt_timer;
		break;
	case SCTP_TIMER_TYPE_SHUTDOWNGUARD:
		if (stcb == NULL) {
			return (EFAULT);
		}
		tmr = &stcb->asoc.shut_guard_timer;
		break;
	case SCTP_TIMER_TYPE_STRRESET:
		if (stcb == NULL) {
			return (EFAULT);
		}
		tmr = &stcb->asoc.strreset_timer;
		break;
	case SCTP_TIMER_TYPE_ASCONF:
		if (stcb == NULL) {
			return (EFAULT);
		}
		tmr = &stcb->asoc.asconf_timer;
		break;
	case SCTP_TIMER_TYPE_AUTOCLOSE:
		if (stcb == NULL) {
			return (EFAULT);
		}
		tmr = &stcb->asoc.autoclose_timer;
		break;
	default:
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_TIMER1) {
			printf("sctp_timer_stop:Unknown timer type %d\n",
			       t_type);
		}
#endif /* SCTP_DEBUG */
		break;
	};
	if (tmr == NULL)
		return (EFAULT);

	if ((tmr->type != t_type) && tmr->type) {
		/*
		 * Ok we have a timer that is under joint use. Cookie timer
		 * per chance with the SEND timer. We therefore are NOT
		 * running the timer that the caller wants stopped.  So just
		 * return.
		 */
		return (0);
	}
	if (t_type == SCTP_TIMER_TYPE_SEND) {
		stcb->asoc.num_send_timers_up--;
		if (stcb->asoc.num_send_timers_up < 0) {
			stcb->asoc.num_send_timers_up = 0;
		}
	}
	callout_stop(&tmr->timer);
	return (0);
}

#ifdef SCTP_USE_ADLER32
static uint32_t
update_adler32(uint32_t adler, uint8_t *buf, int32_t len)
{
	u_int32_t s1 = adler & 0xffff;
	u_int32_t s2 = (adler >> 16) & 0xffff;
	int n;

	for (n = 0; n < len; n++, buf++) {
		/* s1 = (s1 + buf[n]) % BASE */
		/* first we add */
		s1 = (s1 + *buf);
		/*
		 * now if we need to, we do a mod by subtracting. It seems
		 * a bit faster since I really will only ever do one subtract
		 * at the MOST, since buf[n] is a max of 255.
		 */
		if (s1 >= SCTP_ADLER32_BASE) {
			s1 -= SCTP_ADLER32_BASE;
		}
		/* s2 = (s2 + s1) % BASE */
		/* first we add */
		s2 = (s2 + s1);
		/*
		 * again, it is more efficent (it seems) to subtract since
		 * the most s2 will ever be is (BASE-1 + BASE-1) in the worse
		 * case. This would then be (2 * BASE) - 2, which will still
		 * only do one subtract. On Intel this is much better to do
		 * this way and avoid the divide. Have not -pg'd on sparc.
		 */
		if (s2 >= SCTP_ADLER32_BASE) {
			s2 -= SCTP_ADLER32_BASE;
		}
	}
	/* Return the adler32 of the bytes buf[0..len-1] */
	return ((s2 << 16) + s1);
}

#endif


u_int32_t
sctp_calculate_len(struct mbuf *m)
{
	u_int32_t tlen=0;
	struct mbuf *at;
	at = m;
	while (at) {
		tlen += at->m_len;
		at = at->m_next;
	}
	return (tlen);
}

#if defined(SCTP_WITH_NO_CSUM)

uint32_t
sctp_calculate_sum(struct mbuf *m, int32_t *pktlen, uint32_t offset)
{
	/*
	 * given a mbuf chain with a packetheader offset by 'offset'
	 * pointing at a sctphdr (with csum set to 0) go through
	 * the chain of m_next's and calculate the SCTP checksum.
	 * This is currently Adler32 but will change to CRC32x
	 * soon. Also has a side bonus calculate the total length
	 * of the mbuf chain.
	 * Note: if offset is greater than the total mbuf length,
	 * checksum=1, pktlen=0 is returned (ie. no real error code)
	 */
	if (pktlen == NULL)
		return (0);
	*pktlen = sctp_calculate_len(m);
	return (0);
}

#elif defined(SCTP_USE_INCHKSUM)

#include <machine/in_cksum.h>

uint32_t
sctp_calculate_sum(struct mbuf *m, int32_t *pktlen, uint32_t offset)
{
	/*
	 * given a mbuf chain with a packetheader offset by 'offset'
	 * pointing at a sctphdr (with csum set to 0) go through
	 * the chain of m_next's and calculate the SCTP checksum.
	 * This is currently Adler32 but will change to CRC32x
	 * soon. Also has a side bonus calculate the total length
	 * of the mbuf chain.
	 * Note: if offset is greater than the total mbuf length,
	 * checksum=1, pktlen=0 is returned (ie. no real error code)
	 */
	int32_t tlen=0;
	struct mbuf *at;
	uint32_t the_sum, retsum;

	at = m;
	while (at) {
		tlen += at->m_len;
		at = at->m_next;
	}
	the_sum = (uint32_t)(in_cksum_skip(m, tlen, offset));
	if (pktlen != NULL)
		*pktlen = (tlen-offset);
	retsum = htons(the_sum);
	return (the_sum);
}

#else

uint32_t
sctp_calculate_sum(struct mbuf *m, int32_t *pktlen, uint32_t offset)
{
	/*
	 * given a mbuf chain with a packetheader offset by 'offset'
	 * pointing at a sctphdr (with csum set to 0) go through
	 * the chain of m_next's and calculate the SCTP checksum.
	 * This is currently Adler32 but will change to CRC32x
	 * soon. Also has a side bonus calculate the total length
	 * of the mbuf chain.
	 * Note: if offset is greater than the total mbuf length,
	 * checksum=1, pktlen=0 is returned (ie. no real error code)
	 */
	int32_t tlen=0;
#ifdef SCTP_USE_ADLER32
	uint32_t base = 1L;
#else
	uint32_t base = 0xffffffff;
#endif /* SCTP_USE_ADLER32 */
	struct mbuf *at;
	at = m;
	/* find the correct mbuf and offset into mbuf */
	while ((at != NULL) && (offset > (uint32_t)at->m_len)) {
		offset -= at->m_len;	/* update remaining offset left */
		at = at->m_next;
	}

	while (at != NULL) {
#ifdef SCTP_USE_ADLER32
		base = update_adler32(base, at->m_data + offset,
		    at->m_len - offset);
#else
		base = update_crc32(base, at->m_data + offset,
		    at->m_len - offset);
#endif /* SCTP_USE_ADLER32 */
		tlen += at->m_len - offset;
		/* we only offset once into the first mbuf */
		if (offset) {
			offset = 0;
		}
		at = at->m_next;
	}
	if (pktlen != NULL) {
		*pktlen = tlen;
	}
#ifdef SCTP_USE_ADLER32
	/* Adler32 */
	base = htonl(base);
#else
	/* CRC-32c */
	base = sctp_csum_finalize(base);
#endif
	return (base);
}


#endif

void
sctp_mtu_size_reset(struct sctp_inpcb *inp,
		    struct sctp_association *asoc, u_long mtu)
{
	/*
	 * Reset the P-MTU size on this association, this involves changing
	 * the asoc MTU, going through ANY chunk+overhead larger than mtu
	 * to allow the DF flag to be cleared.
	 */
	struct sctp_tmit_chunk *chk;
	struct sctp_stream_out *strm;
	unsigned int eff_mtu, ovh;
	asoc->smallest_mtu = mtu;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
		ovh = SCTP_MIN_OVERHEAD;
	} else {
		ovh = SCTP_MIN_V4_OVERHEAD;
	}
	eff_mtu = mtu - ovh;
	/* Now mark any chunks that need to let IP fragment */
	TAILQ_FOREACH(strm, &asoc->out_wheel, next_spoke) {
		TAILQ_FOREACH(chk, &strm->outqueue, sctp_next) {
			if (chk->send_size > eff_mtu) {
				chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
			}
		}
	}
	TAILQ_FOREACH(chk, &asoc->send_queue, sctp_next) {

		if (chk->send_size > eff_mtu) {
			chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
		}
	}
	TAILQ_FOREACH(chk, &asoc->sent_queue, sctp_next) {
		if (chk->send_size > eff_mtu) {
			chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
		}
	}
}


/*
 * given an association and starting time of the current RTT period
 * return RTO in number of usecs
 * net should point to the current network
 */
u_int32_t
sctp_calculate_rto(struct sctp_tcb *stcb,
		   struct sctp_association *asoc,
		   struct sctp_nets *net,
		   struct timeval *old)
{
	/*
	 * given an association and the starting time of the current RTT
	 * period (in value1/value2) return RTO in number of usecs.
	 */
	int calc_time = 0;
	int o_calctime;
	unsigned int new_rto = 0;
	int first_measure = 0;
	struct timeval now;

	/************************/
	/* 1. calculate new RTT */
	/************************/
	/* get the current time */
	SCTP_GETTIME_TIMEVAL(&now);
	/* compute the RTT value */
	if ((u_long)now.tv_sec > (u_long)old->tv_sec) {
		calc_time = ((u_long)now.tv_sec - (u_long)old->tv_sec) * 1000;
		if ((u_long)now.tv_usec > (u_long)old->tv_usec) {
			calc_time += (((u_long)now.tv_usec -
				       (u_long)old->tv_usec)/1000);
		} else if ((u_long)now.tv_usec < (u_long)old->tv_usec) {
			/* Borrow 1,000ms from current calculation */
			calc_time -= 1000;
			/* Add in the slop over */
			calc_time += ((int)now.tv_usec/1000);
			/* Add in the pre-second ms's */
			calc_time += (((int)1000000 - (int)old->tv_usec)/1000);
		}
	} else if ((u_long)now.tv_sec == (u_long)old->tv_sec) {
		if ((u_long)now.tv_usec > (u_long)old->tv_usec) {
			calc_time = ((u_long)now.tv_usec -
				     (u_long)old->tv_usec)/1000;
		} else if ((u_long)now.tv_usec < (u_long)old->tv_usec) {
			/* impossible .. garbage in nothing out */
			return (((net->lastsa >> 2) + net->lastsv) >> 1);
		} else {
			/* impossible .. garbage in nothing out */
			return (((net->lastsa >> 2) + net->lastsv) >> 1);
		}
	} else {
		/* Clock wrapped? */
		return (((net->lastsa >> 2) + net->lastsv) >> 1);
	}
	/***************************/
	/* 2. update RTTVAR & SRTT */
	/***************************/
#if 0
	/*	if (net->lastsv || net->lastsa) {*/
	/* per Section 5.3.1 C3 in SCTP */
	/*		net->lastsv = (int) 	*//* RTTVAR */
	/*			(((double)(1.0 - 0.25) * (double)net->lastsv) +
				(double)(0.25 * (double)abs(net->lastsa - calc_time)));
				net->lastsa = (int) */	/* SRTT */
	/*(((double)(1.0 - 0.125) * (double)net->lastsa) +
	  (double)(0.125 * (double)calc_time));
	  } else {
	*//* the first RTT calculation, per C2 Section 5.3.1 */
	/*		net->lastsa = calc_time;	*//* SRTT */
	/*		net->lastsv = calc_time / 2;	*//* RTTVAR */
	/*	}*/
	/* if RTTVAR goes to 0 you set to clock grainularity */
	/*	if (net->lastsv == 0) {
		net->lastsv = SCTP_CLOCK_GRANULARITY;
		}
		new_rto = net->lastsa + 4 * net->lastsv;
	*/
#endif
	o_calctime = calc_time;
	/* this is Van Jacobson's integer version */
	if (net->RTO) {
		calc_time -= (net->lastsa >> 3);
		if(net->prev_rtt > o_calctime) {
		  net->rtt_variance = net->prev_rtt - o_calctime;
		  /* decreasing */
		  net->rto_variance_dir = 0;
		} else {
		  /* increasing */
		  net->rtt_variance = o_calctime - net->prev_rtt;
		  net->rto_variance_dir = 1;
		}
#ifdef SCTP_RTTVAR_LOGGING
		rto_logging(net, SCTP_LOG_RTTVAR);
#endif
		net->prev_rtt = o_calctime;
		net->lastsa += calc_time;
		if (calc_time < 0) {
			calc_time = -calc_time;
		}
		calc_time -= (net->lastsv >> 2);
		net->lastsv += calc_time;
		if (net->lastsv == 0) {
			net->lastsv = SCTP_CLOCK_GRANULARITY;
		}
	} else {
		/* First RTO measurment */
		net->lastsa = calc_time;
		net->lastsv = calc_time >> 1;
		first_measure = 1;
		net->rto_variance_dir = 1;
		net->prev_rtt = o_calctime;
		net->rtt_variance = 0;
#ifdef SCTP_RTTVAR_LOGGING
		rto_logging(net, SCTP_LOG_INITIAL_RTT);
#endif
	}
	new_rto = ((net->lastsa >> 2) + net->lastsv) >> 1;
	if ((new_rto > SCTP_SAT_NETWORK_MIN) &&
	    (stcb->asoc.sat_network_lockout == 0)) {
		stcb->asoc.sat_network = 1;
	} else 	if ((!first_measure) && stcb->asoc.sat_network) {
		stcb->asoc.sat_network = 0;
		stcb->asoc.sat_network_lockout = 1;
	}
	/* bound it, per C6/C7 in Section 5.3.1 */
	if (new_rto < stcb->asoc.minrto) {
		new_rto = stcb->asoc.minrto;
	}
	if (new_rto > stcb->asoc.maxrto) {
		new_rto = stcb->asoc.maxrto;
	}
	/* we are now returning the RTT Smoothed */
	return ((u_int32_t)new_rto);
}


/*
 * return a pointer to a contiguous piece of data from the given
 * mbuf chain starting at 'off' for 'len' bytes.  If the desired
 * piece spans more than one mbuf, a copy is made at 'ptr'.
 * caller must ensure that the buffer size is >= 'len'
 * returns NULL if there there isn't 'len' bytes in the chain.
 */
caddr_t
sctp_m_getptr(struct mbuf *m, int off, int len, u_int8_t *in_ptr)
{
	uint32_t count;
	uint8_t *ptr;
	ptr = in_ptr;
	if ((off < 0) || (len <= 0))
		return (NULL);

	/* find the desired start location */
	while ((m != NULL) && (off > 0)) {
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	if (m == NULL)
		return (NULL);

	/* is the current mbuf large enough (eg. contiguous)? */
	if ((m->m_len - off) >= len) {
		return (mtod(m, caddr_t) + off);
	} else {
		/* else, it spans more than one mbuf, so save a temp copy... */
		while ((m != NULL) && (len > 0)) {
			count = min(m->m_len - off, len);
			bcopy(mtod(m, caddr_t) + off, ptr, count);
			len -= count;
			ptr += count;
			off = 0;
			m = m->m_next;
		}
		if ((m == NULL) && (len > 0))
			return (NULL);
		else
			return ((caddr_t)in_ptr);
	}
}


struct sctp_paramhdr *
sctp_get_next_param(struct mbuf *m,
		    int offset,
		    struct sctp_paramhdr *pull,
		    int pull_limit)
{
	/* This just provides a typed signature to Peter's Pull routine */
	return ((struct sctp_paramhdr *)sctp_m_getptr(m, offset, pull_limit,
    	    (u_int8_t *)pull));
}


int
sctp_add_pad_tombuf(struct mbuf *m, int padlen)
{
	/*
	 * add padlen bytes of 0 filled padding to the end of the mbuf.
	 * If padlen is > 3 this routine will fail.
	 */
	u_int8_t *dp;
	int i;
	if (padlen > 3) {
		return (ENOBUFS);
	}
	if (M_TRAILINGSPACE(m)) {
		/*
		 * The easy way.
		 * We hope the majority of the time we hit here :)
		 */
		dp = (u_int8_t *)(mtod(m, caddr_t) + m->m_len);
		m->m_len += padlen;
	} else {
		/* Hard way we must grow the mbuf */
		struct mbuf *tmp;
		MGET(tmp, M_DONTWAIT, MT_DATA);
		if (tmp == NULL) {
			/* Out of space GAK! we are in big trouble. */
			return (ENOSPC);
		}
		/* setup and insert in middle */
		tmp->m_next = m->m_next;
		tmp->m_len = padlen;
		m->m_next = tmp;
		dp = mtod(tmp, u_int8_t *);
	}
	/* zero out the pad */
	for (i=  0; i < padlen; i++) {
		*dp = 0;
		dp++;
	}
	return (0);
}

int
sctp_pad_lastmbuf(struct mbuf *m, int padval)
{
	/* find the last mbuf in chain and pad it */
	struct mbuf *m_at;
	m_at = m;
	while (m_at) {
		if (m_at->m_next == NULL) {
			return (sctp_add_pad_tombuf(m_at, padval));
		}
		m_at = m_at->m_next;
	}
	return (EFAULT);
}

static void
sctp_notify_assoc_change(u_int32_t event, struct sctp_tcb *stcb,
    u_int32_t error, void *data)
{
	struct mbuf *m_notify;
	struct sctp_assoc_change *sac;
	struct sockaddr *to;
	struct sockaddr_in6 sin6, lsa6;

	/*
	 * First if we are are going down dump everything we
	 * can to the socket rcv queue.
	 */
	if ((event == SCTP_SHUTDOWN_COMP) || (event == SCTP_COMM_LOST)) {
		sctp_deliver_data(stcb, &stcb->asoc, NULL, 0);
	}

	/*
	 * For TCP model AND UDP connected sockets we will send
	 * an error up when an ABORT comes in.
	 */
	if (((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
	     (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) &&
	    (event == SCTP_COMM_LOST)) {
		stcb->sctp_socket->so_error = ECONNRESET;
		/* Wake ANY sleepers */
		sowwakeup(stcb->sctp_socket);
		sorwakeup(stcb->sctp_socket);
	}
#if 0
	if ((event == SCTP_COMM_UP) &&
	    (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
 	    (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		 soisconnected(stcb->sctp_socket);
	}
#endif
	if (!(stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_RECVASSOCEVNT)) {
		/* event not enabled */
		return;
	}
	MGETHDR(m_notify, M_DONTWAIT, MT_DATA);
	if (m_notify == NULL)
		/* no space left */
		return;
	m_notify->m_len = 0;

	sac = mtod(m_notify, struct sctp_assoc_change *);
	sac->sac_type = SCTP_ASSOC_CHANGE;
	sac->sac_flags = 0;
	sac->sac_length = sizeof(struct sctp_assoc_change);
	sac->sac_state = event;
	sac->sac_error = error;
	/* XXX verify these stream counts */
	sac->sac_outbound_streams = stcb->asoc.streamoutcnt;
	sac->sac_inbound_streams = stcb->asoc.streamincnt;
	sac->sac_assoc_id = sctp_get_associd(stcb);
	if ((data != NULL) && (event == SCTP_RESTART)) {
	  sac->sac_old_assoc_id = *(sctp_assoc_t *)data;
	} else {
	  sac->sac_old_assoc_id = sctp_get_associd(stcb);
	}
	m_notify->m_flags |= M_EOR | M_NOTIFICATION;
	m_notify->m_pkthdr.len = sizeof(struct sctp_assoc_change);
	m_notify->m_pkthdr.rcvif = 0;
	m_notify->m_len = sizeof(struct sctp_assoc_change);
	m_notify->m_next = NULL;

	/* append to socket */
	to = (struct sockaddr *)&stcb->asoc.primary_destination->ro._l_addr;
	if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) &&
	    to->sa_family == AF_INET) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)to;
		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr.s6_addr16[2] = 0xffff;
		bcopy(&sin->sin_addr, &sin6.sin6_addr.s6_addr16[3],
		    sizeof(sin6.sin6_addr.s6_addr16[3]));
		sin6.sin6_port = sin->sin_port;
		to = (struct sockaddr *)&sin6;
	}
	/* check and strip embedded scope junk */
	to = (struct sockaddr *)sctp_recover_scope((struct sockaddr_in6 *)to,
						   &lsa6);
	/*
	 * We need to always notify comm changes.
	 * if (sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv) < m_notify->m_len) {
	 * 	sctp_m_freem(m_notify);
	 *	return;
	 * }
	*/
	SCTP_TCB_UNLOCK(stcb);
	SCTP_INP_WLOCK(stcb->sctp_ep);
	SCTP_TCB_LOCK(stcb);
	if (!sbappendaddr_nocheck(&stcb->sctp_socket->so_rcv,
	    to, m_notify, NULL, stcb->asoc.my_vtag, stcb->sctp_ep, stcb)) {
		/* not enough room */
		sctp_m_freem(m_notify);
		SCTP_INP_WUNLOCK(stcb->sctp_ep);
		return;
	}
	SCTP_INP_WUNLOCK(stcb->sctp_ep);
	/* Wake up any sleeper */
	sctp_sorwakeup(stcb->sctp_ep, stcb->sctp_socket);
	sctp_sowwakeup(stcb->sctp_ep, stcb->sctp_socket);
}

static void
sctp_notify_peer_addr_change(struct sctp_tcb *stcb, uint32_t state,
    struct sockaddr *sa, uint32_t error)
{
	struct mbuf *m_notify;
	struct sctp_paddr_change *spc;
	struct sockaddr *to;
	struct sockaddr_in6 sin6, lsa6;

	if (!(stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_RECVPADDREVNT))
		/* event not enabled */
		return;

	MGETHDR(m_notify, M_DONTWAIT, MT_DATA);
	if (m_notify == NULL)
		return;
	m_notify->m_len = 0;

	MCLGET(m_notify, M_DONTWAIT);
	if ((m_notify->m_flags & M_EXT) != M_EXT) {
		sctp_m_freem(m_notify);
		return;
	}

	spc = mtod(m_notify, struct sctp_paddr_change *);
	spc->spc_type = SCTP_PEER_ADDR_CHANGE;
	spc->spc_flags = 0;
	spc->spc_length = sizeof(struct sctp_paddr_change);
	if (sa->sa_family == AF_INET) {
		memcpy(&spc->spc_aaddr, sa, sizeof(struct sockaddr_in));
	} else {
		memcpy(&spc->spc_aaddr, sa, sizeof(struct sockaddr_in6));
	}
	spc->spc_state = state;
	spc->spc_error = error;
	spc->spc_assoc_id = sctp_get_associd(stcb);

	m_notify->m_flags |= M_EOR | M_NOTIFICATION;
	m_notify->m_pkthdr.len = sizeof(struct sctp_paddr_change);
	m_notify->m_pkthdr.rcvif = 0;
	m_notify->m_len = sizeof(struct sctp_paddr_change);
	m_notify->m_next = NULL;

	to = (struct sockaddr *)(struct sockaddr *)
	    &stcb->asoc.primary_destination->ro._l_addr;
	if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) &&
	    to->sa_family == AF_INET) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)to;
		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr.s6_addr16[2] = 0xffff;
		bcopy(&sin->sin_addr, &sin6.sin6_addr.s6_addr16[3],
		    sizeof(sin6.sin6_addr.s6_addr16[3]));
		sin6.sin6_port = sin->sin_port;
		to = (struct sockaddr *)&sin6;
	}
	/* check and strip embedded scope junk */
	to = (struct sockaddr *)sctp_recover_scope((struct sockaddr_in6 *)to,
	    &lsa6);

	if (sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv) < m_notify->m_len) {
		sctp_m_freem(m_notify);
		return;
	}
	/* append to socket */
	SCTP_TCB_UNLOCK(stcb);
	SCTP_INP_WLOCK(stcb->sctp_ep);
	SCTP_TCB_LOCK(stcb);
	if (!sbappendaddr_nocheck(&stcb->sctp_socket->so_rcv, to,
	    m_notify, NULL, stcb->asoc.my_vtag, stcb->sctp_ep, stcb)) {
		/* not enough room */
		sctp_m_freem(m_notify);
		SCTP_INP_WUNLOCK(stcb->sctp_ep);
		return;
	}
	SCTP_INP_WUNLOCK(stcb->sctp_ep);
	sctp_sorwakeup(stcb->sctp_ep, stcb->sctp_socket);
}


static void
sctp_notify_send_failed(struct sctp_tcb *stcb, u_int32_t error,
			struct sctp_tmit_chunk *chk)
{
	struct mbuf *m_notify;
	struct sctp_send_failed *ssf;
	struct sockaddr_in6 sin6, lsa6;
	struct sockaddr *to;
	int length;

	if (!(stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_RECVSENDFAILEVNT))
		/* event not enabled */
		return;

	length = sizeof(struct sctp_send_failed) + chk->send_size;
	MGETHDR(m_notify, M_DONTWAIT, MT_DATA);
	if (m_notify == NULL)
		/* no space left */
		return;
	m_notify->m_len = 0;
	ssf = mtod(m_notify, struct sctp_send_failed *);
	ssf->ssf_type = SCTP_SEND_FAILED;
	if (error == SCTP_NOTIFY_DATAGRAM_UNSENT)
		ssf->ssf_flags = SCTP_DATA_UNSENT;
	else
		ssf->ssf_flags = SCTP_DATA_SENT;
	ssf->ssf_length = length;
	ssf->ssf_error = error;
	/* not exactly what the user sent in, but should be close :) */
	ssf->ssf_info.sinfo_stream = chk->rec.data.stream_number;
	ssf->ssf_info.sinfo_ssn = chk->rec.data.stream_seq;
	ssf->ssf_info.sinfo_flags = chk->rec.data.rcv_flags;
	ssf->ssf_info.sinfo_ppid = chk->rec.data.payloadtype;
	ssf->ssf_info.sinfo_context = chk->rec.data.context;
	ssf->ssf_info.sinfo_assoc_id = sctp_get_associd(stcb);
	ssf->ssf_assoc_id = sctp_get_associd(stcb);
	m_notify->m_next = chk->data;
	if (m_notify->m_next == NULL)
		m_notify->m_flags |= M_EOR | M_NOTIFICATION;
	else {
		struct mbuf *m;
		m_notify->m_flags |= M_NOTIFICATION;
		m = m_notify;
		while (m->m_next != NULL)
			m = m->m_next;
		m->m_flags |= M_EOR;
	}
	m_notify->m_pkthdr.len = length;
	m_notify->m_pkthdr.rcvif = 0;
	m_notify->m_len = sizeof(struct sctp_send_failed);

	/* Steal off the mbuf */
	chk->data = NULL;
	to = (struct sockaddr *)(struct sockaddr *)&stcb->asoc.primary_destination->ro._l_addr;
	if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) &&
	    to->sa_family == AF_INET) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)to;
		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr.s6_addr16[2] = 0xffff;
		bcopy(&sin->sin_addr, &sin6.sin6_addr.s6_addr16[3],
		    sizeof(sin6.sin6_addr.s6_addr16[3]));
		sin6.sin6_port = sin->sin_port;
		to = (struct sockaddr *)&sin6;
	}
	/* check and strip embedded scope junk */
	to = (struct sockaddr *)sctp_recover_scope((struct sockaddr_in6 *)to,
						   &lsa6);

	/* For this case, we check the actual socket buffer, since the
	 * assoc is going away we don't want to overfill the socket
	 * buffer for a non-reader
	 */
	if (sctp_sbspace_failedmsgs(&stcb->sctp_socket->so_rcv) < m_notify->m_len) {
		sctp_m_freem(m_notify);
		return;
	}
	/* append to socket */
	SCTP_TCB_UNLOCK(stcb);
	SCTP_INP_WLOCK(stcb->sctp_ep);
	SCTP_TCB_LOCK(stcb);
	if (!sbappendaddr_nocheck(&stcb->sctp_socket->so_rcv, to,
	    m_notify, NULL, stcb->asoc.my_vtag, stcb->sctp_ep, stcb)) {
		/* not enough room */
		sctp_m_freem(m_notify);
		SCTP_INP_WUNLOCK(stcb->sctp_ep);
		return;
	}
	SCTP_INP_WUNLOCK(stcb->sctp_ep);
	sctp_sorwakeup(stcb->sctp_ep, stcb->sctp_socket);
}

static void
sctp_notify_adaption_layer(struct sctp_tcb *stcb,
			   u_int32_t error)
{
	struct mbuf *m_notify;
	struct sctp_adaption_event *sai;
	struct sockaddr_in6 sin6, lsa6;
	struct sockaddr *to;

	if (!(stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_ADAPTIONEVNT))
		/* event not enabled */
		return;

	MGETHDR(m_notify, M_DONTWAIT, MT_DATA);
	if (m_notify == NULL)
		/* no space left */
		return;
	m_notify->m_len = 0;
	sai = mtod(m_notify, struct sctp_adaption_event *);
	sai->sai_type = SCTP_ADAPTION_INDICATION;
	sai->sai_flags = 0;
	sai->sai_length = sizeof(struct sctp_adaption_event);
	sai->sai_adaption_ind = error;
	sai->sai_assoc_id = sctp_get_associd(stcb);

	m_notify->m_flags |= M_EOR | M_NOTIFICATION;
	m_notify->m_pkthdr.len = sizeof(struct sctp_adaption_event);
	m_notify->m_pkthdr.rcvif = 0;
	m_notify->m_len = sizeof(struct sctp_adaption_event);
	m_notify->m_next = NULL;

	to = (struct sockaddr *)(struct sockaddr *)&stcb->asoc.primary_destination->ro._l_addr;
	if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) &&
	    (to->sa_family == AF_INET)) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)to;
		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr.s6_addr16[2] = 0xffff;
		bcopy(&sin->sin_addr, &sin6.sin6_addr.s6_addr16[3],
		    sizeof(sin6.sin6_addr.s6_addr16[3]));
		sin6.sin6_port = sin->sin_port;
		to = (struct sockaddr *)&sin6;
	}
	/* check and strip embedded scope junk */
	to = (struct sockaddr *)sctp_recover_scope((struct sockaddr_in6 *)to,
						   &lsa6);
	if (sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv) < m_notify->m_len) {
		sctp_m_freem(m_notify);
		return;
	}
	/* append to socket */
	SCTP_TCB_UNLOCK(stcb);
	SCTP_INP_WLOCK(stcb->sctp_ep);
	SCTP_TCB_LOCK(stcb);
	if (!sbappendaddr_nocheck(&stcb->sctp_socket->so_rcv, to,
	    m_notify, NULL, stcb->asoc.my_vtag, stcb->sctp_ep, stcb)) {
		/* not enough room */
		sctp_m_freem(m_notify);
		SCTP_INP_WUNLOCK(stcb->sctp_ep);
		return;
	}
	SCTP_INP_WUNLOCK(stcb->sctp_ep);
	sctp_sorwakeup(stcb->sctp_ep, stcb->sctp_socket);
}

static void
sctp_notify_partial_delivery_indication(struct sctp_tcb *stcb,
					u_int32_t error)
{
	struct mbuf *m_notify;
	struct sctp_pdapi_event *pdapi;
	struct sockaddr_in6 sin6, lsa6;
	struct sockaddr *to;

	if (!(stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_PDAPIEVNT))
		/* event not enabled */
		return;

	MGETHDR(m_notify, M_DONTWAIT, MT_DATA);
	if (m_notify == NULL)
		/* no space left */
		return;
	m_notify->m_len = 0;
	pdapi = mtod(m_notify, struct sctp_pdapi_event *);
	pdapi->pdapi_type = SCTP_PARTIAL_DELIVERY_EVENT;
	pdapi->pdapi_flags = 0;
	pdapi->pdapi_length = sizeof(struct sctp_pdapi_event);
	pdapi->pdapi_indication = error;
	pdapi->pdapi_assoc_id = sctp_get_associd(stcb);

	m_notify->m_flags |= M_EOR | M_NOTIFICATION;
	m_notify->m_pkthdr.len = sizeof(struct sctp_pdapi_event);
	m_notify->m_pkthdr.rcvif = 0;
	m_notify->m_len = sizeof(struct sctp_pdapi_event);
	m_notify->m_next = NULL;

	to = (struct sockaddr *)(struct sockaddr *)&stcb->asoc.primary_destination->ro._l_addr;
	if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) &&
	    (to->sa_family == AF_INET)) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)to;
		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr.s6_addr16[2] = 0xffff;
		bcopy(&sin->sin_addr, &sin6.sin6_addr.s6_addr16[3],
		    sizeof(sin6.sin6_addr.s6_addr16[3]));
		sin6.sin6_port = sin->sin_port;
		to = (struct sockaddr *)&sin6;
	}
	/* check and strip embedded scope junk */
	to = (struct sockaddr *)sctp_recover_scope((struct sockaddr_in6 *)to,
						   &lsa6);
	if (sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv) < m_notify->m_len) {
		sctp_m_freem(m_notify);
		return;
	}
	/* append to socket */
	SCTP_TCB_UNLOCK(stcb);
	SCTP_INP_WLOCK(stcb->sctp_ep);
	SCTP_TCB_LOCK(stcb);
	if (!sbappendaddr_nocheck(&stcb->sctp_socket->so_rcv, to,
	    m_notify, NULL, stcb->asoc.my_vtag, stcb->sctp_ep, stcb)) {
		/* not enough room */
		sctp_m_freem(m_notify);
		SCTP_INP_WUNLOCK(stcb->sctp_ep);
		return;
	}
	SCTP_INP_WUNLOCK(stcb->sctp_ep);
	sctp_sorwakeup(stcb->sctp_ep, stcb->sctp_socket);
}

static void
sctp_notify_shutdown_event(struct sctp_tcb *stcb)
{
	struct mbuf *m_notify;
	struct sctp_shutdown_event *sse;
	struct sockaddr_in6 sin6, lsa6;
	struct sockaddr *to;

	/*
	 * For TCP model AND UDP connected sockets we will send
	 * an error up when an SHUTDOWN completes
	 */
	if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
	    (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {
		/* mark socket closed for read/write and wakeup! */
		socantrcvmore(stcb->sctp_socket);
		socantsendmore(stcb->sctp_socket);
	}

	if (!(stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT))
		/* event not enabled */
		return;

	MGETHDR(m_notify, M_DONTWAIT, MT_DATA);
	if (m_notify == NULL)
		/* no space left */
		return;
	m_notify->m_len = 0;
	sse = mtod(m_notify, struct sctp_shutdown_event *);
	sse->sse_type = SCTP_SHUTDOWN_EVENT;
	sse->sse_flags = 0;
	sse->sse_length = sizeof(struct sctp_shutdown_event);
	sse->sse_assoc_id = sctp_get_associd(stcb);

	m_notify->m_flags |= M_EOR | M_NOTIFICATION;
	m_notify->m_pkthdr.len = sizeof(struct sctp_shutdown_event);
	m_notify->m_pkthdr.rcvif = 0;
	m_notify->m_len = sizeof(struct sctp_shutdown_event);
	m_notify->m_next = NULL;

	to = (struct sockaddr *)(struct sockaddr *)&stcb->asoc.primary_destination->ro._l_addr;
	if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) &&
	    to->sa_family == AF_INET) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)to;
		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr.s6_addr16[2] = 0xffff;
		bcopy(&sin->sin_addr, &sin6.sin6_addr.s6_addr16[3],
		    sizeof(sin6.sin6_addr.s6_addr16[3]));
		sin6.sin6_port = sin->sin_port;
		to = (struct sockaddr *)&sin6;
	}
	/* check and strip embedded scope junk */
	to = (struct sockaddr *)sctp_recover_scope((struct sockaddr_in6 *)to,
	    &lsa6);
	if (sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv) < m_notify->m_len) {
		sctp_m_freem(m_notify);
		return;
	}
	/* append to socket */
	SCTP_TCB_UNLOCK(stcb);
	SCTP_INP_WLOCK(stcb->sctp_ep);
	SCTP_TCB_LOCK(stcb);
	if (!sbappendaddr_nocheck(&stcb->sctp_socket->so_rcv, to,
	    m_notify, NULL, stcb->asoc.my_vtag, stcb->sctp_ep, stcb)) {
		/* not enough room */
		sctp_m_freem(m_notify);
		SCTP_INP_WUNLOCK(stcb->sctp_ep);
		return;
	}
	SCTP_INP_WUNLOCK(stcb->sctp_ep);
	sctp_sorwakeup(stcb->sctp_ep, stcb->sctp_socket);
}

static void
sctp_notify_stream_reset(struct sctp_tcb *stcb,
    int number_entries, uint16_t *list, int flag)
{
	struct mbuf *m_notify;
	struct sctp_stream_reset_event *strreset;
	struct sockaddr_in6 sin6, lsa6;
	struct sockaddr *to;
	int len;

	if (!(stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_STREAM_RESETEVNT))
		/* event not enabled */
		return;

	MGETHDR(m_notify, M_DONTWAIT, MT_DATA);
	if (m_notify == NULL)
		/* no space left */
		return;
	m_notify->m_len = 0;
	len = sizeof(struct sctp_stream_reset_event) + (number_entries * sizeof(uint16_t));
	if (len > M_TRAILINGSPACE(m_notify)) {
		MCLGET(m_notify, M_DONTWAIT);
	}
	if (m_notify == NULL)
		/* no clusters */
		return;

	if (len > M_TRAILINGSPACE(m_notify)) {
		/* never enough room */
		m_freem(m_notify);
		return;
	}
	strreset = mtod(m_notify, struct sctp_stream_reset_event *);
	strreset->strreset_type = SCTP_STREAM_RESET_EVENT;
	if (number_entries == 0) {
		strreset->strreset_flags = flag | SCTP_STRRESET_ALL_STREAMS;
	} else {
		strreset->strreset_flags = flag | SCTP_STRRESET_STREAM_LIST;
	}
	strreset->strreset_length = len;
	strreset->strreset_assoc_id = sctp_get_associd(stcb);
	if (number_entries) {
		int i;
		for (i=0; i<number_entries; i++) {
			strreset->strreset_list[i] = list[i];
		}
	}
	m_notify->m_flags |= M_EOR | M_NOTIFICATION;
	m_notify->m_pkthdr.len = len;
	m_notify->m_pkthdr.rcvif = 0;
	m_notify->m_len = len;
	m_notify->m_next = NULL;
	if (sctp_sbspace(&stcb->asoc, &stcb->sctp_socket->so_rcv) < m_notify->m_len) {
		/* no space */
		sctp_m_freem(m_notify);
		return;
	}
	to = (struct sockaddr *)(struct sockaddr *)&stcb->asoc.primary_destination->ro._l_addr;
	if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) &&
	    to->sa_family == AF_INET) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)to;
		bzero(&sin6, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_addr.s6_addr16[2] = 0xffff;
		bcopy(&sin->sin_addr, &sin6.sin6_addr.s6_addr16[3],
		    sizeof(sin6.sin6_addr.s6_addr16[3]));
		sin6.sin6_port = sin->sin_port;
		to = (struct sockaddr *)&sin6;
	}
	/* check and strip embedded scope junk */
	to = (struct sockaddr *)sctp_recover_scope((struct sockaddr_in6 *)to,
	    &lsa6);
	/* append to socket */
	SCTP_TCB_UNLOCK(stcb);
	SCTP_INP_WLOCK(stcb->sctp_ep);
	SCTP_TCB_LOCK(stcb);
	if (!sbappendaddr_nocheck(&stcb->sctp_socket->so_rcv, to,
	    m_notify, NULL, stcb->asoc.my_vtag, stcb->sctp_ep, stcb)) {
		/* not enough room */
		sctp_m_freem(m_notify);
		SCTP_INP_WUNLOCK(stcb->sctp_ep);
		return;
	}
	SCTP_INP_WUNLOCK(stcb->sctp_ep);
	sctp_sorwakeup(stcb->sctp_ep, stcb->sctp_socket);
}


void
sctp_ulp_notify(u_int32_t notification, struct sctp_tcb *stcb,
		u_int32_t error, void *data)
{
	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
		/* No notifications up when we are in a no socket state */
		return;
	}
	if (stcb->asoc.state & SCTP_STATE_CLOSED_SOCKET) {
		/* Can't send up to a closed socket any notifications */
		return;
	}
	switch (notification) {
	case SCTP_NOTIFY_ASSOC_UP:
		sctp_notify_assoc_change(SCTP_COMM_UP, stcb, error, NULL);
		break;
	case SCTP_NOTIFY_ASSOC_DOWN:
		sctp_notify_assoc_change(SCTP_SHUTDOWN_COMP, stcb, error, NULL);
		break;
	case SCTP_NOTIFY_INTERFACE_DOWN:
	{
		struct sctp_nets *net;
		net = (struct sctp_nets *)data;
		sctp_notify_peer_addr_change(stcb, SCTP_ADDR_UNREACHABLE,
		    (struct sockaddr *)&net->ro._l_addr, error);
		break;
	}
	case SCTP_NOTIFY_INTERFACE_UP:
	{
		struct sctp_nets *net;
		net = (struct sctp_nets *)data;
		sctp_notify_peer_addr_change(stcb, SCTP_ADDR_AVAILABLE,
		    (struct sockaddr *)&net->ro._l_addr, error);
		break;
	}
	case SCTP_NOTIFY_INTERFACE_CONFIRMED:
	{
		struct sctp_nets *net;
		net = (struct sctp_nets *)data;
		sctp_notify_peer_addr_change(stcb, SCTP_ADDR_CONFIRMED,
		    (struct sockaddr *)&net->ro._l_addr, error);
		break;
	}
	case SCTP_NOTIFY_DG_FAIL:
		sctp_notify_send_failed(stcb, error,
		    (struct sctp_tmit_chunk *)data);
		break;
	case SCTP_NOTIFY_ADAPTION_INDICATION:
		/* Here the error is the adaption indication */
		sctp_notify_adaption_layer(stcb, error);
		break;
	case SCTP_NOTIFY_PARTIAL_DELVIERY_INDICATION:
		sctp_notify_partial_delivery_indication(stcb, error);
		break;
	case SCTP_NOTIFY_STRDATA_ERR:
		break;
	case SCTP_NOTIFY_ASSOC_ABORTED:
		sctp_notify_assoc_change(SCTP_COMM_LOST, stcb, error, NULL);
		break;
	case SCTP_NOTIFY_PEER_OPENED_STREAM:
		break;
	case SCTP_NOTIFY_STREAM_OPENED_OK:
		break;
	case SCTP_NOTIFY_ASSOC_RESTART:
		sctp_notify_assoc_change(SCTP_RESTART, stcb, error, data);
		break;
	case SCTP_NOTIFY_HB_RESP:
		break;
	case SCTP_NOTIFY_STR_RESET_SEND:
		sctp_notify_stream_reset(stcb, error, ((uint16_t *)data), SCTP_STRRESET_OUTBOUND_STR);
		break;
	case SCTP_NOTIFY_STR_RESET_RECV:
		sctp_notify_stream_reset(stcb, error, ((uint16_t *)data), SCTP_STRRESET_INBOUND_STR);
		break;
	case SCTP_NOTIFY_ASCONF_ADD_IP:
		sctp_notify_peer_addr_change(stcb, SCTP_ADDR_ADDED, data,
		    error);
		break;
	case SCTP_NOTIFY_ASCONF_DELETE_IP:
		sctp_notify_peer_addr_change(stcb, SCTP_ADDR_REMOVED, data,
		    error);
		break;
	case SCTP_NOTIFY_ASCONF_SET_PRIMARY:
		sctp_notify_peer_addr_change(stcb, SCTP_ADDR_MADE_PRIM, data,
		    error);
		break;
	case SCTP_NOTIFY_ASCONF_SUCCESS:
		break;
	case SCTP_NOTIFY_ASCONF_FAILED:
		break;
	case SCTP_NOTIFY_PEER_SHUTDOWN:
		sctp_notify_shutdown_event(stcb);
		break;
	default:
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_UTIL1) {
			printf("NOTIFY: unknown notification %xh (%u)\n",
			    notification, notification);
		}
#endif /* SCTP_DEBUG */
		break;
	} /* end switch */
}

void
sctp_report_all_outbound(struct sctp_tcb *stcb)
{
	struct sctp_association *asoc;
	struct sctp_stream_out *outs;
	struct sctp_tmit_chunk *chk;

	asoc = &stcb->asoc;

	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
		return;
	}
	/* now through all the gunk freeing chunks */

	TAILQ_FOREACH(outs, &asoc->out_wheel, next_spoke) {
		/* now clean up any chunks here */
		chk = TAILQ_FIRST(&outs->outqueue);
		while (chk) {
			stcb->asoc.stream_queue_cnt--;
			TAILQ_REMOVE(&outs->outqueue, chk, sctp_next);
			sctp_ulp_notify(SCTP_NOTIFY_DG_FAIL, stcb,
			    SCTP_NOTIFY_DATAGRAM_UNSENT, chk);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			if (chk->whoTo)
				sctp_free_remote_addr(chk->whoTo);
			chk->whoTo = NULL;
			chk->asoc = NULL;
			/* Free the chunk */
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			SCTP_DECR_CHK_COUNT();
			chk = TAILQ_FIRST(&outs->outqueue);
		}
	}
	/* pending send queue SHOULD be empty */
	if (!TAILQ_EMPTY(&asoc->send_queue)) {
		chk = TAILQ_FIRST(&asoc->send_queue);
		while (chk) {
			TAILQ_REMOVE(&asoc->send_queue, chk, sctp_next);
			sctp_ulp_notify(SCTP_NOTIFY_DG_FAIL, stcb, SCTP_NOTIFY_DATAGRAM_UNSENT, chk);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			if (chk->whoTo)
				sctp_free_remote_addr(chk->whoTo);
			chk->whoTo = NULL;
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			SCTP_DECR_CHK_COUNT();
			chk = TAILQ_FIRST(&asoc->send_queue);
		}
	}
	/* sent queue SHOULD be empty */
	if (!TAILQ_EMPTY(&asoc->sent_queue)) {
		chk = TAILQ_FIRST(&asoc->sent_queue);
		while (chk) {
			TAILQ_REMOVE(&asoc->sent_queue, chk, sctp_next);
			sctp_ulp_notify(SCTP_NOTIFY_DG_FAIL, stcb,
			    SCTP_NOTIFY_DATAGRAM_SENT, chk);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			if (chk->whoTo)
				sctp_free_remote_addr(chk->whoTo);
			chk->whoTo = NULL;
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			SCTP_DECR_CHK_COUNT();
			chk = TAILQ_FIRST(&asoc->sent_queue);
		}
	}
}

void
sctp_abort_notification(struct sctp_tcb *stcb, int error)
{

	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
		return;
	}
	/* Tell them we lost the asoc */
	sctp_report_all_outbound(stcb);
	sctp_ulp_notify(SCTP_NOTIFY_ASSOC_ABORTED, stcb, error, NULL);
}

void
sctp_abort_association(struct sctp_inpcb *inp, struct sctp_tcb *stcb,
    struct mbuf *m, int iphlen, struct sctphdr *sh, struct mbuf *op_err)
{
	u_int32_t vtag;

	vtag = 0;
	if (stcb != NULL) {
		/* We have a TCB to abort, send notification too */
		vtag = stcb->asoc.peer_vtag;
		sctp_abort_notification(stcb, 0);
	}
	sctp_send_abort(m, iphlen, sh, vtag, op_err);
	if (stcb != NULL) {
		/* Ok, now lets free it */
		sctp_free_assoc(inp, stcb);
	} else {
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
			if (LIST_FIRST(&inp->sctp_asoc_list) == NULL) {
				sctp_inpcb_free(inp, 1);
			}
		}
	}
}

void
sctp_abort_an_association(struct sctp_inpcb *inp, struct sctp_tcb *stcb,
    int error, struct mbuf *op_err)
{
	u_int32_t vtag;

	if (stcb == NULL) {
		/* Got to have a TCB */
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
			if (LIST_FIRST(&inp->sctp_asoc_list) == NULL) {
				sctp_inpcb_free(inp, 1);
			}
		}
		return;
	}
	vtag = stcb->asoc.peer_vtag;
	/* notify the ulp */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) == 0)
		sctp_abort_notification(stcb, error);
	/* notify the peer */
	sctp_send_abort_tcb(stcb, op_err);
	/* now free the asoc */
	sctp_free_assoc(inp, stcb);
}

void
sctp_handle_ootb(struct mbuf *m, int iphlen, int offset, struct sctphdr *sh,
    struct sctp_inpcb *inp, struct mbuf *op_err)
{
	struct sctp_chunkhdr *ch, chunk_buf;
	unsigned int chk_length;

	/* Generate a TO address for future reference */
	if (inp && (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE)) {
		if (LIST_FIRST(&inp->sctp_asoc_list) == NULL) {
			sctp_inpcb_free(inp, 1);
		}
	}
	ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, offset,
	    sizeof(*ch), (u_int8_t *)&chunk_buf);
	while (ch != NULL) {
		chk_length = ntohs(ch->chunk_length);
		if (chk_length < sizeof(*ch)) {
			/* break to abort land */
			break;
		}
		switch (ch->chunk_type) {
		case SCTP_PACKET_DROPPED:
			/* we don't respond to pkt-dropped */
			return;
		case SCTP_ABORT_ASSOCIATION:
			/* we don't respond with an ABORT to an ABORT */
			return;
		case SCTP_SHUTDOWN_COMPLETE:
			/*
			 * we ignore it since we are not waiting for it
			 * and peer is gone
			 */
			return;
		case SCTP_SHUTDOWN_ACK:
			sctp_send_shutdown_complete2(m, iphlen, sh);
			return;
		default:
			break;
		}
		offset += SCTP_SIZE32(chk_length);
		ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, offset,
		    sizeof(*ch), (u_int8_t *)&chunk_buf);
	}
	sctp_send_abort(m, iphlen, sh, 0, op_err);
}

/*
 * check the inbound datagram to make sure there is not an abort
 * inside it, if there is return 1, else return 0.
 */
int
sctp_is_there_an_abort_here(struct mbuf *m, int iphlen, int *vtagfill)
{
	struct sctp_chunkhdr *ch;
	struct sctp_init_chunk *init_chk, chunk_buf;
	int offset;
	unsigned int chk_length;

	offset = iphlen + sizeof(struct sctphdr);
	ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, offset, sizeof(*ch),
	    (u_int8_t *)&chunk_buf);
	while (ch != NULL) {
		chk_length = ntohs(ch->chunk_length);
		if (chk_length < sizeof(*ch)) {
			/* packet is probably corrupt */
			break;
		}
		/* we seem to be ok, is it an abort? */
		if (ch->chunk_type == SCTP_ABORT_ASSOCIATION) {
			/* yep, tell them */
			return (1);
		}
		if (ch->chunk_type == SCTP_INITIATION) {
			/* need to update the Vtag */
			init_chk = (struct sctp_init_chunk *)sctp_m_getptr(m,
			    offset, sizeof(*init_chk), (u_int8_t *)&chunk_buf);
			if (init_chk != NULL) {
				*vtagfill = ntohl(init_chk->init.initiate_tag);
			}
		}
		/* Nope, move to the next chunk */
		offset += SCTP_SIZE32(chk_length);
		ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, offset,
		    sizeof(*ch), (u_int8_t *)&chunk_buf);
	}
	return (0);
}

/*
 * currently (2/02), ifa_addr embeds scope_id's and don't
 * have sin6_scope_id set (i.e. it's 0)
 * so, create this function to compare link local scopes
 */
uint32_t
sctp_is_same_scope(struct sockaddr_in6 *addr1, struct sockaddr_in6 *addr2)
{
	struct sockaddr_in6 a, b;

	/* save copies */
	a = *addr1;
	b = *addr2;

	if (a.sin6_scope_id == 0)
		if (in6_recoverscope(&a, &a.sin6_addr, NULL)) {
			/* can't get scope, so can't match */
			return (0);
		}
	if (b.sin6_scope_id == 0)
		if (in6_recoverscope(&b, &b.sin6_addr, NULL)) {
			/* can't get scope, so can't match */
			return (0);
		}
	if (a.sin6_scope_id != b.sin6_scope_id)
		return (0);

	return (1);
}

/*
 * returns a sockaddr_in6 with embedded scope recovered and removed
 */
struct sockaddr_in6 *
sctp_recover_scope(struct sockaddr_in6 *addr, struct sockaddr_in6 *store)
{

	/* check and strip embedded scope junk */
	if (addr->sin6_family == AF_INET6) {
		if (IN6_IS_SCOPE_LINKLOCAL(&addr->sin6_addr)) {
			if (addr->sin6_scope_id == 0) {
				*store = *addr;
				if (!in6_recoverscope(store, &store->sin6_addr,
				    NULL)) {
					/* use the recovered scope */
					addr = store;
				}
				/* else, return the original "to" addr */
			}
		}
	}
	return (addr);
}

/*
 * are the two addresses the same?  currently a "scopeless" check
 * returns: 1 if same, 0 if not
 */
int
sctp_cmpaddr(struct sockaddr *sa1, struct sockaddr *sa2)
{

	/* must be valid */
	if (sa1 == NULL || sa2 == NULL)
		return (0);

	/* must be the same family */
	if (sa1->sa_family != sa2->sa_family)
		return (0);

	if (sa1->sa_family == AF_INET6) {
		/* IPv6 addresses */
		struct sockaddr_in6 *sin6_1, *sin6_2;

		sin6_1 = (struct sockaddr_in6 *)sa1;
		sin6_2 = (struct sockaddr_in6 *)sa2;
		return (SCTP6_ARE_ADDR_EQUAL(&sin6_1->sin6_addr,
		    &sin6_2->sin6_addr));
	} else if (sa1->sa_family == AF_INET) {
		/* IPv4 addresses */
		struct sockaddr_in *sin_1, *sin_2;

		sin_1 = (struct sockaddr_in *)sa1;
		sin_2 = (struct sockaddr_in *)sa2;
		return (sin_1->sin_addr.s_addr == sin_2->sin_addr.s_addr);
	} else {
		/* we don't do these... */
		return (0);
	}
}

void
sctp_print_address(struct sockaddr *sa)
{

	if (sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6;
		sin6 = (struct sockaddr_in6 *)sa;
		printf("IPv6 address: %s:%d scope:%u\n",
		    ip6_sprintf(&sin6->sin6_addr), ntohs(sin6->sin6_port),
		    sin6->sin6_scope_id);
	} else if (sa->sa_family == AF_INET) {
		struct sockaddr_in *sin;
		sin = (struct sockaddr_in *)sa;
		printf("IPv4 address: %s:%d\n", inet_ntoa(sin->sin_addr),
		    ntohs(sin->sin_port));
	} else {
		printf("?\n");
	}
}

void
sctp_print_address_pkt(struct ip *iph, struct sctphdr *sh)
{
	if (iph->ip_v == IPVERSION) {
		struct sockaddr_in lsa, fsa;

		bzero(&lsa, sizeof(lsa));
		lsa.sin_len = sizeof(lsa);
		lsa.sin_family = AF_INET;
		lsa.sin_addr = iph->ip_src;
		lsa.sin_port = sh->src_port;
		bzero(&fsa, sizeof(fsa));
		fsa.sin_len = sizeof(fsa);
		fsa.sin_family = AF_INET;
		fsa.sin_addr = iph->ip_dst;
		fsa.sin_port = sh->dest_port;
		printf("src: ");
		sctp_print_address((struct sockaddr *)&lsa);
		printf("dest: ");
		sctp_print_address((struct sockaddr *)&fsa);
	} else if (iph->ip_v == (IPV6_VERSION >> 4)) {
		struct ip6_hdr *ip6;
		struct sockaddr_in6 lsa6, fsa6;

		ip6 = (struct ip6_hdr *)iph;
		bzero(&lsa6, sizeof(lsa6));
		lsa6.sin6_len = sizeof(lsa6);
		lsa6.sin6_family = AF_INET6;
		lsa6.sin6_addr = ip6->ip6_src;
		lsa6.sin6_port = sh->src_port;
		bzero(&fsa6, sizeof(fsa6));
		fsa6.sin6_len = sizeof(fsa6);
		fsa6.sin6_family = AF_INET6;
		fsa6.sin6_addr = ip6->ip6_dst;
		fsa6.sin6_port = sh->dest_port;
		printf("src: ");
		sctp_print_address((struct sockaddr *)&lsa6);
		printf("dest: ");
		sctp_print_address((struct sockaddr *)&fsa6);
	}
}

#if defined(__FreeBSD__) || defined(__APPLE__)

/* cloned from uipc_socket.c */

#define SCTP_SBLINKRECORD(sb, m0) do {					\
	if ((sb)->sb_lastrecord != NULL)				\
		(sb)->sb_lastrecord->m_nextpkt = (m0);			\
	else								\
		(sb)->sb_mb = (m0);					\
	(sb)->sb_lastrecord = (m0);					\
} while (/*CONSTCOND*/0)
#endif


int
sbappendaddr_nocheck(sb, asa, m0, control, tag, inp, stcb)
	struct sockbuf *sb;
	struct sockaddr *asa;
	struct mbuf *m0, *control;
	u_int32_t tag;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
{
#ifdef __NetBSD__
	struct mbuf *m, *n;

	if (m0 && (m0->m_flags & M_PKTHDR) == 0)
		panic("sbappendaddr_nocheck");

	m0->m_pkthdr.csum_data = (int)tag;

	for (n = control; n; n = n->m_next) {
		if (n->m_next == 0)	/* keep pointer to last control buf */
			break;
	}
	MGETHDR(m, M_DONTWAIT, MT_SONAME);
	if (m == 0)
		return (0);
	m->m_len = 0;
	if (asa->sa_len > MHLEN) {
		MEXTMALLOC(m, asa->sa_len, M_NOWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return (0);
		}
	}
	m->m_len = asa->sa_len;
	memcpy(mtod(m, caddr_t), (caddr_t)asa, asa->sa_len);
	if (n) {
		n->m_next = m0;		/* concatenate data to control */
	}else {
		control = m0;
	}
	if (m)
		m->m_next = control;
	else
		m = control;
	m->m_pkthdr.csum_data = tag;

	for (n = m; n; n = n->m_next) {
		sctp_sballoc(stcb, sb, n);
		if(n->m_next == NULL) {
		  stcb->last_record_insert = nlast;
		}
	}
	if ((n = sb->sb_mb) != NULL) {
		if ((n->m_nextpkt != inp->sb_last_mpkt) && (n->m_nextpkt == NULL)) {
			inp->sb_last_mpkt = NULL;
		}
		if (inp->sb_last_mpkt)
			inp->sb_last_mpkt->m_nextpkt = m;
 		else {
			while (n->m_nextpkt) {
				n = n->m_nextpkt;
			}
			n->m_nextpkt = m;
		}
		inp->sb_last_mpkt = m;
	} else {
		inp->sb_last_mpkt = sb->sb_mb = m;
		inp->sctp_vtag_first = tag;
	}
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) == 0) &&
	    ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) == 0)) {
		if (sctp_add_to_socket_q(inp, stcb)) {
			stcb->asoc.my_rwnd_control_len +=
				sizeof(struct mbuf);
		}
	} else {
		stcb->asoc.my_rwnd_control_len += sizeof(struct mbuf);
	}
	return (1);
#endif
#if defined(__FreeBSD__) || defined(__APPLE__)
	struct mbuf *m, *n, *nlast;
	int msg_eor_seen = 0;
	int cnt=0;

	if (m0 && (m0->m_flags & M_PKTHDR) == 0)
		panic("sbappendaddr_nocheck");

	for (n = control; n; n = n->m_next) {
	  if (n->m_next == 0)	/* get pointer to last control buf */
	    break;
	}
	cnt = 0;
#if MSIZE <= 256
	if (asa->sa_len > MHLEN)
		return (0);
#endif

 try_again:

	MGET(m, M_DONTWAIT, MT_SONAME);
	if (m == 0)
		return (0);
	/* safety */
	if (m == m0) {
		printf("Duplicate mbuf allocated %p in and mget returned %p?\n",
		       m0, m);
		if (cnt) {
			panic("more than once");
		}
		cnt++;
		goto try_again;
	}
#ifdef SCTP_LOCK_LOGGING
	sctp_log_lock(stcb->sctp_ep, stcb, SCTP_LOG_LOCK_SOCKBUF_R);
#endif
	SOCKBUF_LOCK(sb);
	m->m_len = asa->sa_len;
	bcopy((caddr_t)asa, mtod(m, caddr_t), asa->sa_len);
	if (n)
		n->m_next = m0;		/* concatenate data to control */
	else
		control = m0;

	if (m)
		m->m_next = control;
	else
		m = control;
	m->m_pkthdr.csum_data = (int)tag;
	
	for (n = m; n->m_next != NULL; n = n->m_next) {
		sctp_sballoc(stcb, sb, n);
		if(n->m_flags & M_EOR) {
			msg_eor_seen = 1;
		}
	}
	cnt = 0;
	sctp_sballoc(stcb, sb, n);
	nlast = n;
	if (sb->sb_mb == NULL) {
		inp->sctp_vtag_first = tag;
	}
#if defined(__FreeBSD__) 
	SCTP_SBLINKRECORD(sb, m);
	sb->sb_mbtail = nlast;
	if(msg_eor_seen == 0) {
		/* only move if we don't have a whole msg */
		stcb->last_record_insert = nlast;
	}
#else
	if ((n = sb->sb_mb) != NULL) {
		if ((n->m_nextpkt != inp->sb_last_mpkt) && (n->m_nextpkt == NULL)) {
			inp->sb_last_mpkt = NULL;
		}
		if (inp->sb_last_mpkt)
			inp->sb_last_mpkt->m_nextpkt = m;
 		else {
			while (n->m_nextpkt) {
				n = n->m_nextpkt;
			}
			n->m_nextpkt = m;
		}
		inp->sb_last_mpkt = m;
	} else {
		inp->sb_last_mpkt = sb->sb_mb = m;
		inp->sctp_vtag_first = tag;
	}
#endif
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) == 0) &&
	    ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) == 0)) {
		if (sctp_add_to_socket_q(inp, stcb)) {
			stcb->asoc.my_rwnd_control_len +=
				sizeof(struct mbuf);
		} else {
		  printf("Add to socket queue failed!\n");
		}
	} else {
		stcb->asoc.my_rwnd_control_len += sizeof(struct mbuf);
	}
	SOCKBUF_UNLOCK(sb);
	return (1);
#endif
#ifdef __OpenBSD__
	struct mbuf *m, *n;

	if (m0 && (m0->m_flags & M_PKTHDR) == 0)
		panic("sbappendaddr_nocheck");
	m0->m_pkthdr.csum = (int)tag;
	for (n = control; n; n = n->m_next) {
		if (n->m_next == 0)	/* keep pointer to last control buf */
			break;
	}
	if (asa->sa_len > MHLEN)
		return (0);
	MGETHDR(m, M_DONTWAIT, MT_SONAME);
	if (m == 0)
		return (0);
	m->m_len = asa->sa_len;
	bcopy((caddr_t)asa, mtod(m, caddr_t), asa->sa_len);
	if (n)
		n->m_next = m0;		/* concatenate data to control */
	else
		control = m0;

	m->m_pkthdr.csum = (int)tag;
	m->m_next = control;
	for (n = m; n; n = n->m_next) {
		sctp_sballoc(stcb, sb, n);
		if(n->m_next == NULL) {
		  stcb->last_record_insert = nlast;
		}
	}
	if ((n = sb->sb_mb) != NULL) {
		if ((n->m_nextpkt != inp->sb_last_mpkt) && (n->m_nextpkt == NULL)) {
			inp->sb_last_mpkt = NULL;
		}
		if (inp->sb_last_mpkt)
			inp->sb_last_mpkt->m_nextpkt = m;
 		else {
			while (n->m_nextpkt) {
				n = n->m_nextpkt;
			}
			n->m_nextpkt = m;
		}
		inp->sb_last_mpkt = m;
	} else {
		inp->sb_last_mpkt = sb->sb_mb = m;
		inp->sctp_vtag_first = tag;
	}


		if (sctp_add_to_socket_q(inp, stcb)) {
			stcb->asoc.my_rwnd_control_len +=
				sizeof(struct mbuf);
		}
	} else {
		stcb->asoc.my_rwnd_control_len += sizeof(struct mbuf);
	}
	return (1);
#endif
}

/*************HOLD THIS COMMENT FOR PATCH FILE OF
 *************ALTERNATE ROUTING CODE
 */

/*************HOLD THIS COMMENT FOR END OF PATCH FILE OF
 *************ALTERNATE ROUTING CODE
 */

struct mbuf *
sctp_generate_invmanparam(int err)
{
	/* Return a MBUF with a invalid mandatory parameter */
	struct mbuf *m;

	MGET(m, M_DONTWAIT, MT_DATA);
	if (m) {
		struct sctp_paramhdr *ph;
		m->m_len = sizeof(struct sctp_paramhdr);
		ph = mtod(m, struct sctp_paramhdr *);
		ph->param_length = htons(sizeof(struct sctp_paramhdr));
		ph->param_type = htons(err);
	}
	return (m);
}

static int
sctp_should_be_moved(struct mbuf *this, struct sctp_association *asoc)
{
	struct mbuf *m;
	/*
	 * given a mbuf chain, look through it finding
	 * the M_PKTHDR and return 1 if it belongs to
	 * the association given. We tell this by
	 * a kludge where we stuff the my_vtag of the asoc
	 * into the m->m_pkthdr.csum_data/csum field.
	 */
	m = this;
	while (m) {
		if (m->m_flags & M_PKTHDR) {
			/* check it */
#if defined(__OpenBSD__)
			if ((u_int32_t)m->m_pkthdr.csum == asoc->my_vtag)
#else
			if ((u_int32_t)m->m_pkthdr.csum_data == asoc->my_vtag)
#endif
			{
				/* Yep */
				return (1);
			}
		}
		m = m->m_next;
	}
	return (0);
}

u_int32_t
sctp_get_first_vtag_from_sb(struct socket *so)
{
	struct mbuf *this, *at;
	u_int32_t retval;

	retval = 0;
	if (so->so_rcv.sb_mb) {
		/* grubbing time */
		this = so->so_rcv.sb_mb;
		while (this) {
			at = this;
			/* get to the m_pkthdr */
			while (at) {
				if (at->m_flags & M_PKTHDR)
					break;
				else {
					at = at->m_next;
				}
			}
			/* now do we have a m_pkthdr */
			if (at && (at->m_flags & M_PKTHDR)) {
				/* check it */
#if defined(__OpenBSD__)
				if ((u_int32_t)at->m_pkthdr.csum != 0)
#else
				if ((u_int32_t)at->m_pkthdr.csum_data != 0)
#endif
				{
					/* its the one */
#if defined(__OpenBSD__)
					retval = (u_int32_t)at->m_pkthdr.csum;
#else
					retval =
					    (u_int32_t)at->m_pkthdr.csum_data;
#endif
					break;
				}
			}
			this = this->m_nextpkt;
		}

	}
	return (retval);

}
void
sctp_grub_through_socket_buffer(struct sctp_inpcb *inp, struct socket *old,
    struct socket *new, struct sctp_tcb *stcb)
{
	struct mbuf **put, **take, *next, *this;
	struct sockbuf *old_sb, *new_sb;
	struct sctp_association *asoc;
	int moved_top = 0;

	asoc = &stcb->asoc;
	old_sb = &old->so_rcv;
	new_sb = &new->so_rcv;
	if (old_sb->sb_mb == NULL) {
		/* Nothing to move */
		return;
	}
#ifdef SCTP_LOCK_LOGGING
	sctp_log_lock(inp, stcb, SCTP_LOG_LOCK_SOCKBUF_R);
#endif
	SOCKBUF_LOCK(old_sb);
	SOCKBUF_LOCK(new_sb);

	if (inp->sctp_vtag_first == asoc->my_vtag) {
		/* First one must be moved */
		struct mbuf *mm;
		for (mm = old_sb->sb_mb; mm; mm = mm->m_next) {
			/*
			 * Go down the chain and fix
			 * the space allocation of the
			 * two sockets.
			 */
			sctp_sbfree(stcb, old_sb, mm);
			sctp_sballoc(stcb, new_sb, mm);
		}
		new_sb->sb_mb = old_sb->sb_mb;
		old_sb->sb_mb = new_sb->sb_mb->m_nextpkt;
		new_sb->sb_mb->m_nextpkt = NULL;
		put = &new_sb->sb_mb->m_nextpkt;
		moved_top = 1;
	} else {
		put = &new_sb->sb_mb;
	}

	take = &old_sb->sb_mb;
	next = old_sb->sb_mb;
	while (next) {
		this = next;
		/* postion for next one */
		next = this->m_nextpkt;
		/* check the tag of this packet */
		if (sctp_should_be_moved(this, asoc)) {
			/* yes this needs to be moved */
			struct mbuf *mm;
			*take = this->m_nextpkt;
			this->m_nextpkt = NULL;
			*put = this;
			for (mm = this; mm; mm = mm->m_next) {
				/*
				 * Go down the chain and fix
				 * the space allocation of the
				 * two sockets.
				 */
				sctp_sbfree(stcb, old_sb, mm);
				sctp_sballoc(stcb, new_sb, mm);
			}
			put = &this->m_nextpkt;

		} else {
			/* no advance our take point. */
			take = &this->m_nextpkt;
		}
	}
	if (moved_top) {
		/*
		 * Ok so now we must re-postion vtag_first to
		 * match the new first one since we moved the
		 * mbuf at the top.
		 */
		inp->sctp_vtag_first = sctp_get_first_vtag_from_sb(old);
	}
	SOCKBUF_UNLOCK(old_sb);
	SOCKBUF_UNLOCK(new_sb);
}

void
sctp_free_bufspace(struct sctp_tcb *stcb, struct sctp_association *asoc,
    struct sctp_tmit_chunk *tp1)
{
	if (tp1->data == NULL) {
		return;
	}
#ifdef SCTP_MBCNT_LOGGING
	sctp_log_mbcnt(SCTP_LOG_MBCNT_DECREASE,
		       asoc->total_output_queue_size,
		       tp1->book_size,
		       asoc->total_output_mbuf_queue_size,
		       tp1->mbcnt);
#endif
	if (asoc->total_output_queue_size >= tp1->book_size) {
		asoc->total_output_queue_size -= tp1->book_size;
	} else {
		asoc->total_output_queue_size = 0;
	}

	/* Now free the mbuf */
	if (asoc->total_output_mbuf_queue_size >= tp1->mbcnt) {
		asoc->total_output_mbuf_queue_size -= tp1->mbcnt;
	} else {
		asoc->total_output_mbuf_queue_size = 0;
	}
	if (((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) == 0) &&
	    ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) == 0)) {
		if (stcb->sctp_socket->so_snd.sb_cc >= tp1->book_size) {
			stcb->sctp_socket->so_snd.sb_cc -= tp1->book_size;
		} else {
			stcb->sctp_socket->so_snd.sb_cc = 0;

		}
		if (stcb->sctp_socket->so_snd.sb_mbcnt >= tp1->mbcnt) {
			stcb->sctp_socket->so_snd.sb_mbcnt -= tp1->mbcnt;
		} else {
			stcb->sctp_socket->so_snd.sb_mbcnt = 0;
		}
	}
}

int
sctp_release_pr_sctp_chunk(struct sctp_tcb *stcb, struct sctp_tmit_chunk *tp1,
    int reason, struct sctpchunk_listhead *queue)
{
	int ret_sz = 0;
	int notdone;
	uint8_t foundeom = 0;

	do {
		ret_sz += tp1->book_size;
		tp1->sent = SCTP_FORWARD_TSN_SKIP;
		if (tp1->data) {
			sctp_free_bufspace(stcb, &stcb->asoc, tp1);
			sctp_ulp_notify(SCTP_NOTIFY_DG_FAIL, stcb, reason, tp1);
			sctp_m_freem(tp1->data);
			tp1->data = NULL;
			sctp_sowwakeup(stcb->sctp_ep, stcb->sctp_socket);
		}
		if (PR_SCTP_BUF_ENABLED(tp1->flags)) {
			stcb->asoc.sent_queue_cnt_removeable--;
		}
		if (queue == &stcb->asoc.send_queue) {
			TAILQ_REMOVE(&stcb->asoc.send_queue, tp1, sctp_next);
			/* on to the sent queue */
			TAILQ_INSERT_TAIL(&stcb->asoc.sent_queue, tp1,
			    sctp_next);
			stcb->asoc.sent_queue_cnt++;
		}
		if ((tp1->rec.data.rcv_flags & SCTP_DATA_NOT_FRAG) ==
		    SCTP_DATA_NOT_FRAG) {
			/* not frag'ed we ae done   */
			notdone = 0;
			foundeom = 1;
		} else if (tp1->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG) {
			/* end of frag, we are done */
			notdone = 0;
			foundeom = 1;
		} else {
			/* Its a begin or middle piece, we must mark all of it */
			notdone = 1;
			tp1 = TAILQ_NEXT(tp1, sctp_next);
		}
	} while (tp1 && notdone);
	if ((foundeom == 0) && (queue == &stcb->asoc.sent_queue)) {
		/*
		 * The multi-part message was scattered
		 * across the send and sent queue.
		 */
		tp1 = TAILQ_FIRST(&stcb->asoc.send_queue);
		/*
		 * recurse throught the send_queue too, starting at the
		 * beginning.
		 */
		if (tp1) {
			ret_sz += sctp_release_pr_sctp_chunk(stcb, tp1, reason,
			    &stcb->asoc.send_queue);
		} else {
			printf("hmm, nothing on the send queue and no EOM?\n");
		}
	}
	return (ret_sz);
}

/*
 * checks to see if the given address, sa, is one that is currently
 * known by the kernel
 * note: can't distinguish the same address on multiple interfaces and
 *       doesn't handle multiple addresses with different zone/scope id's
 * note: ifa_ifwithaddr() compares the entire sockaddr struct
 */
struct ifaddr *
sctp_find_ifa_by_addr(struct sockaddr *sa)
{
	struct ifnet *ifn;
	struct ifaddr *ifa;

	/* go through all our known interfaces */
	TAILQ_FOREACH(ifn, &ifnet, if_list) {
		/* go through each interface addresses */
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			/* correct family? */
			if (ifa->ifa_addr->sa_family != sa->sa_family)
				continue;

#ifdef INET6
			if (ifa->ifa_addr->sa_family == AF_INET6) {
				/* IPv6 address */
				struct sockaddr_in6 *sin1, *sin2, sin6_tmp;
				sin1 = (struct sockaddr_in6 *)ifa->ifa_addr;
				if (IN6_IS_SCOPE_LINKLOCAL(&sin1->sin6_addr)) {
					/* create a copy and clear scope */
					memcpy(&sin6_tmp, sin1,
					    sizeof(struct sockaddr_in6));
					sin1 = &sin6_tmp;
					in6_clearscope(&sin1->sin6_addr);
				}
				sin2 = (struct sockaddr_in6 *)sa;
				if (memcmp(&sin1->sin6_addr, &sin2->sin6_addr,
					   sizeof(struct in6_addr)) == 0) {
					/* found it */
					return (ifa);
				}
			} else
#endif
			if (ifa->ifa_addr->sa_family == AF_INET) {
				/* IPv4 address */
				struct sockaddr_in *sin1, *sin2;
				sin1 = (struct sockaddr_in *)ifa->ifa_addr;
				sin2 = (struct sockaddr_in *)sa;
				if (sin1->sin_addr.s_addr ==
				    sin2->sin_addr.s_addr) {
					/* found it */
					return (ifa);
				}
			}
			/* else, not AF_INET or AF_INET6, so skip */
		} /* end foreach ifa */
	} /* end foreach ifn */
	/* not found! */
	return (NULL);
}


#ifdef __APPLE__
/*
 * here we hack in a fix for Apple's m_copym for the case where the first mbuf
 * in the chain is a M_PKTHDR and the length is zero
 */
static void
sctp_pkthdr_fix(struct mbuf *m)
{
	struct mbuf *m_nxt;

	if ((m->m_flags & M_PKTHDR) == 0) {
		/* not a PKTHDR */
		return;
	}

	if (m->m_len != 0) {
		/* not a zero length PKTHDR mbuf */
		return;
	}

	/* let's move in a word into the first mbuf... yes, ugly! */
	m_nxt = m->m_next;
	if (m_nxt == NULL) {
		/* umm... not a very useful mbuf chain... */
		return;
	}
	if ((size_t)m_nxt->m_len > sizeof(long)) {
		/* move over a long */
		bcopy(mtod(m_nxt, caddr_t), mtod(m, caddr_t), sizeof(long));
		/* update mbuf data pointers and lengths */
		m->m_len += sizeof(long);
		m_nxt->m_data += sizeof(long);
		m_nxt->m_len -= sizeof(long);
	}
}

inline struct mbuf *
sctp_m_copym(struct mbuf *m, int off, int len, int wait)
{
	sctp_pkthdr_fix(m);
	return (m_copym(m, off, len, wait));
}
#endif /* __APPLE__ */


#if defined(__FreeBSD__)



/*
 * Following replacement or removal of the first mbuf on the first mbuf chain
 * of a socket buffer, push necessary state changes back into the socket
 * buffer so that other consumers see the values consistently.  'nextrecord'
 * is the callers locally stored value of the original value of
 * sb->sb_mb->m_nextpkt which must be restored when the lead mbuf changes.
 * NOTE: 'nextrecord' may be NULL.
 */
static __inline void
sockbuf_pushsync(struct sockbuf *sb, struct mbuf *nextrecord)
{

	SOCKBUF_LOCK_ASSERT(sb);
	/*
	 * First, update for the new value of nextrecord.  If necessary, make
	 * it the first record.
	 */
	if (sb->sb_mb != NULL)
		sb->sb_mb->m_nextpkt = nextrecord;
	else
		sb->sb_mb = nextrecord;

        /*
         * Now update any dependent socket buffer fields to reflect the new
         * state.  This is an expanded inline of SB_EMPTY_FIXUP(), with the
	 * addition of a second clause that takes care of the case where
	 * sb_mb has been updated, but remains the last record.
         */
        if (sb->sb_mb == NULL) {
                sb->sb_mbtail = NULL;
                sb->sb_lastrecord = NULL;
        } else if (sb->sb_mb->m_nextpkt == NULL)
                sb->sb_lastrecord = sb->sb_mb;
}


#define	SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)
/*
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf or mbufs containing ancillary data,
 * and then zero or more mbufs of data.
 * In order to avoid blocking network interrupts for the entire time here,
 * we splx() while doing the actual copy to user space.
 * Although the sockbuf is locked, new data may still be appended,
 * and thus we must maintain consistency of the sockbuf during that time.
 *
 * The caller may receive the data as a single mbuf chain by supplying
 * an mbuf **mp0 for use in returning the chain.  The uio is then used
 * only for the count in uio_resid.
 */

/* This is a HORRIBLE hack.. to get the U-Vancover folks through
 * until I can re-write this customized for SCTP.. which I
 * always intended ;-0
 */

int
sctp_soreceive(so, psa, uio, mp0, controlp, flagsp)
	struct socket *so;
	struct sockaddr **psa;
	struct uio *uio;
	struct mbuf **mp0;
	struct mbuf **controlp;
	int *flagsp;
{
	struct mbuf *m, **mp;
	int flags, len, error, offset;
	struct protosw *pr = so->so_proto;
	struct mbuf *nextrecord;
	int moff, type = 0;
	int at_eor = 0;
	int orig_resid = uio->uio_resid;
	int special_mark=0;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	struct sctp_socket_q_list *sq=NULL;


	/* pickup the assoc we are reading from */
	sctp_pegs[SCTP_ENTER_SCTPSORCV]++;
	if (flagsp != NULL)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;
	if (flags & MSG_OOB)
	        return (EOPNOTSUPP);

	if ((so->so_rcv.sb_mb == NULL) &&
	    ((so->so_state & SS_NBIO) ||
	     (flags & (MSG_DONTWAIT
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
		       |MSG_NBIO
#endif
		     )))) {
		return (EWOULDBLOCK);
		
	}
	inp = (struct sctp_inpcb *)so->so_pcb;
	SCTP_INP_RLOCK(inp);
	at_eor = 0;
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) || 
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		if(stcb == NULL) {
		  SCTP_INP_RUNLOCK(inp);
		  return (ENOTCONN);
		}
		if(stcb->last_record_insert == NULL) {
		  at_eor = 1;
		}
	} else {
		sq = TAILQ_FIRST(&inp->sctp_queue_list);
		if(sq != NULL) {
			stcb = sq->tcb;
			if(stcb) {
			  if(stcb->last_record_insert == NULL) {
			    at_eor = 1;
			  }
			} else {
			  at_eor = 1;			  
			}
		} else {
			stcb = NULL;
			at_eor = 0;
		}
	}
	SCTP_INP_RUNLOCK(inp);
	mp = mp0;
	if (psa != NULL)
		*psa = NULL;
	if (controlp != NULL)
		*controlp = NULL;
	if (mp != NULL)
		*mp = NULL;
#ifdef SCTP_LOCK_LOGGING
	sctp_log_lock(inp, stcb, SCTP_LOG_LOCK_SOCKBUF_R);
#endif
	SOCKBUF_LOCK(&so->so_rcv);
 restart:
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
	error = sblock(&so->so_rcv, SBLOCKWAIT(flags));
	if (error)
		goto out;

	m = so->so_rcv.sb_mb;
	if((m != NULL) && (m->m_len == 0) && (m->m_next == NULL) &&
	   (stcb) && (stcb->asoc.fragmented_delivery_inprogress)) {
		if(so->so_rcv.sb_cc) {
			/* HACK .. if this is not bad enough this
			 * is icing on the cake of hacks. We hide
			 * the len in sb_cc from the socket buf. That
			 * way sleep won't re-wake until our data
			 * comes.
			 */
			m->m_pkthdr.len += so->so_rcv.sb_cc;
			stcb->hidden_from_sb += so->so_rcv.sb_cc;
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_UTIL1) {
			  printf("We now have %d bytes hidden from sb_cc nextrecord:%x\n", 
				 (int)m->m_pkthdr.len,
				 (u_int)m->m_nextpkt);
			}
#endif
			so->so_rcv.sb_cc = 0;
		}
		if(flags & MSG_DONTWAIT) {
			error = EWOULDBLOCK;
			goto release;
		}
		SBLASTRECORDCHK(&so->so_rcv);
		SBLASTMBUFCHK(&so->so_rcv);
		SOCKBUF_UNLOCK(&so->so_rcv);
		error = sbwait(&so->so_rcv);
		if (error)
			goto out;
		goto restart;
	} else if ((m != NULL) && (m->m_len == 0) && (m->m_next) &&
		   (stcb)) {
		/* restore hacked value */
		sctp_pegs[SCTP_PDAPI_HAD_TORCVR_RCV]++;
		if ((m->m_nextpkt == NULL) && m->m_pkthdr.len) {
			panic("Huh, restoring to a null record?");
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_UTIL1) {
		  printf("We now restore %d bytes that were hidden nextrecord:%x\n", 
			 (int)m->m_pkthdr.len,
			 (u_int)m->m_nextpkt);
		}
#endif
		so->so_rcv.sb_cc += m->m_pkthdr.len;
		if(m->m_pkthdr.len != stcb->hidden_from_sb) {
			panic("At restoral, mismatch");
		}
		m->m_pkthdr.len = 0;
		/* move off the nextrecord pointer */
		m->m_next->m_nextpkt = m->m_nextpkt;
		if(so->so_rcv.sb_lastrecord == m) {
		  /* last record pointed to our 0 len mbuf, fix this too */
		  so->so_rcv.sb_lastrecord = m->m_nextpkt;
		}
		so->so_rcv.sb_mb = m_free(m);
		m = so->so_rcv.sb_mb;
	} else if ((m != NULL) && (stcb == NULL)) {
	  if(sq)
	    sctp_pegs[SCTP_PDAPI_NOSTCB_ATC]++;
	}
	/*
	 * If we have less data than requested, block awaiting more
	 * (subject to any timeout) if:
	 *   1. the current count is less than the low water mark, or
	 *   2. MSG_WAITALL is set, and it is possible to do the entire
	 *	receive operation at once if we block (resid <= hiwat).
	 *   3. MSG_DONTWAIT is not set
	 * If MSG_WAITALL is set but resid is larger than the receive buffer,
	 * we have to do the receive in sections, and thus risk returning
	 * a short count if a timeout or signal occurs after we start.
	 */
	if (m == NULL || 
	    (
	     ((flags & MSG_DONTWAIT) == 0 && so->so_rcv.sb_cc < uio->uio_resid) &&
	     (so->so_rcv.sb_cc < so->so_rcv.sb_lowat || ((flags & MSG_WAITALL) && uio->uio_resid <= so->so_rcv.sb_hiwat)) &&
	     (m->m_nextpkt == NULL)
	    )) {
		KASSERT(m != NULL || !so->so_rcv.sb_cc,
			("receive: m == %p so->so_rcv.sb_cc == %u",
			 m, so->so_rcv.sb_cc));
		if (so->so_error) {
			if (m != NULL)
				goto dontblock;
			error = so->so_error;
			if ((flags & MSG_PEEK) == 0)
				so->so_error = 0;
			goto release;
		}
		SOCKBUF_LOCK_ASSERT(&so->so_rcv);
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
		if (so->so_rcv.sb_state & SBS_CANTRCVMORE) 
#else
		if (so->so_state & SS_CANTRCVMORE) 
#endif
		{

			if (m)
				goto dontblock;
			else
				goto release;
		}
		if(m && (at_eor)) {
		  goto dontblock;
		} else if (m) {
		  for (; m != NULL; m = m->m_next) {
		    if (m->m_flags & M_EOR) {
		      m = so->so_rcv.sb_mb;
		      goto dontblock;
		    }
		  }
		}

		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
 		        error = ENOTCONN;
			goto release;
		}
		if (uio->uio_resid == 0)
			goto release;
		if ((so->so_state & SS_NBIO) ||

		    (flags & (MSG_DONTWAIT
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
			      |MSG_NBIO
#endif
			    ))) {
			error = EWOULDBLOCK;
			goto release;
		}
		SBLASTRECORDCHK(&so->so_rcv);
		SBLASTMBUFCHK(&so->so_rcv);
		SOCKBUF_UNLOCK(&so->so_rcv);
		error = sbwait(&so->so_rcv);
		if (error)
			goto out;
		if ((m == NULL) || (stcb == NULL)){
			/* we could not have had a stcb,
			 * or we don't have one.
			 */
			SOCKBUF_UNLOCK(&so->so_rcv);
			SCTP_INP_RLOCK(inp);
			SOCKBUF_LOCK(&so->so_rcv);
			at_eor = 0;
			if ((inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) || 
			    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if(stcb == NULL) {
				  SCTP_INP_RUNLOCK(inp);
				  error = ENOTCONN;
				  goto out;
				}
				if(stcb->last_record_insert == NULL) {
				  at_eor = 1;
				}
			} else {
				sq = TAILQ_FIRST(&inp->sctp_queue_list);
				if(sq != NULL) {
					stcb = sq->tcb;
					if(stcb) {
					  if(stcb->last_record_insert == NULL) {
					     at_eor = 1;
					  }
					} else {
					  at_eor = 1;
					}
				} else {
					stcb = NULL;
					at_eor = 0;
				}
			}
			SCTP_INP_RUNLOCK(inp);
			goto restart;
		}
		goto restart;
	}
 dontblock:
	/*
	 * From this point onward, we maintain 'nextrecord' as a cache of the
	 * pointer to the next record in the socket buffer.  We must keep the
	 * various socket buffer pointers and local stack versions of the
	 * pointers in sync, pushing out modifications before dropping the
	 * socket buffer mutex, and re-reading them when picking it up.
	 *
	 * Otherwise, we will race with the network stack appending new data
	 * or records onto the socket buffer by using inconsistent/stale
	 * versions of the field, possibly resulting in socket buffer
	 * corruption.
	 *
	 * By holding the high-level sblock(), we prevent simultaneous
	 * readers from pulling off the front of the socket buffer.
	 */
        if(stcb == NULL){
		sctp_pegs[SCTP_NO_TCB_IN_RCV]++;
	} else {
		sctp_pegs[SCTP_HAD_TCB_IN_RCV]++;
		if(stcb->asoc.fragmented_delivery_inprogress) {
			sctp_pegs[SCTP_PDAPI_UP_IN_RCV]++;
		}
	}
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
	if (uio->uio_td)
		uio->uio_td->td_proc->p_stats->p_ru.ru_msgrcv++;
#else
	if (uio->uio_procp)
		uio->uio_procp->p_stats->p_ru.ru_msgrcv++;
#endif
	KASSERT(m == so->so_rcv.sb_mb, ("soreceive: m != so->so_rcv.sb_mb"));
	SBLASTRECORDCHK(&so->so_rcv);
	SBLASTMBUFCHK(&so->so_rcv);
	nextrecord = m->m_nextpkt;
	if (m->m_type == MT_SONAME) {
	  KASSERT(m->m_type == MT_SONAME,
		  ("m->m_type == %d", m->m_type));
	  orig_resid = 0;
	  if (psa != NULL)
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
	    *psa = sodupsockaddr(mtod(m, struct sockaddr *),
				 M_NOWAIT);
#else
	  *psa = dup_sockaddr(mtod(m, struct sockaddr *),
			      mp0 == 0);
#endif
	  if (flags & MSG_PEEK) {
	    m = m->m_next;
	  } else {
	    sctp_sbfree(stcb, &so->so_rcv, m);
	    so->so_rcv.sb_mb = m_free(m);
	    m = so->so_rcv.sb_mb;
	    sockbuf_pushsync(&so->so_rcv, nextrecord);
	  }
	}

	/*
	 * Process one or more MT_CONTROL mbufs present before any data mbufs
	 * in the first mbuf chain on the socket buffer.  If MSG_PEEK, we
	 * just copy the data; if !MSG_PEEK, we call into the protocol to
	 * perform externalization (or freeing if controlp == NULL).
	 */
	if (m != NULL && m->m_type == MT_CONTROL) {
		struct mbuf *cm = NULL, *cmn;
		struct mbuf **cme = &cm;

		do {
			if (flags & MSG_PEEK) {
				if (controlp != NULL) {
					*controlp = m_copy(m, 0, m->m_len);
					controlp = &(*controlp)->m_next;
				}
				m = m->m_next;
			} else {
				sctp_sbfree(stcb, &so->so_rcv, m);
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = NULL;
				*cme = m;
				cme = &(*cme)->m_next;
				m = so->so_rcv.sb_mb;
			}
		} while (m != NULL && m->m_type == MT_CONTROL);
		if ((flags & MSG_PEEK) == 0)
			sockbuf_pushsync(&so->so_rcv, nextrecord);
		while (cm != NULL) {
			cmn = cm->m_next;
			cm->m_next = NULL;
			if (controlp != NULL)
				*controlp = cm;
			else
				m_freem(cm);
			if (controlp != NULL) {
				orig_resid = 0;
				while (*controlp != NULL)
					controlp = &(*controlp)->m_next;
			}
			cm = cmn;
		}
		nextrecord = so->so_rcv.sb_mb->m_nextpkt;
		orig_resid = 0;
	}
	if (m != NULL) {
		if ((flags & MSG_PEEK) == 0) {
			KASSERT(m->m_nextpkt == nextrecord,
				("soreceive: post-control, nextrecord !sync"));
			if (nextrecord == NULL) {
				KASSERT(so->so_rcv.sb_mb == m,
					("soreceive: post-control, sb_mb!=m"));
				KASSERT(so->so_rcv.sb_lastrecord == m,
					("soreceive: post-control, lastrecord!=m"));
			}
		}
		type = m->m_type;
	} else {
		if ((flags & MSG_PEEK) == 0) {
			KASSERT(so->so_rcv.sb_mb == nextrecord,
				("soreceive: sb_mb != nextrecord"));
			if (so->so_rcv.sb_mb == NULL) {
				KASSERT(so->so_rcv.sb_lastrecord == NULL,
					("soreceive: sb_lastercord != NULL"));
			}
		}
	}
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
	SBLASTRECORDCHK(&so->so_rcv);
	SBLASTMBUFCHK(&so->so_rcv);

	/*
	 * Now continue to read any data mbufs off of the head of the socket
	 * buffer until the read request is satisfied.  Note that 'type' is
	 * used to store the type of any mbuf reads that have happened so far
	 * such that soreceive() can stop reading if the type changes, which
	 * causes soreceive() to return only one of regular data and inline
	 * out-of-band data in a single socket receive operation.
	 */
	moff = 0;
	offset = 0;
	while (m != NULL && uio->uio_resid > 0 && error == 0) {
		SOCKBUF_LOCK_ASSERT(&so->so_rcv);
		KASSERT(m->m_type == MT_DATA || m->m_type == MT_HEADER,
			("m->m_type == %d", m->m_type));
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
		so->so_rcv.sb_state &= ~SBS_RCVATMARK;
#else
		so->so_state &= ~SS_RCVATMARK;
#endif
		len = uio->uio_resid;
		if (so->so_oobmark && len > so->so_oobmark - offset)
			len = so->so_oobmark - offset;
		if (len > m->m_len - moff)
			len = m->m_len - moff;
		/*
		 * If mp is set, just pass back the mbufs.
		 * Otherwise copy them out via the uio, then free.
		 * Sockbuf must be consistent here (points to current mbuf,
		 * it points to next record) when we drop priority;
		 * we must note any additions to the sockbuf when we
		 * block interrupts again.
		 */
		if (mp == NULL) {
			SOCKBUF_LOCK_ASSERT(&so->so_rcv);
			SBLASTRECORDCHK(&so->so_rcv);
			SBLASTMBUFCHK(&so->so_rcv);
			SOCKBUF_UNLOCK(&so->so_rcv);
#ifdef ZERO_COPY_SOCKETS
			if (so_zero_copy_receive) {
				vm_page_t pg;
				int disposable;

				if ((m->m_flags & M_EXT)
				    && (m->m_ext.ext_type == EXT_DISPOSABLE))
					disposable = 1;
				else
					disposable = 0;

				pg = PHYS_TO_VM_PAGE(vtophys(mtod(m, caddr_t) +
							     moff));

				if (uio->uio_offset == -1)
					uio->uio_offset =IDX_TO_OFF(pg->pindex);

				error = uiomoveco(mtod(m, char *) + moff,
						  (int)len, uio,pg->object,
						  disposable);
			} else
#endif /* ZERO_COPY_SOCKETS */
				error = uiomove(mtod(m, char *) + moff, (int)len, uio);
#ifdef SCTP_LOCK_LOGGING
			sctp_log_lock(inp, stcb, SCTP_LOG_LOCK_SOCKBUF_R);
#endif
			SOCKBUF_LOCK(&so->so_rcv);
			if (error)
				goto release;
		} else
			uio->uio_resid -= len;
		SOCKBUF_LOCK_ASSERT(&so->so_rcv);
		if (len == m->m_len - moff) {
			if (m->m_flags & M_EOR)
				flags |= MSG_EOR;
			if (m->m_flags & M_NOTIFICATION)
				flags |= MSG_NOTIFICATION;
			if (flags & MSG_PEEK) {
				m = m->m_next;
				moff = 0;
			} else {
				nextrecord = m->m_nextpkt;
				if (mp != NULL) {
				        sctp_sbfree(stcb, &so->so_rcv, m);
					*mp = m;
					mp = &m->m_next;
					so->so_rcv.sb_mb = m = m->m_next;
					*mp = NULL;
				} else {
				        if (flags & MSG_EOR) {
					        sctp_sbfree(stcb, &so->so_rcv, m);
						so->so_rcv.sb_mb = m_free(m);
						m = so->so_rcv.sb_mb;
					} else {
						/* not eor. what should we do here */
						if ( (m->m_next == NULL) &&
						     (stcb) &&
						     (stcb->asoc.fragmented_delivery_inprogress)){
							/* special case, we must wait at this point */
							sctp_pegs[SCTP_PDAPI_HAD_TOWAIT_RCV]++;
 					                sctp_sbfree(stcb, &so->so_rcv, m);
							m->m_len = 0;
							stcb->hidden_from_sb = so->so_rcv.sb_cc;
							m->m_pkthdr.len = so->so_rcv.sb_cc;
							so->so_rcv.sb_cc = 0;
#ifdef SCTP_DEBUG
							if (sctp_debug_on & SCTP_DEBUG_UTIL1) {
							  printf("Hide %d bytes nextrecord:%x\n",
								 m->m_pkthdr.len, (u_int)nextrecord);
							}
#endif
							special_mark = 1;
						} else {
							/* normal thing is ok */
 					                sctp_sbfree(stcb, &so->so_rcv, m);
							so->so_rcv.sb_mb = m_free(m);
							m = so->so_rcv.sb_mb;
						}
					}
				}
				if (m != NULL) {
					m->m_nextpkt = nextrecord;
					if (nextrecord == NULL)
						so->so_rcv.sb_lastrecord = m;
				} else {
					so->so_rcv.sb_mb = nextrecord;
					SB_EMPTY_FIXUP(&so->so_rcv);
				}
				SBLASTRECORDCHK(&so->so_rcv);
				SBLASTMBUFCHK(&so->so_rcv);
			}
		} else {
			if (flags & MSG_PEEK)
				moff += len;
			else {
				if (mp != NULL) {
					SOCKBUF_UNLOCK(&so->so_rcv);
					*mp = m_copym(m, 0, len, 
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
						      M_TRYWAIT
#else
						      M_WAIT
#endif
						);
#ifdef SCTP_LOCK_LOGGING
					sctp_log_lock(inp, stcb, SCTP_LOG_LOCK_SOCKBUF_R);
#endif
					SOCKBUF_LOCK(&so->so_rcv);
				}
				m->m_data += len;
				m->m_len -= len;
				so->so_rcv.sb_cc -= len;
			}
		}
		SOCKBUF_LOCK_ASSERT(&so->so_rcv);
		if (so->so_oobmark) {
			if ((flags & MSG_PEEK) == 0) {
				so->so_oobmark -= len;
				if (so->so_oobmark == 0) {
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
					so->so_rcv.sb_state |= SBS_RCVATMARK;
#else
					so->so_state |= SS_RCVATMARK;
#endif
					break;
				}
			} else {
				offset += len;
				if (offset == so->so_oobmark)
					break;
			}
		}
		if (flags & MSG_EOR) {
			break;
		}

		if (special_mark) {
		  break;
		}

		/*
		 * If the MSG_WAITALL flag is set (for non-atomic socket),
		 * we must not quit until "uio->uio_resid == 0" or an error
		 * termination.  If a signal/timeout occurs, return
		 * with a short count but without error.
		 * Keep sockbuf locked against other readers.
		 */
		while (flags & MSG_WAITALL && m == NULL && uio->uio_resid > 0 &&
		       !sosendallatonce(so) && nextrecord == NULL) {
			SOCKBUF_LOCK_ASSERT(&so->so_rcv);
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
			if (so->so_error || so->so_rcv.sb_state & SBS_CANTRCVMORE)
				break;
#else
			 if (so->so_error || so->so_state & SS_CANTRCVMORE) 
				break;
#endif
			/*
			 * Notify the protocol that some data has been
			 * drained before blocking.
			 */
			if (pr->pr_flags & PR_WANTRCVD && so->so_pcb != NULL) {
				SOCKBUF_UNLOCK(&so->so_rcv);
				(*pr->pr_usrreqs->pru_rcvd)(so, flags);
#ifdef SCTP_LOCK_LOGGING
				sctp_log_lock(inp, stcb, SCTP_LOG_LOCK_SOCKBUF_R);
#endif

				SOCKBUF_LOCK(&so->so_rcv);
			}
			SBLASTRECORDCHK(&so->so_rcv);
			SBLASTMBUFCHK(&so->so_rcv);
			error = sbwait(&so->so_rcv);
			if (error)
				goto release;
			m = so->so_rcv.sb_mb;
			if (m != NULL)
				nextrecord = m->m_nextpkt;
		}
	}
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
	if ((flags & MSG_PEEK) == 0) {
		if (m == NULL) {
			/*
			 * First part is an inline SB_EMPTY_FIXUP().  Second
			 * part makes sure sb_lastrecord is up-to-date if
			 * there is still data in the socket buffer.
			 */
			so->so_rcv.sb_mb = nextrecord;
			if (so->so_rcv.sb_mb == NULL) {
				so->so_rcv.sb_mbtail = NULL;
				so->so_rcv.sb_lastrecord = NULL;
			} else if (nextrecord->m_nextpkt == NULL)
				so->so_rcv.sb_lastrecord = nextrecord;
		}
		SBLASTRECORDCHK(&so->so_rcv);
		SBLASTMBUFCHK(&so->so_rcv);
		SOCKBUF_UNLOCK(&so->so_rcv);
		(*pr->pr_usrreqs->pru_rcvd)(so, flags);
#ifdef SCTP_LOCK_LOGGING
		sctp_log_lock(inp, stcb, SCTP_LOG_LOCK_SOCKBUF_R);
#endif
		SOCKBUF_LOCK(&so->so_rcv);
	}
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
	if ((orig_resid == uio->uio_resid) && 
	    (orig_resid) &&
	    ((flags & MSG_EOR) == 0) && 
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
	    ((so->so_rcv.sb_state & SBS_CANTRCVMORE) == 0)
#else
	    ((so->so_state & SS_CANTRCVMORE)  == 0)
#endif
	    ){
		SOCKBUF_UNLOCK(&so->so_rcv);
		goto restart;
	}
	if (flagsp != NULL)
		*flagsp |= flags;
 release:
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
	SOCKBUF_UNLOCK(&so->so_rcv);
 out:
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
	SOCKBUF_UNLOCK(&so->so_rcv);
	return (error);
}


void
sctp_sbappend( struct sockbuf *sb,
	       struct mbuf *m,
	       struct sctp_tcb *stcb)
{
  register struct mbuf *n;
#ifdef SCTP_LOCK_LOGGING
  sctp_log_lock(stcb->sctp_ep, stcb, SCTP_LOG_LOCK_SOCKBUF_R);
#endif

  SOCKBUF_LOCK(sb);

  if (m == 0)
    return;
  if (stcb == NULL)
    return;

  if(stcb->last_record_insert == NULL) {
    panic("stcb->last_record_insert is NULL?");
  }

#if defined(__FreeBSD__) && __FreeBSD_version > 500000
  if(stcb->last_record_insert->m_flags & M_FREELIST) {
    panic("stcb->last_record_insert is FREE?");
  }
#else
  if(stcb->last_record_insert->m_type == MT_FREE) {
    panic("stcb->last_record_insert is FREE?");
  }
#endif

  if(sb->sb_mb == stcb->last_record_insert) {
	  if((sb->sb_mb->m_len == 0) &&
 	     (sb->sb_mb->m_pkthdr.len != stcb->hidden_from_sb)) {
		  panic("At append, mismatch");
	  }
  }
  n = stcb->last_record_insert;
  if ((sb->sb_mb == n) && sb->sb_mb->m_len == 0) {
    /* need a new control for this guy */
    struct mbuf *x;
    x = sctp_build_ctl_nchunk(stcb, 
			      stcb->asoc.tsn_last_delivered, 
			      stcb->asoc.pdapi_ppid,
			      stcb->asoc.context, 
			      stcb->asoc.str_of_pdapi, 
			      stcb->asoc.ssn_of_pdapi, 
			      stcb->asoc.fragment_flags);
    if(x) {
      x->m_next = m;
      m = x;
    }
  }
  n->m_next = m;
  while(m) {
    sctp_sballoc(stcb, sb, m);
    if(m->m_flags & M_EOR) {
      /* done with this record */
      stcb->last_record_insert = NULL;
      if(m->m_next) {
	panic("m_next set with M_EOR on m");
      }
    } else if (m->m_next == NULL) {
      /* move last pointer */
      stcb->last_record_insert = m;
    }
    m = m->m_next;
  }
  SOCKBUF_UNLOCK(sb);
}

#endif
