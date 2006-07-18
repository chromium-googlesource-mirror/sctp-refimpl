#ifndef __rserpool_util_h__
#define __rserpool_util_h__
#include <rserpool_lib.h>

/* in rsp_timer.c */

void rsp_free_req(struct rsp_enrp_req *req);

struct rsp_enrp_req *rsp_aloc_req(const char *name, int namelen, void *msg, int msglen, int type);


void rsp_start_enrp_server_hunt(struct rsp_enrp_scope *sd);

void rsp_send_enrp_req(struct rsp_socket *sd, struct rsp_enrp_req *req);

int
rsp_enrp_make_name_request(struct rsp_socket *sd,
			   struct rsp_pool *pool,
			   const char *name,
			   int namelen);


struct rsp_timer_entry *
asap_find_req(struct rsp_enrp_scope *scp,
	      char *name, 
	      int name_len, 
	      int type,
	      int leave_locked);

int
rsp_start_timer(struct rsp_enrp_scope *scp,
		struct rsp_socket	*sdata, 
		uint32_t time_out_ms, 
		struct rsp_enrp_req *msg,
		int type, 
		struct rsp_timer_entry **ote);

int rsp_stop_timer(struct rsp_timer_entry *te);

int rsp_process_fds(int poll_ret);

void rsp_expire_timer(struct rsp_timer_entry *entry);


void
handle_enrpserver_notification (struct rsp_enrp_scope *scp, 
				char *buf, 
				struct sctp_sndrcvinfo *sinfo, 
				ssize_t sz,
				struct sockaddr *from, 
				socklen_t from_len);

void
handle_asapmsg_fromenrp (struct rsp_enrp_scope *scp, 
			 char *buf, 
			 struct sctp_sndrcvinfo *sinfo, 
			 ssize_t sz,
			 struct sockaddr *from, 
			 socklen_t from_len);
int
rsp_internal_poll(nfds_t nfds, int timeout, int ret_from_enrp);
#endif
