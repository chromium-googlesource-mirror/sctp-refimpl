/*-
 * Copyright (c) 2001-2007, Cisco Systems, Inc. All rights reserved.
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

/* $KAME: sctp_pcb.h,v 1.21 2005/07/16 01:18:47 suz Exp $	 */

#ifdef __FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netinet/sctp_pcb.h,v 1.13 2007/05/02 12:50:13 rrs Exp $");
#endif

#ifndef __sctp_pcb_h__
#define __sctp_pcb_h__

#include <netinet/sctp_os.h>
#include <netinet/sctp.h>
#include <netinet/sctp_constants.h>

LIST_HEAD(sctppcbhead, sctp_inpcb);
LIST_HEAD(sctpasochead, sctp_tcb);
LIST_HEAD(sctpladdr, sctp_laddr);
LIST_HEAD(sctpvtaghead, sctp_tagblock);
LIST_HEAD(sctp_vrflist, sctp_vrf);
LIST_HEAD(sctp_ifnlist, sctp_ifn);
LIST_HEAD(sctp_ifalist, sctp_ifa);
TAILQ_HEAD(sctp_readhead, sctp_queued_to_read);
TAILQ_HEAD(sctp_streamhead, sctp_stream_queue_pending);

#include <netinet/sctp_structs.h>
#include <netinet/sctp_auth.h>

#define SCTP_PCBHASH_ALLADDR(port, mask) (port & mask)
#define SCTP_PCBHASH_ASOC(tag, mask) (tag & mask)

struct sctp_vrf {
	LIST_ENTRY (sctp_vrf) next_vrf;
	struct sctp_ifalist *vrf_addr_hash;
	struct sctp_ifnlist *vrf_ifn_hash;
	struct sctp_ifnlist ifnlist;
	uint32_t vrf_id;
	uint32_t total_ifa_count;
	u_long   vrf_addr_hashmark;
	u_long   vrf_ifn_hashmark;
};

struct sctp_ifn {
	struct sctp_ifalist ifalist;
	struct sctp_vrf *vrf;
	LIST_ENTRY(sctp_ifn) next_ifn;
	LIST_ENTRY(sctp_ifn) next_bucket;
	void     *ifn_p;	/* never access without appropriate lock */
	uint32_t ifn_mtu;
	uint32_t ifn_type;
	uint32_t ifn_index;	/* shorthand way to look at ifn for reference */
	uint32_t refcount;	/* number of reference held should be >= ifa_count */
	uint32_t ifa_count;	/* IFA's we hold (in our list - ifalist)*/
	char     ifn_name[SCTP_IFNAMSIZ];
};

/* SCTP local IFA flags */
#define SCTP_ADDR_VALID         0x00000001	/* its up and active */
#define SCTP_BEING_DELETED      0x00000002	/* being deleted,
						 * when refcount = 0. Note
						 * that it is pulled from the ifn list
						 * and ifa_p is nulled right away but
						 * it cannot be freed until the last *net 
						 * pointing to it is deleted.
						 */
#define SCTP_ADDR_DEFER_USE     0x00000004	/* Hold off using this one */
#define SCTP_ADDR_IFA_UNUSEABLE 0x00000008

struct sctp_ifa {
	LIST_ENTRY(sctp_ifa) next_ifa;
	LIST_ENTRY(sctp_ifa) next_bucket;
	struct sctp_ifn *ifn_p;	/* back pointer to parent ifn */
	void    *ifa;		/* pointer to ifa, needed for flag
				 * update for that we MUST lock
				 * appropriate locks. This is for V6.
				 */
	union sctp_sockstore address;
#if defined(__Panda__)
	struct ip_addr ip_addr;	/* internal address format */
#endif
	uint32_t refcount;	/* number of folks refering to this */
 	uint32_t flags;
	uint32_t localifa_flags;
	uint8_t src_is_loop;
	uint8_t src_is_priv;
	uint8_t src_is_glob;
	uint8_t resv;

};

