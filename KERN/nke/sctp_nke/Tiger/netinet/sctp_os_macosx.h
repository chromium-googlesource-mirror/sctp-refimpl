/*-
 * Copyright (c) 2006-2007, by Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 * a) Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in 
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its 
 *    contributors may be used to endorse or promote products derived 
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __sctp_os_macosx_h__
#define __sctp_os_macosx_h__

/*
 * includes
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h> 
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/resourcevar.h>
#include <sys/uio.h>
#if defined(__APPLE__)
#include <sys/proc_internal.h>
#include <sys/uio_internal.h>
#endif
#include <sys/random.h>
/*#include <sys/queue.h>*/
#include <sys/appleapiopts.h>

#include <machine/limits.h>

#include <IOKit/IOLib.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>

#ifdef INET6
#include <sys/domain.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6protosw.h>
#include <netinet6/nd6.h>
#include <netinet6/scope6_var.h>
#endif /* INET6 */

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#endif /* IPSEC */

#include <stdarg.h>

#if defined(HAVE_SCTP_PEELOFF_SOCKOPT)
#include <sys/file.h>
#include <sys/filedesc.h>
extern struct fileops socketops;
#endif /* HAVE_SCTP_PEELOFF_SOCKOPT */

#if defined(HAVE_NRL_INPCB)
#ifndef in6pcb
#define in6pcb		inpcb
#endif
#endif

#if defined(KERNEL) && !defined(_KERNEL)
#define _KERNEL
#endif

#define SCTP_LIST_EMPTY(list)	LIST_EMPTY(list)

#if defined(SCTP_LOCAL_TRACE_BUF) 
#define SCTP_CTR6 sctp_log_trace
#else
#define SCTP_CTR6 CTR6
#endif

/* Empty ktr statement for mac */
#define	CTR6(m, d, p1, p2, p3, p4, p5, p6)
#define SCTP_LTRACE_CHK(a, b, c, d)
#define SCTP_LTRACE_ERR(a, b, c, d) 
#define SCTP_LTRACE_ERR_RET_PKT(m, inp, stcb, net, file, err)
#define SCTP_LTRACE_ERR_RET(inp, stcb, net, file, err)


#define SCTP_BASE_INFO(type) system_base_info.sctppcbinfo.type
#define SCTP_BASE_STATS system_base_info.sctpstat
#define SCTP_BASE_STAT(elem) system_base_info.sctpstat.elem
#define SCTP_BASE_SYSCTL(var) system_base_info.sctpsysctl.var
#define SCTP_BASE_VAR(var) system_base_info.var

/*
 * debug macro
 */
#if defined(SCTP_DEBUG)
#define SCTPDBG(level, params...)					\
{									\
    do {								\
	if (SCTP_BASE_SYSCTL(sctp_debug_on) & level ) {			\
	    printf(params);						\
	}								\
    } while (0);							\
}
#define SCTPDBG_ADDR(level, addr)					\
{									\
    do {								\
	if (SCTP_BASE_SYSCTL(sctp_debug_on) & level ) {			\
	    sctp_print_address(addr);					\
	}								\
    } while (0);							\
}
#define SCTPDBG_PKT(level, iph, sh)					\
{									\
    do {								\
	    if (SCTP_BASE_SYSCTL(sctp_debug_on) & level) {		\
		    sctp_print_address_pkt(iph, sh);			\
	    }								\
    } while (0);							\
}
#else
#define SCTPDBG(level, params...)
#define SCTPDBG_ADDR(level, addr)
#define SCTPDBG_PKT(level, iph, sh)
#endif
#define SCTP_PRINTF(params...)	printf(params)

/*
 * Local address and interface list handling
 */
#define SCTP_MAX_VRF_ID		0
#define SCTP_SIZE_OF_VRF_HASH	1
#define SCTP_IFNAMSIZ		IFNAMSIZ
#define SCTP_DEFAULT_VRFID	0
#define SCTP_VRF_ADDR_HASH_SIZE	16
#define SCTP_VRF_IFN_HASH_SIZE	3
#define	SCTP_INIT_VRF_TABLEID(vrf)

