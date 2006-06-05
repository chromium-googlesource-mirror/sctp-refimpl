#ifndef __rserpool_lib_h__
#define __rserpool_lib_h__
#include <pthread.h>
#include <dlist.h>
#include <HashedTbl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
/*
 * We first need to have the base structure
 * of a Pool that is kept inside the library.
 * This is what will be the holder for all
 * rserpool stuff. We next have the stucture
 * for the actual PE's themselves. These will
 * need to point back to the Pool and reference
 * count it. Each one will also need to be hashed
 * via the asocid and all the addresses/ports so
 * we can figure out from a received message what
 * the actual Pool.
 *
 * So we end up with:
 *
 * assoc-id    --maps--> PE
 * port+addr's --maps--> PE
 * name        --maps--> Pool
 * Pool has a list of PE's
 *
 * We map into a global process wide
 * table based on the socket descriptor.
 * This table determines which PE/PU we are.
 *
 * sd --sd2pool--> hashes out a hash table for the socket
 * you get back a pointer to a struct xxx
 *
 */


/* The extern socket hash */
struct rsp_global_info {
	int			rsp_number_sd;	/* count of sd's when 0 un-init */
	HashedTbl		*sd_pool;	/* hashed pool of sd's */
	dlist_t			*timer_list;	/* list of timers running */
	pthread_t 		tmr_thread; 	/* thread for timeouts */
	pthread_mutex_t		sd_pool_mtx;	/* mutex for sd_pool   */
	pthread_cond_t		rsp_tmr_cnd;	/* condition sleep when no entries on timer_list */
	pthread_mutex_t		rsp_tmr_mtx;	/* mutex for timers   */
	uint32_t       		minimumTimerQuantum;	/* shortest wait time used by timer thread */
};

extern struct rsp_global_info rsp_pcbinfo;
extern int rsp_inited;	/* boolean have_inited */

/* when we make an ENRP request we save it
 * in one of these, stick it on the sd list
 * and have a timer pointing to it.
 */

struct rsp_enrp_req {
	char *req;
	int len;
	int request_type;
	char *name;
	uint32_t timerval;
};

/* timer const's */
#define RSP_T1_ENRP_REQUEST		0x0000
#define RSP_T2_REGISTRATION		0x0001
#define RSP_T3_DEREGISTRATION		0x0002
#define RSP_T4_REREGISTRATION		0x0003
#define RSP_T5_SERVERHUNT		0x0004
#define RSP_T6_SERVERANNOUNCE		0x0005
#define RSP_T7_ENRPOUTDATE		0x0006
#define RSP_NUMBER_TIMERS 7

struct rsp_enrp_entry {
	uint32_t enrpId;
	sctp_assoc_t	asocid;		/* sctp asoc id */
	struct sockaddr *addrList;	        /* list of addresses, gotten from sctp_getpaddr() */	
	int number_of_addresses;
	int size_of_addrList;
	uint8_t state;
};
/* State values for rsp_enrp_entry's */
#define RSP_NO_ASSOCIATION 	1
#define RSP_START_ASSOCIATION   2
#define RSP_ASSOCIATION_UP      3
#define RSP_ASSOCIATION_FAILED  4

/* For each socket descriptor we will have one of these */
struct rsp_socket_hash {
	int	 	sd;			/* sctp socket */
	dlist_t 	*allPools;		/* list of all pools */
	HashedTbl	*cache;			/* cache of names */
	HashedTbl	*vtagHash;		/* assoc id-> rsp_pool_element */
	HashedTbl	*ipaddrPortHash;		/* ipadd -> rsp_pool_ele */
	dlist_t		*enrp_reqs;		/* ENRP requests outstanding */
	dlist_t		*address_reg;		/* setup w/addrlist w/ctl&data seperate */
	dlist_t 	*enrpList;		/* List of ENRP servers */
	struct rsp_enrp_entry *homeServer;	/* direct pointer to home server */
	uint32_t 	refcnt;			/* number of names in use */
	uint32_t	enrpID;			/* ID of home ENRP server */
	pthread_mutex_t	rsp_sd_mtx;		/* mutex for sleepers to serialize upon  */
	char 		*registeredName;	/* our name if registered */
	uint32_t 	timers[RSP_NUMBER_TIMERS]; /* sd timers */
	uint32_t	state;
	uint32_t        stale_cache_ms;
	uint32_t	myPEid;			/* my 32 bit PE id */
	uint32_t	reglifetime;		/* how long my reg is good for */
	uint32_t        myPolicy;
	uint16_t	registration_count;	/* times I have attempted to reg */
	uint16_t	registration_threshold;	/* threshold where I fail reg and start server hunt*/
	uint16_t        max_request_retransmit; /* max request retransmit value */
	uint16_t	port;			/* our port number */
	uint8_t		registered;		/* boolean flag if we are reg'd */
	uint8_t		useThisSd;		/* flag say's if sd is data channel */
};