struct sctp_laddr {
	LIST_ENTRY(sctp_laddr) sctp_nxt_addr;	/* next in list */
	struct sctp_ifa *ifa;
	uint32_t action;		/* Used during asconf and adding 
					 * if no-zero src-addr selection will
					 * not consider this address.
					 */
};

struct sctp_block_entry {
	int error;
};

struct sctp_timewait {
	uint32_t tv_sec_at_expire;	/* the seconds from boot to expire */
	uint32_t v_tag;		/* the vtag that can not be reused */
};

struct sctp_tagblock {
	LIST_ENTRY(sctp_tagblock) sctp_nxt_tagblock;
	struct sctp_timewait vtag_block[SCTP_NUMBER_IN_VTAG_BLOCK];
};

struct sctp_epinfo {
	struct sctpasochead *sctp_asochash;
	u_long hashasocmark;

	struct sctppcbhead *sctp_ephash;
	u_long hashmark;

	struct sctpasochead *sctp_restarthash;
	u_long hashrestartmark;
	/*-
	 * The TCP model represents a substantial overhead in that we get an
	 * additional hash table to keep explicit connections in. The
	 * listening TCP endpoint will exist in the usual ephash above and
	 * accept only INIT's. It will be incapable of sending off an INIT.
	 * When a dg arrives we must look in the normal ephash. If we find a
	 * TCP endpoint that will tell us to go to the specific endpoint
	 * hash and re-hash to find the right assoc/socket. If we find a UDP
	 * model socket we then must complete the lookup. If this fails,
	 * i.e. no association can be found then we must continue to see if
	 * a sctp_peeloff()'d socket is in the tcpephash (a spun off socket
	 * acts like a TCP model connected socket).
	 */
	struct sctppcbhead *sctp_tcpephash;
	u_long hashtcpmark;
	uint32_t hashtblsize;

	struct sctp_vrflist *sctp_vrfhash;
	u_long hashvrfmark;

	struct sctppcbhead listhead;
	struct sctpladdr addr_wq;

#if defined(SCTP_APPLE_FINE_GRAINED_LOCKING)
	struct inpcbhead inplisthead;
#endif
	struct sctpiterators iteratorhead;

	/* ep zone info */
	sctp_zone_t ipi_zone_ep;
	sctp_zone_t ipi_zone_asoc;
	sctp_zone_t ipi_zone_laddr;
	sctp_zone_t ipi_zone_net;
	sctp_zone_t ipi_zone_chunk;
	sctp_zone_t ipi_zone_readq;
	sctp_zone_t ipi_zone_strmoq;

#if defined(__FreeBSD__) && __FreeBSD_version >= 503000
	struct mtx ipi_ep_mtx;
	struct mtx it_mtx;
	struct mtx ipi_iterator_wq_mtx;
	struct mtx ipi_addr_mtx;
#elif defined(SCTP_PROCESS_LEVEL_LOCKS)
	pthread_mutex_t ipi_ep_mtx;
	pthread_mutex_t it_mtx;
	pthread_mutex_t ipi_iterator_wq_mtx;
	pthread_mutex_t ipi_addr_mtx;
	pthread_mutex_t ipi_count_mtx;
#elif defined(SCTP_APPLE_FINE_GRAINED_LOCKING)
#ifdef _KERN_LOCKS_H_
	lck_grp_attr_t *mtx_grp_attr;
	lck_grp_t *mtx_grp;
	lck_attr_t *mtx_attr;
	lck_rw_t *ipi_ep_mtx;
	lck_mtx_t *it_mtx;
	lck_mtx_t *ipi_iterator_wq_mtx;
	lck_mtx_t *ipi_count_mtx;
	lck_mtx_t *logging_mtx;
#else
	void *mtx_grp_attr;
	void *mtx_grp;
	void *mtx_attr;
	void *ipi_ep_mtx;
	void *it_mtx;
	void *ipi_iterator_wq_mtx;
	void *ipi_count_mtx;
	void *logging_mtx;
#endif /* _KERN_LOCKS_H_ */
#endif
	uint32_t ipi_count_ep;