#define SCTP_IFN_IS_IFT_LOOP(ifn) ((ifn)->ifn_type == IFT_LOOP)
#define SCTP_ROUTE_IS_REAL_LOOP(ro) ((ro)->ro_rt && (ro)->ro_rt->rt_ifa && (ro)->ro_rt->rt_ifa->ifa_ifp && (ro)->ro_rt->rt_ifa->ifa_ifp->if_type == IFT_LOOP)

/* MAC always needs a lock */
#define SCTP_PKTLOG_WRITERS_NEED_LOCK 0
/*
 * Access to IFN's to help with src-addr-selection
 */
/* This could return VOID if the index works but for BSD we provide both. */
#define SCTP_GET_IFN_VOID_FROM_ROUTE(ro) (void *)ro->ro_rt->rt_ifp
#define SCTP_GET_IF_INDEX_FROM_ROUTE(ro) (ro)->ro_rt->rt_ifp->if_index
#define SCTP_ROUTE_HAS_VALID_IFN(ro) ((ro)->ro_rt && (ro)->ro_rt->rt_ifp)

#define SCTP_UNUSED __attribute__((unused))

/* 
 * for per socket level locking strategy:
 * SCTP_INP_SO(sctpinp): returns socket on base inp structure from sctp_inpcb
 * SCTP_SOCKET_LOCK(so, refcnt): locks socket so with refcnt
 * SCTP_SOCKET_UNLOCK(so, refcnt): unlocks socket so with refcnt
 * SCTP_MTX_LOCK(lck): lock mutex
 * SCTP_MTX_UNLOCK(lck): unlock mutex
 * SCTP_MTX_TRYLOCK(lck): try lock mutex
 * SCTP_LOCK_EX(lck): lock exclusive
 * SCTP_UNLOCK_EX(lck): unlock exclusive
 * SCTP_TRYLOCK_EX(lck): trylock exclusive
 * SCTP_LOCK_SHARED(lck): lock shared
 * SCTP_UNLOCK_SHARED(lck): unlock shared
 * SCTP_TRYLOCK_SHARED(lck): trylock shared
 */
#define SCTP_INP_SO(sctpinp)	(sctpinp)->ip_inp.inp.inp_socket
#define SCTP_SOCKET_LOCK(so, refcnt)	socket_lock(so, refcnt)
#define SCTP_SOCKET_UNLOCK(so, refcnt)	socket_unlock(so, refcnt)
#define SCTP_MTX_LOCK(mtx)	lck_mtx_lock(mtx)
#define SCTP_MTX_UNLOCK(mtx)	lck_mtx_unlock(mtx)
#define SCTP_MTX_TRYLOCK(mtx)	lck_mtx_try_lock(mtx)
#define SCTP_LOCK_EXC(lck)	lck_rw_lock_exclusive(lck)
#define SCTP_UNLOCK_EXC(lck)	lck_rw_unlock_exclusive(lck)
#define SCTP_TRYLOCK_EXC(lck)	lck_rw_try_lock_exclusive(lck)
#define SCTP_LOCK_SHARED(lck)	lck_rw_lock_shared(lck)
#define SCTP_UNLOCK_SHARED(lck)	lck_rw_unlock_shared(lck)
#define SCTP_TRYLOCK_SHARED(lck) lck_rw_try_lock_shared(lck)