/* State values for rsp_socket_hash */
#define RSP_NO_ENRP_SERVER   0x00000000
#define RSP_SERVER_HUNT_IP   0x00000001
#define RSP_ENRP_HS_FOUND    0x00000002
#define RSP_ENRP_REGISTERED  0x00000004


/* Timers will have this in a list */
struct rsp_timer_entry {
	struct timeval 		started;	/* time of start */
	struct timeval 		expireTime;	/* time of expire */
	struct rsp_socket_hash 	*sd;		/* pointer back to sd */
	/* The Req field is filled in if timer does something for you */
	struct rsp_enrp_req 	*req;		/* data being sent */
	int 			timer_type;	/* type of timer */
	/* If the rsp_sleeper/cond_awake are used then req should be NULL */
	pthread_cond_t		rsp_sleeper;	/* sleeper to awake */
	uint16_t		sleeper_count;  /* number of professed sleepers */
	uint8_t 		cond_awake; 	/* Has a condition var been made */
};

/* A pool entry */
struct rsp_pool {
	char 		*name;			/* string name */
	uint32_t 	name_len;		/* len of string */
	dlist_t 	*peList;		/* list of all pe's */
	void		*lastCookie;		/* last cookie received */
	int32_t		cookieSize;		/* length of cookie */
	uint32_t 	refcnt;			/* number of PE's pointing to me */
	uint32_t	regType;		/* reg type - the policy */
	uint32_t	policy_value;		/* policy/count */
	uint32_t	policy_actvalue;	/* current count */
	uint8_t		failover_allowed;	/* auto failover of queued messages? */
	uint8_t         auto_update;		/* did we subscribe to upds */
};


struct rsp_info {
	char 		*name;
	uint32_t	namelen;
	uint32_t	number_entries;
};

#define RSP_PE_STATE_ACTIVE	0x00000001	/* reachable */
#define RSP_PE_STATE_INACTIVE   0x00000002	/* can't reach it */
#define RSP_PE_STATE_BEING_DEL  0x00000004	/* being deleted */
#define RSP_PE_STATE_REPORTED   0x00000008	/* reported state to ENRP */

/* Each entry aka the actual PE */
struct rsp_pool_ele {
	char 		*name; 		/* pointer to pool name */
	struct rsp_pool *pool;		/* pointer to pool entry */
	struct sockaddr	*addrList;	/* list of addresses, gotten from sctp_getpaddr() */
	uint32_t	number_of_addr;	/* cnt of addresses */
	struct rsp_pool_ele *failover_list;
	uint32_t	pe_identifer;	/* identifier of this PE */
	uint32_t	state;		/* What state we think its in */
	int		protocol_type;	/* what type of protocol are we using */
	sctp_assoc_t	asocid;		/* sctp asoc id */
	int		sd;		/* sctp socket it belongs to */
	int		failoverlist_size;
};

/* An address entry in the list */
struct pe_address {
	union {
		struct sockaddr_in  sin;
		struct sockaddr_in6 sin6;
	}sa;
};


/* default settings and such */
#define RSP_SD_HASH_TABLE_NAME "rsp_sd_hashtable" 
#define RSP_SD_HASH_TBL_SIZE 4

#define RSP_CACHE_HASH_TABLE_NAME "rsp_names_to_pool"
#define RSP_CACHE_HASH_TBL_SIZE 25

#define RSP_VTAG_HASH_TABLE_NAME "rsp_vtag_to_pe"
#define RSP_VTAG_HASH_TBL_SIZE 10

#define RSP_IPADDR_HASH_TABLE_NAME "rsp_ipaddr_to_pe"
#define RSP_IPADDR_HASH_TBL_SIZE 50

#define DEF_RSP_T1_ENRP_REQUEST		15000
#define DEF_RSP_T2_REGISTRATION		30000
#define DEF_RSP_T3_DEREGISTRATION	30000
#define DEF_RSP_T4_REREGISTRATION	20000
#define DEF_RSP_T5_SERVERHUNT	       120000
#define DEF_RSP_T6_SERVERANNOUNCE	1000
#define DEF_RSP_T7_ENRPOUTDATE		5000

#define DEF_MAX_REG_ATTEMPT	    2
#define DEF_MAX_REQUEST_RETRANSMIT  2 
#define DEF_STALE_CACHE_VALUE       30000

#define DEF_MINIMUM_TIMER_QUANTUM   500	/* minimum poll fd ms */

#define ENRP_DEFAULT_PORT_FOR_ASAP  5555

#define ENRP_MAX_SERVER_HUNTS       3

#endif