	/* assoc/tcb zone info */
	uint32_t ipi_count_asoc;

	/* local addrlist zone info */
	uint32_t ipi_count_laddr;

	/* remote addrlist zone info */
	uint32_t ipi_count_raddr;

	/* chunk structure list for output */
	uint32_t ipi_count_chunk;

	/* socket queue zone info */
	uint32_t ipi_count_readq;

	/* socket queue zone info */
	uint32_t ipi_count_strmoq;

	/* system wide number of free chunks hanging around */
	uint32_t ipi_free_chunks;
	uint32_t ipi_free_strmoq;

	struct sctpvtaghead vtag_timewait[SCTP_STACK_VTAG_HASH_SIZE];

	/* address work queue handling */
#if defined(SCTP_USE_THREAD_BASED_ITERATOR)
	uint32_t iterator_running;
	SCTP_PROCESS_STRUCT thread_proc;
#if defined(SCTP_PROCESS_LEVEL_LOCKS)
	pthread_cond_t iterator_wakeup;
#endif
#endif
	struct sctp_timer addr_wq_timer;

#ifdef _SCTP_NEEDS_CALLOUT_
	struct calloutlist callqueue;
#endif
};

/*-
 * Here we have all the relevant information for each SCTP entity created. We
 * will need to modify this as approprate. We also need to figure out how to
 * access /dev/random.
 */
struct sctp_pcb {
	unsigned int time_of_secret_change;	/* number of seconds from
						 * timeval.tv_sec */
	uint32_t secret_key[SCTP_HOW_MANY_SECRETS][SCTP_NUMBER_OF_SECRETS];
	unsigned int size_of_a_cookie;

	unsigned int sctp_timeoutticks[SCTP_NUM_TMRS];
	unsigned int sctp_minrto;
	unsigned int sctp_maxrto;
	unsigned int initial_rto;
	int initial_init_rto_max;

	unsigned int sctp_sack_freq;
	uint32_t sctp_sws_sender;
	uint32_t sctp_sws_receiver;

	/* authentication related fields */
	struct sctp_keyhead shared_keys;
	sctp_auth_chklist_t *local_auth_chunks;
	sctp_hmaclist_t *local_hmacs;
	uint16_t default_keyid;

	/* various thresholds */
	/* Max times I will init at a guy */
	uint16_t max_init_times;

	/* Max times I will send before we consider someone dead */
	uint16_t max_send_times;

	uint16_t def_net_failure;

	/* number of streams to pre-open on a association */
	uint16_t pre_open_stream_count;
	uint16_t max_open_streams_intome;

	/* random number generator */
	uint32_t random_counter;
	uint8_t random_numbers[SCTP_SIGNATURE_ALOC_SIZE];
	uint8_t random_store[SCTP_SIGNATURE_ALOC_SIZE];

	/*
	 * This timer is kept running per endpoint.  When it fires it will
	 * change the secret key.  The default is once a hour
	 */
	struct sctp_timer signature_change;
	int def_cookie_life;
	/* defaults to 0 */
	int auto_close_time;
	uint32_t initial_sequence_debug;
	uint32_t adaptation_layer_indicator;
	char store_at;
	uint8_t max_burst;
	char current_secret_number;
	char last_secret_number;
};

#ifndef SCTP_ALIGNMENT
#define SCTP_ALIGNMENT 32
#endif

#ifndef SCTP_ALIGNM1
#define SCTP_ALIGNM1 (SCTP_ALIGNMENT-1)
#endif

#define sctp_lport ip_inp.inp.inp_lport

struct sctp_inpcb {
	/*-
	 * put an inpcb in front of it all, kind of a waste but we need to
	 * for compatability with all the other stuff.
	 */
	union {
		struct inpcb inp;
		char align[(sizeof(struct in6pcb) + SCTP_ALIGNM1) &
		        ~SCTP_ALIGNM1];
	}     ip_inp;

#if defined(SCTP_APPLE_FINE_GRAINED_LOCKING)
	/* leave some space in case i386 inpcb is bigger than ppc */
	uint8_t		padding[128];
#endif

