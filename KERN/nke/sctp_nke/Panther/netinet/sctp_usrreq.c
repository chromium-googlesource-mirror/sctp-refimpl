/*-
 * Copyright (c) 2001-2006 Cisco Systems, Inc.
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

/* $KAME: sctp_usrreq.c,v 1.48 2005/03/07 23:26:08 itojun Exp $	 */

#ifdef __FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD:$");
#endif


#if !(defined(__OpenBSD__) || defined(__APPLE__))
#include "opt_ipsec.h"
#endif
#if defined(__FreeBSD__)
#include "opt_inet6.h"
#include "opt_inet.h"
#include "opt_global.h"
#endif
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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <net/if_types.h>
#if defined(__FreeBSD__) || defined(__APPLE__)
#include <net/if_var.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/scope6_var.h>
#if defined(__APPLE__) && !defined(SCTP_APPLE_PANTHER)
#include <sys/domain.h>
#endif
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_header.h>
#include <netinet/sctp_var.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_indata.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctp_auth.h>
#ifdef IPSEC
#ifndef __OpenBSD__
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#else
#undef IPSEC
#endif
#endif				/* IPSEC */

#ifndef __APPLE__
#include <net/net_osdep.h>
#endif

#if defined(HAVE_SCTP_PEELOFF_SOCKOPT)
#include <netinet/sctp_peeloff.h>
#endif				/* HAVE_SCTP_PEELOFF_SOCKOPT */

#if defined(HAVE_NRL_INPCB)
#ifndef in6pcb
#define in6pcb		inpcb
#endif
#ifndef sotoin6pcb
#define sotoin6pcb      sotoinpcb
#endif
#endif

#if defined(__FreeBSD__)
#ifndef in6pcb
#define in6pcb		inpcb
#endif
#ifndef sotoin6pcb
#define sotoin6pcb      sotoinpcb
#endif
#endif



/*
 * sysctl tunable variables
 */
int sctp_sendspace = (128 * 1024);
int sctp_recvspace = 128 * (1024 +
#ifdef INET6
    sizeof(struct sockaddr_in6)
#else
    sizeof(struct sockaddr_in)
#endif
);
int sctp_mbuf_threshold_count = SCTP_DEFAULT_MBUFS_IN_CHAIN;
int sctp_auto_asconf = SCTP_DEFAULT_AUTO_ASCONF;
int sctp_ecn_enable = 1;
int sctp_ecn_nonce = 0;
int sctp_strict_sacks = 0;
int sctp_no_csum_on_loopback = 1;
int sctp_strict_init = 1;
int sctp_abort_if_one_2_one_hits_limit = 0;
int sctp_strict_data_order = 0;

int sctp_peer_chunk_oh = sizeof(struct mbuf);
int sctp_max_burst_default = SCTP_DEF_MAX_BURST;
int sctp_use_cwnd_based_maxburst = 1;
int sctp_do_drain = 1;
int sctp_warm_the_crc32_table = 0;

unsigned int sctp_max_chunks_on_queue = SCTP_ASOC_MAX_CHUNKS_ON_QUEUE;
unsigned int sctp_delayed_sack_time_default = SCTP_RECV_MSEC;
unsigned int sctp_heartbeat_interval_default = SCTP_HB_DEFAULT_MSEC;
unsigned int sctp_pmtu_raise_time_default = SCTP_DEF_PMTU_RAISE_SEC;
unsigned int sctp_shutdown_guard_time_default = SCTP_DEF_MAX_SHUTDOWN_SEC;
unsigned int sctp_secret_lifetime_default = SCTP_DEFAULT_SECRET_LIFE_SEC;
unsigned int sctp_rto_max_default = SCTP_RTO_UPPER_BOUND;
unsigned int sctp_rto_min_default = SCTP_RTO_LOWER_BOUND;
unsigned int sctp_rto_initial_default = SCTP_RTO_INITIAL;
unsigned int sctp_init_rto_max_default = SCTP_RTO_UPPER_BOUND;
unsigned int sctp_valid_cookie_life_default = SCTP_DEFAULT_COOKIE_LIFE;
unsigned int sctp_init_rtx_max_default = SCTP_DEF_MAX_INIT;
unsigned int sctp_assoc_rtx_max_default = SCTP_DEF_MAX_SEND;
unsigned int sctp_path_rtx_max_default = SCTP_DEF_MAX_PATH_RTX;
unsigned int sctp_nr_outgoing_streams_default = SCTP_OSTREAM_INITIAL;
unsigned int sctp_cmt_on_off = 0;
unsigned int sctp_cmt_sockopt_on_off = 0;
unsigned int sctp_cmt_use_dac = 0;
unsigned int sctp_cmt_sockopt_use_dac = 0;
int sctp_L2_abc_variable = 1;
unsigned int sctp_early_fr = 0;
unsigned int sctp_early_fr_msec = SCTP_MINFR_MSEC_TIMER;
unsigned int sctp_use_rttvar_cc = 0;
int sctp_says_check_for_deadlock = 0;
unsigned int sctp_auth_disable = 0;
unsigned int sctp_auth_random_len = SCTP_AUTH_RANDOM_SIZE_DEFAULT;
unsigned int sctp_auth_hmac_id_default = SCTP_AUTH_HMAC_ID_SHA1;

#ifdef SCTP_DEBUG
extern uint32_t sctp_debug_on;

#endif				/* SCTP_DEBUG */


void
sctp_init(void)
{
#ifdef __OpenBSD__
#define nmbclusters	nmbclust
#endif
	/* Init the SCTP pcb in sctp_pcb.c */
	u_long sb_max_adj;

	sctp_pcb_init();

#ifndef __OpenBSD__
	if ((nmbclusters / 8) > SCTP_ASOC_MAX_CHUNKS_ON_QUEUE)
		sctp_max_chunks_on_queue = (nmbclusters / 8);
#else
	if ((nmbclust / 8) > SCTP_ASOC_MAX_CHUNKS_ON_QUEUE)
		sctp_max_chunks_on_queue = nmbclust / 8;
#endif
	/*
	 * Allow a user to take no more than 1/2 the number of clusters or
	 * the SB_MAX whichever is smaller for the send window.
	 */
	sb_max_adj = (u_long)((u_quad_t) (SB_MAX) * MCLBYTES / (MSIZE + MCLBYTES));
	sctp_sendspace = min((min(SB_MAX, sb_max_adj)),
#ifndef __OpenBSD__
	    ((nmbclusters / 2) * SCTP_DEFAULT_MAXSEGMENT));
#else
	    ((nmbclust / 2) * SCTP_DEFAULT_MAXSEGMENT));
#endif
	/*
	 * Now for the recv window, should we take the same amount? or
	 * should I do 1/2 the SB_MAX instead in the SB_MAX min above. For
	 * now I will just copy.
	 */
	sctp_recvspace = sctp_sendspace;
#ifdef __OpenBSD__
#undef nmbclusters
#endif
}

#ifdef SCTP_APPLE_FINE_GRAINED_LOCKING
void
sctp_finish(void)
{
	sctp_pcb_finish();
}

#endif

#ifdef INET6
void
ip_2_ip6_hdr(struct ip6_hdr *ip6, struct ip *ip)
{
	bzero(ip6, sizeof(*ip6));

	ip6->ip6_vfc = IPV6_VERSION;
	ip6->ip6_plen = ip->ip_len;
	ip6->ip6_nxt = ip->ip_p;
	ip6->ip6_hlim = ip->ip_ttl;
	ip6->ip6_src.s6_addr32[2] = ip6->ip6_dst.s6_addr32[2] =
	    IPV6_ADDR_INT32_SMP;
	ip6->ip6_src.s6_addr32[3] = ip->ip_src.s_addr;
	ip6->ip6_dst.s6_addr32[3] = ip->ip_dst.s_addr;
}

#endif				/* INET6 */

static void
sctp_split_chunks(struct sctp_association *asoc,
    struct sctp_stream_out *strm,
    struct sctp_tmit_chunk *chk)
{
	struct sctp_tmit_chunk *new_chk;

	/* First we need a chunk */
	new_chk = (struct sctp_tmit_chunk *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_chunk);
	if (new_chk == NULL) {
		chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
		return;
	}
	SCTP_INCR_CHK_COUNT();
	/* Copy it all */
	*new_chk = *chk;
	/* split the data */
	new_chk->data = m_split(chk->data, (chk->send_size >> 1), M_DONTWAIT);
	if (new_chk->data == NULL) {
		/* Can't split */
		chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, new_chk);
		SCTP_DECR_CHK_COUNT();
		return;
	}
	/* Data is now split adjust sizes */
	chk->no_fr_allowed = 0;

	chk->send_size >>= 1;
	new_chk->send_size -= chk->send_size;

	chk->book_size >>= 1;
	new_chk->book_size -= chk->book_size;


	/* now adjust the marks */
	if (chk->rec.data.rcv_flags & SCTP_DATA_NOT_FRAG) {
		chk->rec.data.rcv_flags &= ~SCTP_DATA_LAST_FRAG;
		chk->rec.data.rcv_flags |= SCTP_DATA_FIRST_FRAG;

		new_chk->rec.data.rcv_flags &= ~SCTP_DATA_FIRST_FRAG;
		new_chk->rec.data.rcv_flags |= SCTP_DATA_LAST_FRAG;
	} else {
		/* Its already frag'd */
		if (chk->rec.data.rcv_flags & SCTP_DATA_FIRST_FRAG) {
			/* just turn off the first on the new piece */
			new_chk->rec.data.rcv_flags &= ~SCTP_DATA_FIRST_FRAG;
		} else if (chk->rec.data.rcv_flags & SCTP_DATA_LAST_FRAG) {
			/*
			 * just turn off the last frag on the old chunk the
			 * new one gets to keep it.
			 */
			chk->rec.data.rcv_flags &= ~SCTP_DATA_LAST_FRAG;
			new_chk->rec.data.rcv_flags |= SCTP_DATA_FIRST_FRAG;
		}
	}
	/* Increase ref count if dest is set */
	if (chk->whoTo) {
		atomic_add_int(&new_chk->whoTo->ref_count, 1);
	}
	/* now drop it on the end of the list */
	asoc->stream_queue_cnt++;
	TAILQ_INSERT_AFTER(&strm->outqueue, chk, new_chk, sctp_next);
}

static void
sctp_pathmtu_adustment(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net,
    uint16_t nxtsz)
{
	struct sctp_tmit_chunk *chk, *nchk;
	struct sctp_stream_out *strm;

	/* Adjust that too */
	stcb->asoc.smallest_mtu = nxtsz;
	/* now off to subtract IP_DF flag if needed */

	TAILQ_FOREACH(chk, &stcb->asoc.send_queue, sctp_next) {
		if ((chk->send_size + IP_HDR_SIZE) > nxtsz) {
			chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
		}
	}
	TAILQ_FOREACH(chk, &stcb->asoc.sent_queue, sctp_next) {
		if ((chk->send_size + IP_HDR_SIZE) > nxtsz) {
			/*
			 * For this guy we also mark for immediate resend
			 * since we sent to big of chunk
			 */
			chk->flags |= CHUNK_FLAGS_FRAGMENT_OK;
			if (chk->sent != SCTP_DATAGRAM_RESEND) {
				sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
			}
			chk->sent = SCTP_DATAGRAM_RESEND;
			chk->rec.data.doing_fast_retransmit = 0;

			/* Clear any time so NO RTT is being done */
			chk->do_rtt = 0;
			if (stcb->asoc.total_flight >= chk->book_size)
				stcb->asoc.total_flight -= chk->book_size;
			else
				stcb->asoc.total_flight = 0;
			if (stcb->asoc.total_flight_count > 0)
				stcb->asoc.total_flight_count--;
			if (net->flight_size >= chk->book_size)
				net->flight_size -= chk->book_size;
			else
				net->flight_size = 0;
		}
	}
	TAILQ_FOREACH(strm, &stcb->asoc.out_wheel, next_spoke) {
		chk = TAILQ_FIRST(&strm->outqueue);
		while (chk) {
			nchk = TAILQ_NEXT(chk, sctp_next);
			if ((chk->send_size + SCTP_MED_OVERHEAD) > nxtsz) {
				sctp_split_chunks(&stcb->asoc, strm, chk);
			}
			chk = nchk;
		}
	}
}

static void
sctp_notify_mbuf(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net,
    struct ip *ip,
    struct sctphdr *sh)
{
	struct icmp *icmph;
	int totsz, tmr_stopped = 0;
	uint16_t nxtsz;

	/* protection */
	if ((inp == NULL) || (stcb == NULL) || (net == NULL) ||
	    (ip == NULL) || (sh == NULL)) {
		if (stcb != NULL)
			SCTP_TCB_UNLOCK(stcb);
		return;
	}
	/* First job is to verify the vtag matches what I would send */
	if (ntohl(sh->v_tag) != (stcb->asoc.peer_vtag)) {
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	icmph = (struct icmp *)((caddr_t)ip - (sizeof(struct icmp) -
	    sizeof(struct ip)));
	if (icmph->icmp_type != ICMP_UNREACH) {
		/* We only care about unreachable */
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	if (icmph->icmp_code != ICMP_UNREACH_NEEDFRAG) {
		/* not a unreachable message due to frag. */
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	totsz = ip->ip_len;

	nxtsz = ntohs(icmph->icmp_seq);
	if (nxtsz == 0) {
		/*
		 * old type router that does not tell us what the next size
		 * mtu is. Rats we will have to guess (in a educated fashion
		 * of course)
		 */
		nxtsz = find_next_best_mtu(totsz);
	}
	/* Stop any PMTU timer */
	if (callout_pending(&net->pmtu_timer.timer)) {
		tmr_stopped = 1;
		sctp_timer_stop(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net);
	}
	/* Adjust destination size limit */
	if (net->mtu > nxtsz) {
		net->mtu = nxtsz;
	}
	/* now what about the ep? */
	if (stcb->asoc.smallest_mtu > nxtsz) {
		sctp_pathmtu_adustment(inp, stcb, net, nxtsz);
	}
	if (tmr_stopped)
		sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net);

	SCTP_TCB_UNLOCK(stcb);
}


void
sctp_notify(struct sctp_inpcb *inp,
    int errno,
    struct sctphdr *sh,
    struct sockaddr *to,
    struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	/* protection */
	if ((inp == NULL) || (stcb == NULL) || (net == NULL) ||
	    (sh == NULL) || (to == NULL)) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("sctp-notify, bad call\n");
		}
#endif				/* SCTP_DEBUG */
		return;
	}
	/* First job is to verify the vtag matches what I would send */
	if (ntohl(sh->v_tag) != (stcb->asoc.peer_vtag)) {
		return;
	}
	/* FIX ME FIX ME PROTOPT i.e. no SCTP should ALWAYS be an ABORT */

	if ((errno == EHOSTUNREACH) ||	/* Host is not reachable */
	    (errno == EHOSTDOWN) ||	/* Host is down */
	    (errno == ECONNREFUSED) ||	/* Host refused the connection, (not
					 * an abort?) */
	    (errno == ENOPROTOOPT)	/* SCTP is not present on host */
	    ) {
		/*
		 * Hmm reachablity problems we must examine closely. If its
		 * not reachable, we may have lost a network. Or if there is
		 * NO protocol at the other end named SCTP. well we consider
		 * it a OOTB abort.
		 */
		if ((errno == EHOSTUNREACH) || (errno == EHOSTDOWN)) {
			if (net->dest_state & SCTP_ADDR_REACHABLE) {
				/* Ok that destination is NOT reachable */
				net->dest_state &= ~SCTP_ADDR_REACHABLE;
				net->dest_state |= SCTP_ADDR_NOT_REACHABLE;
				net->error_count = net->failure_threshold + 1;
				sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_DOWN,
				    stcb, SCTP_FAILED_THRESHOLD,
				    (void *)net);
			}
			if (stcb)
				SCTP_TCB_UNLOCK(stcb);
		} else {
			/*
			 * Here the peer is either playing tricks on us,
			 * including an address that belongs to someone who
			 * does not support SCTP OR was a userland
			 * implementation that shutdown and now is dead. In
			 * either case treat it like a OOTB abort with no
			 * TCB
			 */
			sctp_abort_notification(stcb, SCTP_PEER_FAULTY);
			sctp_free_assoc(inp, stcb, 0);
			/* no need to unlock here, since the TCB is gone */
		}
	} else {
		/* Send all others to the app */
		if (stcb)
			SCTP_TCB_UNLOCK(stcb);


		if (inp->sctp_socket) {
#ifdef SCTP_LOCK_LOGGING
			sctp_log_lock(inp, stcb, SCTP_LOG_LOCK_SOCK);
#endif
			SOCK_LOCK(inp->sctp_socket);
			inp->sctp_socket->so_error = errno;
			sctp_sowwakeup(inp, inp->sctp_socket);
			SOCK_UNLOCK(inp->sctp_socket);
		}
	}
}

#if defined(__FreeBSD__) || defined(__APPLE__)
void
#else
void *
#endif
sctp_ctlinput(cmd, sa, vip)
	int cmd;
	struct sockaddr *sa;
	void *vip;
{
	struct ip *ip = vip;
	struct sctphdr *sh;
	int s;


	if (sa->sa_family != AF_INET ||
	    ((struct sockaddr_in *)sa)->sin_addr.s_addr == INADDR_ANY) {
#if defined(__FreeBSD__) || defined(__APPLE__)
		return;
#else
		return (NULL);
#endif
	}
	if (PRC_IS_REDIRECT(cmd)) {
		ip = 0;
	} else if ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0) {
#if defined(__FreeBSD__) || defined(__APPLE__)
		return;
#else
		return (NULL);
#endif
	}
	if (ip) {
		struct sctp_inpcb *inp;
		struct sctp_tcb *stcb;
		struct sctp_nets *net;
		struct sockaddr_in to, from;

		sh = (struct sctphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		bzero(&to, sizeof(to));
		bzero(&from, sizeof(from));
		from.sin_family = to.sin_family = AF_INET;
		from.sin_len = to.sin_len = sizeof(to);
		from.sin_port = sh->src_port;
		from.sin_addr = ip->ip_src;
		to.sin_port = sh->dest_port;
		to.sin_addr = ip->ip_dst;

		/*
		 * 'to' holds the dest of the packet that failed to be sent.
		 * 'from' holds our local endpoint address. Thus we reverse
		 * the to and the from in the lookup.
		 */
#if defined(__NetBSD__) || defined(__OpenBSD__)
		s = splsoftnet();
#else
		s = splnet();
#endif
		stcb = sctp_findassociation_addr_sa((struct sockaddr *)&from,
		    (struct sockaddr *)&to,
		    &inp, &net, 1);
		if (stcb != NULL && inp && (inp->sctp_socket != NULL)) {
			if (cmd != PRC_MSGSIZE) {
				int cm;

				if (cmd == PRC_HOSTDEAD) {
					cm = EHOSTUNREACH;
				} else {
					cm = inetctlerrmap[cmd];
				}
				sctp_notify(inp, cm, sh,
				    (struct sockaddr *)&to, stcb,
				    net);
			} else {
				/* handle possible ICMP size messages */
				sctp_notify_mbuf(inp, stcb, net, ip, sh);
			}
		} else {
#if defined(__FreeBSD__) && __FreeBSD_version < 500000
			/*
			 * XXX must be fixed for 5.x and higher, leave for
			 * 4.x
			 */
			if (PRC_IS_REDIRECT(cmd) && inp) {
				in_rtchange((struct inpcb *)inp,
				    inetctlerrmap[cmd]);
			}
#endif
			if ((stcb == NULL) && (inp != NULL)) {
				/* reduce ref-count */
				SCTP_INP_WLOCK(inp);
				SCTP_INP_DECR_REF(inp);
				SCTP_INP_WUNLOCK(inp);
			}
		}
		splx(s);
	}
#if defined(__FreeBSD__) || defined(__APPLE__)
	return;
#else
	return (NULL);
#endif
}

#if defined(__FreeBSD__)
static int
sctp_getcred(SYSCTL_HANDLER_ARGS)
{
	struct sockaddr_in addrs[2];
	struct sctp_inpcb *inp;
	struct sctp_nets *net;
	struct sctp_tcb *stcb;
	int error, s;

#if __FreeBSD_version >= 500000
	error = suser(req->td);
#else
	error = suser(req->p);
#endif
	if (error)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof(addrs));
	if (error)
		return (error);

	s = splnet();
	stcb = sctp_findassociation_addr_sa(sintosa(&addrs[0]),
	    sintosa(&addrs[1]),
	    &inp, &net, 1);
	if (stcb == NULL || inp == NULL || inp->sctp_socket == NULL) {
		if ((inp != NULL) && (stcb == NULL)) {
			/* reduce ref-count */
			SCTP_INP_WLOCK(inp);
			SCTP_INP_DECR_REF(inp);
			SCTP_INP_WUNLOCK(inp);
		}
		error = ENOENT;
		goto out;
	}
	error = SYSCTL_OUT(req, inp->sctp_socket->so_cred, sizeof(struct ucred));
	SCTP_TCB_UNLOCK(stcb);
out:
	splx(s);
	return (error);
}

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, getcred, CTLTYPE_OPAQUE | CTLFLAG_RW,
    0, 0, sctp_getcred, "S,ucred", "Get the ucred of a SCTP connection");
#endif				/* #if defined(__FreeBSD__) */

/*
 * sysctl definitions
 */
#if defined(__FreeBSD__) || defined (__APPLE__)