#define SCTP_M_SOCKOPT "sctp socketopt"
#define SCTP_M_ITER "sctp_iterator"
#define SCTP_M_VRF "sctp_vrf"
#define SCTP_M_IFA "sctp_ifa"
#define SCTP_M_IFN "sctp_ifn"
#define SCTP_M_MVRF "sctp_mvrf"
#define SCTP_M_TIMW "sctp_timewait"
#define SCTP_M_CMSG "SCTP_CMSG"
#define SCTP_M_STRESET "stream reset"
#define SCTp_M_MAP "maparray"
#define SCTP_M_STRMI "streamin"
#define SCTP_M_STRMO "streamout"
#define SCTP_M_ASC_ADDR "sctp_aadr"
#define SCTP_M_ASC_IT "sctp_asconf_iter"
#define SCTP_M_AUTH_CL "sctp auth chunklist"
#define SCTP_M_AUTH_KY  "sctp auth key"
#define SCTP_M_AUTH_HL "sctp auth hmac list"
#define SCTP_M_AUTH_IF "sctp_athi"
#define SCTP_M_COPYAL "sctp copy all"
#define SCTP_M_MAP "sctp asoc map descriptor"


/*
 * general memory allocation
 */
#if defined(__APPLE__)
#define SCTP_MALLOC(var, type, size, name) \
    do { \
	MALLOC(var, type, size, M_PCB, M_WAITOK); \
    } while (0)
#else
#define SCTP_MALLOC(var, type, size, name) \
    do { \
	MALLOC(var, type, size, name, M_NOWAIT); \
    } while (0)
#endif

#define SCTP_FREE(var, type)	FREE(var, M_PCB)

#define SCTP_MALLOC_SONAME(var, type, size) \
    do { \
	MALLOC(var, type, size, M_SONAME, M_WAITOK | M_ZERO); \
    } while (0)
#define SCTP_FREE_SONAME(var)	FREE(var, M_SONAME)


/*
 * zone allocation functions
 */
typedef struct vm_zone *sctp_zone_t;
extern zone_t kalloc_zone(vm_size_t);	/* XXX */

/* SCTP_ZONE_INIT: initialize the zone */
#define SCTP_ZONE_INIT(zone, name, size, number) \
	zone = (void *)kalloc_zone(size);

/* SCTP_ZONE_GET: allocate element from the zone */
#define SCTP_ZONE_GET(zone, type) \
	(type *)zalloc(zone);

/* SCTP_ZONE_FREE: free element from the zone */
#define SCTP_ZONE_FREE(zone, element) \
	zfree(zone, element);

#define SCTP_HASH_INIT(size, hashmark) hashinit(size, M_PCB, hashmark)
#define SCTP_HASH_FREE(table, hashmark) SCTP_FREE(table, M_PCB)

#if defined(APPLE_LEOPARD)
#define SCTP_M_COPYM m_copym
#else
struct mbuf *sctp_m_copym(struct mbuf *m, int off, int len, int wait);
#define SCTP_M_COPYM sctp_m_copym
#endif
/*
 * timers
 */
#include <netinet/sctp_callout.h>
#if defined(__APPLE__)
#ifdef _KERN_LOCKS_H_
extern lck_rw_t *sctp_calloutq_mtx;
#else
extern void *sctp_calloutq_mtx;
#endif
#define SCTP_TIMERQ_LOCK()	lck_rw_lock_exclusive(sctp_calloutq_mtx)
#define SCTP_TIMERQ_UNLOCK()	lck_rw_unlock_exclusive(sctp_calloutq_mtx)
#define SCTP_TIMERQ_LOCK_INIT()	sctp_calloutq_mtx = lck_rw_alloc_init(SCTP_MTX_GRP, SCTP_MTX_ATTR)
#define SCTP_TIMERQ_LOCK_DESTROY() lck_rw_free(sctp_calloutq_mtx, SCTP_MTX_GRP)
#endif