	/* Socket buffer lock protects read_queue and of course sb_cc */
	struct sctp_readhead read_queue;

        LIST_ENTRY(sctp_inpcb) sctp_list;	/* lists all endpoints */
        /* hash of all endpoints for model */
	LIST_ENTRY(sctp_inpcb) sctp_hash;
	/* count of local addresses bound, 0 if bound all */
	int laddr_count;

	/* list of addrs in use by the EP, NULL if bound-all */
	struct sctpladdr sctp_addr_list;
	/* used for source address selection rotation when we are subset bound */
	struct sctp_laddr *next_addr_touse;

	/* back pointer to our socket */
	struct socket *sctp_socket;
	uint32_t sctp_flags;	/* INP state flag set */
	uint32_t sctp_features;	/* Feature flags */
	struct sctp_pcb sctp_ep;/* SCTP ep data */
	/* head of the hash of all associations */
	struct sctpasochead *sctp_tcbhash;
	u_long sctp_hashmark;
	/* head of the list of all associations */
	struct sctpasochead sctp_asoc_list;
#ifdef SCTP_TRACK_FREED_ASOCS
	struct sctpasochead sctp_asoc_free_list;
#endif
	struct sctp_iterator *inp_starting_point_for_iterator;
	uint32_t sctp_frag_point;
	uint32_t partial_delivery_point;
	uint32_t sctp_context;
	struct sctp_sndrcvinfo def_send;
	/*-
	 * These three are here for the sosend_dgram
	 * (pkt, pkt_last and control).
	 * routine. However, I don't think anyone in
	 * the current FreeBSD kernel calls this. So
	 * they are candidates with sctp_sendm for
	 * de-supporting.
	 */
	struct mbuf *pkt, *pkt_last;
	struct mbuf *control;
#if !(defined(__FreeBSD__) || defined(__APPLE__))
#ifndef INP_IPV6
#define INP_IPV6	0x1
#endif
#ifndef INP_IPV4
#define INP_IPV4	0x2
#endif
	uint8_t inp_vflag;
	uint8_t inp_ip_ttl;
	uint8_t inp_ip_tos;
	uint8_t inp_ip_resv;
#endif
#if defined(__FreeBSD__) && __FreeBSD_version >= 503000
	struct mtx inp_mtx;
	struct mtx inp_create_mtx;
	struct mtx inp_rdata_mtx;
	int32_t refcount;
#elif defined(SCTP_PROCESS_LEVEL_LOCKS)
	pthread_mutex_t inp_mtx;
	pthread_mutex_t inp_create_mtx;
	pthread_mutex_t inp_rdata_mtx;
	int32_t refcount;
#endif
#if defined(SCTP_APPLE_FINE_GRAINED_LOCKING)
	int32_t refcount;

	uint32_t lock_caller1;
	uint32_t lock_caller2;
	uint32_t lock_caller3;
	uint32_t unlock_caller1;
	uint32_t unlock_caller2;
	uint32_t unlock_caller3;
	uint32_t getlock_caller1;
	uint32_t getlock_caller2;
	uint32_t getlock_caller3;
	uint32_t gen_count;
	uint32_t lock_gen_count;
	uint32_t unlock_gen_count;
	uint32_t getlock_gen_count;

	uint32_t i_am_here_file;
	uint32_t i_am_here_line;
#endif
	uint32_t def_vrf_id;
#ifdef SCTP_MVRF
	uint32_t *m_vrf_ids;
	uint32_t num_vrfs;
	uint32_t vrf_size;
#endif
	uint32_t total_sends;
	uint32_t total_recvs;
	uint32_t last_abort_code;
	uint32_t total_nospaces;
};