SYSCTL_INT(_net_inet_sctp, OID_AUTO, sendspace, CTLFLAG_RW,
    &sctp_sendspace, 0, "Maximum outgoing SCTP buffer size");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, recvspace, CTLFLAG_RW,
    &sctp_recvspace, 0, "Maximum incoming SCTP buffer size");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, auto_asconf, CTLFLAG_RW,
    &sctp_auto_asconf, 0, "Enable SCTP Auto-ASCONF");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, ecn_enable, CTLFLAG_RW,
    &sctp_ecn_enable, 0, "Enable SCTP ECN");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, ecn_nonce, CTLFLAG_RW,
    &sctp_ecn_nonce, 0, "Enable SCTP ECN Nonce");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, strict_sacks, CTLFLAG_RW,
    &sctp_strict_sacks, 0, "Enable SCTP Strict SACK checking");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, loopback_nocsum, CTLFLAG_RW,
    &sctp_no_csum_on_loopback, 0,
    "Enable NO Csum on packets sent on loopback");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, strict_init, CTLFLAG_RW,
    &sctp_strict_init, 0,
    "Enable strict INIT/INIT-ACK singleton enforcement");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, peer_chkoh, CTLFLAG_RW,
    &sctp_peer_chunk_oh, 0,
    "Amount to debit peers rwnd per chunk sent");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, maxburst, CTLFLAG_RW,
    &sctp_max_burst_default, 0,
    "Default max burst for sctp endpoints");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, maxchunks, CTLFLAG_RW,
    &sctp_max_chunks_on_queue, 0,
    "Default max chunks on queue per asoc");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, delayed_sack_time, CTLFLAG_RW,
    &sctp_delayed_sack_time_default, 0,
    "Default delayed SACK timer in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, heartbeat_interval, CTLFLAG_RW,
    &sctp_heartbeat_interval_default, 0,
    "Default heartbeat interval in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, pmtu_raise_time, CTLFLAG_RW,
    &sctp_pmtu_raise_time_default, 0,
    "Default PMTU raise timer in sec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, shutdown_guard_time, CTLFLAG_RW,
    &sctp_shutdown_guard_time_default, 0,
    "Default shutdown guard timer in sec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, secret_lifetime, CTLFLAG_RW,
    &sctp_secret_lifetime_default, 0,
    "Default secret lifetime in sec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, rto_max, CTLFLAG_RW,
    &sctp_rto_max_default, 0,
    "Default maximum retransmission timeout in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, rto_min, CTLFLAG_RW,
    &sctp_rto_min_default, 0,
    "Default minimum retransmission timeout in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, rto_initial, CTLFLAG_RW,
    &sctp_rto_initial_default, 0,
    "Default initial retransmission timeout in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, init_rto_max, CTLFLAG_RW,
    &sctp_init_rto_max_default, 0,
    "Default maximum retransmission timeout during association setup in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, valid_cookie_life, CTLFLAG_RW,
    &sctp_valid_cookie_life_default, 0,
    "Default cookie lifetime in sec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, init_rtx_max, CTLFLAG_RW,
    &sctp_init_rtx_max_default, 0,
    "Default maximum number of retransmission for INIT chunks");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, assoc_rtx_max, CTLFLAG_RW,
    &sctp_assoc_rtx_max_default, 0,
    "Default maximum number of retransmissions per association");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, path_rtx_max, CTLFLAG_RW,
    &sctp_path_rtx_max_default, 0,
    "Default maximum of retransmissions per path");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, nr_outgoing_streams, CTLFLAG_RW,
    &sctp_nr_outgoing_streams_default, 0,
    "Default number of outgoing streams");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, cmt_on_off, CTLFLAG_RW,
    &sctp_cmt_on_off, 0,
    "CMT ON/OFF flag");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, cwnd_maxburst, CTLFLAG_RW,
    &sctp_use_cwnd_based_maxburst, 0,
    "Use a CWND adjusting maxburst");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, early_fast_retran, CTLFLAG_RW,
    &sctp_early_fr, 0,
    "Early Fast Retransmit with Timer");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, use_rttvar_congctrl, CTLFLAG_RW,
    &sctp_use_rttvar_cc, 0,
    "Use congestion control via rtt variation");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, deadlock_detect, CTLFLAG_RW,
    &sctp_says_check_for_deadlock, 0,
    "SMP Deadlock detection on/off");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, early_fast_retran_msec, CTLFLAG_RW,
    &sctp_early_fr_msec, 0,
    "Early Fast Retransmit minimum timer value");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, auth_disable, CTLFLAG_RW,
    &sctp_auth_disable, 0,
    "Disable SCTP AUTH chunk requirement/function");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, auth_random_len, CTLFLAG_RW,
    &sctp_auth_random_len, 0,
    "Length of AUTH RANDOMs");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, auth_hmac_id, CTLFLAG_RW,
    &sctp_auth_hmac_id_default, 0,
    "Default HMAC Id for SCTP AUTHenthication");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, abc_l_var, CTLFLAG_RW,
    &sctp_L2_abc_variable, 0,
    "SCTP ABC max increase per SACK (L)");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, max_chained_mbufs, CTLFLAG_RW,
    &sctp_mbuf_threshold_count, 0,
    "Default max number of small mbufs on a chain");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, cmt_use_dac, CTLFLAG_RW,
    &sctp_cmt_use_dac, 0,
    "CMT DAC ON/OFF flag");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, do_sctp_drain, CTLFLAG_RW,
    &sctp_do_drain, 0,
    "Should SCTP respond to the drain calls");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, warm_crc_table, CTLFLAG_RW,
    &sctp_warm_the_crc32_table, 0,
    "Should the CRC32c tables be warmed before checksum?");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, abort_at_limit, CTLFLAG_RW,
    &sctp_abort_if_one_2_one_hits_limit, 0,
    "When one-2-one hits qlimit abort");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, strict_data_order, CTLFLAG_RW,
    &sctp_strict_data_order, 0,
    "Enforce strict data ordering, abort if control inside data");

#ifdef SCTP_DEBUG
SYSCTL_INT(_net_inet_sctp, OID_AUTO, debug, CTLFLAG_RW,
    &sctp_debug_on, 0, "Configure debug output");
#endif				/* SCTP_DEBUG */
#endif

#if defined(__FreeBSD__) && __FreeBSD_version > 690000
static void
#else
static int
#endif
sctp_abort(struct socket *so)
{
	struct sctp_inpcb *inp;
	int s;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
#if defined(__FreeBSD__) && __FreeBSD_version > 690000
		return;
#else
		return EINVAL;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	sctp_inpcb_free(inp, 1);
	splx(s);
#if defined(__FreeBSD__) && __FreeBSD_version > 690000
	return;
#else
	return(0);
#endif
}

static int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_attach(struct socket *so, int proto, struct thread *p)
#else
sctp_attach(struct socket *so, int proto, struct proc *p)
#endif
{
	struct sctp_inpcb *inp;
	struct inpcb *ip_inp;
	int s, error;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp != 0) {
		splx(s);
		return EINVAL;
	}
	error = soreserve(so, sctp_sendspace, sctp_recvspace);
	if (error) {
		splx(s);
		return error;
	}
	error = sctp_inpcb_alloc(so);
	if (error) {
		splx(s);
		return error;
	}
	inp = (struct sctp_inpcb *)so->so_pcb;
	SCTP_INP_WLOCK(inp);

	inp->sctp_flags &= ~SCTP_PCB_FLAGS_BOUND_V6;	/* I'm not v6! */
	ip_inp = &inp->ip_inp.inp;
#if defined(__FreeBSD__) || defined(__APPLE__)
	ip_inp->inp_vflag |= INP_IPV4;
	ip_inp->inp_ip_ttl = ip_defttl;
#else
	inp->inp_vflag |= INP_IPV4;
	inp->inp_ip_ttl = ip_defttl;
#endif

#ifdef IPSEC
#if !(defined(__OpenBSD__) || defined(__APPLE__))
	error = ipsec_init_pcbpolicy(so, &ip_inp->inp_sp);
	if (error != 0) {
		sctp_inpcb_free(inp, 1);
		return error;
	}
#endif
#endif				/* IPSEC */
	SCTP_INP_WUNLOCK(inp);
#if defined(__NetBSD__)
	so->so_send = sctp_sosend;
	so->so_receive = sctp_soreceive;
#endif
	splx(s);
	return 0;
}

static int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_bind(struct socket *so, struct sockaddr *addr, struct thread *p)
{
#elif defined(__FreeBSD__) || defined(__APPLE__)
	sctp_bind(struct socket *so, struct sockaddr *addr, struct proc *p){
#else
sctp_bind(struct socket *so, struct mbuf *nam, struct proc *p)
{
	struct sockaddr *addr = nam ? mtod(nam, struct sockaddr *): NULL;

#endif
	struct sctp_inpcb *inp;
	int s, error;

#ifdef INET6
	if (addr && addr->sa_family != AF_INET)
		/* must be a v4 address! */
		return EINVAL;
#endif				/* INET6 */

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
		return EINVAL;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	error = sctp_inpcb_bind(so, addr, p);
	splx(s);
	return error;
}


#if defined(__FreeBSD__) && __FreeBSD_version > 690000
static void
#else
static int
#endif
sctp_detach(struct socket *so)
{
	struct sctp_inpcb *inp;
	int s;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
#if defined(__FreeBSD__) && __FreeBSD_version > 690000
		return;
#else
		return EINVAL;
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	if (((so->so_options & SO_LINGER) && (so->so_linger == 0)) ||
	    (so->so_rcv.sb_cc > 0)) {
		sctp_inpcb_free(inp, 1);
	} else {
		sctp_inpcb_free(inp, 0);
	}
	splx(s);
#if defined(__FreeBSD__) && __FreeBSD_version > 690000
	return;
#else
	return(0);
#endif
}

int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_sendm(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *p);

#else
sctp_sendm(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct proc *p);

#endif

int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_sendm(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *p)
{
#else
sctp_sendm(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct proc *p)
{
#endif
	struct sctp_inpcb *inp;
	int error;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		if (control) {
			sctp_m_freem(control);
			control = NULL;
		}
		sctp_m_freem(m);
		return EINVAL;
	}
	/* Got to have an to address if we are NOT a connected socket */
	if ((addr == NULL) &&
	    ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE))
	    ) {
		goto connected_type;
	} else if (addr == NULL) {
		error = EDESTADDRREQ;
		sctp_m_freem(m);
		if (control) {
			sctp_m_freem(control);
			control = NULL;
		}
		return (error);
	}
#ifdef INET6
	if (addr->sa_family != AF_INET) {
		/* must be a v4 address! */
		sctp_m_freem(m);
		if (control) {
			sctp_m_freem(control);
			control = NULL;
		}
		error = EDESTADDRREQ;
		return EINVAL;
	}
#endif				/* INET6 */
connected_type:
	/* now what about control */
	if (control) {
		if (inp->control) {
			printf("huh? control set?\n");
			sctp_m_freem(inp->control);
			inp->control = NULL;
		}
		inp->control = control;
	}
	/* add it in possibly */
	if ((inp->pkt) && (inp->pkt->m_flags & M_PKTHDR)) {
		struct mbuf *x;
		int c_len;

		c_len = 0;
		/* How big is it */
		for (x = m; x; x = x->m_next) {
			c_len += x->m_len;
		}
		inp->pkt->m_pkthdr.len += c_len;
	}
	/* Place the data */
	if (inp->pkt) {
		inp->pkt_last->m_next = m;
		inp->pkt_last = m;
	} else {
		inp->pkt_last = inp->pkt = m;
	}
	if (
#if defined (__FreeBSD__) || defined(__APPLE__)
	/* FreeBSD uses a flag passed */
	    ((flags & PRUS_MORETOCOME) == 0)
#elif defined( __NetBSD__)
	/* NetBSD uses the so_state field */
	    ((so->so_state & SS_MORETOCOME) == 0)
#else
	    1			/* Open BSD does not have any "more to come"
				 * indication */
#endif
	    ) {
		/*
		 * note with the current version this code will only be used
		 * by OpenBSD-- NetBSD, FreeBSD, and MacOS have methods for
		 * re-defining sosend to use the sctp_sosend. One can
		 * optionally switch back to this code (by changing back the
		 * definitions) but this is not advisable. This code is used
		 * by FreeBSD when sending a file with sendfile() though.
		 */
		int ret;

		ret = sctp_output(inp, inp->pkt, addr, inp->control, p, flags);
		inp->pkt = NULL;
		inp->control = NULL;
		return (ret);
	} else {
		return (0);
	}
}

static int
sctp_disconnect(struct socket *so)
{
	struct sctp_inpcb *inp;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == NULL) {
		splx(s);
		return (ENOTCONN);
	}
	SCTP_INP_RLOCK(inp);
	if (inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		if (LIST_EMPTY(&inp->sctp_asoc_list)) {
			/* No connection */
			splx(s);
			SCTP_INP_RUNLOCK(inp);
			return (0);
		} else {
			int some_on_streamwheel = 0;
			struct sctp_association *asoc;
			struct sctp_tcb *stcb;

			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb == NULL) {
				splx(s);
				SCTP_INP_RUNLOCK(inp);
				return (EINVAL);
			}
			SCTP_TCB_LOCK(stcb);
			asoc = &stcb->asoc;
			if (((so->so_options & SO_LINGER) &&
			    (so->so_linger == 0)) ||
			    (so->so_rcv.sb_cc > 0)) {
				if (SCTP_GET_STATE(asoc) !=
				    SCTP_STATE_COOKIE_WAIT) {
					/* Left with Data unread */
					struct mbuf *err;

					err = NULL;
					MGET(err, M_DONTWAIT, MT_DATA);
					if (err) {
						/*
						 * Fill in the user
						 * initiated abort
						 */
						struct sctp_paramhdr *ph;

						ph = mtod(err, struct sctp_paramhdr *);
						err->m_len = sizeof(struct sctp_paramhdr);
						ph->param_type = htons(SCTP_CAUSE_USER_INITIATED_ABT);
						ph->param_length = htons(err->m_len);
					}
					sctp_send_abort_tcb(stcb, err);
				}
				SCTP_INP_RUNLOCK(inp);
				sctp_free_assoc(inp, stcb, 0);
				/* No unlock tcb assoc is gone */
				splx(s);
				return (0);
			}
			if (!TAILQ_EMPTY(&asoc->out_wheel)) {
				/* Check to see if some data queued */
				struct sctp_stream_out *outs;

				TAILQ_FOREACH(outs, &asoc->out_wheel,
				    next_spoke) {
					if (!TAILQ_EMPTY(&outs->outqueue)) {
						some_on_streamwheel = 1;
						break;
					}
				}
			}
			if (TAILQ_EMPTY(&asoc->send_queue) &&
			    TAILQ_EMPTY(&asoc->sent_queue) &&
			    (some_on_streamwheel == 0)) {
				/* there is nothing queued to send, so done */
				if ((SCTP_GET_STATE(asoc) !=
				    SCTP_STATE_SHUTDOWN_SENT) &&
				    (SCTP_GET_STATE(asoc) !=
				    SCTP_STATE_SHUTDOWN_ACK_SENT)) {
					/* only send SHUTDOWN 1st time thru */
#ifdef SCTP_DEBUG
					if (sctp_debug_on & SCTP_DEBUG_OUTPUT4) {
						printf("%s:%d sends a shutdown\n",
						    __FILE__,
						    __LINE__
						    );
					}
#endif
					sctp_send_shutdown(stcb,
					    stcb->asoc.primary_destination);
					sctp_chunk_output(stcb->sctp_ep, stcb, SCTP_OUTPUT_FROM_T3);
					asoc->state = SCTP_STATE_SHUTDOWN_SENT;
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
					    stcb->sctp_ep, stcb,
					    asoc->primary_destination);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
					    stcb->sctp_ep, stcb,
					    asoc->primary_destination);
				}
			} else {
				/*
				 * we still got (or just got) data to send,
				 * so set SHUTDOWN_PENDING
				 */
				/*
				 * XXX sockets draft says that SCTP_EOF
				 * should be sent with no data. currently,
				 * we will allow user data to be sent first
				 * and move to SHUTDOWN-PENDING
				 */
				asoc->state |= SCTP_STATE_SHUTDOWN_PENDING;
			}
			SCTP_TCB_UNLOCK(stcb);
			SCTP_INP_RUNLOCK(inp);
			splx(s);
			return (0);
		}
		/* not reached */
	} else {
		/* UDP model does not support this */
		SCTP_INP_RUNLOCK(inp);
		splx(s);
		return EOPNOTSUPP;
	}
}

int
sctp_shutdown(struct socket *so)
{
	struct sctp_inpcb *inp;
	int s;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		splx(s);
		return EINVAL;
	}
	SCTP_INP_RLOCK(inp);
	/* For UDP model this is a invalid call */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE) {
		/* Restore the flags that the soshutdown took away. */
#if defined(__FreeBSD__) && __FreeBSD_version >= 502115
		so->so_rcv.sb_state &= ~SBS_CANTRCVMORE;
#else
		so->so_state &= ~SS_CANTRCVMORE;
#endif
		/* This proc will wakeup for read and do nothing (I hope) */
		splx(s);
		SCTP_INP_RUNLOCK(inp);
		return (EOPNOTSUPP);
	}
	/*
	 * Ok if we reach here its the TCP model and it is either a SHUT_WR
	 * or SHUT_RDWR. This means we put the shutdown flag against it.
	 */
	{
		int some_on_streamwheel = 0;
		struct sctp_tcb *stcb;
		struct sctp_association *asoc;

		socantsendmore(so);

		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		if (stcb == NULL) {
			/*
			 * Ok we hit the case that the shutdown call was
			 * made after an abort or something. Nothing to do
			 * now.
			 */
			splx(s);
			return (0);
		}
		SCTP_TCB_LOCK(stcb);
		asoc = &stcb->asoc;

		if (!TAILQ_EMPTY(&asoc->out_wheel)) {
			/* Check to see if some data queued */
			struct sctp_stream_out *outs;

			TAILQ_FOREACH(outs, &asoc->out_wheel, next_spoke) {
				if (!TAILQ_EMPTY(&outs->outqueue)) {
					some_on_streamwheel = 1;
					break;
				}
			}
		}
		if (TAILQ_EMPTY(&asoc->send_queue) &&
		    TAILQ_EMPTY(&asoc->sent_queue) &&
		    (some_on_streamwheel == 0)) {
			/* there is nothing queued to send, so I'm done... */
			if (SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_SENT) {
				/* only send SHUTDOWN the first time through */
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_OUTPUT4) {
					printf("%s:%d sends a shutdown\n",
					    __FILE__,
					    __LINE__
					    );
				}
#endif
				sctp_send_shutdown(stcb,
				    stcb->asoc.primary_destination);
				sctp_chunk_output(stcb->sctp_ep, stcb, SCTP_OUTPUT_FROM_T3);
				asoc->state = SCTP_STATE_SHUTDOWN_SENT;
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN,
				    stcb->sctp_ep, stcb,
				    asoc->primary_destination);
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
				    stcb->sctp_ep, stcb,
				    asoc->primary_destination);
			}
		} else {
			/*
			 * we still got (or just got) data to send, so set
			 * SHUTDOWN_PENDING
			 */
			asoc->state |= SCTP_STATE_SHUTDOWN_PENDING;
		}
		SCTP_TCB_UNLOCK(stcb);
	}
	SCTP_INP_RUNLOCK(inp);
	splx(s);
	return 0;
}

/*
 * copies a "user" presentable address and removes embedded scope, etc.
 * returns 0 on success, 1 on error
 */
static uint32_t
sctp_fill_user_address(struct sockaddr_storage *ss, struct sockaddr *sa)
{
	struct sockaddr_in6 lsa6;

	sa = (struct sockaddr *)sctp_recover_scope((struct sockaddr_in6 *)sa,
	    &lsa6);
	memcpy(ss, sa, sa->sa_len);
	return (0);
}


#if defined(__NetBSD__) || defined(__OpenBSD__)
/*
 * On NetBSD and OpenBSD in6_sin_2_v4mapsin6() not used and not exported, so
 * we have to export it here.
 */
void in6_sin_2_v4mapsin6
__P((struct sockaddr_in *sin,
    struct sockaddr_in6 *sin6));

#endif

	static int
	    sctp_fill_up_addresses(struct sctp_inpcb *inp,
        struct sctp_tcb *stcb,
        int limit,
        struct sockaddr_storage *sas)
{
	struct ifnet *ifn;
	struct ifaddr *ifa;
	int loopback_scope, ipv4_local_scope, local_scope, site_scope, actual;
	int ipv4_addr_legal, ipv6_addr_legal;

	actual = 0;
	if (limit <= 0)
		return (actual);

	if (stcb) {
		/* Turn on all the appropriate scope */
		loopback_scope = stcb->asoc.loopback_scope;
		ipv4_local_scope = stcb->asoc.ipv4_local_scope;
		local_scope = stcb->asoc.local_scope;
		site_scope = stcb->asoc.site_scope;
	} else {
		/* Turn on ALL scope, since we look at the EP */
		loopback_scope = ipv4_local_scope = local_scope =
		    site_scope = 1;
	}
	ipv4_addr_legal = ipv6_addr_legal = 0;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
		ipv6_addr_legal = 1;
		if (
#if defined(__OpenBSD__)
		    (0)		/* we always do dual bind */
#elif defined (__NetBSD__)
		    (((struct in6pcb *)inp)->in6p_flags & IN6P_IPV6_V6ONLY)
#else
		    (((struct in6pcb *)inp)->inp_flags & IN6P_IPV6_V6ONLY)
#endif
		    == 0) {
			ipv4_addr_legal = 1;
		}
	} else {
		ipv4_addr_legal = 1;
	}

	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		TAILQ_FOREACH(ifn, &ifnet, if_list) {
			if ((loopback_scope == 0) &&
			    (ifn->if_type == IFT_LOOP)) {
				/* Skip loopback if loopback_scope not set */
				continue;
			}
			TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
				if (stcb) {
					/*
					 * For the BOUND-ALL case, the list
					 * associated with a TCB is Always
					 * considered a reverse list.. i.e.
					 * it lists addresses that are NOT
					 * part of the association. If this
					 * is one of those we must skip it.
					 */
					if (sctp_is_addr_restricted(stcb,
					    ifa->ifa_addr)) {
						continue;
					}
				}
				if ((ifa->ifa_addr->sa_family == AF_INET) &&
				    (ipv4_addr_legal)) {
					struct sockaddr_in *sin;

					sin = (struct sockaddr_in *)ifa->ifa_addr;
					if (sin->sin_addr.s_addr == 0) {
						/*
						 * we skip unspecifed
						 * addresses
						 */
						continue;
					}
					if ((ipv4_local_scope == 0) &&
					    (IN4_ISPRIVATE_ADDRESS(&sin->sin_addr))) {
						continue;
					}
					if (inp->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) {
						in6_sin_2_v4mapsin6(sin, (struct sockaddr_in6 *)sas);
						((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
						sas = (struct sockaddr_storage *)((caddr_t)sas + sizeof(struct sockaddr_in6));
						actual += sizeof(sizeof(struct sockaddr_in6));
					} else {
						memcpy(sas, sin, sizeof(*sin));
						((struct sockaddr_in *)sas)->sin_port = inp->sctp_lport;
						sas = (struct sockaddr_storage *)((caddr_t)sas + sizeof(*sin));
						actual += sizeof(*sin);
					}
					if (actual >= limit) {
						return (actual);
					}
				} else if ((ifa->ifa_addr->sa_family == AF_INET6) &&
				    (ipv6_addr_legal)) {
					struct sockaddr_in6 *sin6;

#ifndef SCTP_KAME
					struct sockaddr_in6 lsa6;

#endif
					sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
					if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
						/*
						 * we skip unspecifed
						 * addresses
						 */
						continue;
					}
					if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
						if (local_scope == 0)
							continue;
						if (sin6->sin6_scope_id == 0) {
#ifdef SCTP_KAME
							if (sa6_recoverscope(sin6) != 0)
								/*
								 * bad link
								 * local
								 * address
								 */
								continue;
#else
							lsa6 = *sin6;
							if (in6_recoverscope(&lsa6,
							    &lsa6.sin6_addr,
							    NULL))
								/*
								 * bad link
								 * local
								 * address
								 */
								continue;
							sin6 = &lsa6;
#endif				/* SCTP_KAME */
						}
					}
					if ((site_scope == 0) &&
					    (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr))) {
						continue;
					}
					memcpy(sas, sin6, sizeof(*sin6));
					((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
					sas = (struct sockaddr_storage *)((caddr_t)sas + sizeof(*sin6));
					actual += sizeof(*sin6);
					if (actual >= limit) {
						return (actual);
					}
				}
			}
		}
	} else {
		struct sctp_laddr *laddr;

		/*
		 * If we have a TCB and we do NOT support ASCONF (it's
		 * turned off or otherwise) then the list is always the true
		 * list of addresses (the else case below).  Otherwise the
		 * list on the association is a list of addresses that are
		 * NOT part of the association.
		 */
		if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_DO_ASCONF)) {
			/* The list is a NEGATIVE list */
			LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
				if (stcb) {
					if (sctp_is_addr_restricted(stcb, laddr->ifa->ifa_addr)) {
						continue;
					}
				}
				if (sctp_fill_user_address(sas, laddr->ifa->ifa_addr))
					continue;

				((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
				sas = (struct sockaddr_storage *)((caddr_t)sas +
				    laddr->ifa->ifa_addr->sa_len);
				actual += laddr->ifa->ifa_addr->sa_len;
				if (actual >= limit) {
					return (actual);
				}
			}
		} else {
			/* The list is a positive list if present */
			if (stcb) {
				/* Must use the specific association list */
				LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list,
				    sctp_nxt_addr) {
					if (sctp_fill_user_address(sas,
					    laddr->ifa->ifa_addr))
						continue;
					((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
					sas = (struct sockaddr_storage *)((caddr_t)sas +
					    laddr->ifa->ifa_addr->sa_len);
					actual += laddr->ifa->ifa_addr->sa_len;
					if (actual >= limit) {
						return (actual);
					}
				}
			} else {
				/*
				 * No endpoint so use the endpoints
				 * individual list
				 */
				LIST_FOREACH(laddr, &inp->sctp_addr_list,
				    sctp_nxt_addr) {
					if (sctp_fill_user_address(sas,
					    laddr->ifa->ifa_addr))
						continue;
					((struct sockaddr_in6 *)sas)->sin6_port = inp->sctp_lport;
					sas = (struct sockaddr_storage *)((caddr_t)sas +
					    laddr->ifa->ifa_addr->sa_len);
					actual += laddr->ifa->ifa_addr->sa_len;
					if (actual >= limit) {
						return (actual);
					}
				}
			}
		}
	}
	return (actual);
}