/* Mbuf manipulation and access macros  */
#define SCTP_BUF_LEN(m) (m->m_len)
#define SCTP_BUF_NEXT(m) (m->m_next)
#define SCTP_BUF_NEXT_PKT(m) (m->m_nextpkt)
#define SCTP_BUF_RESV_UF(m, size) m->m_data += size
#define SCTP_BUF_AT(m, size) m->m_data + size
#define SCTP_BUF_IS_EXTENDED(m) (m->m_flags & M_EXT)
#define SCTP_BUF_EXTEND_SIZE(m) (m->m_ext.ext_size)
#define SCTP_BUF_TYPE(m) (m->m_type)
#define SCTP_BUF_RECVIF(m) (m->m_pkthdr.rcvif)
#define SCTP_BUF_PREPEND(m, plen, how) ((m) = sctp_m_prepend_2((m), (plen), (how)))
struct mbuf *sctp_m_prepend_2(struct mbuf *m, int len, int how);


/*************************/
/*      MTU              */
/*************************/
#define SCTP_GATHER_MTU_FROM_IFN_INFO(ifn, ifn_index, af) ((struct ifnet *)ifn)->if_mtu
#define SCTP_GATHER_MTU_FROM_ROUTE(sctp_ifa, sa, rt) ((rt != NULL) ? rt->rt_rmx.rmx_mtu : 0)
#define SCTP_GATHER_MTU_FROM_INTFC(sctp_ifn) ((sctp_ifn->ifn_p != NULL) ? ((struct ifnet *)(sctp_ifn->ifn_p))->if_mtu : 0)
#define SCTP_SET_MTU_OF_ROUTE(sa, rt, mtu) \
	do { \
		if (rt != NULL) \
			rt->rt_rmx.rmx_mtu = mtu; \
	} while (0) 
/* (de-)register interface event notifications */
#define SCTP_REGISTER_INTERFACE(ifhandle, af)
#define SCTP_DEREGISTER_INTERFACE(ifhandle, af)

/*************************/
/* These are for logging */
/*************************/
/* return the base ext data pointer */
#define SCTP_BUF_EXTEND_BASE(m) (m->m_ext.ext_buf)
 /* return the refcnt of the data pointer */
#define SCTP_BUF_EXTEND_REFCNT(m) (*m->m_ext.ref_cnt)
/* return any buffer related flags, this is
 * used beyond logging for apple only.
 */
#define SCTP_BUF_GET_FLAGS(m) (m->m_flags)

/*
 * For APPLE this just accesses the M_PKTHDR length so it operates on an
 * mbuf with hdr flag. Other O/S's may have seperate packet header and mbuf
 * chain pointers.. thus the macro.
 */
#define SCTP_HEADER_TO_CHAIN(m) (m)
#define SCTP_DETACH_HEADER_FROM_CHAIN(m)
#define SCTP_HEADER_LEN(m) (m->m_pkthdr.len)
#define SCTP_GET_HEADER_FOR_OUTPUT(o_pak) 0
#define SCTP_RELEASE_HEADER(m)
#define SCTP_RELEASE_PKT(m)	sctp_m_freem(m)
#define SCTP_ENABLE_UDP_CSUM(m) do { \
					m->m_pkthdr.csum_flags = CSUM_UDP; \
					m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum); \
				} while (0)
static inline int SCTP_GET_PKT_VRFID(void *m, uint32_t vrf_id) {
	vrf_id = SCTP_DEFAULT_VRFID;
	return (0);
}

/* Attach the chain of data into the sendable packet. */
#define SCTP_ATTACH_CHAIN(pak, m, packet_length) do { \
                                                 pak = m; \
                                                 pak->m_pkthdr.len = packet_length; \
                         } while(0)

/* Other m_pkthdr type things */
#define SCTP_IS_IT_BROADCAST(dst, m) in_broadcast(dst, m->m_pkthdr.rcvif)
#define SCTP_IS_IT_LOOPBACK(m) ((m->m_pkthdr.rcvif == NULL) || (m->m_pkthdr.rcvif->if_type == IFT_LOOP))

#define SCTP_ALIGN_TO_END(m, len) if(m->m_flags & M_PKTHDR) { \
                                     MH_ALIGN(m, len); \
                                  } else if ((m->m_flags & M_EXT) == 0) { \
                                     M_ALIGN(m, len); \
                                  }