struct sctp_tcb {
	struct socket *sctp_socket;	/* back pointer to socket */
	struct sctp_inpcb *sctp_ep;	/* back pointer to ep */
	LIST_ENTRY(sctp_tcb) sctp_tcbhash;	/* next link in hash
						 * table */
        LIST_ENTRY(sctp_tcb) sctp_tcblist;	/* list of all of the
						 * TCB's */
        LIST_ENTRY(sctp_tcb) sctp_tcbrestarhash;	/* next link in restart
							 * hash table */
        LIST_ENTRY(sctp_tcb) sctp_asocs;	/* vtag hash list */
	struct sctp_block_entry *block_entry;	/* pointer locked by  socket
						 * send buffer */
	struct sctp_association asoc;
	/*-
	 * freed_by_sorcv_sincelast is protected by the sockbuf_lock NOT the
	 * tcb_lock. Its special in this way to help avoid extra mutex calls
	 * in the reading of data.
	 */
	uint32_t freed_by_sorcv_sincelast;
	uint32_t total_sends;
	uint32_t total_recvs;
	int freed_from_where;
	uint16_t rport;		/* remote port in network format */
	uint16_t resv;
#if defined(__FreeBSD__) && __FreeBSD_version >= 503000
	struct mtx tcb_mtx;
	struct mtx tcb_send_mtx;
#elif defined(SCTP_PROCESS_LEVEL_LOCKS)
	pthread_mutex_t tcb_mtx;
	pthread_mutex_t tcb_send_mtx;
#endif
};


#if defined(__FreeBSD__) && __FreeBSD_version >= 503000

#include <netinet/sctp_lock_bsd.h>

#elif defined(SCTP_APPLE_FINE_GRAINED_LOCKING)
/*
 * Apple MacOS X 10.4 "Tiger"
 */

#include <netinet/sctp_lock_apple_fg.h>

#elif defined(SCTP_PROCESS_LEVEL_LOCKS)

#include <netinet/sctp_process_lock.h>

#else
/*
 * Pre-5.x FreeBSD, NetBSD and others.
 */
#include <netinet/sctp_lock_empty.h>
#endif

#if defined(_KERNEL)

extern struct sctp_epinfo sctppcbinfo;

int SCTP6_ARE_ADDR_EQUAL(struct in6_addr *a, struct in6_addr *b);

void sctp_fill_pcbinfo(struct sctp_pcbinfo *);

struct sctp_ifn *
sctp_find_ifn(struct sctp_vrf *vrf, void *ifn, uint32_t ifn_index);

struct sctp_vrf *sctp_allocate_vrf(int vrfid);

struct sctp_vrf *sctp_find_vrf(uint32_t vrfid);

struct sctp_ifa *
sctp_add_addr_to_vrf(uint32_t vrfid,
		     void *ifn, uint32_t ifn_index, uint32_t ifn_type,
		     const char *if_name,
		     void *ifa, struct sockaddr *addr, uint32_t ifa_flags, int dynamic_add);

void sctp_update_ifn_mtu(uint32_t vrf_id, uint32_t ifn_index, uint32_t mtu);

void sctp_free_ifn(struct sctp_ifn *sctp_ifnp);
void sctp_free_ifa(struct sctp_ifa *sctp_ifap);

void sctp_delete_ifn(struct sctp_ifn *sctp_ifnp);

void sctp_del_addr_from_vrf(uint32_t vrfid, struct sockaddr *addr,
			    uint32_t ifn_index);



struct sctp_nets *sctp_findnet(struct sctp_tcb *, struct sockaddr *);

struct sctp_inpcb *sctp_pcb_findep(struct sockaddr *, int, int, uint32_t);

#if defined(__FreeBSD__) && __FreeBSD_version >= 500000
int sctp_inpcb_bind(struct socket *, struct sockaddr *, struct thread *);

#else
int sctp_inpcb_bind(struct socket *, struct sockaddr *, struct proc *);

#endif