static int
sctp_count_max_addresses(struct sctp_inpcb *inp)
{
	int cnt = 0;

	/*
	 * In both sub-set bound an bound_all cases we return the MAXIMUM
	 * number of addresses that you COULD get. In reality the sub-set
	 * bound may have an exclusion list for a given TCB OR in the
	 * bound-all case a TCB may NOT include the loopback or other
	 * addresses as well.
	 */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		struct ifnet *ifn;
		struct ifaddr *ifa;

		TAILQ_FOREACH(ifn, &ifnet, if_list) {
			TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
				/* Count them if they are the right type */
				if (ifa->ifa_addr->sa_family == AF_INET) {
					if (inp->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4)
						cnt += sizeof(struct sockaddr_in6);
					else
						cnt += sizeof(struct sockaddr_in);

				} else if (ifa->ifa_addr->sa_family == AF_INET6)
					cnt += sizeof(struct sockaddr_in6);
			}
		}
	} else {
		struct sctp_laddr *laddr;

		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa->ifa_addr->sa_family == AF_INET) {
				if (inp->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4)
					cnt += sizeof(struct sockaddr_in6);
				else
					cnt += sizeof(struct sockaddr_in);

			} else if (laddr->ifa->ifa_addr->sa_family == AF_INET6)
				cnt += sizeof(struct sockaddr_in6);
		}
	}
	return (cnt);
}

static int
sctp_do_connect_x(struct socket *so,
    struct sctp_inpcb *inp,
    struct mbuf *m,
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
    struct thread *p,
#else
    struct proc *p,
#endif
    int delay
)
{
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int s = splsoftnet();

#else
	int s = splnet();

#endif
	int error = 0;
	struct sctp_tcb *stcb = NULL;
	struct sockaddr *sa;
	int num_v6 = 0, num_v4 = 0, *totaddrp, totaddr, i, incr, at;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Connectx called\n");
	}
#endif				/* SCTP_DEBUG */

	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		/* We are already connected AND the TCP model */
		splx(s);
		return (EADDRINUSE);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
		SCTP_INP_RLOCK(inp);
		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		SCTP_INP_RUNLOCK(inp);
	}
	if (stcb) {
		splx(s);
		return (EALREADY);

	}
	SCTP_ASOC_CREATE_LOCK(inp);
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE)) {
		SCTP_ASOC_CREATE_UNLOCK(inp);
		splx(s);
		return (EFAULT);
	}
	totaddrp = mtod(m, int *);
	totaddr = *totaddrp;
	sa = (struct sockaddr *)(totaddrp + 1);
	at = incr = 0;
	/* account and validate addresses */
	SCTP_INP_WLOCK(inp);
	SCTP_INP_INCR_REF(inp);
	SCTP_INP_WUNLOCK(inp);
	for (i = 0; i < totaddr; i++) {
		if (sa->sa_family == AF_INET) {
			num_v4++;
			incr = sizeof(struct sockaddr_in);
		} else if (sa->sa_family == AF_INET6) {
			struct sockaddr_in6 *sin6;

			sin6 = (struct sockaddr_in6 *)sa;
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
				/* Must be non-mapped for connectx */
				SCTP_ASOC_CREATE_UNLOCK(inp);
				splx(s);
				return EINVAL;
			}
			num_v6++;
			incr = sizeof(struct sockaddr_in6);
		} else {
			totaddr = i;
			break;
		}
		stcb = sctp_findassociation_ep_addr(&inp, sa, NULL, NULL, NULL);
		if (stcb != NULL) {
			/* Already have or am bring up an association */
			SCTP_ASOC_CREATE_UNLOCK(inp);
			SCTP_TCB_UNLOCK(stcb);
			splx(s);
			return (EALREADY);
		}
		if ((at + incr) > m->m_len) {
			totaddr = i;
			break;
		}
		sa = (struct sockaddr *)((caddr_t)sa + incr);
	}
	sa = (struct sockaddr *)(totaddrp + 1);
	SCTP_INP_WLOCK(inp);
	SCTP_INP_DECR_REF(inp);
	SCTP_INP_WUNLOCK(inp);
#ifdef INET6
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0) &&
	    (num_v6 > 0)) {
		splx(s);
		SCTP_INP_WUNLOCK(inp);
		SCTP_ASOC_CREATE_UNLOCK(inp);
		return (EINVAL);
	}
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
	    (num_v4 > 0)) {
		struct in6pcb *inp6;

		inp6 = (struct in6pcb *)inp;
		if (
#if defined(__OpenBSD__)
		    (0)		/* we always do dual bind */
#elif defined (__NetBSD__)
		    (inp6->in6p_flags & IN6P_IPV6_V6ONLY)
#else
		    (inp6->inp_flags & IN6P_IPV6_V6ONLY)
#endif
		    ) {
			/*
			 * if IPV6_V6ONLY flag, ignore connections destined
			 * to a v4 addr or v4-mapped addr
			 */
			SCTP_INP_WUNLOCK(inp);
			SCTP_ASOC_CREATE_UNLOCK(inp);
			splx(s);
			return EINVAL;
		}
	}
#endif				/* INET6 */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) ==
	    SCTP_PCB_FLAGS_UNBOUND) {
		/* Bind a ephemeral port */
		SCTP_INP_WUNLOCK(inp);
		error = sctp_inpcb_bind(so, NULL, p);
		if (error) {
			SCTP_ASOC_CREATE_UNLOCK(inp);
			splx(s);
			return (error);
		}
	} else {
		SCTP_INP_WUNLOCK(inp);
	}
	/* We are GOOD to go */
	stcb = sctp_aloc_assoc(inp, sa, 1, &error, 0);
	if (stcb == NULL) {
		/* Gak! no memory */
		SCTP_ASOC_CREATE_UNLOCK(inp);
		splx(s);
		return (error);
	}
	/* move to second address */
	if (sa->sa_family == AF_INET)
		sa = (struct sockaddr *)((caddr_t)sa + sizeof(struct sockaddr_in));
	else
		sa = (struct sockaddr *)((caddr_t)sa + sizeof(struct sockaddr_in6));

	for (i = 1; i < totaddr; i++) {
		if (sa->sa_family == AF_INET) {
			incr = sizeof(struct sockaddr_in);
			if (sctp_add_remote_addr(stcb, sa, 0, 8)) {
				/* assoc gone no un-lock */
				sctp_free_assoc(inp, stcb, 0);
				SCTP_ASOC_CREATE_UNLOCK(inp);
				splx(s);
				return (ENOBUFS);
			}
		} else if (sa->sa_family == AF_INET6) {
			incr = sizeof(struct sockaddr_in6);
			if (sctp_add_remote_addr(stcb, sa, 0, 8)) {
				/* assoc gone no un-lock */
				sctp_free_assoc(inp, stcb, 0);
				SCTP_ASOC_CREATE_UNLOCK(inp);
				splx(s);
				return (ENOBUFS);
			}
		}
		sa = (struct sockaddr *)((caddr_t)sa + incr);
	}
	stcb->asoc.state = SCTP_STATE_COOKIE_WAIT;
	/*
	 * initialize authentication parameters for the assoc
	 */
	/* generate a RANDOM for this assoc */
	stcb->asoc.authinfo.random =
	    sctp_generate_random_key(sctp_auth_random_len);
	/* initialize hmac list from endpoint */
	stcb->asoc.local_hmacs = sctp_copy_hmaclist(inp->sctp_ep.local_hmacs);
	/* initialize auth chunks list from endpoint */
	stcb->asoc.local_auth_chunks =
	    sctp_copy_chunklist(inp->sctp_ep.local_auth_chunks);
	/* copy defaults from the endpoint */
	stcb->asoc.authinfo.assoc_keyid = inp->sctp_ep.default_keyid;

	if (delay) {
		/* doing delayed connection */
		stcb->asoc.delayed_connection = 1;
		sctp_timer_start(SCTP_TIMER_TYPE_INIT, inp, stcb, stcb->asoc.primary_destination);
	} else {
		SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_entered);
		sctp_send_initiate(inp, stcb);
	}
	SCTP_TCB_UNLOCK(stcb);
	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		stcb->sctp_ep->sctp_flags |= SCTP_PCB_FLAGS_CONNECTED;
		/* Set the connected flag so we can queue data */
		soisconnecting(so);
	}
	SCTP_ASOC_CREATE_UNLOCK(inp);
	splx(s);
	return error;
}



static int
sctp_optsget(struct socket *so,
    int opt,
    struct mbuf **mp,
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
    struct thread *p
#else
    struct proc *p
#endif
)
{
	struct sctp_inpcb *inp;
	struct mbuf *m;
	int error, optval = 0;
	struct sctp_tcb *stcb = NULL;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
		return EINVAL;
	error = 0;

	if (mp == NULL) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("optsget:MP is NULL EINVAL\n");
		}
#endif				/* SCTP_DEBUG */
		return (EINVAL);
	}
	m = *mp;
	if (m == NULL) {
		/* Got to have a mbuf */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("Huh no mbuf\n");
		}
#endif				/* SCTP_DEBUG */
		return (EINVAL);
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
		printf("optsget opt:0x%lx sz:%u\n", (unsigned long)opt,
		    m->m_len);
	}
#endif				/* SCTP_DEBUG */

	switch (opt) {
	case SCTP_NODELAY:
	case SCTP_AUTOCLOSE:
	case SCTP_AUTO_ASCONF:
	case SCTP_DISABLE_FRAGMENTS:
	case SCTP_I_WANT_MAPPED_V4_ADDR:
	case SCTP_USE_EXT_RCVINFO:
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
			printf("other stuff\n");
		}
#endif				/* SCTP_DEBUG */
		SCTP_INP_RLOCK(inp);
		switch (opt) {
		case SCTP_DISABLE_FRAGMENTS:
			optval = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NO_FRAGMENT);
			break;
		case SCTP_I_WANT_MAPPED_V4_ADDR:
			optval = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NEEDS_MAPPED_V4);
			break;
		case SCTP_AUTO_ASCONF:
			optval = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_AUTO_ASCONF);
			break;
		case SCTP_NODELAY:
			optval = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_NODELAY);
			break;
		case SCTP_USE_EXT_RCVINFO:			
			optval = sctp_is_feature_on(inp, SCTP_PCB_FLAGS_EXT_RCVINFO);
			break;
		case SCTP_AUTOCLOSE:
			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_AUTOCLOSE))
				optval = TICKS_TO_SEC(inp->sctp_ep.auto_close_time);
			else
				optval = 0;
			break;

		default:
			error = ENOPROTOOPT;
		}		/* end switch (sopt->sopt_name) */
		if (opt != SCTP_AUTOCLOSE) {
			/* make it an "on/off" value */
			optval = (optval != 0);
		}
		if ((size_t)m->m_len < sizeof(int)) {
			error = EINVAL;
		}
		SCTP_INP_RUNLOCK(inp);
		if (error == 0) {
			/* return the option value */
			*mtod(m, int *)= optval;
			m->m_len = sizeof(optval);
		}
		break;
	case SCTP_PARTIAL_DELIVERY_POINT:
		{
			if ((size_t)m->m_len < sizeof(unsigned int)) {
				error = EINVAL;
				break;
			}
			*mtod(m, unsigned int *)= inp->partial_delivery_point;
			m->m_len = sizeof(unsigned int);
		}
		break;
	case SCTP_FRAGMENT_INTERLEAVE:
		{
			if ((size_t)m->m_len < sizeof(unsigned int)) {
				error = EINVAL;
				break;
			}
			*mtod(m, unsigned int *)= sctp_is_feature_on(inp, SCTP_PCB_FLAGS_FRAG_INTERLEAVE);
			m->m_len = sizeof(unsigned int);
		}
		break;
	case SCTP_CMT_ON_OFF:
		{
			if ((size_t)m->m_len < sizeof(unsigned int)) {
				error = EINVAL;
				break;
			}
			*mtod(m, unsigned int *)= sctp_cmt_sockopt_on_off;
			m->m_len = sizeof(unsigned int);
		}
		break;
	case SCTP_CMT_USE_DAC:
		{
			*mtod(m, unsigned int *)= sctp_cmt_sockopt_use_dac;
			m->m_len = sizeof(unsigned int);
		}
		break;
	case SCTP_GET_ADDR_LEN:
		{
			struct sctp_assoc_value *av;

			if ((size_t)m->m_len < sizeof(struct sctp_assoc_value)) {
				error = EINVAL;
				break;
			}
			av = mtod(m, struct sctp_assoc_value *);
			error = EINVAL;
#ifdef AF_INET
			if (av->assoc_value == AF_INET) {
				av->assoc_value = sizeof(struct sockaddr_in);
				error = 0;
			}
#endif
#ifdef AF_INET6
			if (av->assoc_value == AF_INET6) {
				av->assoc_value = sizeof(struct sockaddr_in6);
				error = 0;
			}
#endif
		}
		break;
	case SCTP_GET_ASOC_ID_LIST:
		{
			struct sctp_assoc_ids *ids;
			int cnt, at;
			uint16_t orig;

			if ((size_t)m->m_len < sizeof(struct sctp_assoc_ids)) {
				error = EINVAL;
				break;
			}
			ids = mtod(m, struct sctp_assoc_ids *);
			cnt = 0;
			SCTP_INP_RLOCK(inp);
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb == NULL) {
		none_out_now:
				ids->asls_numb_present = 0;
				ids->asls_more_to_get = 0;
				SCTP_INP_RUNLOCK(inp);
				break;
			}
			orig = ids->asls_assoc_start;
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			while (orig) {
				stcb = LIST_NEXT(stcb, sctp_tcblist);
				orig--;
				cnt--;
				if (stcb == NULL)
					goto none_out_now;
			}
			if (stcb == NULL)
				goto none_out_now;

			at = 0;
			ids->asls_numb_present = 0;
			ids->asls_more_to_get = 1;
			while (at < MAX_ASOC_IDS_RET) {
				ids->asls_assoc_id[at] = sctp_get_associd(stcb);
				at++;
				ids->asls_numb_present++;
				stcb = LIST_NEXT(stcb, sctp_tcblist);
				if (stcb == NULL) {
					ids->asls_more_to_get = 0;
					break;
				}
			}
			SCTP_INP_RUNLOCK(inp);
		}
		break;
	case SCTP_CONTEXT:
		{

			struct sctp_assoc_value *av;

			if ((size_t)m->m_len < sizeof(struct sctp_assoc_value)) {
				error = EINVAL;
				break;
			}
			av = mtod(m, struct sctp_assoc_value *);
			if(av->assoc_id) {
				stcb = sctp_findassociation_ep_asocid(inp, av->assoc_id);
				if (stcb == NULL) {
					error = ENOTCONN;
				} else {
					av->assoc_value = stcb->asoc.context;
					SCTP_TCB_UNLOCK(stcb);
				}
			} else {
				av->assoc_value = inp->sctp_context;
			}
		}
		break;
	case SCTP_GET_NONCE_VALUES:
		{
			struct sctp_get_nonce_values *gnv;

			if ((size_t)m->m_len < sizeof(struct sctp_get_nonce_values)) {
				error = EINVAL;
				break;
			}
			gnv = mtod(m, struct sctp_get_nonce_values *);
			stcb = sctp_findassociation_ep_asocid(inp, gnv->gn_assoc_id);
			if (stcb == NULL) {
				error = ENOTCONN;
			} else {
				gnv->gn_peers_tag = stcb->asoc.peer_vtag;
				gnv->gn_local_tag = stcb->asoc.my_vtag;
				SCTP_TCB_UNLOCK(stcb);
			}

		}
		break;
	case SCTP_DELAYED_ACK_TIME:
		{
			struct sctp_assoc_value *tm;

			if ((size_t)m->m_len < sizeof(struct sctp_assoc_value)) {
				error = EINVAL;
				break;
			}
			tm = mtod(m, struct sctp_assoc_value *);

			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb) {
					SCTP_TCB_LOCK(stcb);
					tm->assoc_value = stcb->asoc.delayed_ack;
					SCTP_TCB_UNLOCK(stcb);
				} else {
					tm->assoc_value = TICKS_TO_MSEC(inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_RECV]);
				}
				SCTP_INP_RUNLOCK(inp);
			} else {
				stcb = sctp_findassociation_ep_asocid(inp, tm->assoc_id);
				if (stcb == NULL) {
					error = ENOTCONN;
					tm->assoc_value = 0;
				} else {
					stcb->asoc.delayed_ack = tm->assoc_value;
					SCTP_TCB_UNLOCK(stcb);
				}
			}
		}
		break;

	case SCTP_GET_SNDBUF_USE:
		if ((size_t)m->m_len < sizeof(struct sctp_sockstat)) {
			error = EINVAL;
		} else {
			struct sctp_sockstat *ss;
			struct sctp_tcb *stcb;
			struct sctp_association *asoc;

			ss = mtod(m, struct sctp_sockstat *);
			stcb = sctp_findassociation_ep_asocid(inp, ss->ss_assoc_id);
			if (stcb == NULL) {
				error = ENOTCONN;
			} else {
				asoc = &stcb->asoc;
				ss->ss_total_sndbuf = (uint32_t) asoc->total_output_queue_size;
				ss->ss_total_mbuf_sndbuf = (uint32_t) asoc->total_output_mbuf_queue_size;
				ss->ss_total_recv_buf = (uint32_t) (asoc->size_on_reasm_queue +
				    asoc->size_on_all_streams);
				SCTP_TCB_UNLOCK(stcb);
				error = 0;
				m->m_len = sizeof(struct sctp_sockstat);
			}
		}
		break;
	case SCTP_MAXBURST:
		{
			uint8_t *burst;

			burst = mtod(m, uint8_t *);
			SCTP_INP_RLOCK(inp);
			*burst = inp->sctp_ep.max_burst;
			SCTP_INP_RUNLOCK(inp);
			m->m_len = sizeof(uint8_t);
		}
		break;

	case SCTP_MAXSEG:
		{
			uint32_t *segsize;
			sctp_assoc_t *assoc_id;
			int ovh;

			if ((size_t)m->m_len < sizeof(uint32_t)) {
				error = EINVAL;
				break;
			}
			if ((size_t)m->m_len < sizeof(sctp_assoc_t)) {
				error = EINVAL;
				break;
			}
			assoc_id = mtod(m, sctp_assoc_t *);
			segsize = mtod(m, uint32_t *);
			m->m_len = sizeof(uint32_t);

			if (((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
			    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) ||
			    (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {
				struct sctp_tcb *stcb;

				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb) {
					SCTP_TCB_LOCK(stcb);
					SCTP_INP_RUNLOCK(inp);
					*segsize = sctp_get_frag_point(stcb, &stcb->asoc);
					SCTP_TCB_UNLOCK(stcb);
				} else {
					SCTP_INP_RUNLOCK(inp);
					goto skipit;
				}
			} else {
				stcb = sctp_findassociation_ep_asocid(inp, *assoc_id);
				if (stcb) {
					*segsize = sctp_get_frag_point(stcb, &stcb->asoc);
					SCTP_TCB_UNLOCK(stcb);
					break;
				}
		skipit:
				/*
				 * default is to get the max, if I can't
				 * calculate from an existing association.
				 */
				if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
					ovh = SCTP_MED_OVERHEAD;
				} else {
					ovh = SCTP_MED_V4_OVERHEAD;
				}
				*segsize = inp->sctp_frag_point - ovh;
			}
		}
		break;

	case SCTP_SET_DEBUG_LEVEL:
#ifdef SCTP_DEBUG
		{
			uint32_t *level;

			if ((size_t)m->m_len < sizeof(uint32_t)) {
				error = EINVAL;
				break;
			}
			level = mtod(m, uint32_t *);
			error = 0;
			*level = sctp_debug_on;
			m->m_len = sizeof(uint32_t);
			printf("Returning DEBUG LEVEL %x is set\n",
			    (uint32_t) sctp_debug_on);
		}
#else				/* SCTP_DEBUG */
		error = EOPNOTSUPP;
#endif
		break;
	case SCTP_GET_STAT_LOG:
#ifdef SCTP_STAT_LOGGING
		error = sctp_fill_stat_log(m);
#else				/* SCTP_DEBUG */
		error = EOPNOTSUPP;
#endif
		break;
	case SCTP_GET_PEGS:
		{
			uint32_t *pt;

			if ((size_t)m->m_len < sizeof(sctp_pegs)) {
				error = EINVAL;
				break;
			}
			pt = mtod(m, uint32_t *);
			memcpy(pt, sctp_pegs, sizeof(sctp_pegs));
			m->m_len = sizeof(sctp_pegs);
		}
		break;
	case SCTP_EVENTS:
		{
			struct sctp_event_subscribe *events;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
				printf("get events\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(struct sctp_event_subscribe)) {
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
					printf("M->M_LEN is %d not %d\n",
					    (int)m->m_len,
					    (int)sizeof(struct sctp_event_subscribe));
				}
#endif				/* SCTP_DEBUG */
				error = EINVAL;
				break;
			}
			events = mtod(m, struct sctp_event_subscribe *);
			memset(events, 0, sizeof(*events));
			SCTP_INP_RLOCK(inp);
			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT))
				events->sctp_data_io_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVASSOCEVNT))
				events->sctp_association_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVPADDREVNT))
				events->sctp_address_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVSENDFAILEVNT))
				events->sctp_send_failure_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVPEERERR))
				events->sctp_peer_error_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT))
				events->sctp_shutdown_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_PDAPIEVNT))
				events->sctp_partial_delivery_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_ADAPTATIONEVNT))
				events->sctp_adaptation_layer_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_AUTHEVNT))
				events->sctp_authentication_event = 1;

			if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_STREAM_RESETEVNT))
				events->sctp_stream_reset_events = 1;
			SCTP_INP_RUNLOCK(inp);
			m->m_len = sizeof(struct sctp_event_subscribe);

		}
		break;

	case SCTP_ADAPTATION_LAYER:
		if ((size_t)m->m_len < sizeof(int)) {
			error = EINVAL;
			break;
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("getadaptation ind\n");
		}
