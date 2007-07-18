#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/signal.h>
#include "pdapi_req.h"


struct pdapi_request request;
uint8_t large_buffer[132096];
struct pdapi_request sum;
struct sockaddr_in peer;
int sd;

int
send_a_request()
{
	int i, *p, ret;
	size_t sz;
	struct sctp_sndrcvinfo sinfo;
	struct pdapi_request *msg;
	uint32_t base_crc = 0xffffffff;
	/* We need to send three messages */
	/* 1) The message start with the size */
	/* 2) The data message that has a crc32 run over it */
	/* 3) The end message with the crc32 we caculated */
	memset(large_buffer,0, sizeof(large_buffer));
	memset(&sinfo, 0, sizeof(sinfo));
	msg = (struct pdapi_request *)&large_buffer[0];
	request.request = PDAPI_DATA_MESSAGE;
	msg->request = PDAPI_REQUEST_MESSAGE;
	sum.request = PDAPI_END_MESSAGE;
	/* First we must generate the size of
	 * the message. 
	 */
	request.msg.size = rand() & 0x0003ffff;
	/* round it down to even word boundary */
	request.msg.size &= ~0x03;
	p = (int *)msg->msg.data;
	sz = request.msg.size;
	for(i=0;i<sz;i+=sizeof(int)) {
		*p = rand();
		p++;
	}
	/* now we need to csum it */
	base_crc = update_crc32(base_crc, msg->msg.data, request.msg.size);
	sum.msg.checksum = sctp_csum_finalize(base_crc);
	/* now our messages are formed. Send them */

	/* network-ify the size after increase to include
	 * the sizeo of the structure header.
	 */
	sz += sizeof(request);
	request.msg.size = ntohl(sz);

	/* Note for now we just send in stream 0 */
	printf("sending a message set\n");
	ret = sctp_sendx(sd, &request, sizeof(request), 
			 (struct sockaddr *)&peer, 1,
			 &sinfo, 0);
	if (ret < sizeof(request)) {
		printf("error in sending request %d errno:%d\n",
		       ret, errno);
		return(0);
	}
	ret = sctp_sendx(sd, large_buffer, sz,
			 (struct sockaddr *)&peer, 1,
			 &sinfo, 0);
	if (ret < sz) {
		printf("error in sending message %d errno:%d\n",
		       ret, errno);
		return(0);
	
	}
	ret = sctp_sendx(sd, &sum, sizeof(sum), 
			 (struct sockaddr *)&peer, 1,
			 &sinfo, 0);
	if (ret < sizeof(request)) {
		printf("error in sending sum %d errno:%d\n",
		       ret, errno);
		return(0);
	}
	printf("message set sent\n");
	return (1);
}

int
handle_notification(char *buf)
{
	struct sctp_tlv *sn_header;
	sn_header = (struct sctp_tlv *)buf;
	struct sctp_assoc_change  *asoc;

	switch (sn_header->sn_type) {
	case SCTP_ASSOC_CHANGE:
		asoc = (struct sctp_assoc_change  *)sn_header;
		if (asoc->sac_state == SCTP_COMM_UP) {
			break;
		} else if (asoc->sac_state == SCTP_COMM_LOST) {
			printf("Got a comm-lost event, exiting\n");
			return(0);
		}
		break;
	case SCTP_SHUTDOWN_EVENT:
		printf("Got a shutdown event, exiting\n");
		return (0);
		break;
	case SCTP_PARTIAL_DELIVERY_EVENT:
	case SCTP_REMOTE_ERROR:
	case SCTP_SEND_FAILED:
	case SCTP_ADAPTATION_INDICATION:
	case SCTP_STREAM_RESET_EVENT:
	case SCTP_PEER_ADDR_CHANGE:
	default:
		break;
	}
	return (1);
}

int
main(int argc, char **argv) 
{
	int i, not_done=1, ret;
	unsigned seed=0;
	int flags;
	int cnt=0;
	int limit = 0;
	struct sctp_sndrcvinfo sinfo;
	uint16_t port=0;
	char *host=NULL;
	struct sockaddr_in rcvfrom;
	socklen_t rcvsz;
	char read_buffer[2048];
	struct sctp_event_subscribe event;
	
	memset (&peer, 0, sizeof(peer));
	while((i= getopt(argc,argv,"p:h:s:l:")) != EOF){
		switch(i){
		case 's':
			seed = strtoul(optarg, NULL, 0);
			break;
		case 'h':
			host = optarg;
			break;
		case 'p':
			port = (uint16_t)(strtol(optarg, NULL,0) & 0x0000ffff);
			break;
		case 'l':
			limit = strtol(optarg, NULL, 0);
			break;
		default:
			goto use;
			break;
		};
	}

	if ((host == NULL) || (seed = 0) || (port == 0)) {
	use:
		printf("Use %s -s seed -p port -h dot.host.ip.addr [-l limit]\n",
		       argv[0]);
		printf("Where seed is the random number seed\n");
		printf("Where port is the peer port where pdapi_server is\n");
		printf("Where dot.host.ip.addr is the peer ipv4 address\n");
		printf("Where limit is the number of msg sets to send (0 is unlimited)\n");
		return(-1);
	}
	
	if (inet_pton(AF_INET, host, &peer.sin_addr)) {
		peer.sin_family = AF_INET;
		peer.sin_len = sizeof(peer);
		peer.sin_port = htons(port);
	}else{
		printf("can't set IPv4: incorrect address format\n");
		goto use;
	}
	/* setup the random number */
	srand(seed);
	sd = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
	if (sd == -1) {
		printf("error %d can't open sctp socket\n", errno);
		return (-1);
	}
	memset(&event, 0, sizeof(event));
	event.sctp_data_io_event = 1;
	event.sctp_association_event = 1;
	event.sctp_shutdown_event = 1;
	if (setsockopt(sd, IPPROTO_SCTP, SCTP_EVENTS, &event, sizeof(event)) != 0) {
		printf("Can't do SET_EVENTS socket option! err:%d\n", errno);
		return (-1);
	}
	while(not_done) {
		not_done = send_a_request();
		if(not_done == 0) {
			printf("Process ends\n");
			continue;
		}
		cnt++;
		rcvsz = sizeof(rcvfrom);
		flags = MSG_DONTWAIT;
		ret = sctp_recvmsg(sd, read_buffer, sizeof(read_buffer),
				   (struct sockaddr *)&rcvfrom, &rcvsz,
				   &sinfo, &flags);
		if ((ret > 0) && (flags & MSG_NOTIFICATION)) {
			not_done = handle_notification(read_buffer);
		}
		if(cnt >= limit) {
			printf("Sends complete\n");
			not_done = 0;
		}
	}
	return (0);
}