/*
 * This converts any input packet header into the chain of data holders,
 * for APPLE its a NOP.
 */

/* Macro's for getting length from V6/V4 header */
#define SCTP_GET_IPV4_LENGTH(iph) (iph->ip_len)
#define SCTP_GET_IPV6_LENGTH(ip6) (ip6->ip6_plen)

/* get the v6 hop limit */
#define SCTP_GET_HLIM(inp, ro)	in6_selecthlim((struct in6pcb *)&inp->ip_inp.inp, (ro ? (ro->ro_rt ? (ro->ro_rt->rt_ifp) : (NULL)) : (NULL)));

/* is the endpoint v6only? */
#define SCTP_IPV6_V6ONLY(inp)	(((struct inpcb *)inp)->inp_flags & IN6P_IPV6_V6ONLY)
/* is the socket non-blocking? */
#define SCTP_SO_IS_NBIO(so)	((so)->so_state & SS_NBIO)
#define SCTP_SET_SO_NBIO(so)	((so)->so_state |= SS_NBIO)
#define SCTP_CLEAR_SO_NBIO(so)	((so)->so_state &= ~SS_NBIO)
/* get the socket type */
#define SCTP_SO_TYPE(so)	((so)->so_type)
/* reserve sb space for a socket */
#define SCTP_SORESERVE(so, send, recv)	soreserve(so, send, recv)
/* wakeup a socket */
#define SCTP_SOWAKEUP(so)	wakeup(&(so)->so_timeo)
/* clear the socket buffer state */
#define SCTP_SB_CLEAR(sb)	\
	(sb).sb_cc = 0;		\
	(sb).sb_mb = NULL;	\
	(sb).sb_mbcnt = 0;

#define SCTP_SB_LIMIT_RCV(so) so->so_rcv.sb_hiwat
#define SCTP_SB_LIMIT_SND(so) so->so_snd.sb_hiwat

/*
 * routes, output, etc.
 */
typedef struct route	sctp_route_t;
typedef struct rtentry	sctp_rtentry_t;
#define SCTP_RTALLOC(ro, vrf_id) rtalloc_ign((struct route *)ro, 0UL)

/* Future zero copy wakeup/send  function */
#define SCTP_ZERO_COPY_EVENT(inp, so)
/* This is re-pulse ourselves for sendbuf */  
#define SCTP_ZERO_COPY_SENDQ_EVENT(inp, so)

/*
 * IP output routines
 */
#define SCTP_IP_ID(inp) (ip_id)

#if defined(APPLE_LEOPARD)
#define SCTP_IP_OUTPUT(result, o_pak, ro, stcb, vrf_id) \
{ \
	int o_flgs = 0; \
	if (stcb && stcb->sctp_ep && stcb->sctp_ep->sctp_socket) { \
		o_flgs = IP_RAWOUTPUT | (stcb->sctp_ep->sctp_socket->so_options & SO_DONTROUTE); \
	} else { \
		o_flgs = IP_RAWOUTPUT; \
	} \
	result = ip_output(o_pak, NULL, ro, o_flgs, NULL, NULL); \
}
#else
#define SCTP_IP_OUTPUT(result, o_pak, ro, stcb, vrf_id) \
{ \
	int o_flgs = 0; \
	if (stcb && stcb->sctp_ep && stcb->sctp_ep->sctp_socket) { \
		o_flgs = IP_RAWOUTPUT | (stcb->sctp_ep->sctp_socket->so_options & SO_DONTROUTE); \
	} else { \
		o_flgs = IP_RAWOUTPUT; \
	} \
	result = ip_output(o_pak, NULL, ro, o_flgs, NULL); \
}
#endif
#define SCTP_IP6_OUTPUT(result, o_pak, ro, ifp, stcb, vrf_id) \
{ \
 	if (stcb && stcb->sctp_ep) \
		result = ip6_output(o_pak, \
				    ((struct in6pcb *)(stcb->sctp_ep))->in6p_outputopts, \
				    (ro), 0, 0, ifp, 0); \
	else \
		result = ip6_output(o_pak, NULL, (ro), 0, NULL, ifp, 0); \
}