#endif				/* SCTP_DEBUG */
		SCTP_INP_RLOCK(inp);
		*mtod(m, int *)= inp->sctp_ep.adaptation_layer_indicator;
		SCTP_INP_RUNLOCK(inp);
		m->m_len = sizeof(int);
		break;
	case SCTP_SET_INITIAL_DBG_SEQ:
		if ((size_t)m->m_len < sizeof(int)) {
			error = EINVAL;
			break;
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("get initial dbg seq\n");
		}
#endif				/* SCTP_DEBUG */
		SCTP_INP_RLOCK(inp);
		*mtod(m, int *)= inp->sctp_ep.initial_sequence_debug;
		SCTP_INP_RUNLOCK(inp);
		m->m_len = sizeof(int);
		break;
	case SCTP_GET_LOCAL_ADDR_SIZE:
		if ((size_t)m->m_len < sizeof(int)) {
			error = EINVAL;
			break;
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("get local sizes\n");
		}
#endif				/* SCTP_DEBUG */
		SCTP_INP_RLOCK(inp);
		*mtod(m, int *)= sctp_count_max_addresses(inp);
		SCTP_INP_RUNLOCK(inp);
		m->m_len = sizeof(int);
		break;
	case SCTP_GET_REMOTE_ADDR_SIZE:
		{
			sctp_assoc_t *assoc_id;
			uint32_t *val, sz;
			struct sctp_nets *net;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("get remote size\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(sctp_assoc_t)) {
#ifdef SCTP_DEBUG
				printf("m->m_len:%d not %d\n",
				    m->m_len, sizeof(sctp_assoc_t));
#endif				/* SCTP_DEBUG */
				error = EINVAL;
				break;
			}
			stcb = NULL;
			val = mtod(m, uint32_t *);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			}
			if (stcb == NULL) {
				assoc_id = mtod(m, sctp_assoc_t *);
				stcb = sctp_findassociation_ep_asocid(inp, *assoc_id);
			}
			if (stcb == NULL) {
				error = EINVAL;
				break;
			}
			*val = 0;
			sz = 0;
			/* Count the sizes */
			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
				if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) ||
				    (((struct sockaddr *)&net->ro._l_addr)->sa_family == AF_INET6)) {
					sz += sizeof(struct sockaddr_in6);
				} else if (((struct sockaddr *)&net->ro._l_addr)->sa_family == AF_INET) {
					sz += sizeof(struct sockaddr_in);
				} else {
					/* huh */
					break;
				}
			}
			SCTP_TCB_UNLOCK(stcb);
			*val = sz;
			m->m_len = sizeof(uint32_t);
		}
		break;
	case SCTP_GET_PEER_ADDRESSES:
		/*
		 * Get the address information, an array is passed in to
		 * fill up we pack it.
		 */
		{
			int cpsz, left;
			struct sockaddr_storage *sas;
			struct sctp_nets *net;
			struct sctp_getaddresses *saddr;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("get peer addresses\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(struct sctp_getaddresses)) {
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
					printf("gave %d bytes need min:%d\n",
					    m->m_len, sizeof(struct sctp_getaddresses));
				}
#endif				/* SCTP_DEBUG */
				error = EINVAL;
				break;
			}
			left = m->m_len - sizeof(struct sctp_getaddresses);
			saddr = mtod(m, struct sctp_getaddresses *);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else
				stcb = sctp_findassociation_ep_asocid(inp,
				    saddr->sget_assoc_id);
			if (stcb == NULL) {
				error = ENOENT;
				break;
			}
			m->m_len = sizeof(struct sctp_getaddresses);
			sas = (struct sockaddr_storage *)&saddr->addr[0];

			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
				if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) ||
				    (((struct sockaddr *)&net->ro._l_addr)->sa_family == AF_INET6)) {
					cpsz = sizeof(struct sockaddr_in6);
				} else if (((struct sockaddr *)&net->ro._l_addr)->sa_family == AF_INET) {
					cpsz = sizeof(struct sockaddr_in);
				} else {
					/* huh */
					break;
				}
				if (left < cpsz) {
					/* not enough room. */
#ifdef SCTP_DEBUG
					if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
						printf("Out of room\n");
					}
#endif				/* SCTP_DEBUG */
					break;
				}
				if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_NEEDS_MAPPED_V4) &&
				    (((struct sockaddr *)&net->ro._l_addr)->sa_family == AF_INET)) {
					/* Must map the address */
					in6_sin_2_v4mapsin6((struct sockaddr_in *)&net->ro._l_addr,
					    (struct sockaddr_in6 *)sas);
				} else {
					memcpy(sas, &net->ro._l_addr, cpsz);
				}
				((struct sockaddr_in *)sas)->sin_port = stcb->rport;

				sas = (struct sockaddr_storage *)((caddr_t)sas + cpsz);
				left -= cpsz;
				m->m_len += cpsz;
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
					printf("left now:%d mlen:%d\n",
					    left, m->m_len);
				}
#endif				/* SCTP_DEBUG */
			}
			SCTP_TCB_UNLOCK(stcb);
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("All done\n");
		}
#endif				/* SCTP_DEBUG */
		break;
	case SCTP_GET_LOCAL_ADDRESSES:
		{
			int limit, actual;
			struct sockaddr_storage *sas;
			struct sctp_getaddresses *saddr;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("get local addresses\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(struct sctp_getaddresses)) {
				error = EINVAL;
				break;
			}
			saddr = mtod(m, struct sctp_getaddresses *);

			if (saddr->sget_assoc_id) {
				if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
					SCTP_INP_RLOCK(inp);
					stcb = LIST_FIRST(&inp->sctp_asoc_list);
					if (stcb)
						SCTP_TCB_LOCK(stcb);
					SCTP_INP_RUNLOCK(inp);
				} else
					stcb = sctp_findassociation_ep_asocid(inp, saddr->sget_assoc_id);

			} else {
				stcb = NULL;
			}
			/*
			 * assure that the TCP model does not need a assoc
			 * id once connected.
			 */
			if ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) &&
			    (stcb == NULL)) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			}
			sas = (struct sockaddr_storage *)&saddr->addr[0];
			limit = m->m_len - sizeof(sctp_assoc_t);
			actual = sctp_fill_up_addresses(inp, stcb, limit, sas);
			if (stcb)
				SCTP_TCB_UNLOCK(stcb);
			m->m_len = sizeof(struct sockaddr_storage) + actual;
		}
		break;
	case SCTP_PEER_ADDR_PARAMS:
		{
			struct sctp_paddrparams *paddrp;
			struct sctp_nets *net;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("Getting peer_addr_params\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(struct sctp_paddrparams)) {
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_USRREQ2) {
					printf("Hmm m->m_len:%d is to small\n",
					    m->m_len);
				}
#endif				/* SCTP_DEBUG */
				error = EINVAL;
				break;
			}
			paddrp = mtod(m, struct sctp_paddrparams *);

			net = NULL;
			if (paddrp->spp_assoc_id) {
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
					printf("In spp_assoc_id find type\n");
				}
#endif				/* SCTP_DEBUG */
				if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
					SCTP_INP_RLOCK(inp);
					stcb = LIST_FIRST(&inp->sctp_asoc_list);
					if (stcb) {
						SCTP_TCB_LOCK(stcb);
						net = sctp_findnet(stcb, (struct sockaddr *)&paddrp->spp_address);
					}
					SCTP_INP_RLOCK(inp);
				} else {
					stcb = sctp_findassociation_ep_asocid(inp, paddrp->spp_assoc_id);
				}
				if (stcb == NULL) {
					error = ENOENT;
					break;
				}
			}
			if ((stcb == NULL) &&
			    ((((struct sockaddr *)&paddrp->spp_address)->sa_family == AF_INET) ||
			    (((struct sockaddr *)&paddrp->spp_address)->sa_family == AF_INET6))) {
				/* Lookup via address */
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
					printf("Ok we need to lookup a param\n");
				}
#endif				/* SCTP_DEBUG */
				if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
					SCTP_INP_RLOCK(inp);
					stcb = LIST_FIRST(&inp->sctp_asoc_list);
					if (stcb) {
						SCTP_TCB_LOCK(stcb);
						net = sctp_findnet(stcb, (struct sockaddr *)&paddrp->spp_address);
					}
					SCTP_INP_RUNLOCK(inp);
				} else {
					SCTP_INP_WLOCK(inp);
					SCTP_INP_INCR_REF(inp);
					SCTP_INP_WUNLOCK(inp);
					stcb = sctp_findassociation_ep_addr(&inp,
					    (struct sockaddr *)&paddrp->spp_address,
					    &net, NULL, NULL);
					if (stcb == NULL) {
						SCTP_INP_WLOCK(inp);
						SCTP_INP_DECR_REF(inp);
						SCTP_INP_WUNLOCK(inp);
					}
				}

				if (stcb == NULL) {
					error = ENOENT;
					break;
				}
			}
			if (stcb) {
				/* Applys to the specific association */
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
					printf("In TCB side\n");
				}
#endif				/* SCTP_DEBUG */
				paddrp->spp_flags = 0;
				if (net) {
					paddrp->spp_pathmaxrxt = net->failure_threshold;
					paddrp->spp_pathmtu = net->mtu;
					/* get flags for HB */
					if (net->dest_state & SCTP_ADDR_NOHB)
						paddrp->spp_flags |= SPP_HB_DISABLE;
					else
						paddrp->spp_flags |= SPP_HB_ENABLE;
					/* get flags for PMTU */
					if (callout_pending(&net->pmtu_timer.timer)) {
						paddrp->spp_flags |= SPP_PMTUD_ENABLE;
					} else {
						paddrp->spp_flags |= SPP_PMTUD_DISABLE;
					}
#ifdef AF_INET
					if (net->ro._l_addr.sin.sin_family == AF_INET) {
						paddrp->spp_ipv4_tos = net->tos_flowlabel & 0x000000fc;
						paddrp->spp_flags |= SPP_IPV4_TOS;
					}
#endif
#ifdef AF_INET6
					if (net->ro._l_addr.sin6.sin6_family == AF_INET6) {
						paddrp->spp_ipv6_flowlabel = net->tos_flowlabel;
						paddrp->spp_flags |= SPP_IPV6_FLOWLABEL;
					}
#endif
				} else {
					/*
					 * No destination so return default
					 * value
					 */
					paddrp->spp_pathmaxrxt = stcb->asoc.def_net_failure;
					paddrp->spp_pathmtu = sctp_get_frag_point(stcb, &stcb->asoc);
#ifdef AF_INET
					paddrp->spp_ipv4_tos = stcb->asoc.default_tos & 0x000000fc;
					paddrp->spp_flags |= SPP_IPV4_TOS;
#endif
#ifdef AF_INET6
					paddrp->spp_ipv6_flowlabel = stcb->asoc.default_flowlabel;
					paddrp->spp_flags |= SPP_IPV6_FLOWLABEL;
#endif
					/* default settings should be these */
					if (sctp_is_hb_timer_running(stcb)) {
						paddrp->spp_flags |= SPP_HB_ENABLE;
					}
				}
				paddrp->spp_hbinterval = stcb->asoc.heart_beat_delay;
				paddrp->spp_sackdelay = stcb->asoc.delayed_ack;
				/*
				 * Currently we don't support no sack delay
				 * aka SPP_SACKDELAY_DISABLE.
				 */
				paddrp->spp_flags |= SPP_SACKDELAY_ENABLE;
				paddrp->spp_assoc_id = sctp_get_associd(stcb);
				SCTP_TCB_UNLOCK(stcb);
			} else {
				/* Use endpoint defaults */
				SCTP_INP_RLOCK(inp);
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
					printf("In EP levle info\n");
				}
#endif				/* SCTP_DEBUG */
				paddrp->spp_pathmaxrxt = inp->sctp_ep.def_net_failure;
				paddrp->spp_hbinterval = TICKS_TO_MSEC(inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_HEARTBEAT]);
				paddrp->spp_sackdelay = TICKS_TO_MSEC(inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_RECV]);
				paddrp->spp_assoc_id = (sctp_assoc_t) 0;
				/* get inp's default */
#ifdef AF_INET
#if defined(__FreeBSD__) || defined(__APPLE__)
				paddrp->spp_ipv4_tos = inp->ip_inp.inp.inp_ip_tos;
#elif defined(__NetBSD__)
				paddrp->spp_ipv4_tos = inp->ip_inp.inp.inp_ip.ip_tos;
#else
				paddrp->spp_ipv4_tos = inp->inp_ip_tos;
#endif
				paddrp->spp_flags |= SPP_IPV4_TOS;
#endif
#ifdef AF_INET6
				if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
					paddrp->spp_ipv6_flowlabel = ((struct in6pcb *)inp)->in6p_flowinfo;
					paddrp->spp_flags |= SPP_IPV6_FLOWLABEL;
				}
#endif
				/* can't return this */
				paddrp->spp_pathmaxrxt = 0;
				paddrp->spp_pathmtu = 0;
				/* default behavior, no stcb */
				paddrp->spp_flags = SPP_HB_ENABLE | SPP_SACKDELAY_ENABLE | SPP_PMTUD_ENABLE;

				SCTP_INP_RUNLOCK(inp);
			}
			m->m_len = sizeof(struct sctp_paddrparams);
		}
		break;
	case SCTP_GET_PEER_ADDR_INFO:
		{
			struct sctp_paddrinfo *paddri;
			struct sctp_nets *net;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("GetPEER ADDR_INFO\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(struct sctp_paddrinfo)) {
				error = EINVAL;
				break;
			}
			paddri = mtod(m, struct sctp_paddrinfo *);
			net = NULL;
			if ((((struct sockaddr *)&paddri->spinfo_address)->sa_family == AF_INET) ||
			    (((struct sockaddr *)&paddri->spinfo_address)->sa_family == AF_INET6)) {
				/* Lookup via address */
				if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
					SCTP_INP_RLOCK(inp);
					stcb = LIST_FIRST(&inp->sctp_asoc_list);
					if (stcb) {
						SCTP_TCB_LOCK(stcb);
						net = sctp_findnet(stcb,
						    (struct sockaddr *)&paddri->spinfo_address);
					}
					SCTP_INP_RUNLOCK(inp);
				} else {
					SCTP_INP_WLOCK(inp);
					SCTP_INP_INCR_REF(inp);
					SCTP_INP_WUNLOCK(inp);
					stcb = sctp_findassociation_ep_addr(&inp,
					    (struct sockaddr *)&paddri->spinfo_address,
					    &net, NULL, NULL);
					if (stcb == NULL) {
						SCTP_INP_WLOCK(inp);
						SCTP_INP_DECR_REF(inp);
						SCTP_INP_WUNLOCK(inp);
					}
				}

			} else {
				stcb = NULL;
			}
			if ((stcb == NULL) || (net == NULL)) {
				if (stcb) {
					SCTP_TCB_UNLOCK(stcb);
				}
				error = ENOENT;
				break;
			}
			m->m_len = sizeof(struct sctp_paddrinfo);
			paddri->spinfo_state = net->dest_state & (SCTP_REACHABLE_MASK | SCTP_ADDR_NOHB);
			paddri->spinfo_cwnd = net->cwnd;
			paddri->spinfo_srtt = ((net->lastsa >> 2) + net->lastsv) >> 1;
			paddri->spinfo_rto = net->RTO;
			paddri->spinfo_assoc_id = sctp_get_associd(stcb);
			SCTP_TCB_UNLOCK(stcb);
		}
		break;
	case SCTP_PCB_STATUS:
		{
			struct sctp_pcbinfo *spcb;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("PCB status\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(struct sctp_pcbinfo)) {
				error = EINVAL;
				break;
			}
			spcb = mtod(m, struct sctp_pcbinfo *);
#if defined(SCTP_APPLE_FINE_GRAINED_LOCKING)
			if (!lck_rw_try_lock_shared(sctppcbinfo.ipi_ep_mtx)) {
				socket_unlock(inp->ip_inp.inp.inp_socket, 0);
				lck_rw_lock_shared(sctppcbinfo.ipi_ep_mtx);
				socket_lock(inp->ip_inp.inp.inp_socket, 0);
			}
#endif
			sctp_fill_pcbinfo(spcb);
#if defined(SCTP_APPLE_FINE_GRAINED_LOCKING)
			lck_rw_unlock_shared(sctppcbinfo.ipi_ep_mtx);
#endif
			m->m_len = sizeof(struct sctp_pcbinfo);
		}
		break;
	case SCTP_STATUS:
		{
			struct sctp_nets *net;
			struct sctp_status *sstat;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("SCTP status\n");
			}