struct sctp_tcb *
sctp_findassociation_addr(struct mbuf *, int, int,
    struct sctphdr *, struct sctp_chunkhdr *, struct sctp_inpcb **,
    struct sctp_nets **, uint32_t vrf_id);

struct sctp_tcb *
sctp_findassociation_addr_sa(struct sockaddr *,
    struct sockaddr *, struct sctp_inpcb **, struct sctp_nets **, int, uint32_t);

void
sctp_move_pcb_and_assoc(struct sctp_inpcb *, struct sctp_inpcb *,
    struct sctp_tcb *);

/*-
 * For this call ep_addr, the to is the destination endpoint address of the
 * peer (relative to outbound). The from field is only used if the TCP model
 * is enabled and helps distingush amongst the subset bound (non-boundall).
 * The TCP model MAY change the actual ep field, this is why it is passed.
 */
struct sctp_tcb *
sctp_findassociation_ep_addr(struct sctp_inpcb **,
    struct sockaddr *, struct sctp_nets **, struct sockaddr *,
    struct sctp_tcb *);

struct sctp_tcb *
sctp_findassociation_ep_asocid(struct sctp_inpcb *,
    sctp_assoc_t, int);

struct sctp_tcb *
sctp_findassociation_ep_asconf(struct mbuf *, int, int,
    struct sctphdr *, struct sctp_inpcb **, struct sctp_nets **);

int sctp_inpcb_alloc(struct socket *);

int sctp_is_address_on_local_host(struct sockaddr *addr, uint32_t vrf_id);

void sctp_inpcb_free(struct sctp_inpcb *, int, int);

struct sctp_tcb *
sctp_aloc_assoc(struct sctp_inpcb *, struct sockaddr *,
    int, int *, uint32_t, uint32_t);

int sctp_free_assoc(struct sctp_inpcb *, struct sctp_tcb *, int, int);

void
sctp_add_vtag_to_timewait(struct sctp_inpcb *, uint32_t, uint32_t);

int sctp_add_local_addr_ep(struct sctp_inpcb *, struct sctp_ifa *, uint32_t);

int sctp_insert_laddr(struct sctpladdr *, struct sctp_ifa *, uint32_t);

void sctp_remove_laddr(struct sctp_laddr *);

int sctp_del_local_addr_ep(struct sctp_inpcb *, struct sctp_ifa *);

void sctp_set_initial_cc_param(struct sctp_tcb *,struct sctp_nets *net);


int sctp_add_remote_addr(struct sctp_tcb *, struct sockaddr *, int, int);

void sctp_remove_net(struct sctp_tcb *, struct sctp_nets *);

int sctp_del_remote_addr(struct sctp_tcb *, struct sockaddr *);

void sctp_pcb_init(void);

int sctp_add_local_addr_assoc(struct sctp_tcb *, struct sctp_ifa *, int);

int sctp_del_local_addr_assoc(struct sctp_tcb *, struct sctp_ifa *);

int
sctp_load_addresses_from_init(struct sctp_tcb *, struct mbuf *, int, int,
    int, struct sctphdr *, struct sockaddr *);

int
sctp_set_primary_addr(struct sctp_tcb *, struct sockaddr *,
    struct sctp_nets *);

int sctp_is_vtag_good(struct sctp_inpcb *, uint32_t, struct timeval *);

/* void sctp_drain(void); */

int sctp_destination_is_reachable(struct sctp_tcb *, struct sockaddr *);

/*-
 * Null in last arg inpcb indicate run on ALL ep's. Specific inp in last arg
 * indicates run on ONLY assoc's of the specified endpoint.
 */
int
sctp_initiate_iterator(inp_func inpf, 
		       asoc_func af, 
		       inp_func inpe, 
		       uint32_t, uint32_t,
		       uint32_t, void *, 
		       uint32_t, 
		       end_func ef, 
		       struct sctp_inpcb *, 
		       uint8_t co_off);

#endif				/* _KERNEL */
#endif				/* !__sctp_pcb_h__ */