struct mbuf *
sctp_get_mbuf_for_msg(unsigned int space_needed, 
		      int want_header, int how, int allonebuf, int type);

/*
 * SCTP AUTH
 */
#define SCTP_READ_RANDOM(buf, len)	read_random(buf, len)

#ifdef USE_SCTP_SHA1
#include <netinet/sctp_sha1.h>
#else
#if defined(APPLE_LEOPARD)
#include <libkern/crypto/sha1.h>
#else
#include <crypto/sha1.h>
#endif
/* map standard crypto API names */
#define SHA1_Init	SHA1Init
#define SHA1_Update	SHA1Update
#define SHA1_Final(x,y)	SHA1Final((caddr_t)x, y)
#endif

#if defined(HAVE_SHA2)
#include <crypto/sha2/sha2.h>
#endif

#if defined(APPLE_LEOPARD)
#include <libkern/crypto/md5.h>
#else
#include <sys/md5.h>
#endif
/* map standard crypto API names */
#define MD5_Init	MD5Init
#define MD5_Update	MD5Update
#define MD5_Final	MD5Final


/*
 * Other MacOS specific
 */

/* Apple KPI defines for atomic operations */
#include <libkern/OSAtomic.h>
#define atomic_add_int(addr, val)	OSAddAtomic(val, (SInt32 *)addr)
#define atomic_fetchadd_int(addr, val)	OSAddAtomic(val, (SInt32 *)addr)
#define atomic_subtract_int(addr, val)	OSAddAtomic((-val), (SInt32 *)addr)
#define atomic_add_16(addr, val)	OSAddAtomic16(val, (SInt16 *)addr)
#define atomic_cmpset_int(dst, exp, src) OSCompareAndSwap(exp, src, (UInt32 *)dst)

#define SCTP_DECREMENT_AND_CHECK_REFCOUNT(addr) (atomic_fetchadd_int(addr, -1) == 1)
#if defined(INVARIANTS)
#define SCTP_SAVE_ATOMIC_DECREMENT(addr, val) \
{ \
	int32_t oldval; \
	oldval = atomic_fetchadd_int(addr, -val); \
	if (oldval < val) { \
		panic("Counter goes negative"); \
	} \
}
#else
#define SCTP_SAVE_ATOMIC_DECREMENT(addr, val) \
{ \
	int32_t oldval; \
	oldval = atomic_fetchadd_int(addr, -val); \
	if (oldval < val) { \
		*addr = 0; \
	} \
}
#endif
/* additional protosw entries for Mac OS X 10.4 */
#if defined(__APPLE__)
int sctp_lock(struct socket *so, int refcount, int lr);
int sctp_unlock(struct socket *so, int refcount, int lr);

#ifdef _KERN_LOCKS_H_
lck_mtx_t *sctp_getlock(struct socket *so, int locktype);
#else
void *sctp_getlock(struct socket *so, int locktype);
#endif /* _KERN_LOCKS_H_ */
void sctp_lock_assert(struct socket *so);
void sctp_unlock_assert(struct socket *so);
#endif /* __APPLE__ */

/* emulate the BSD 'ticks' clock */
extern int ticks;
#define SCTP_GET_CYCLECOUNT ticks;
#define KTR_SUBSYS 1

#define sctp_get_tick_count() (ticks)

/* XXX: Hopefully temporary until APPLE changes to newer defs like other BSDs */
#define if_addrlist	if_addrhead
#define if_list		if_link
#define ifa_list	ifa_link

/* MacOS specific timer functions */
void sctp_start_main_timer(void);
void sctp_stop_main_timer(void);

/* address monitor thread */
void sctp_address_monitor_start(void);
void sctp_address_monitor_stop(void);
#define SCTP_PROCESS_STRUCT thread_t

#endif