#endif				/* SCTP_DEBUG */

			if ((size_t)m->m_len < sizeof(struct sctp_status)) {
				error = EINVAL;
				break;
			}
			sstat = mtod(m, struct sctp_status *);

			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else
				stcb = sctp_findassociation_ep_asocid(inp, sstat->sstat_assoc_id);

			if (stcb == NULL) {
				error = EINVAL;
				break;
			}
			/*
			 * I think passing the state is fine since
			 * sctp_constants.h will be available to the user
			 * land.
			 */
			sstat->sstat_state = stcb->asoc.state;
			sstat->sstat_rwnd = stcb->asoc.peers_rwnd;
			sstat->sstat_unackdata = stcb->asoc.sent_queue_cnt;
			/*
			 * We can't include chunks that have been passed to
			 * the socket layer. Only things in queue.
			 */
			sstat->sstat_penddata = (stcb->asoc.cnt_on_reasm_queue +
			    stcb->asoc.cnt_on_all_streams);


			sstat->sstat_instrms = stcb->asoc.streamincnt;
			sstat->sstat_outstrms = stcb->asoc.streamoutcnt;
			sstat->sstat_fragmentation_point = sctp_get_frag_point(stcb, &stcb->asoc);
			memcpy(&sstat->sstat_primary.spinfo_address,
			    &stcb->asoc.primary_destination->ro._l_addr,
			    ((struct sockaddr *)(&stcb->asoc.primary_destination->ro._l_addr))->sa_len);
			net = stcb->asoc.primary_destination;
			((struct sockaddr_in *)&sstat->sstat_primary.spinfo_address)->sin_port = stcb->rport;
			/*
			 * Again the user can get info from sctp_constants.h
			 * for what the state of the network is.
			 */
			sstat->sstat_primary.spinfo_state = net->dest_state & SCTP_REACHABLE_MASK;
			sstat->sstat_primary.spinfo_cwnd = net->cwnd;
			sstat->sstat_primary.spinfo_srtt = net->lastsa;
			sstat->sstat_primary.spinfo_rto = net->RTO;
			sstat->sstat_primary.spinfo_mtu = net->mtu;
			sstat->sstat_primary.spinfo_assoc_id = sctp_get_associd(stcb);
			SCTP_TCB_UNLOCK(stcb);
			m->m_len = sizeof(*sstat);
		}
		break;
	case SCTP_RTOINFO:
		{
			struct sctp_rtoinfo *srto;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("RTO Info\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(struct sctp_rtoinfo)) {
				error = EINVAL;
				break;
			}
			srto = mtod(m, struct sctp_rtoinfo *);
			if (srto->srto_assoc_id == 0) {
				/* Endpoint only please */
				SCTP_INP_RLOCK(inp);
				srto->srto_initial = inp->sctp_ep.initial_rto;
				srto->srto_max = inp->sctp_ep.sctp_maxrto;
				srto->srto_min = inp->sctp_ep.sctp_minrto;
				SCTP_INP_RUNLOCK(inp);
				break;
			}
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else
				stcb = sctp_findassociation_ep_asocid(inp, srto->srto_assoc_id);

			if (stcb == NULL) {
				error = EINVAL;
				break;
			}
			srto->srto_initial = stcb->asoc.initial_rto;
			srto->srto_max = stcb->asoc.maxrto;
			srto->srto_min = stcb->asoc.minrto;
			SCTP_TCB_UNLOCK(stcb);
			m->m_len = sizeof(*srto);
		}
		break;
	case SCTP_ASSOCINFO:
		{
			struct sctp_assocparams *sasoc;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("Associnfo\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(struct sctp_assocparams)) {
				error = EINVAL;
				break;
			}
			sasoc = mtod(m, struct sctp_assocparams *);
			stcb = NULL;

			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb) {

					SCTP_TCB_LOCK(stcb);
				}
				SCTP_INP_RUNLOCK(inp);
			} else if (sasoc->sasoc_assoc_id) {
				stcb = sctp_findassociation_ep_asocid(inp,
				    sasoc->sasoc_assoc_id);
				if (stcb == NULL) {
					error = ENOENT;
					break;
				}
			} else {
				stcb = NULL;
			}
			if (stcb) {
				sasoc->sasoc_asocmaxrxt = stcb->asoc.max_send_times;
				sasoc->sasoc_number_peer_destinations = stcb->asoc.numnets;
				sasoc->sasoc_peer_rwnd = stcb->asoc.peers_rwnd;
				sasoc->sasoc_local_rwnd = stcb->asoc.my_rwnd;
				sasoc->sasoc_cookie_life = stcb->asoc.cookie_life;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_RLOCK(inp);
				sasoc->sasoc_asocmaxrxt = inp->sctp_ep.max_send_times;
				sasoc->sasoc_number_peer_destinations = 0;
				sasoc->sasoc_peer_rwnd = 0;
				sasoc->sasoc_local_rwnd = sbspace(&inp->sctp_socket->so_rcv);
				sasoc->sasoc_cookie_life = inp->sctp_ep.def_cookie_life;
				SCTP_INP_RUNLOCK(inp);
			}
			m->m_len = sizeof(*sasoc);
		}
		break;
	case SCTP_DEFAULT_SEND_PARAM:
		{
			struct sctp_sndrcvinfo *s_info;

			if (m->m_len != sizeof(struct sctp_sndrcvinfo)) {
				error = EINVAL;
				break;
			}
			s_info = mtod(m, struct sctp_sndrcvinfo *);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else
				stcb = sctp_findassociation_ep_asocid(inp, s_info->sinfo_assoc_id);

			if (stcb == NULL) {
				error = ENOENT;
				break;
			}
			/* Copy it out */
			*s_info = stcb->asoc.def_send;
			SCTP_TCB_UNLOCK(stcb);
			m->m_len = sizeof(*s_info);
		}
		break;
	case SCTP_INITMSG:
		{
			struct sctp_initmsg *sinit;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("initmsg\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(struct sctp_initmsg)) {
				error = EINVAL;
				break;
			}
			sinit = mtod(m, struct sctp_initmsg *);
			SCTP_INP_RLOCK(inp);
			sinit->sinit_num_ostreams = inp->sctp_ep.pre_open_stream_count;
			sinit->sinit_max_instreams = inp->sctp_ep.max_open_streams_intome;
			sinit->sinit_max_attempts = inp->sctp_ep.max_init_times;
			sinit->sinit_max_init_timeo = inp->sctp_ep.initial_init_rto_max;
			SCTP_INP_RUNLOCK(inp);
			m->m_len = sizeof(*sinit);
		}
		break;
	case SCTP_PRIMARY_ADDR:
		/* we allow a "get" operation on this */
		{
			struct sctp_setprim *ssp;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("setprimary\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(struct sctp_setprim)) {
				error = EINVAL;
				break;
			}
			ssp = mtod(m, struct sctp_setprim *);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else {
				stcb = sctp_findassociation_ep_asocid(inp, ssp->ssp_assoc_id);
				if (stcb == NULL) {
					/*
					 * one last shot, try it by the
					 * address in
					 */
					struct sctp_nets *net;

					SCTP_INP_WLOCK(inp);
					SCTP_INP_INCR_REF(inp);
					SCTP_INP_WUNLOCK(inp);
					stcb = sctp_findassociation_ep_addr(&inp,
					    (struct sockaddr *)&ssp->ssp_addr,
					    &net, NULL, NULL);
					if (stcb == NULL) {
						SCTP_INP_WLOCK(inp);
						SCTP_INP_DECR_REF(inp);
						SCTP_INP_WUNLOCK(inp);
					}
				}
				if (stcb == NULL) {
					error = EINVAL;
					break;
				}
			}
			/* simply copy out the sockaddr_storage... */
			memcpy(&ssp->ssp_addr,
			    &stcb->asoc.primary_destination->ro._l_addr,
			    ((struct sockaddr *)&stcb->asoc.primary_destination->ro._l_addr)->sa_len);
			SCTP_TCB_UNLOCK(stcb);
			m->m_len = sizeof(*ssp);
		}
		break;

	case SCTP_HMAC_IDENT:
		{
			struct sctp_hmacalgo *shmac;
			sctp_hmaclist_t *hmaclist;
			uint32_t size;
			int i;

			if ((size_t)(m->m_len) < sizeof(*shmac)) {
				error = EINVAL;
				break;
			}
			shmac = mtod(m, struct sctp_hmacalgo *);
			SCTP_INP_RLOCK(inp);
			hmaclist = inp->sctp_ep.local_hmacs;
			if (hmaclist == NULL) {
				/* no HMACs to return */
				m->m_len = sizeof(*shmac);
				break;
			}
			/* is there room for all of the hmac ids? */
			size = sizeof(*shmac) + (hmaclist->num_algo *
			    sizeof(shmac->shmac_idents[0]));
			if ((size_t)(m->m_len) < size) {
				error = EINVAL;
				SCTP_INP_RUNLOCK(inp);
				break;
			}
			/* copy in the list */
			for (i = 0; i < hmaclist->num_algo; i++)
				shmac->shmac_idents[i] = hmaclist->hmac[i];
			SCTP_INP_RUNLOCK(inp);
			m->m_len = size;
			break;
		}
	case SCTP_AUTH_ACTIVE_KEY:
		{
			struct sctp_authkeyid *scact;

			if ((size_t)(m->m_len) < sizeof(*scact)) {
				error = EINVAL;
				break;
			}
			scact = mtod(m, struct sctp_authkeyid *);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				/*
				 * if one-to-one, get from the connected
				 * assoc; else endpoint
				 */
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else if (scact->scact_assoc_id) {
				stcb = sctp_findassociation_ep_asocid(inp, scact->scact_assoc_id);
				if (stcb == NULL) {
					error = ENOENT;
					break;
				}
			}
			if (stcb != NULL) {
				/* get the active key on the assoc */
				scact->scact_keynumber = stcb->asoc.authinfo.assoc_keyid;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				/* get the endpoint active key */
				SCTP_INP_RLOCK(inp);
				scact->scact_keynumber = inp->sctp_ep.default_keyid;
				SCTP_INP_RUNLOCK(inp);
			}
			m->m_len = sizeof(*scact);
			break;
		}
	case SCTP_LOCAL_AUTH_CHUNKS:
		{
			struct sctp_authchunks *sac;
			sctp_auth_chklist_t *chklist = NULL;
			int size = 0;

			if ((size_t)(m->m_len) < sizeof(*sac)) {
				error = EINVAL;
				break;
			}
			sac = mtod(m, struct sctp_authchunks *);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				/*
				 * if one-to-one, get from the connected
				 * assoc; else endpoint
				 */
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb != NULL)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else if (sac->gauth_assoc_id) {
				stcb = sctp_findassociation_ep_asocid(inp, sac->gauth_assoc_id);
				if (stcb == NULL) {
					error = ENOENT;
					break;
				}
			}
			if (stcb != NULL) {
				/* get off the assoc */
				chklist = stcb->asoc.local_auth_chunks;
				if (chklist == NULL) {
					error = EINVAL;
					SCTP_TCB_UNLOCK(stcb);
					break;
				}
				/* is there enough space? */
				size = sctp_auth_get_chklist_size(chklist);
				if ((size_t)m->m_len < (sizeof(struct sctp_authchunks) + size)) {
					error = EINVAL;
					SCTP_TCB_UNLOCK(stcb);
					break;
				}
				/* copy in the chunks */
				sctp_serialize_auth_chunks(chklist, sac->gauth_chunks);
				SCTP_TCB_UNLOCK(stcb);
			} else {
				/* get off the endpoint */
				SCTP_INP_RLOCK(inp);
				chklist = inp->sctp_ep.local_auth_chunks;
				if (chklist == NULL) {
					error = EINVAL;
					SCTP_INP_RUNLOCK(inp);
					break;
				}
				/* is there enough space? */
				size = sctp_auth_get_chklist_size(chklist);
				if ((size_t)m->m_len < (sizeof(struct sctp_authchunks) + size)) {
					error = EINVAL;
					SCTP_INP_RUNLOCK(inp);
					break;
				}
				/* copy in the chunks */
				sctp_serialize_auth_chunks(chklist, sac->gauth_chunks);
				SCTP_INP_RUNLOCK(inp);
			}
			m->m_len = sizeof(struct sctp_authchunks) + size;
			break;
		}
	case SCTP_PEER_AUTH_CHUNKS:
		{
			struct sctp_authchunks *sac;
			sctp_auth_chklist_t *chklist = NULL;
			int size = 0;

			if ((size_t)(m->m_len) < sizeof(*sac)) {
				error = EINVAL;
				break;
			}
			sac = mtod(m, struct sctp_authchunks *);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				/*
				 * if one-to-one, get from the connected
				 * assoc, else endpoint
				 */
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb != NULL)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else if (sac->gauth_assoc_id) {
				stcb = sctp_findassociation_ep_asocid(inp, sac->gauth_assoc_id);
			}
			if (stcb == NULL) {
				error = ENOENT;
				break;
			}
			/* get off the assoc */
			chklist = stcb->asoc.peer_auth_chunks;
			if (chklist == NULL) {
				error = EINVAL;
				SCTP_TCB_UNLOCK(stcb);
				break;
			}
			/* is there enough space? */
			size = sctp_auth_get_chklist_size(chklist);
			if ((size_t)m->m_len < (sizeof(struct sctp_authchunks) + size)) {
				error = EINVAL;
				SCTP_TCB_UNLOCK(stcb);
				break;
			}
			/* copy in the chunks */
			sctp_serialize_auth_chunks(chklist, sac->gauth_chunks);
			SCTP_TCB_UNLOCK(stcb);
			m->m_len = sizeof(struct sctp_authchunks) + size;
			break;
		}

#if defined(HAVE_SCTP_PEELOFF_SOCKOPT)
	case SCTP_PEELOFF:
		{
			struct sctp_peeloff_opt *peeloff;

#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
				printf("peeloff\n");
			}
#endif				/* SCTP_DEBUG */
			if ((size_t)m->m_len < sizeof(*peeloff)) {
				error = EINVAL;
				break;
			}
			peeloff = mtod(m, struct sctp_peeloff_opt *);
			/* do the peeloff */
			error = sctp_peeloff_option(p, peeloff);
			m->m_len = sizeof(*peeloff);
		}
		break;
#endif				/* HAVE_SCTP_PEELOFF_SOCKOPT */

	default:
		error = ENOPROTOOPT;
		m->m_len = 0;
		break;
	}			/* end switch (sopt->sopt_name) */
	return (error);
}


static int
sctp_optsset(struct socket *so,
    int opt,
    struct mbuf **mp,
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
    struct thread *p
#else
    struct proc *p
#endif
)
{
	int error, *mopt, set_opt, s;
	struct mbuf *m;
	struct sctp_tcb *stcb = NULL;
	struct sctp_inpcb *inp;

#if defined(SCTP_APPLE_FINE_GRAINED_LOCKING)
	sctp_lock_assert(so);
#endif
	if (mp == NULL) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("optsset:MP is NULL EINVAL\n");
		}
#endif				/* SCTP_DEBUG */
		return (EINVAL);
	}
	m = *mp;
	if (m == NULL)
		return (EINVAL);

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0)
		return EINVAL;

	error = 0;
	switch (opt) {
	case SCTP_NODELAY:
	case SCTP_AUTOCLOSE:
	case SCTP_AUTO_ASCONF:
	case SCTP_DISABLE_FRAGMENTS:
	case SCTP_USE_EXT_RCVINFO:
	case SCTP_I_WANT_MAPPED_V4_ADDR:
		/* copy in the option value */
		if ((size_t)m->m_len < sizeof(int)) {
			error = EINVAL;
			break;
		}
		mopt = mtod(m, int *);
		set_opt = 0;
		if (error)
			break;
		switch (opt) {
		case SCTP_DISABLE_FRAGMENTS:
			set_opt = SCTP_PCB_FLAGS_NO_FRAGMENT;
			break;
		case SCTP_AUTO_ASCONF:
			set_opt = SCTP_PCB_FLAGS_AUTO_ASCONF;
			break;
		case SCTP_USE_EXT_RCVINFO:			
			set_opt = SCTP_PCB_FLAGS_EXT_RCVINFO;
			break;
		case SCTP_I_WANT_MAPPED_V4_ADDR:
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
				set_opt = SCTP_PCB_FLAGS_NEEDS_MAPPED_V4;
			} else {
				return (EINVAL);
			}
			break;
		case SCTP_NODELAY:
			set_opt = SCTP_PCB_FLAGS_NODELAY;
			break;
		case SCTP_AUTOCLOSE:
			set_opt = SCTP_PCB_FLAGS_AUTOCLOSE;
			/*
			 * The value is in ticks. Note this does not effect
			 * old associations, only new ones.
			 */
			inp->sctp_ep.auto_close_time = SEC_TO_TICKS(*mopt);
			break;
		}
		SCTP_INP_WLOCK(inp);
		if (*mopt != 0) {
			sctp_feature_on(inp, set_opt);
		} else {
			sctp_feature_off(inp, set_opt);
		}
		SCTP_INP_WUNLOCK(inp);
		break;
	case SCTP_PARTIAL_DELIVERY_POINT:
		{
			if ((size_t)m->m_len < sizeof(unsigned int)) {
				error = EINVAL;
				break;
			}
			inp->partial_delivery_point = *mtod(m, unsigned int *);
			m->m_len = sizeof(unsigned int);
		}
	case SCTP_FRAGMENT_INTERLEAVE:
		/* not yet until we re-write sctp_recvmsg() */
		{
			int on_off;

			if ((size_t)m->m_len < sizeof(int)) {
				error = EINVAL;
				break;
			}
			on_off = (mtod(m, int));
			if (on_off) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_FRAG_INTERLEAVE);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_FRAG_INTERLEAVE);
			}
		}
		break;
	case SCTP_CMT_ON_OFF:
		{
			struct sctp_assoc_value *av;

			if ((size_t)m->m_len < sizeof(struct sctp_assoc_value)) {
				error = EINVAL;
				break;
			}
			av = mtod(m, struct sctp_assoc_value *);
			stcb = sctp_findassociation_ep_asocid(inp, av->assoc_id);
			if (stcb == NULL) {
				error = ENOTCONN;
			} else {
				if (sctp_cmt_on_off) {
					stcb->asoc.sctp_cmt_on_off = (uint8_t) av->assoc_value;
				} else {
					if ((stcb->asoc.sctp_cmt_on_off) && (av->assoc_value == 0)) {
						stcb->asoc.sctp_cmt_on_off = 0;
					} else {
						error = EACCES;
					}
				}
				SCTP_TCB_UNLOCK(stcb);
			}
		}
		break;
	case SCTP_CMT_USE_DAC:
		{
			if ((size_t)m->m_len < sizeof(unsigned int)) {
				error = EINVAL;
				break;
			}
			sctp_cmt_sockopt_use_dac = *mtod(m, unsigned int *);
			if (sctp_cmt_sockopt_use_dac != 0)
				sctp_cmt_sockopt_use_dac = 1;
		}
		break;
	case SCTP_CLR_STAT_LOG:
#ifdef SCTP_STAT_LOGGING
		sctp_clr_stat_log();
#else
		error = EOPNOTSUPP;
#endif
		break;
	case SCTP_CONTEXT:
		{

			struct sctp_assoc_value *av;

			if ((size_t)m->m_len < sizeof(struct sctp_assoc_value)) {
				error = EINVAL;
				break;
			}
			av = mtod(m, struct sctp_assoc_value *);
			if(av->assoc_id) {
				stcb = sctp_findassociation_ep_asocid(inp, av->assoc_id);
				if (stcb == NULL) {
					error = ENOTCONN;
				} else {
					stcb->asoc.context = av->assoc_value;
					SCTP_TCB_UNLOCK(stcb);
				}
			} else {
				inp->sctp_context = av->assoc_value;
			}
		}
		break;
	case SCTP_DELAYED_ACK_TIME:
		{
			struct sctp_assoc_value *tm;

			if ((size_t)m->m_len < sizeof(struct sctp_assoc_value)) {
				error = EINVAL;
				break;
			}
			tm = mtod(m, struct sctp_assoc_value *);

			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_WLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb) {
					SCTP_TCB_LOCK(stcb);
					stcb->asoc.delayed_ack = tm->assoc_value;
					SCTP_TCB_UNLOCK(stcb);
				} else {
					inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_RECV] = MSEC_TO_TICKS(tm->assoc_value);
				}
				SCTP_INP_WUNLOCK(inp);
			} else {
				if (tm->assoc_id) {
					stcb = sctp_findassociation_ep_asocid(inp, tm->assoc_id);
					if (stcb == NULL) {
						error = ENOTCONN;
					} else {
						stcb->asoc.delayed_ack = tm->assoc_value;
						SCTP_TCB_UNLOCK(stcb);
					}
				} else {
					SCTP_INP_WLOCK(inp);
					inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_RECV] = MSEC_TO_TICKS(tm->assoc_value);
					SCTP_INP_WUNLOCK(inp);
				}
			}
		}
		break;

	case SCTP_AUTH_CHUNK:
		{
			struct sctp_authchunk *sauth;

			if ((size_t)m->m_len < sizeof(*sauth)) {
				error = EINVAL;
				break;
			}
			sauth = mtod(m, struct sctp_authchunk *);
			if (sctp_auth_add_chunk(sauth->sauth_chunk,
			    inp->sctp_ep.local_auth_chunks))
				error = EINVAL;
			break;
		}
	case SCTP_AUTH_KEY:
		{
			struct sctp_authkey *sca;
			struct sctp_keyhead *shared_keys;
			sctp_sharedkey_t *shared_key;
			sctp_key_t *key = NULL;
			int size;

			size = m->m_len - sizeof(*sca);
			if (size < 0) {
				error = EINVAL;
				break;
			}
			sca = mtod(m, struct sctp_authkey *);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				/*
				 * if one-to-one, set it on the connected
				 * assoc; else endpoint
				 */
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else if (sca->sca_assoc_id) {
				stcb = sctp_findassociation_ep_asocid(inp, sca->sca_assoc_id);
				if (stcb == NULL) {
					error = ENOENT;
					break;
				}
			}
			if (stcb != NULL) {
				/* set it on the assoc */
				shared_keys = &stcb->asoc.shared_keys;
				/* clear the cached keys for this key id */
				sctp_clear_cachedkeys(stcb, sca->sca_keynumber);
				/*
				 * create the new shared key and
				 * insert/replace it
				 */
				if (size > 0) {
					key = sctp_set_key(sca->sca_key, (uint32_t) size);
					if (key == NULL) {
						error = ENOMEM;
						SCTP_TCB_UNLOCK(stcb);
						break;
					}
				}
				shared_key = sctp_alloc_sharedkey();
				if (shared_key == NULL) {
					sctp_free_key(key);
					error = ENOMEM;
					SCTP_TCB_UNLOCK(stcb);
					break;
				}
				shared_key->key = key;
				shared_key->keyid = sca->sca_keynumber;
				sctp_insert_sharedkey(shared_keys, shared_key);
				SCTP_TCB_UNLOCK(stcb);
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_AUTH1) {
					printf("SCTP_AUTH_KEY: adding assoc key id %u, len %u, assoc %xh\n",
					    shared_key->keyid, size, sctp_get_associd(stcb));
				}
#endif				/* SCTP_DEBUG */
			} else {
				/* ste it on the endpoint */
				SCTP_INP_WLOCK(inp);
				shared_keys = &inp->sctp_ep.shared_keys;
				/*
				 * clear the cached keys on all assocs for
				 * this key id
				 */
				sctp_clear_cachedkeys_ep(inp, sca->sca_keynumber);
				/*
				 * create the new shared key and
				 * insert/replace it
				 */
				if (size > 0) {
					key = sctp_set_key(sca->sca_key, (uint32_t) size);
					if (key == NULL) {
						error = ENOMEM;
						SCTP_INP_WUNLOCK(inp);
						break;
					}
				}
				shared_key = sctp_alloc_sharedkey();
				if (shared_key == NULL) {
					sctp_free_key(key);
					error = ENOMEM;
					SCTP_INP_WUNLOCK(inp);
					break;
				}
				shared_key->key = key;
				shared_key->keyid = sca->sca_keynumber;
				sctp_insert_sharedkey(shared_keys, shared_key);
				SCTP_INP_WUNLOCK(inp);
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_AUTH1) {
					printf("SCTP_AUTH_KEY: adding endpoint key id %u, len %u\n",
					    shared_key->keyid, size);
				}
#endif				/* SCTP_DEBUG */
			}
			break;
		}
	case SCTP_HMAC_IDENT:
		{
			struct sctp_hmacalgo *shmac;
			sctp_hmaclist_t *hmaclist;
			uint32_t hmacid;
			int size, i;

			size = m->m_len - sizeof(*shmac);
			if (size < 0) {
				error = EINVAL;
				break;
			}
			shmac = mtod(m, struct sctp_hmacalgo *);
			size = size / sizeof(shmac->shmac_idents[0]);
			hmaclist = sctp_alloc_hmaclist(size);
			for (i = 0; i < size; i++) {
				hmacid = shmac->shmac_idents[i];
				sctp_auth_add_hmacid(hmaclist, (uint16_t) hmacid);
			}
			/* set it on the endpoint */
			SCTP_INP_WLOCK(inp);
			if (inp->sctp_ep.local_hmacs)
				sctp_free_hmaclist(inp->sctp_ep.local_hmacs);
			inp->sctp_ep.local_hmacs = hmaclist;
			SCTP_INP_WUNLOCK(inp);
			break;
		}
	case SCTP_AUTH_ACTIVE_KEY:
		{
			struct sctp_authkeyid *scact;

			if ((size_t)m->m_len < sizeof(*scact)) {
				error = EINVAL;
				break;
			}
			scact = mtod(m, struct sctp_authkeyid *);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				/*
				 * if one-to-one, set it on the connected
				 * assoc; else endpoint
				 */
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else if (scact->scact_assoc_id) {
				stcb = sctp_findassociation_ep_asocid(inp, scact->scact_assoc_id);
				if (stcb == NULL) {
					error = ENOENT;
					break;
				}
			}
			/* set the active key on the right place */
			if (stcb != NULL) {
				/* set the active key on the assoc */
				if (sctp_auth_setactivekey(stcb, scact->scact_keynumber))
					error = EINVAL;
				SCTP_TCB_UNLOCK(stcb);
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_AUTH1) {
					printf("SCTP_AUTH_ACTIVE_KEY: setting key id %u active for assoc %xh\n",
					    scact->scact_keynumber, sctp_get_associd(stcb));
				}
#endif				/* SCTP_DEBUG */
			} else {
				/* set the active key on the endpoint */
				SCTP_INP_WLOCK(inp);
				if (sctp_auth_setactivekey_ep(inp, scact->scact_keynumber))
					error = EINVAL;
				SCTP_INP_WUNLOCK(inp);
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_AUTH1) {
					printf("SCTP_AUTH_ACTIVE_KEY: setting default endpoint key id %u\n",
					    scact->scact_keynumber);
				}
#endif				/* SCTP_DEBUG */
			}
			break;
		}
	case SCTP_AUTH_DELETE_KEY:
		{
			struct sctp_authkeyid *scdel;

			if ((size_t)m->m_len < sizeof(*scdel)) {
				error = EINVAL;
				break;
			}
			scdel = mtod(m, struct sctp_authkeyid *);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				/*
				 * if one-to-one, delete from the connected
				 * assoc; else endpoint
				 */
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else if (scdel->scact_assoc_id) {
				stcb = sctp_findassociation_ep_asocid(inp, scdel->scact_assoc_id);
				if (stcb == NULL) {
					error = ENOENT;
					break;
				}
			}
			/* delete the key from the right place */
			if (stcb != NULL) {
				if (sctp_delete_sharedkey(stcb, scdel->scact_keynumber))
					error = EINVAL;
				SCTP_TCB_UNLOCK(stcb);
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_AUTH1) {
					printf("SCTP_AUTH_DELETE_KEY: deleting key id %u from assoc %xh\n",
					    scdel->scact_keynumber, sctp_get_associd(stcb));
				}
#endif				/* SCTP_DEBUG */
			} else {
				SCTP_INP_WLOCK(inp);
				if (sctp_delete_sharedkey_ep(inp, scdel->scact_keynumber))
					error = EINVAL;
				SCTP_INP_WUNLOCK(inp);
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_AUTH1) {
					printf("SCTP_AUTH_DELETE_KEY: deleting endpoint key id %u\n",
					    scdel->scact_keynumber);
				}
#endif				/* SCTP_DEBUG */
			}
			break;
		}

	case SCTP_RESET_STREAMS:
		{
			struct sctp_stream_reset *strrst;
			uint8_t send_in = 0, send_tsn = 0, send_out = 0;
			int i;

			if ((size_t)m->m_len < sizeof(struct sctp_stream_reset)) {
				error = EINVAL;
				break;
			}
			strrst = mtod(m, struct sctp_stream_reset *);

			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else
				stcb = sctp_findassociation_ep_asocid(inp, strrst->strrst_assoc_id);
			if (stcb == NULL) {
				error = ENOENT;
				break;
			}
			if (stcb->asoc.peer_supports_strreset == 0) {
				/*
				 * Peer does not support it, we return
				 * protocol not supported since this is true
				 * for this feature and this peer, not the
				 * socket request in general.
				 */
				error = EPROTONOSUPPORT;
				SCTP_TCB_UNLOCK(stcb);
				break;
			}
			if (stcb->asoc.stream_reset_outstanding) {
				error = EALREADY;
				SCTP_TCB_UNLOCK(stcb);
				break;
			}
			if (strrst->strrst_flags == SCTP_RESET_LOCAL_RECV) {
				send_in = 1;
			} else if (strrst->strrst_flags == SCTP_RESET_LOCAL_SEND) {
				send_out = 1;
			} else if (strrst->strrst_flags == SCTP_RESET_BOTH) {
				send_in = 1;
				send_out = 1;
			} else if (strrst->strrst_flags == SCTP_RESET_TSN) {
				send_tsn = 1;
			} else {
				error = EINVAL;
				SCTP_TCB_UNLOCK(stcb);
				break;
			}
			for (i = 0; i < strrst->strrst_num_streams; i++) {
				if ((send_in) &&

				    (strrst->strrst_list[i] > stcb->asoc.streamincnt)) {
					error = EINVAL;
					goto get_out;
				}
				if ((send_out) &&
				    (strrst->strrst_list[i] > stcb->asoc.streamoutcnt)) {
					error = EINVAL;
					goto get_out;
				}
			}
			if (error) {
		get_out:
				SCTP_TCB_UNLOCK(stcb);
				break;
			}
			error = sctp_send_str_reset_req(stcb, strrst->strrst_num_streams,
			    strrst->strrst_list,
			    send_out, (stcb->asoc.str_reset_seq_in - 3),
			    send_in, send_tsn);

#if defined(__NetBSD__) || defined(__OpenBSD__)
			s = splsoftnet();
#else
			s = splnet();
#endif
			sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_STRRST_REQ);
			SCTP_TCB_UNLOCK(stcb);
			splx(s);

		}
		break;
	case SCTP_RESET_PEGS:
		memset(sctp_pegs, 0, sizeof(sctp_pegs));
		error = 0;
		break;
	case SCTP_CONNECT_X:
		if ((size_t)m->m_len < (sizeof(int) + sizeof(struct sockaddr_in))) {
			error = EINVAL;
			break;
		}
		error = sctp_do_connect_x(so, inp, m, p, 0);
		break;

	case SCTP_CONNECT_X_DELAYED:
		if ((size_t)m->m_len < (sizeof(int) + sizeof(struct sockaddr_in))) {
			error = EINVAL;
			break;
		}
		error = sctp_do_connect_x(so, inp, m, p, 1);
		break;

	case SCTP_CONNECT_X_COMPLETE:
		{
			struct sockaddr *sa;
			struct sctp_nets *net;

			if ((size_t)m->m_len < sizeof(struct sockaddr_in)) {
				error = EINVAL;
				break;
			}
			sa = mtod(m, struct sockaddr *);
			/* find tcb */
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb) {
					SCTP_TCB_LOCK(stcb);
					net = sctp_findnet(stcb, sa);
				}
				SCTP_INP_RUNLOCK(inp);
			} else {
				SCTP_INP_WLOCK(inp);
				SCTP_INP_INCR_REF(inp);
				SCTP_INP_WUNLOCK(inp);
				stcb = sctp_findassociation_ep_addr(&inp, sa, &net, NULL, NULL);
				if (stcb == NULL) {
					SCTP_INP_WLOCK(inp);
					SCTP_INP_DECR_REF(inp);
					SCTP_INP_WUNLOCK(inp);
				}
			}

			if (stcb == NULL) {
				error = ENOENT;
				break;
			}
			if (stcb->asoc.delayed_connection == 1) {
				stcb->asoc.delayed_connection = 0;
				SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_entered);
				sctp_timer_stop(SCTP_TIMER_TYPE_INIT, inp, stcb, stcb->asoc.primary_destination);
				sctp_send_initiate(inp, stcb);
			} else {
				/*
				 * already expired or did not use delayed
				 * connectx
				 */
				error = EALREADY;
			}
			SCTP_TCB_UNLOCK(stcb);
		}
		break;
	case SCTP_MAXBURST:
		{
			uint8_t *burst;

			SCTP_INP_WLOCK(inp);
			burst = mtod(m, uint8_t *);
			if (*burst) {
				inp->sctp_ep.max_burst = *burst;
			}
			SCTP_INP_WUNLOCK(inp);
		}
		break;
	case SCTP_MAXSEG:
		{
			uint32_t *segsize;
			int ovh;

			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
				ovh = SCTP_MED_OVERHEAD;
			} else {
				ovh = SCTP_MED_V4_OVERHEAD;
			}
			segsize = mtod(m, uint32_t *);
			if (*segsize < 1) {
				error = EINVAL;
				break;
			}
			SCTP_INP_WLOCK(inp);
			inp->sctp_frag_point = (*segsize + ovh);
			if (inp->sctp_frag_point < MHLEN) {
				inp->sctp_frag_point = MHLEN;
			}
			SCTP_INP_WUNLOCK(inp);
		}
		break;
	case SCTP_SET_DEBUG_LEVEL:
#ifdef SCTP_DEBUG
		{
			uint32_t *level;

			if ((size_t)m->m_len < sizeof(uint32_t)) {
				error = EINVAL;
				break;
			}
			level = mtod(m, uint32_t *);
			error = 0;
			sctp_debug_on = (*level & (SCTP_DEBUG_ALL |
			    SCTP_DEBUG_NOISY));
			printf("SETTING DEBUG LEVEL to %x\n",
			    (uint32_t) sctp_debug_on);

		}
#else
		error = EOPNOTSUPP;
#endif				/* SCTP_DEBUG */
		break;
	case SCTP_EVENTS:
		{
			struct sctp_event_subscribe *events;

			if ((size_t)m->m_len < sizeof(struct sctp_event_subscribe)) {
				error = EINVAL;
				break;
			}
			SCTP_INP_WLOCK(inp);
			events = mtod(m, struct sctp_event_subscribe *);
			if (events->sctp_data_io_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT);
			}

			if (events->sctp_association_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVASSOCEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVASSOCEVNT);
			}

			if (events->sctp_address_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVPADDREVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVPADDREVNT);
			}

			if (events->sctp_send_failure_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVSENDFAILEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVSENDFAILEVNT);
			}

			if (events->sctp_peer_error_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVPEERERR);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVPEERERR);
			}

			if (events->sctp_shutdown_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT);
			}

			if (events->sctp_partial_delivery_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_PDAPIEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_PDAPIEVNT);
			}

			if (events->sctp_adaptation_layer_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_ADAPTATIONEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_ADAPTATIONEVNT);
			}

			if (events->sctp_authentication_event) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_AUTHEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_AUTHEVNT);
			}

			if (events->sctp_stream_reset_events) {
				sctp_feature_on(inp, SCTP_PCB_FLAGS_STREAM_RESETEVNT);
			} else {
				sctp_feature_off(inp, SCTP_PCB_FLAGS_STREAM_RESETEVNT);
			}
			SCTP_INP_WUNLOCK(inp);
		}
		break;

	case SCTP_ADAPTATION_LAYER:
		{
			struct sctp_setadaptation *adap_bits;

			if ((size_t)m->m_len < sizeof(struct sctp_setadaptation)) {
				error = EINVAL;
				break;
			}
			SCTP_INP_WLOCK(inp);
			adap_bits = mtod(m, struct sctp_setadaptation *);
			inp->sctp_ep.adaptation_layer_indicator = adap_bits->ssb_adaptation_ind;
			SCTP_INP_WUNLOCK(inp);
		}
		break;
	case SCTP_SET_INITIAL_DBG_SEQ:
		{
			uint32_t *vvv;

			if ((size_t)m->m_len < sizeof(uint32_t)) {
				error = EINVAL;
				break;
			}
			SCTP_INP_WLOCK(inp);
			vvv = mtod(m, uint32_t *);
			inp->sctp_ep.initial_sequence_debug = *vvv;
			SCTP_INP_WUNLOCK(inp);
		}
		break;
	case SCTP_DEFAULT_SEND_PARAM:
	{
			struct sctp_sndrcvinfo *s_info;

			if (m->m_len != sizeof(struct sctp_sndrcvinfo)) {
				error = EINVAL;
				break;
			}
			s_info = mtod(m, struct sctp_sndrcvinfo *);

			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else {
				if(s_info->sinfo_assoc_id) {
					stcb = sctp_findassociation_ep_asocid(inp, s_info->sinfo_assoc_id);
				} else {
					stcb = NULL;
				}
			}
			if ((s_info->sinfo_assoc_id == 0) &&
			    (stcb == NULL)) {
				inp->def_send = *s_info;
			} else  if (stcb == NULL) {
				error = ENOENT;
				break;
			}
			/* Validate things */
			if (s_info->sinfo_stream > stcb->asoc.streamoutcnt) {
				SCTP_TCB_UNLOCK(stcb);
				error = EINVAL;
				break;
			}
			/* Copy it in */
			stcb->asoc.def_send = *s_info;
			SCTP_TCB_UNLOCK(stcb);
		}
		break;
	case SCTP_PEER_ADDR_PARAMS:
		/* Applys to the specific association */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("In TCB side\n");
		}
#endif				/* SCTP_DEBUG */
		{
			struct sctp_paddrparams *paddrp;
			struct sctp_nets *net;

			if ((size_t)m->m_len < sizeof(struct sctp_paddrparams)) {
				error = EINVAL;
				break;
			}
			paddrp = mtod(m, struct sctp_paddrparams *);
			net = NULL;
			if (paddrp->spp_assoc_id) {
				if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
					SCTP_INP_RLOCK(inp);
					stcb = LIST_FIRST(&inp->sctp_asoc_list);
					if (stcb) {
						SCTP_TCB_LOCK(stcb);
						net = sctp_findnet(stcb, (struct sockaddr *)&paddrp->spp_address);
					}
					SCTP_INP_RUNLOCK(inp);
				} else {
					stcb = sctp_findassociation_ep_asocid(inp, paddrp->spp_assoc_id);
				}
				if (stcb == NULL) {
					error = ENOENT;
					break;
				}
			}
			if ((stcb == NULL) &&
			    ((((struct sockaddr *)&paddrp->spp_address)->sa_family == AF_INET) ||
			    (((struct sockaddr *)&paddrp->spp_address)->sa_family == AF_INET6))) {
				/* Lookup via address */
				if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
					SCTP_INP_RLOCK(inp);
					stcb = LIST_FIRST(&inp->sctp_asoc_list);
					if (stcb) {
						SCTP_TCB_LOCK(stcb);
						net = sctp_findnet(stcb,
						    (struct sockaddr *)&paddrp->spp_address);
					}
					SCTP_INP_RUNLOCK(inp);
				} else {
					SCTP_INP_WLOCK(inp);
					SCTP_INP_INCR_REF(inp);
					SCTP_INP_WUNLOCK(inp);
					stcb = sctp_findassociation_ep_addr(&inp,
					    (struct sockaddr *)&paddrp->spp_address,
					    &net, NULL, NULL);
					if (stcb == NULL) {
						SCTP_INP_WLOCK(inp);
						SCTP_INP_DECR_REF(inp);
						SCTP_INP_WUNLOCK(inp);
					}
				}
			}
			if (stcb) {
				/************************TCB SPECIFIC SET ******************/
				/* sack delay first */
				if (paddrp->spp_flags & SPP_SACKDELAY_ENABLE) {
					/*
					 * we do NOT support turning it off
					 * (yet). only setting the delay.
					 */
					if (paddrp->spp_sackdelay >= SCTP_CLOCK_GRANULARITY)
						stcb->asoc.delayed_ack = paddrp->spp_sackdelay;
					else
						stcb->asoc.delayed_ack = SCTP_CLOCK_GRANULARITY;

				} else if (paddrp->spp_flags & SPP_SACKDELAY_DISABLE) {
					stcb->asoc.delayed_ack = 0;
				}
				/*
				 * do we change the timer for HB, we run
				 * only one?
				 */
				if (paddrp->spp_hbinterval)
					stcb->asoc.heart_beat_delay = paddrp->spp_hbinterval;
				else if (paddrp->spp_flags & SPP_HB_TIME_IS_ZERO)
					stcb->asoc.heart_beat_delay = 0;

				/* network sets ? */
				if (net) {
					/************************NET SPECIFIC SET ******************/
					if (paddrp->spp_flags & SPP_HB_DEMAND) {
						/* on demand HB */
						sctp_send_hb(stcb, 1, net);
					}
					if (paddrp->spp_flags & SPP_HB_DISABLE) {
						net->dest_state |= SCTP_ADDR_NOHB;
					}
					if (paddrp->spp_flags & SPP_HB_ENABLE) {
						net->dest_state &= ~SCTP_ADDR_NOHB;
					}
					if (paddrp->spp_flags & SPP_PMTUD_DISABLE) {
						if (callout_pending(&net->pmtu_timer.timer)) {
							sctp_timer_stop(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net);
						}
						if (paddrp->spp_pathmtu > SCTP_DEFAULT_MINSEGMENT) {
							net->mtu = paddrp->spp_pathmtu;
							if (net->mtu < stcb->asoc.smallest_mtu)
								sctp_pathmtu_adustment(inp, stcb, net, net->mtu);
						}
					}
					if (paddrp->spp_flags & SPP_PMTUD_ENABLE) {
						if (callout_pending(&net->pmtu_timer.timer)) {
							sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, inp, stcb, net);
						}
					}
					if (paddrp->spp_pathmaxrxt)
						net->failure_threshold = paddrp->spp_pathmaxrxt;
#ifdef AF_INET
					if (paddrp->spp_flags & SPP_IPV4_TOS) {
						if (net->ro._l_addr.sin.sin_family == AF_INET) {
							net->tos_flowlabel = paddrp->spp_ipv4_tos & 0x000000fc;
						}
					}
#endif
#ifdef AF_INET6
					if (paddrp->spp_flags & SPP_IPV6_FLOWLABEL) {
						if (net->ro._l_addr.sin6.sin6_family == AF_INET6) {
							net->tos_flowlabel = paddrp->spp_ipv6_flowlabel;
						}
					}
#endif
				} else {
					/************************ASSOC ONLY -- NO NET SPECIFIC SET ******************/
					if (paddrp->spp_pathmaxrxt)
						stcb->asoc.def_net_failure = paddrp->spp_pathmaxrxt;

					if (paddrp->spp_flags & SPP_HB_ENABLE) {
						/* Turn back on the timer */
						stcb->asoc.hb_is_disabled = 0;
						sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net);
					}
					if (paddrp->spp_flags & SPP_HB_DISABLE) {
						int cnt_of_unconf = 0;
						struct sctp_nets *lnet;

						stcb->asoc.hb_is_disabled = 1;
						TAILQ_FOREACH(lnet, &stcb->asoc.nets, sctp_next) {
							if (lnet->dest_state & SCTP_ADDR_UNCONFIRMED) {
								cnt_of_unconf++;
							}
						}
						/*
						 * stop the timer ONLY if we
						 * have no unconfirmed
						 * addresses
						 */
						if (cnt_of_unconf == 0) {
							sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net);
						}
					}
					if (paddrp->spp_flags & SPP_HB_ENABLE) {
						/* start up the timer. */
						sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net);
					}
#ifdef AF_INET
					if (paddrp->spp_flags & SPP_IPV4_TOS)
						stcb->asoc.default_tos = paddrp->spp_ipv4_tos & 0x000000fc;
#endif
#ifdef AF_INET6
					if (paddrp->spp_flags & SPP_IPV6_FLOWLABEL)
						stcb->asoc.default_flowlabel = paddrp->spp_ipv6_flowlabel;
#endif

				}
				SCTP_TCB_UNLOCK(stcb);
			} else {
				/************************NO TCB, SET TO default stuff ******************/
				SCTP_INP_WLOCK(inp);
				/*
				 * For the TOS/FLOWLABEL stuff you set it
				 * with the options on the socket
				 */
				if (paddrp->spp_pathmaxrxt) {
					inp->sctp_ep.def_net_failure = paddrp->spp_pathmaxrxt;
				}
				if (paddrp->spp_flags & SPP_HB_ENABLE) {
					inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_HEARTBEAT] = MSEC_TO_TICKS(paddrp->spp_hbinterval);
					sctp_feature_off(inp, SCTP_PCB_FLAGS_DONOT_HEARTBEAT);
				} else if (paddrp->spp_flags & SPP_HB_DISABLE) {
					sctp_feature_on(inp, SCTP_PCB_FLAGS_DONOT_HEARTBEAT);
				}
				if (paddrp->spp_flags & SPP_SACKDELAY_ENABLE) {
					if (paddrp->spp_sackdelay > SCTP_CLOCK_GRANULARITY)
						inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_RECV] = MSEC_TO_TICKS(paddrp->spp_sackdelay);
					else
						inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_RECV] = MSEC_TO_TICKS(SCTP_CLOCK_GRANULARITY);

				} else if (paddrp->spp_flags & SPP_SACKDELAY_DISABLE) {
					inp->sctp_ep.sctp_timeoutticks[SCTP_TIMER_RECV] = 0;
				}
				SCTP_INP_WUNLOCK(inp);
			}
		}
		break;
	case SCTP_RTOINFO:
		{
			struct sctp_rtoinfo *srto;

			if ((size_t)m->m_len < sizeof(struct sctp_rtoinfo)) {
				error = EINVAL;
				break;
			}
			srto = mtod(m, struct sctp_rtoinfo *);
			if (srto->srto_assoc_id == 0) {
				SCTP_INP_WLOCK(inp);
				/*
				 * If we have a null asoc, its default for
				 * the endpoint
				 */
				if (srto->srto_initial > 10)
					inp->sctp_ep.initial_rto = srto->srto_initial;
				if (srto->srto_max > 10)
					inp->sctp_ep.sctp_maxrto = srto->srto_max;
				if (srto->srto_min > 10)
					inp->sctp_ep.sctp_minrto = srto->srto_min;
				SCTP_INP_WUNLOCK(inp);
				break;
			}
			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_LOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else
				stcb = sctp_findassociation_ep_asocid(inp, srto->srto_assoc_id);
			if (stcb == NULL) {
				error = EINVAL;
				break;
			}
			/* Set in ms we hope :-) */
			if (srto->srto_initial > 10)
				stcb->asoc.initial_rto = srto->srto_initial;
			if (srto->srto_max > 10)
				stcb->asoc.maxrto = srto->srto_max;
			if (srto->srto_min > 10)
				stcb->asoc.minrto = srto->srto_min;
			SCTP_TCB_UNLOCK(stcb);
		}
		break;
	case SCTP_ASSOCINFO:
		{
			struct sctp_assocparams *sasoc;

			if ((size_t)m->m_len < sizeof(struct sctp_assocparams)) {
				error = EINVAL;
				break;
			}
			sasoc = mtod(m, struct sctp_assocparams *);
			if (sasoc->sasoc_assoc_id) {
				if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
					SCTP_INP_RLOCK(inp);
					stcb = LIST_FIRST(&inp->sctp_asoc_list);
					if (stcb)
						SCTP_TCB_LOCK(stcb);
					SCTP_INP_RUNLOCK(inp);
				} else
					stcb = sctp_findassociation_ep_asocid(inp,
					    sasoc->sasoc_assoc_id);
				if (stcb == NULL) {
					error = ENOENT;
					break;
				}
			} else {
				stcb = NULL;
			}
			if (stcb) {
				if (sasoc->sasoc_asocmaxrxt)
					stcb->asoc.max_send_times = sasoc->sasoc_asocmaxrxt;
				sasoc->sasoc_number_peer_destinations = stcb->asoc.numnets;
				sasoc->sasoc_peer_rwnd = 0;
				sasoc->sasoc_local_rwnd = 0;
				if (stcb->asoc.cookie_life)
					stcb->asoc.cookie_life = sasoc->sasoc_cookie_life;
				SCTP_TCB_UNLOCK(stcb);
			} else {
				SCTP_INP_WLOCK(inp);
				if (sasoc->sasoc_asocmaxrxt)
					inp->sctp_ep.max_send_times = sasoc->sasoc_asocmaxrxt;
				sasoc->sasoc_number_peer_destinations = 0;
				sasoc->sasoc_peer_rwnd = 0;
				sasoc->sasoc_local_rwnd = 0;
				if (sasoc->sasoc_cookie_life)
					inp->sctp_ep.def_cookie_life = sasoc->sasoc_cookie_life;
				SCTP_INP_WUNLOCK(inp);
			}
		}
		break;
	case SCTP_INITMSG:
		{
			struct sctp_initmsg *sinit;

			if ((size_t)m->m_len < sizeof(struct sctp_initmsg)) {
				error = EINVAL;
				break;
			}
			sinit = mtod(m, struct sctp_initmsg *);
			SCTP_INP_WLOCK(inp);
			if (sinit->sinit_num_ostreams)
				inp->sctp_ep.pre_open_stream_count = sinit->sinit_num_ostreams;

			if (sinit->sinit_max_instreams)
				inp->sctp_ep.max_open_streams_intome = sinit->sinit_max_instreams;

			if (sinit->sinit_max_attempts)
				inp->sctp_ep.max_init_times = sinit->sinit_max_attempts;

			if (sinit->sinit_max_init_timeo > 10)
				/*
				 * We must be at least a 100ms (we set in
				 * ticks)
				 */
				inp->sctp_ep.initial_init_rto_max = sinit->sinit_max_init_timeo;
			SCTP_INP_WUNLOCK(inp);
		}
		break;
	case SCTP_PRIMARY_ADDR:
		{
			struct sctp_setprim *spa;
			struct sctp_nets *net, *lnet;

			if ((size_t)m->m_len < sizeof(struct sctp_setprim)) {
				error = EINVAL;
				break;
			}
			spa = mtod(m, struct sctp_setprim *);

			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb) {
					SCTP_TCB_LOCK(stcb);
				} else {
					error = EINVAL;
					break;
				}
				SCTP_INP_RUNLOCK(inp);
			} else
				stcb = sctp_findassociation_ep_asocid(inp, spa->ssp_assoc_id);
			if (stcb == NULL) {
				/* One last shot */
				SCTP_INP_WLOCK(inp);
				SCTP_INP_INCR_REF(inp);
				SCTP_INP_WUNLOCK(inp);
				stcb = sctp_findassociation_ep_addr(&inp,
				    (struct sockaddr *)&spa->ssp_addr,
				    &net, NULL, NULL);
				if (stcb == NULL) {
					SCTP_INP_WLOCK(inp);
					SCTP_INP_DECR_REF(inp);
					SCTP_INP_WUNLOCK(inp);
					error = EINVAL;
					break;
				}
			} else {
				/*
				 * find the net, associd or connected lookup
				 * type
				 */
				net = sctp_findnet(stcb, (struct sockaddr *)&spa->ssp_addr);
				if (net == NULL) {
					SCTP_TCB_UNLOCK(stcb);
					error = EINVAL;
					break;
				}
			}
			if ((net != stcb->asoc.primary_destination) &&
			    (!(net->dest_state & SCTP_ADDR_UNCONFIRMED))) {
				/* Ok we need to set it */
				lnet = stcb->asoc.primary_destination;
				if (sctp_set_primary_addr(stcb,
				    (struct sockaddr *)NULL,
				    net) == 0) {
					if (net->dest_state & SCTP_ADDR_SWITCH_PRIMARY) {
						net->dest_state |= SCTP_ADDR_DOUBLE_SWITCH;
					}
					net->dest_state |= SCTP_ADDR_SWITCH_PRIMARY;
				}
			}
			SCTP_TCB_UNLOCK(stcb);
		}
		break;

	case SCTP_SET_PEER_PRIMARY_ADDR:
		{
			struct sctp_setpeerprim *sspp;

			if ((size_t)m->m_len < sizeof(struct sctp_setpeerprim)) {
				error = EINVAL;
				break;
			}
			sspp = mtod(m, struct sctp_setpeerprim *);


			if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
				SCTP_INP_RLOCK(inp);
				stcb = LIST_FIRST(&inp->sctp_asoc_list);
				if (stcb)
					SCTP_TCB_UNLOCK(stcb);
				SCTP_INP_RUNLOCK(inp);
			} else
				stcb = sctp_findassociation_ep_asocid(inp, sspp->sspp_assoc_id);
			if (stcb == NULL) {
				error = EINVAL;
				break;
			}
			if (sctp_set_primary_ip_address_sa(stcb, (struct sockaddr *)&sspp->sspp_addr) != 0) {
				error = EINVAL;
			}
			SCTP_TCB_UNLOCK(stcb);
		}
		break;
	case SCTP_BINDX_ADD_ADDR:
		{
			struct sctp_getaddresses *addrs;
			struct sockaddr *addr_touse;
			struct sockaddr_in sin;

			/* see if we're bound all already! */
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
				error = EINVAL;
				break;
			}
			if ((size_t)m->m_len < sizeof(struct sctp_getaddresses)) {
				error = EINVAL;
				break;
			}
			addrs = mtod(m, struct sctp_getaddresses *);
			addr_touse = addrs->addr;
			if (addrs->addr->sa_family == AF_INET6) {
				struct sockaddr_in6 *sin6;

				sin6 = (struct sockaddr_in6 *)addr_touse;
				if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
					in6_sin6_2_sin(&sin, sin6);
					addr_touse = (struct sockaddr *)&sin;
				}
			}
			if (inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) {
				if (p == NULL) {
					/* Can't get proc for Net/Open BSD */
					error = EINVAL;
					break;
				}
				error = sctp_inpcb_bind(so, addr_touse, p);
				break;
			}
			/*
			 * No locks required here since bind and mgmt_ep_sa
			 * all do their own locking. If we do something for
			 * the FIX: below we may need to lock in that case.
			 */
			if (addrs->sget_assoc_id == 0) {
				/* add the address */
				struct sctp_inpcb *lep;

				((struct sockaddr_in *)addr_touse)->sin_port = inp->sctp_lport;
				lep = sctp_pcb_findep(addr_touse, 1, 0);
				if (lep != NULL) {
					/*
					 * We must decrement the refcount
					 * since we have the ep already and
					 * are binding. No remove going on
					 * here.
					 */
					SCTP_INP_WLOCK(inp);
					SCTP_INP_DECR_REF(inp);
					SCTP_INP_WUNLOCK(inp);
				}
				if (lep == inp) {
					/* already bound to it.. ok */
					break;
				} else if (lep == NULL) {
					((struct sockaddr_in *)addr_touse)->sin_port = 0;
					error = sctp_addr_mgmt_ep_sa(inp, addr_touse,
					    SCTP_ADD_IP_ADDRESS);
				} else {
					error = EADDRNOTAVAIL;
				}
				if (error)
					break;

			} else {
				/*
				 * FIX: decide whether we allow assoc based
				 * bindx
				 */
			}
		}
		break;
	case SCTP_BINDX_REM_ADDR:
		{
			struct sctp_getaddresses *addrs;
			struct sockaddr *addr_touse;
			struct sockaddr_in sin;

			/* see if we're bound all already! */
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
				error = EINVAL;
				break;
			}
			if ((size_t)m->m_len < sizeof(struct sctp_getaddresses)) {
				error = EINVAL;
				break;
			}
			addrs = mtod(m, struct sctp_getaddresses *);
			addr_touse = addrs->addr;
			if (addrs->addr->sa_family == AF_INET6) {
				struct sockaddr_in6 *sin6;

				sin6 = (struct sockaddr_in6 *)addr_touse;
				if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
					in6_sin6_2_sin(&sin, sin6);
					addr_touse = (struct sockaddr *)&sin;
				}
			}
			/*
			 * No lock required mgmt_ep_sa does its own locking.
			 * If the FIX: below is ever changed we may need to
			 * lock before calling association level binding.
			 */
			if (addrs->sget_assoc_id == 0) {
				/* delete the address */
				sctp_addr_mgmt_ep_sa(inp, addr_touse,
				    SCTP_DEL_IP_ADDRESS);
			} else {
				/*
				 * FIX: decide whether we allow assoc based
				 * bindx
				 */
			}
		}
		break;
#ifdef __APPLE__
	case SCTP_LISTEN_FIX:
		/* only applies to one-to-many sockets */
		if (inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE) {
			/* make sure the ACCEPTCONN flag is OFF */
			so->so_options &= ~SO_ACCEPTCONN;
		} else {
			/* otherwise, not allowed */
			error = EINVAL;
		}
		break;
#endif				/* __APPLE__ */
	default:
		error = ENOPROTOOPT;
		break;
	}			/* end switch (opt) */
	return (error);
}


#if defined(__FreeBSD__) || defined(__APPLE__)
int
sctp_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct mbuf *m = NULL;
	struct sctp_inpcb *inp;
	int s, error;

	inp = (struct sctp_inpcb *)so->so_pcb;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	if (inp == 0) {
		splx(s);
		/* I made the same as TCP since we are not setup? */
		return (ECONNRESET);
	}
	if (sopt->sopt_level != IPPROTO_SCTP) {
		/* wrong proto level... send back up to IP */
#ifdef INET6
		if (INP_CHECK_SOCKAF(so, AF_INET6))
			error = ip6_ctloutput(so, sopt);
		else
#endif				/* INET6 */
			error = ip_ctloutput(so, sopt);
		splx(s);
		return (error);
	}
	if (sopt->sopt_valsize > MCLBYTES) {
		/*
		 * Restrict us down to a cluster size, that's all we can
		 * pass either way...
		 */
		sopt->sopt_valsize = MCLBYTES;
	}
	if (sopt->sopt_valsize) {

		m = m_get(M_WAIT, MT_DATA);
		if (sopt->sopt_valsize > MLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				sctp_m_freem(m);
				splx(s);
				return (ENOBUFS);
			}
		}
		error = sooptcopyin(sopt, mtod(m, caddr_t), sopt->sopt_valsize,
		    sopt->sopt_valsize);
		if (error) {
			(void)m_free(m);
			goto out;
		}
		m->m_len = sopt->sopt_valsize;
	}
	if (sopt->sopt_dir == SOPT_SET) {
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
		error = sctp_optsset(so, sopt->sopt_name, &m, sopt->sopt_td);
#else
		error = sctp_optsset(so, sopt->sopt_name, &m, sopt->sopt_p);
#endif
	} else if (sopt->sopt_dir == SOPT_GET) {
#if defined (__FreeBSD__) && __FreeBSD_version >= 500000
		error = sctp_optsget(so, sopt->sopt_name, &m, sopt->sopt_td);
#else
		error = sctp_optsget(so, sopt->sopt_name, &m, sopt->sopt_p);
#endif
	} else {
		error = EINVAL;
	}
	if ((error == 0) && (m != NULL)) {
		error = sooptcopyout(sopt, mtod(m, caddr_t), m->m_len);
		sctp_m_freem(m);
	} else if (m != NULL) {
		sctp_m_freem(m);
	}
out:
	splx(s);
	return (error);
}

#else
/* NetBSD and OpenBSD */
int
sctp_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
{
	int s, error;
	struct inpcb *inp;

#ifdef INET6
	struct in6pcb *in6p;

#endif
	int family;		/* family of the socket */

	family = so->so_proto->pr_domain->dom_family;
	error = 0;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	switch (family) {
	case PF_INET:
		inp = sotoinpcb(so);
#ifdef INET6
		in6p = NULL;
#endif
		break;
#ifdef INET6
	case PF_INET6:
		inp = NULL;
		in6p = sotoin6pcb(so);
		break;
#endif
	default:
		splx(s);
		return EAFNOSUPPORT;
	}
#ifndef INET6
	if (inp == NULL)
#else
	if (inp == NULL && in6p == NULL)
#endif
	{
		splx(s);
		if (op == PRCO_SETOPT && *mp)
			(void)m_free(*mp);
		return (ECONNRESET);
	}
	if (level != IPPROTO_SCTP) {
		switch (family) {
		case PF_INET:
			error = ip_ctloutput(op, so, level, optname, mp);
			break;
#ifdef INET6
		case PF_INET6:
			error = ip6_ctloutput(op, so, level, optname, mp);
			break;
#endif
		}
		splx(s);
		return (error);
	}
	/* Ok if we reach here it is a SCTP option we hope */
	if (op == PRCO_SETOPT) {
		error = sctp_optsset(so, optname, mp, (struct proc *)NULL);
		if (*mp)
			(void)m_free(*mp);
	} else if (op == PRCO_GETOPT) {
		error = sctp_optsget(so, optname, mp, (struct proc *)NULL);
	} else {
		error = EINVAL;
	}
	splx(s);
	return (error);
}

#endif

static int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
sctp_connect(struct socket *so, struct sockaddr *addr, struct thread *p)
{
#else
#if defined(__FreeBSD__) || defined(__APPLE__)
sctp_connect(struct socket *so, struct sockaddr *addr, struct proc *p)
{
#else
sctp_connect(struct socket *so, struct mbuf *nam, struct proc *p)
{
	struct sockaddr *addr = mtod(nam, struct sockaddr *);

#endif
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int s = splsoftnet();

#else
	int s = splnet();

#endif
	int error = 0;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Connect called in SCTP to ");
		sctp_print_address(addr);
		printf("Port %d\n", ntohs(((struct sockaddr_in *)addr)->sin_port));
	}
#endif				/* SCTP_DEBUG */
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		splx(s);
		/* I made the same as TCP since we are not setup? */
		return (ECONNRESET);
	}
	SCTP_ASOC_CREATE_LOCK(inp);
	SCTP_INP_WLOCK(inp);
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE)) {
		/* Should I really unlock ? */
		SCTP_INP_WUNLOCK(inp);
		SCTP_ASOC_CREATE_UNLOCK(inp);
		splx(s);
		return (EFAULT);
	}
#ifdef INET6
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0) &&
	    (addr->sa_family == AF_INET6)) {
		SCTP_INP_WUNLOCK(inp);
		SCTP_ASOC_CREATE_UNLOCK(inp);
		splx(s);
		return (EINVAL);
	}
#endif				/* INET6 */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) ==
	    SCTP_PCB_FLAGS_UNBOUND) {
		/* Bind a ephemeral port */
		SCTP_INP_WUNLOCK(inp);
		error = sctp_inpcb_bind(so, NULL, p);
		if (error) {
			SCTP_ASOC_CREATE_UNLOCK(inp);
			splx(s);
			return (error);
		}
		SCTP_INP_WLOCK(inp);
	}
	/* Now do we connect? */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		/* We are already connected AND the TCP model */
		splx(s);
		SCTP_INP_WUNLOCK(inp);
		SCTP_ASOC_CREATE_UNLOCK(inp);
		return (EADDRINUSE);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		if (stcb)
			SCTP_TCB_UNLOCK(stcb);
		SCTP_INP_WUNLOCK(inp);
	} else {
		SCTP_INP_INCR_REF(inp);
		SCTP_INP_WUNLOCK(inp);
		stcb = sctp_findassociation_ep_addr(&inp, addr, NULL, NULL, NULL);
		if (stcb == NULL) {
			SCTP_INP_WLOCK(inp);
			SCTP_INP_DECR_REF(inp);
			SCTP_INP_WUNLOCK(inp);
		}
	}
	if (stcb != NULL) {
		/* Already have or am bring up an association */
		SCTP_ASOC_CREATE_UNLOCK(inp);
		SCTP_TCB_UNLOCK(stcb);
		splx(s);
		return (EALREADY);
	}
	/* We are GOOD to go */
	stcb = sctp_aloc_assoc(inp, addr, 1, &error, 0);
	if (stcb == NULL) {
		/* Gak! no memory */
		splx(s);
		return (error);
	}
	if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		stcb->sctp_ep->sctp_flags |= SCTP_PCB_FLAGS_CONNECTED;
		/* Set the connected flag so we can queue data */
		soisconnecting(so);
	}
	stcb->asoc.state = SCTP_STATE_COOKIE_WAIT;
	SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_entered);
	/*
	 * initialize authentication parameters for the assoc
	 */
	/* generate a RANDOM for this assoc */
	stcb->asoc.authinfo.random =
	    sctp_generate_random_key(sctp_auth_random_len);
	/* initialize hmac list from endpoint */
	stcb->asoc.local_hmacs = sctp_copy_hmaclist(inp->sctp_ep.local_hmacs);
	/* initialize auth chunks list from endpoint */
	stcb->asoc.local_auth_chunks =
	    sctp_copy_chunklist(inp->sctp_ep.local_auth_chunks);
	/* copy defaults from the endpoint */
	stcb->asoc.authinfo.assoc_keyid = inp->sctp_ep.default_keyid;

	sctp_send_initiate(inp, stcb);
	SCTP_ASOC_CREATE_UNLOCK(inp);
	SCTP_TCB_UNLOCK(stcb);
	splx(s);
	return error;
}

int
#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
#if __FreeBSD_version >= 700000
sctp_listen(struct socket *so, int backlog, struct thread *p)
#else
sctp_listen(struct socket *so, struct thread *p)
#endif
#else
sctp_listen(struct socket *so, struct proc *p)
#endif
{
	/*
	 * Note this module depends on the protocol processing being called
	 * AFTER any socket level flags and backlog are applied to the
	 * socket. The traditional way that the socket flags are applied is
	 * AFTER protocol processing. We have made a change to the
	 * sys/kern/uipc_socket.c module to reverse this but this MUST be in
	 * place if the socket API for SCTP is to work properly.
	 */
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int s = splsoftnet();

#else
	int s = splnet();

#endif
	int error = 0;
	struct sctp_inpcb *inp;

	inp = (struct sctp_inpcb *)so->so_pcb;
	if (inp == 0) {
		splx(s);
		/* I made the same as TCP since we are not setup? */
		return (ECONNRESET);
	}
	SCTP_INP_RLOCK(inp);
#ifdef SCTP_LOCK_LOGGING
	sctp_log_lock(inp, (struct sctp_tcb *)NULL, SCTP_LOG_LOCK_SOCK);
#endif
	SOCK_LOCK(so);
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
	error = solisten_proto_check(so);
	if (error) {
		SOCK_UNLOCK(so);
		return (error);
	}
#endif
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED)) {
		/* We are already connected AND the TCP model */
		splx(s);
		SCTP_INP_RUNLOCK(inp);
		SOCK_UNLOCK(so);
		return (EADDRINUSE);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) {
		/* We must do a bind. */
		SCTP_INP_RUNLOCK(inp);
		if ((error = sctp_inpcb_bind(so, NULL, p))) {
			/* bind error, probably perm */
			SOCK_UNLOCK(so);
			splx(s);
			return (error);
		}
	} else {
		SCTP_INP_RUNLOCK(inp);
	}
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
#if __FreeBSD_version >= 700000
	/* It appears for 7.0 and on, we must always call this. */
	solisten_proto(so, backlog);
#else
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE) == 0) {
		solisten_proto(so);
	}
#endif
#endif

	if (inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE) {
		/* remove the ACCEPTCONN flag for one-to-many sockets */
		so->so_options &= ~SO_ACCEPTCONN;
	}
#if __FreeBSD_version >= 700000
	if (backlog == 0) {
		/* turning off listen */
		so->so_options &= ~SO_ACCEPTCONN;
	}
#endif
	SOCK_UNLOCK(so);
	splx(s);
	return (error);
}

int
#if defined(__FreeBSD__) || defined(__APPLE__)
sctp_accept(struct socket *so, struct sockaddr **addr)
{
#else
sctp_accept(struct socket *so, struct mbuf *nam)
{
	struct sockaddr *addr = mtod(nam, struct sockaddr *);

#endif
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int s = splsoftnet();

#else
	int s = splnet();

#endif
	struct sctp_tcb *stcb;
	struct sctp_inpcb *inp;
	union sctp_sockstore store;

#ifdef SCTP_KAME
	int error;

#endif				/* SCTP_KAME */

	inp = (struct sctp_inpcb *)so->so_pcb;

	if (inp == 0) {
		splx(s);
		return (ECONNRESET);
	}
	SCTP_INP_RLOCK(inp);
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE) {
		return (ENOTSUP);
	}
	if (so->so_state & SS_ISDISCONNECTED) {
		splx(s);
		SCTP_INP_RUNLOCK(inp);
		return (ECONNABORTED);
	}
	stcb = LIST_FIRST(&inp->sctp_asoc_list);
	if (stcb == NULL) {
		splx(s);
		SCTP_INP_RUNLOCK(inp);
		return (ECONNRESET);
	}
	SCTP_TCB_LOCK(stcb);
	SCTP_INP_RUNLOCK(inp);
	store = stcb->asoc.primary_destination->ro._l_addr;
	SCTP_TCB_UNLOCK(stcb);
	if (store.sa.sa_family == AF_INET) {
		struct sockaddr_in *sin;

#if defined(__FreeBSD__) || defined(__APPLE__)
		MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME,
		    M_WAITOK | M_ZERO);
#else
		sin = (struct sockaddr_in *)addr;
		bzero((caddr_t)sin, sizeof(*sin));
#endif
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_port = ((struct sockaddr_in *)&store)->sin_port;
		sin->sin_addr = ((struct sockaddr_in *)&store)->sin_addr;
#if defined(__FreeBSD__) || defined(__APPLE__)
		*addr = (struct sockaddr *)sin;
#else
		nam->m_len = sizeof(*sin);
#endif
	} else {
		struct sockaddr_in6 *sin6;

#if defined(__FreeBSD__) || defined(__APPLE__)
		MALLOC(sin6, struct sockaddr_in6 *, sizeof *sin6, M_SONAME,
		    M_WAITOK | M_ZERO);
#else
		sin6 = (struct sockaddr_in6 *)addr;
		bzero((caddr_t)sin6, sizeof(*sin6));
#endif
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_port = ((struct sockaddr_in6 *)&store)->sin6_port;

		sin6->sin6_addr = ((struct sockaddr_in6 *)&store)->sin6_addr;
#ifdef SCTP_KAME
		if ((error = sa6_recoverscope(sin6)) != 0)
			return (error);
#else
		if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr))
			/*
			 * sin6->sin6_scope_id =
			 * ntohs(sin6->sin6_addr.s6_addr16[1]);
			 */
			in6_recoverscope(sin6, &sin6->sin6_addr, NULL);	/* skip ifp check */
		else
			sin6->sin6_scope_id = 0;	/* XXX */
#endif				/* SCTP_KAME */
#if defined(__FreeBSD__) || defined (__APPLE__)
		*addr = (struct sockaddr *)sin6;
#else
		nam->m_len = sizeof(*sin6);
#endif
	}
	/* Wake any delayed sleep action */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_DONT_WAKE) {
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_DONT_WAKE;
		if (inp->sctp_flags & SCTP_PCB_FLAGS_WAKEOUTPUT) {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_WAKEOUTPUT;
#if defined(__NetBSD__)
			if (sowritable(inp->sctp_socket))
				sowwakeup(inp->sctp_socket);
#else
			if (sowriteable(inp->sctp_socket))
				sowwakeup(inp->sctp_socket);
#endif
		}
		if (inp->sctp_flags & SCTP_PCB_FLAGS_WAKEINPUT) {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_WAKEINPUT;
			if (soreadable(inp->sctp_socket))
				sorwakeup(inp->sctp_socket);
		}
	}
	splx(s);
	return (0);
}

int
#if defined(__FreeBSD__) || defined(__APPLE__)
sctp_ingetaddr(struct socket *so, struct sockaddr **addr)
#else
sctp_ingetaddr(struct socket *so, struct mbuf *nam)
#endif
{
#if defined(__FreeBSD__) || defined(__APPLE__)
	struct sockaddr_in *sin;

#else
	struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);

#endif
	int s;
	struct sctp_inpcb *inp;

	/*
	 * Do the malloc first in case it blocks.
	 */
#if defined(__FreeBSD__) || defined(__APPLE__)
	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME, M_WAITOK |
	    M_ZERO);
#else
	nam->m_len = sizeof(*sin);
	memset(sin, 0, sizeof(*sin));
#endif
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (!inp) {
		splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
		FREE(sin, M_SONAME);
#endif
		return ECONNRESET;
	}
	SCTP_INP_RLOCK(inp);
	sin->sin_port = inp->sctp_lport;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
			struct sctp_tcb *stcb;
			struct sockaddr_in *sin_a;
			struct sctp_nets *net;
			int fnd;

			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb == NULL) {
				goto notConn;
			}
			fnd = 0;
			sin_a = NULL;
			SCTP_TCB_LOCK(stcb);
			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
				sin_a = (struct sockaddr_in *)&net->ro._l_addr;
				if (sin_a->sin_family == AF_INET) {
					fnd = 1;
					break;
				}
			}
			if ((!fnd) || (sin_a == NULL)) {
				/* punt */
				SCTP_TCB_UNLOCK(stcb);
				goto notConn;
			}
			sin->sin_addr = sctp_ipv4_source_address_selection(inp,
			    stcb, (struct route *)&net->ro, net, 0);
			SCTP_TCB_UNLOCK(stcb);
		} else {
			/* For the bound all case you get back 0 */
	notConn:
			sin->sin_addr.s_addr = 0;
		}

	} else {
		/* Take the first IPv4 address in the list */
		struct sctp_laddr *laddr;
		int fnd = 0;

		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa->ifa_addr->sa_family == AF_INET) {
				struct sockaddr_in *sin_a;

				sin_a = (struct sockaddr_in *)laddr->ifa->ifa_addr;
				sin->sin_addr = sin_a->sin_addr;
				fnd = 1;
				break;
			}
		}
		if (!fnd) {
			splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
			FREE(sin, M_SONAME);
#endif
			SCTP_INP_RUNLOCK(inp);
			return ENOENT;
		}
	}
	SCTP_INP_RUNLOCK(inp);
	splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
	(*addr) = (struct sockaddr *)sin;
#endif
	return (0);
}

int
#if defined(__FreeBSD__) || defined(__APPLE__)
sctp_peeraddr(struct socket *so, struct sockaddr **addr)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)*addr;

#else
sctp_peeraddr(struct socket *so, struct mbuf *nam)
{
	struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);

#endif
	int s, fnd;
	struct sockaddr_in *sin_a;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;


	/* Do the malloc first in case it blocks. */
	inp = (struct sctp_inpcb *)so->so_pcb;
	if ((inp == NULL) ||
	    ((inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) == 0)) {
		/* UDP type and listeners will drop out here */
		return (ENOTCONN);
	}
#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif

#if defined(__FreeBSD__) || defined(__APPLE__)
	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME, M_WAITOK |
	    M_ZERO);
#else
	nam->m_len = sizeof(*sin);
	memset(sin, 0, sizeof(*sin));
#endif
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);

	/* We must recapture incase we blocked */
	inp = (struct sctp_inpcb *)so->so_pcb;
	if (!inp) {
		splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
		FREE(sin, M_SONAME);
#endif
		return ECONNRESET;
	}
	SCTP_INP_RLOCK(inp);
	stcb = LIST_FIRST(&inp->sctp_asoc_list);
	if (stcb)
		SCTP_TCB_LOCK(stcb);
	SCTP_INP_RUNLOCK(inp);
	if (stcb == NULL) {
		splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
		FREE(sin, M_SONAME);
#endif
		return ECONNRESET;
	}
	fnd = 0;
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		sin_a = (struct sockaddr_in *)&net->ro._l_addr;
		if (sin_a->sin_family == AF_INET) {
			fnd = 1;
			sin->sin_port = stcb->rport;
			sin->sin_addr = sin_a->sin_addr;
			break;
		}
	}
	SCTP_TCB_UNLOCK(stcb);
	if (!fnd) {
		/* No IPv4 address */
		splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
		FREE(sin, M_SONAME);
#endif
		return ENOENT;
	}
	splx(s);
#if defined(__FreeBSD__) || defined(__APPLE__)
	(*addr) = (struct sockaddr *)sin;
#endif
	return (0);
}

#if defined(__FreeBSD__) || defined(__APPLE__)
struct pr_usrreqs sctp_usrreqs = {
#if __FreeBSD_version >= 600000
	.pru_abort = sctp_abort,
	.pru_accept = sctp_accept,
	.pru_attach = sctp_attach,
	.pru_bind = sctp_bind,
	.pru_connect = sctp_connect,
	.pru_control = in_control,
	.pru_detach = sctp_detach,
	.pru_disconnect = sctp_disconnect,
	.pru_listen = sctp_listen,
	.pru_peeraddr = sctp_peeraddr,
	.pru_send = sctp_sendm,
	.pru_shutdown = sctp_shutdown,
	.pru_sockaddr = sctp_ingetaddr,
	.pru_sopoll = sopoll,
	.pru_sosend = sctp_sosend,
	.pru_soreceive = sctp_soreceive
#else
	sctp_abort,
	sctp_accept,
	sctp_attach,
	sctp_bind,
	sctp_connect,
	pru_connect2_notsupp,
	in_control,
	sctp_detach,
	sctp_disconnect,
	sctp_listen,
	sctp_peeraddr,
	NULL,
	pru_rcvoob_notsupp,
	sctp_sendm,
	pru_sense_null,
	sctp_shutdown,
	sctp_ingetaddr,
	sctp_sosend,
	sctp_soreceive,
	sopoll
#endif
};

#else
#if defined(__NetBSD__)
int
sctp_usrreq(so, req, m, nam, control, p)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
	struct proc *p;
{
#else
int
sctp_usrreq(so, req, m, nam, control)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
{
	struct proc *p = curproc;

#endif
	int s;
	int error = 0;
	int family;

	family = so->so_proto->pr_domain->dom_family;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	s = splsoftnet();
#else
	s = splnet();
#endif
	if (req == PRU_CONTROL) {
		switch (family) {
		case PF_INET:
			error = in_control(so, (long)m, (caddr_t)nam,
			    (struct ifnet *)control
#if defined(__NetBSD__)
			    ,p
#endif
			    );
			break;
#ifdef INET6
		case PF_INET6:
			error = in6_control(so, (long)m, (caddr_t)nam,
			    (struct ifnet *)control, p);
			break;
#endif
		default:
			error = EAFNOSUPPORT;
		}
		splx(s);
		return (error);
	}
#ifdef __NetBSD__
	if (req == PRU_PURGEIF) {
		struct ifnet *ifn;
		struct ifaddr *ifa;

		ifn = (struct ifnet *)control;
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family == family) {
				sctp_delete_ip_address(ifa);
			}
		}
		switch (family) {
		case PF_INET:
			in_purgeif(ifn);
			break;
#ifdef INET6
		case PF_INET6:
			in6_purgeif(ifn);
			break;
#endif				/* INET6 */
		default:
			splx(s);
			return (EAFNOSUPPORT);
		}
		splx(s);
		return (0);
	}
#endif
	switch (req) {
	case PRU_ATTACH:
		error = sctp_attach(so, family, p);
		break;
	case PRU_DETACH:
		error = sctp_detach(so);
		break;
	case PRU_BIND:
		if (nam == NULL) {
			splx(s);
			return (EINVAL);
		}
		error = sctp_bind(so, nam, p);
		break;
	case PRU_LISTEN:
		error = sctp_listen(so, p);
		break;
	case PRU_CONNECT:
		if (nam == NULL) {
			splx(s);
			return (EINVAL);
		}
		error = sctp_connect(so, nam, p);
		break;
	case PRU_DISCONNECT:
		error = sctp_disconnect(so);
		break;
	case PRU_ACCEPT:
		if (nam == NULL) {
			splx(s);
			return (EINVAL);
		}
		error = sctp_accept(so, nam);
		break;
	case PRU_SHUTDOWN:
		error = sctp_shutdown(so);
		break;

	case PRU_RCVD:
		/*
		 * For Open and Net BSD, this is real ugly. The mbuf *nam
		 * that is passed (by soreceive()) is the int flags c ast as
		 * a (mbuf *) yuck!
		 */
		break;

	case PRU_SEND:
		/* Flags are ignored */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_USRREQ1) {
			printf("Send called on V4 side\n");
		}
#endif				/* SCTP_DEBUG */
		{
			struct sockaddr *addr;

			if (nam == NULL)
				addr = NULL;
			else
				addr = mtod(nam, struct sockaddr *);

			error = sctp_sendm(so, 0, m, addr, control, p);
		}
		break;
	case PRU_ABORT:
		error = sctp_abort(so);
		break;

	case PRU_SENSE:
		error = 0;
		break;
	case PRU_RCVOOB:
		error = EAFNOSUPPORT;
		break;
	case PRU_SENDOOB:
		error = EAFNOSUPPORT;
		break;
	case PRU_PEERADDR:
		error = sctp_peeraddr(so, nam);
		break;
	case PRU_SOCKADDR:
		error = sctp_ingetaddr(so, nam);
		break;
	case PRU_SLOWTIMO:
		error = 0;
		break;
	default:
		break;
	}
	splx(s);
	return (error);
}

#endif

#if __OpenBSD__
/*
 * Sysctl for sctp variables.
 */
int
sctp_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	uint32_t namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);
	/* ?? whats this ?? sysctl_int(); */

	switch (name[0]) {
	case SCTPCTL_MAXDGRAM:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_sendspace));
	case SCTPCTL_RECVSPACE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_recvspace));
	case SCTPCTL_AUTOASCONF:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_auto_asconf));
	case SCTPCTL_ECN_ENABLE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_ecn_enable));
	case SCTPCTL_ECN_NONCE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_ecn_nonce));
	case SCTPCTL_STRICT_SACK:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_strict_sacks));
	case SCTPCTL_NOCSUM_LO:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_no_csum_on_loopback));
	case SCTPCTL_STRICT_INIT:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_strict_init));
	case SCTPCTL_PEER_CHK_OH:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_peer_chunk_oh));
	case SCTPCTL_MAXBURST:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_max_burst_default));
	case SCTPCTL_MAXCHUNKONQ:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_max_chunks_on_queue));
	case SCTPCTL_DELAYED_SACK:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_delayed_sack_time_default));
	case SCTPCTL_HB_INTERVAL:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_heartbeat_interval_default));
	case SCTPCTL_PMTU_RAISE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_pmtu_raise_time_default));
	case SCTPCTL_SHUTDOWN_GUARD:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_shutdown_guard_time_default));
	case SCTPCTL_SECRET_LIFETIME:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_secret_lifetime_default));
	case SCTPCTL_RTO_MAX:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_rto_max_default));
	case SCTPCTL_RTO_MIN:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_rto_min_default));
	case SCTPCTL_RTO_INITIAL:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_rto_initial_default));
	case SCTPCTL_INIT_RTO_MAX:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_init_rto_max_default));
	case SCTPCTL_COOKIE_LIFE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_valid_cookie_life_default));
	case SCTPCTL_INIT_RTX_MAX:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_init_rtx_max_default));
	case SCTPCTL_ASSOC_RTX_MAX:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_assoc_rtx_max_default));
	case SCTPCTL_PATH_RTX_MAX:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_path_rtx_max_default));
	case SCTPCTL_NR_OUTGOING_STREAMS:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_nr_outgoing_streams_default));
	case SCTPCTL_CMT_ON_OFF:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_cmt_on_off));
	case SCTPCTL_CWND_MAXBURST:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_use_cwnd_based_maxburst));
	case SCTPCTL_EARLY_FR:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_early_fr));
	case SCTPCTL_RTTVAR_CC:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_use_rttvar_cc));
	case SCTPCTL_DEADLOCK_DET:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_says_check_for_deadlock));
	case SCTPCTL_EARLY_FR_MSEC:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_early_fr_msec));
	case SCTPCTL_AUTH_DISABLE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_auth_disable));
	case SCTPCTL_AUTH_RANDOM_LEN:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_auth_random_len));
	case SCTPCTL_AUTH_HMAC_ID:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_auth_hmac_id_default));
	case SCTPCTL_ABC_L_VAR:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_L2_abc_variable));
	case SCTPCTL_MAX_MBUF_CHAIN:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_max_mbuf_threshold_count));
	case SCTPCTL_CMT_USE_DAC:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_cmt_use_dac));
	case SCTPCTL_DO_DRAIN:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_do_drain));
	case SCTPCTL_WARM_CRC32:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_warm_the_crc32_table));
	case SCTPCTL_QLIMIT_ABORT:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_abort_if_one_2_one_hits_limit));

	case SCTPCTL_STRICT_ORDER:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_strict_data_order));

#ifdef SCTP_DEBUG
	case SCTPCTL_DEBUG:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &sctp_debug_on));
#endif
	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}

#endif
#if __NetBSD__
/*
 * Sysctl for sctp variables.
 */
SYSCTL_SETUP(sysctl_net_inet_sctp_setup, "sysctl net.inet.sctp subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT,
	    CTLTYPE_NODE, "net", NULL,
	    NULL, 0, NULL, 0,
	    CTL_NET, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT,
	    CTLTYPE_NODE, "inet", NULL,
	    NULL, 0, NULL, 0,
	    CTL_NET, PF_INET, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT,
	    CTLTYPE_NODE, "sctp",
	    SYSCTL_DESCR("sctp related settings"),
	    NULL, 0, NULL, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "maxdgram",
	    SYSCTL_DESCR("Maximum outgoing SCTP buffer size"),
	    NULL, 0, &sctp_sendspace, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_MAXDGRAM,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "recvspace",
	    SYSCTL_DESCR("Maximum incoming SCTP buffer size"),
	    NULL, 0, &sctp_recvspace, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_RECVSPACE,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "autoasconf",
	    SYSCTL_DESCR("Enable SCTP Auto-ASCONF"),
	    NULL, 0, &sctp_auto_asconf, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_AUTOASCONF,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "ecn_enable",
	    SYSCTL_DESCR("Enable SCTP ECN"),
	    NULL, 0, &sctp_ecn_enable, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_ECN_ENABLE,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "ecn_nonce",
	    SYSCTL_DESCR("Enable SCTP ECN Nonce"),
	    NULL, 0, &sctp_ecn_nonce, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_ECN_NONCE,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "strict_sack",
	    SYSCTL_DESCR("Enable SCTP Strict SACK checking"),
	    NULL, 0, &sctp_strict_sacks, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_STRICT_SACK,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "loopback_nocsum",
	    SYSCTL_DESCR("Enable NO Csum on packets sent on loopback"),
	    NULL, 0, &sctp_no_csum_on_loopback, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_NOCSUM_LO,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "strict_init",
	    SYSCTL_DESCR("Enable strict INIT/INIT-ACK singleton enforcement"),
	    NULL, 0, &sctp_strict_init, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_STRICT_INIT,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "peer_chkoh",
	    SYSCTL_DESCR("Amount to debit peers rwnd per chunk sent"),
	    NULL, 0, &sctp_peer_chunk_oh, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_PEER_CHK_OH,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "maxburst",
	    SYSCTL_DESCR("Default max burst for sctp endpoints"),
	    NULL, 0, &sctp_max_burst_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_MAXBURST,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "maxchunks",
	    SYSCTL_DESCR("Default max chunks on queue per asoc"),
	    NULL, 0, &sctp_max_chunks_on_queue, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_MAXCHUNKONQ,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "delayed_sack_time",
	    SYSCTL_DESCR("Default delayed SACK timer in msec"),
	    NULL, 0, &sctp_delayed_sack_time_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_DELAYED_SACK,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "heartbeat_interval",
	    SYSCTL_DESCR("Default heartbeat interval in msec"),
	    NULL, 0, &sctp_heartbeat_interval_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_HB_INTERVAL,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "pmtu_raise_time",
	    SYSCTL_DESCR("Default PMTU raise timer in sec"),
	    NULL, 0, &sctp_pmtu_raise_time_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_PMTU_RAISE,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "shutdown_guard_time",
	    SYSCTL_DESCR("Default shutdown guard timer in sec"),
	    NULL, 0, &sctp_shutdown_guard_time_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_SHUTDOWN_GUARD,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "secret_lifetime",
	    SYSCTL_DESCR("Default secret liftime in sec"),
	    NULL, 0, &sctp_secret_lifetime_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_SECRET_LIFETIME,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "rto_max",
	    SYSCTL_DESCR("Default maximum retransmission timeout in msec"),
	    NULL, 0, &sctp_rto_max_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_RTO_MAX,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "rto_min",
	    SYSCTL_DESCR("Default minimum retransmission timeout in msec"),
	    NULL, 0, &sctp_rto_min_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_RTO_MIN,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "rto_initial",
	    SYSCTL_DESCR("Default initial retransmission timeout in msec"),
	    NULL, 0, &sctp_rto_initial_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_RTO_INITIAL,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "init_rto_max",
	    SYSCTL_DESCR("Default maximum retransmission timeout during association setup in msec"),
	    NULL, 0, &sctp_init_rto_max_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_INIT_RTO_MAX,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "valid_cookie_life",
	    SYSCTL_DESCR("Default cookie lifetime in sec"),
	    NULL, 0, &sctp_valid_cookie_life_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_COOKIE_LIFE,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "init_rtx_max",
	    SYSCTL_DESCR("Default maximum number of retransmission for INIT chunks"),
	    NULL, 0, &sctp_init_rtx_max_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_INIT_RTX_MAX,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "assoc_rtx_max",
	    SYSCTL_DESCR("Default maximum number of retransmissions per association"),
	    NULL, 0, &sctp_assoc_rtx_max_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_ASSOC_RTX_MAX,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "path_rtx_max",
	    SYSCTL_DESCR("Default maximum of retransmissions per path"),
	    NULL, 0, &sctp_path_rtx_max_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_PATH_RTX_MAX,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "nr_outgoing_streams",
	    SYSCTL_DESCR("Default outgoing streams"),
	    NULL, 0, &sctp_nr_outgoing_streams_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_NR_OUTGOING_STREAMS,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "cmt_on_off",
	    SYSCTL_DESCR("CMT on-off flag"),
	    NULL, 0, &sctp_cmt_on_off, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_CMT_ON_OFF,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "cwnd_maxburst",
	    SYSCTL_DESCR("Use a CWND adjusting maxburst"),
	    NULL, 0, &sctp_use_cwnd_based_maxburst, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_CWND_MAXBURST,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "early_fast_retran",
	    SYSCTL_DESCR("Early Fast Retransmit with Timer"),
	    NULL, 0, &sctp_early_fr, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_EARLY_FR,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "use_rttvar_congctrl",
	    SYSCTL_DESCR("Use Congestion Control via rtt variation"),
	    NULL, 0, &sctp_use_rttvar_cc, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_RTTVAR_CC,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "deadlock_detect",
	    SYSCTL_DESCR("SMP Deadlock detection on/off"),
	    NULL, 0, &sctp_says_check_for_deadlock, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_DEADLOCK_DET,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "early_fast_retran_msec",
	    SYSCTL_DESCR("Early Fast Retransmit minimum Timer value"),
	    NULL, 0, &sctp_early_fr_msec, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_EARLY_FR_MSEC,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "auth_disable",
	    SYSCTL_DESCR("Disable SCTP AUTH requirement/function"),
	    NULL, 0, &sctp_auth_disable, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_AUTH_DISABLE,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "auth_random_len",
	    SYSCTL_DESCR("Length of AUTH RANDOMs"),
	    NULL, 0, &sctp_auth_random_len, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_AUTH_RANDOM_LEN,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "auth_hmac_id",
	    SYSCTL_DESCR("Default HMAC Id for SCTP AUTHentication"),
	    NULL, 0, &sctp_auth_hmac_id_default, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_AUTH_HMAC_ID,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "abc_l_var",
	    SYSCTL_DESCR("SCTP ABC max increase per SACK (L)"),
	    NULL, 0, &sctp_L2_abc_variable, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_ABC_L_VAR,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "max_chained_mbufs",
	    SYSCTL_DESCR("Default max number of small mbufs on a chain"),
	    NULL, 0, &sctp_mbuf_threshold_count, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_MAX_MBUF_CHAIN,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "cmt_use_dac",
	    SYSCTL_DESCR("CMT DAC on-off flag"),
	    NULL, 0, &sctp_cmt_use_dac, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_CMT_USE_DAC,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "do_sctp_drain",
	    SYSCTL_DESCR("Should SCTP respond to the drain calls"),
	    NULL, 0, &sctp_do_drain, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_DO_DRAIN,
	    CTL_EOL);


	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "warm_crc_table",
	    SYSCTL_DESCR("Should the CRC32c tables be warmed before checksum?"),
	    NULL, 0, &sctp_warm_the_crc32_table, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_WARM_CRC32,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "abort_at_limit",
	    SYSCTL_DESCR("When one-2-one hits qlimit abort"),
	    NULL, 0, &sctp_abort_if_one_2_one_hits_limit, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_QLIMIT_ABORT,
	    CTL_EOL);

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "strict_data_order",
	    SYSCTL_DESCR("Enforce strict data ordering, abort if control inside data"),
	    NULL, 0, &sctp_strict_data_order, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_STRICT_ORDER,
	    CTL_EOL);

#ifdef SCTP_DEBUG
	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
	    CTLTYPE_INT, "debug",
	    SYSCTL_DESCR("Configure debug output"),
	    NULL, 0, &sctp_debug_on, 0,
	    CTL_NET, PF_INET, IPPROTO_SCTP, SCTPCTL_DEBUG,
	    CTL_EOL);
#endif				/* SCTP_DEBUG */

}

#endif				/* __NetBSD__ */

/*
 * additional protosw entries for Mac OS X 10.4 lr is temporarily being used
 * for socket lock/unlock LR debug
 */
#if defined(SCTP_APPLE_FINE_GRAINED_LOCKING)

int
sctp_lock(struct socket *so, int refcount, int lr)
{
	int lr_saved;

#ifdef __ppc__
	if (lr == 0) {
		__asm__ volatile ("mflr %0":"=r" (lr_saved));
	} else
		lr_saved = lr;
#endif
	/*
	 * printf("sctp_lock called for so=%p with so->so_pcb=%p,
	 * so->type=%d, so->so_usecount=%d.\n", so, so->so_pcb, so->so_type,
	 * so->so_usecount);
	 */
	if (so->so_pcb) {
		lck_mtx_assert(((struct inpcb *)so->so_pcb)->inpcb_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(((struct inpcb *)so->so_pcb)->inpcb_mtx);
	} else {
		panic("sctp_lock: so=%x NO PCB! lr =%x\n", so, lr_saved);
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_NOTOWNED);
		lck_mtx_lock(so->so_proto->pr_domain->dom_mtx);
	}

	if (so->so_usecount < 0)
		panic("sctp_lock: so=%x so_pcb=%x lr=%x ref=%x\n",
		    so, so->so_pcb, lr_saved, so->so_usecount);

	if (refcount)
		so->so_usecount++;
	so->reserved3 = (void *)lr_saved;
	/*
	 * printf("sctp_lock returning for %p.\n", so);
	 */
	return (0);
}

int
sctp_unlock(struct socket *so, int refcount, int lr)
{
	int lr_saved;

#ifdef __ppc__
	if (lr == 0) {
		__asm__ volatile ("mflr %0":"=r" (lr_saved));
	} else
		lr_saved = lr;
#endif
	/*
	 * printf("sctp_unlock called for so=%p with so->so_pcb=%p,
	 * so->type=%d, so->so_usecount=%d.\n", so, so->so_pcb, so->so_type,
	 * so->so_usecount);
	 */
	if (refcount)
		so->so_usecount--;

	if (so->so_usecount < 0)
		panic("sctp_unlock: so=%x usecount=%x\n", so, so->so_usecount);

	if (so->so_pcb == NULL) {
		panic("sctp_unlock: so=%x NO PCB! lr =%x\n", so, lr_saved);
		lck_mtx_assert(so->so_proto->pr_domain->dom_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(so->so_proto->pr_domain->dom_mtx);
	} else {
		lck_mtx_assert(((struct inpcb *)so->so_pcb)->inpcb_mtx, LCK_MTX_ASSERT_OWNED);
		lck_mtx_unlock(((struct inpcb *)so->so_pcb)->inpcb_mtx);
	}
	so->reserved4 = (void *)lr_saved;
	/*
	 * printf("sctp_unlock returning for so=%p\n", so);
	 */
	return (0);
}

lck_mtx_t *
sctp_getlock(struct socket *so, int locktype)
{

	/*
	 * printf("sctp_getlock called for so=%p with so->so_pcb=%p,
	 * so->type=%d, so->so_usecount=%d.\n", so, so->so_pcb, so->so_type,
	 * so->so_usecount);
	 */
	if (so->so_pcb) {
		if (so->so_usecount < 0)
			panic("sctp_getlock: so=%x usecount=%x\n", so, so->so_usecount);
		/*
		 * printf("sctp_getlock returning %p.\n", ((struct inpcb
		 * *)so->so_pcb)->inpcb_mtx);
		 */
		return (((struct inpcb *)so->so_pcb)->inpcb_mtx);
	} else {
		panic("sctp_getlock: so=%x NULL so_pcb\n", so);
		return (so->so_proto->pr_domain->dom_mtx);
	}
}

void
sctp_lock_assert(struct socket *so)
{
	if (so->so_pcb) {
		lck_mtx_assert(((struct inpcb *)so->so_pcb)->inpcb_mtx, LCK_MTX_ASSERT_OWNED);
	} else {
		panic("sctp_lock_assert: so=%p has sp->so_pcb==NULL.\n", so);
	}
}

void
sctp_unlock_assert(struct socket *so)
{
	if (so->so_pcb) {
		lck_mtx_assert(((struct inpcb *)so->so_pcb)->inpcb_mtx, LCK_MTX_ASSERT_NOTOWNED);
	} else {
		panic("sctp_unlock_assert: so=%p has sp->so_pcb==NULL.\n", so);
	}
}

#endif				/* SCTP_APPLE_FINE_GRAINED_LOCKING */
