#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <netinet/in.h>

#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/uio.h>
#include <netdb.h>

#include <netinet/sctp.h>
#include <netinet/sctp_uio.h>
#ifdef __NetBSD__
#include <sys/inttypes.h>
#endif

#define SCTP_CRC32C_POLY 0x1EDC6F41
#define SCTP_CRC32C(c,d) (c=(c>>8)^sctp_crc_c[(c^(d))&0xFF])
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Copyright 2001, D. Otis.  Use this program, code or tables    */
/* extracted from it, as desired without restriction.            */
/*                                                               */
/* 32 Bit Reflected CRC table generation for SCTP.               */
/* To accommodate serial byte data being shifted out least       */
/* significant bit first, the table's 32 bit words are reflected */
/* which flips both byte and bit MS and LS positions.  The CRC   */
/* is calculated MS bits first from the perspective of the serial*/
/* stream.  The x^32 term is implied and the x^0 term may also   */
/* be shown as +1.  The polynomial code used is 0x1EDC6F41.      */
/* Castagnoli93                                                  */
/* x^32+x^28+x^27+x^26+x^25+x^23+x^22+x^20+x^19+x^18+x^14+x^13+  */
/* x^11+x^10+x^9+x^8+x^6+x^0                                     */
/* Guy Castagnoli Stefan Braeuer and Martin Herrman              */
/* "Optimization of Cyclic Redundancy-Check Codes                */
/* with 24 and 32 Parity Bits",                                  */
/* IEEE Transactions on Communications, Vol.41, No.6, June 1993  */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static unsigned char buffer1[4100];
static unsigned char buffer2[4100];
static unsigned char buffer3[4100];
static unsigned char buffer4[4100];
static struct sockaddr_in bindto,got;
static socklen_t len;

unsigned long  sctp_crc_c[256] = {
	0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L,
	0xC79A971FL, 0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL,
	0x8AD958CFL, 0x78B2DBCCL, 0x6BE22838L, 0x9989AB3BL,
	0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L, 0x5E133C24L,
	0x105EC76FL, 0xE235446CL, 0xF165B798L, 0x030E349BL,
	0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L,
	0x9A879FA0L, 0x68EC1CA3L, 0x7BBCEF57L, 0x89D76C54L,
	0x5D1D08BFL, 0xAF768BBCL, 0xBC267848L, 0x4E4DFB4BL,
	0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L, 0x33ED7D2AL,
	0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L,
	0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L,
	0x6DFE410EL, 0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL,
	0x30E349B1L, 0xC288CAB2L, 0xD1D83946L, 0x23B3BA45L,
	0xF779DEAEL, 0x05125DADL, 0x1642AE59L, 0xE4292D5AL,
	0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL,
	0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L, 0x6EF07595L,
	0x417B1DBCL, 0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L,
	0x86E18AA3L, 0x748A09A0L, 0x67DAFA54L, 0x95B17957L,
	0xCBA24573L, 0x39C9C670L, 0x2A993584L, 0xD8F2B687L,
	0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L,
	0x5125DAD3L, 0xA34E59D0L, 0xB01EAA24L, 0x42752927L,
	0x96BF4DCCL, 0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L,
	0xDBFC821CL, 0x2997011FL, 0x3AC7F2EBL, 0xC8AC71E8L,
	0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L, 0x0F36E6F7L,
	0x61C69362L, 0x93AD1061L, 0x80FDE395L, 0x72966096L,
	0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L,
	0xEB1FCBADL, 0x197448AEL, 0x0A24BB5AL, 0xF84F3859L,
	0x2C855CB2L, 0xDEEEDFB1L, 0xCDBE2C45L, 0x3FD5AF46L,
	0x7198540DL, 0x83F3D70EL, 0x90A324FAL, 0x62C8A7F9L,
	0xB602C312L, 0x44694011L, 0x5739B3E5L, 0xA55230E6L,
	0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L,
	0x3CDB9BDDL, 0xCEB018DEL, 0xDDE0EB2AL, 0x2F8B6829L,
	0x82F63B78L, 0x709DB87BL, 0x63CD4B8FL, 0x91A6C88CL,
	0x456CAC67L, 0xB7072F64L, 0xA457DC90L, 0x563C5F93L,
	0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L,
	0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL,
	0x92A8FC17L, 0x60C37F14L, 0x73938CE0L, 0x81F80FE3L,
	0x55326B08L, 0xA759E80BL, 0xB4091BFFL, 0x466298FCL,
	0x1871A4D8L, 0xEA1A27DBL, 0xF94AD42FL, 0x0B21572CL,
	0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L,
	0xA24BB5A6L, 0x502036A5L, 0x4370C551L, 0xB11B4652L,
	0x65D122B9L, 0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL,
	0x2892ED69L, 0xDAF96E6AL, 0xC9A99D9EL, 0x3BC21E9DL,
	0xEF087A76L, 0x1D63F975L, 0x0E330A81L, 0xFC588982L,
	0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
	0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L,
	0x38CC2A06L, 0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L,
	0xFF56BD19L, 0x0D3D3E1AL, 0x1E6DCDEEL, 0xEC064EEDL,
	0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L, 0xD0DDD530L,
	0x0417B1DBL, 0xF67C32D8L, 0xE52CC12CL, 0x1747422FL,
	0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL,
	0x8ECEE914L, 0x7CA56A17L, 0x6FF599E3L, 0x9D9E1AE0L,
	0xD3D3E1ABL, 0x21B862A8L, 0x32E8915CL, 0xC083125FL,
	0x144976B4L, 0xE622F5B7L, 0xF5720643L, 0x07198540L,
	0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L,
	0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL,
	0xE330A81AL, 0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL,
	0x24AA3F05L, 0xD6C1BC06L, 0xC5914FF2L, 0x37FACCF1L,
	0x69E9F0D5L, 0x9B8273D6L, 0x88D28022L, 0x7AB90321L,
	0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL,
	0xF36E6F75L, 0x0105EC76L, 0x12551F82L, 0xE03E9C81L,
	0x34F4F86AL, 0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL,
	0x79B737BAL, 0x8BDCB4B9L, 0x988C474DL, 0x6AE7C44EL,
	0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L, 0xAD7D5351L,
};

u_int32_t
update_crc32(u_int32_t crc32,
	     unsigned char *buffer,
	     unsigned int length)
{
	int i;
	for (i = 0; i < length; i++) {
		SCTP_CRC32C(crc32, buffer[i]);
	}
	return (crc32);
}

u_int32_t
sctp_csum_finalize(u_int32_t crc32)
{
	u_int32_t result;
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t byte0, byte1, byte2, byte3;
#endif
	/* Complement the result */
	result = ~crc32;
#if BYTE_ORDER == BIG_ENDIAN
	/*
	 * For BIG-ENDIAN.. aka Motorola byte order the result is in
	 * little-endian form. So we must manually swap the bytes. Then
	 * we can call htonl() which does nothing...
	 */
	byte0 = result & 0x000000ff;
	byte1 = (result >> 8) & 0x000000ff;
	byte2 = (result >> 16) & 0x000000ff;
	byte3 = (result >> 24) & 0x000000ff;
	result = ((byte0 << 24) |
		  (byte1 << 16) |
		  (byte2 << 8) |
		  byte3);
	crc32 = htonl(result);
#else
	/*
	 * For INTEL platforms the result comes out in network order.
	 * No htonl is required or the swap above. So we optimize out
	 * both the htonl and the manual swap above.
	 */
	crc32 = result;
#endif
	return (crc32);
}

int
check_buffers()
{
        /* Prepare buffers */
	u_int32_t temp,*csum;
	int ret=0;
	temp = 0xffffffff;
	temp = update_crc32(temp,buffer1,(sizeof(buffer1)-4));
	temp = sctp_csum_finalize(temp);
	csum = (u_int32_t *)(buffer1+(sizeof(buffer1)-4));
	if(*csum != temp){
		printf("Buffer1: Found csum:%x calculated:%x\n",
		       *csum,temp);
		ret++;
	}
	temp = 0xffffffff;
	temp = update_crc32(temp,buffer2,(sizeof(buffer2)-4));
	temp = sctp_csum_finalize(temp);
	csum = (u_int32_t *)(buffer2+(sizeof(buffer2)-4));
	if(*csum != temp){
		printf("Buffer2: Found csum:%x calculated:%x\n",
		       *csum,temp);
		ret++;
	}

	temp = 0xffffffff;
	temp = update_crc32(temp,buffer3,(sizeof(buffer3)-4));
	temp = sctp_csum_finalize(temp);
	csum = (u_int32_t *)(buffer3+(sizeof(buffer3)-4));
	if(*csum != temp){
		printf("Buffer3: Found csum:%x calculated:%x\n",
		       *csum,temp);
		ret++;
	}

	temp = 0xffffffff;
	temp = update_crc32(temp,buffer4,(sizeof(buffer4)-4));
	temp = sctp_csum_finalize(temp);
	csum = (u_int32_t *)(buffer4+(sizeof(buffer4)-4));
	if(*csum != temp){
		printf("Buffer4: Found csum:%x calculated:%x\n",
		       *csum,temp);
		ret++;
	}
	return(ret);
}

static int
my_handle_notification(int fd,char *notify_buf) {
	union sctp_notification *snp;
	struct sctp_assoc_change *sac;
	struct sctp_paddr_change *spc;
	struct sctp_remote_error *sre;
	struct sctp_send_failed *ssf;
	struct sctp_shutdown_event *sse;
	int asocUp;
	char *str;
	char buf[256];
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;

	snp = (union sctp_notification *)notify_buf;
	asocUp = 0;
	switch(snp->sn_header.sn_type) {
	case SCTP_ASSOC_CHANGE:
		sac = &snp->sn_assoc_change;
		switch(sac->sac_state) {

		case SCTP_COMM_UP:
			str = "COMMUNICATION UP";
			asocUp++;
			break;
		case SCTP_COMM_LOST:
			str = "COMMUNICATION LOST";
			break;
		case SCTP_RESTART:
		        str = "RESTART";
			asocUp++;
			break;
		case SCTP_SHUTDOWN_COMP:
			str = "SHUTDOWN COMPLETE";
			break;
		case SCTP_CANT_STR_ASSOC:
			str = "CANT START ASSOC";
			printf("EXIT:SCTP_ASSOC_CHANGE: %s, assoc=%xh\n", str,
			       (u_int32_t)sac->sac_assoc_id);
			exit(0);
			break;
		default:
			str = "UNKNOWN";
		} /* end switch(sac->sac_state) */
		printf("SCTP_ASSOC_CHANGE: %s, assoc=%xh\n", str,
		       (u_int32_t)sac->sac_assoc_id);
		break;
	case SCTP_PEER_ADDR_CHANGE:
		spc = &snp->sn_paddr_change;
		switch(spc->spc_state) {
		case SCTP_ADDR_AVAILABLE:
			str = "ADDRESS AVAILABLE";
			break;
		case SCTP_ADDR_UNREACHABLE:
			str = "ADDRESS UNAVAILABLE";
			break;
		case SCTP_ADDR_REMOVED:
			str = "ADDRESS REMOVED";
			break;
		case SCTP_ADDR_ADDED:
			str = "ADDRESS ADDED";
			break;
		case SCTP_ADDR_MADE_PRIM:
			str = "ADDRESS MADE PRIMARY";
			break;
		case SCTP_ADDR_CONFIRMED:
			str = "ADDRESS CONFIRMED";
			break;

		default:
			str = "UNKNOWN";
		} /* end switch */
		sin6 = (struct sockaddr_in6 *)&spc->spc_aaddr;
		if (sin6->sin6_family == AF_INET6) {
			inet_ntop(AF_INET6, (char*)&sin6->sin6_addr, buf, sizeof(buf));
		} else {
			sin = (struct sockaddr_in *)&spc->spc_aaddr;
			inet_ntop(AF_INET, (char*)&sin->sin_addr, buf, sizeof(buf));
		}
		printf("SCTP_PEER_ADDR_CHANGE: %s, addr=%s, assoc=%xh\n", str,
		       buf, (u_int32_t)spc->spc_assoc_id);
		break;
	case SCTP_REMOTE_ERROR:
		sre = &snp->sn_remote_error;
		printf("SCTP_REMOTE_ERROR: assoc=%xh\n",
		       (u_int32_t)sre->sre_assoc_id);
		break;
	case SCTP_SEND_FAILED:
		ssf = &snp->sn_send_failed;
		printf("SCTP_SEND_FAILED: assoc=%xh\n",
		       (u_int32_t)ssf->ssf_assoc_id);
		break;
	case SCTP_ADAPTION_INDICATION:
	  {
	    struct sctp_adaption_event *ae;
	    ae = &snp->sn_adaption_event;
	    printf("\nSCTP_adaption_indication bits:0x%x\n",
		   (u_int)ae->sai_adaption_ind);
	  }
	  break;
	case SCTP_PARTIAL_DELIVERY_EVENT:
	  {
	    struct sctp_pdapi_event *pdapi;
	    pdapi = &snp->sn_pdapi_event;
	    printf("SCTP_PD-API event:%u\n",
		   pdapi->pdapi_indication);
	    if(pdapi->pdapi_indication == SCTP_PARTIAL_DELIVERY_ABORTED){
		    printf("PDI- Aborted\n");
	    }
	  }
	  break;

	case SCTP_SHUTDOWN_EVENT:
                sse = &snp->sn_shutdown_event;
		printf("SCTP_SHUTDOWN_EVENT: assoc=%xh\n",
		       (u_int32_t)sse->sse_assoc_id);
		break;
	default:
		printf("Unknown notification event type=%xh\n", 
		       snp->sn_header.sn_type);
	} /* end switch(snp->sn_type) */
	return(asocUp);
}


static char readBuffer[65535];	
static int sz=0;
static char controlVector[65535];
static struct msghdr msg;
int
my_sctpReadInput(int fd,int maxread)
{
	/* receive some number of datagrams and
	 * act on them.
	 */
	struct sctp_sndrcvinfo *s_info;
	int i,disped;

	struct iovec iov[2];
	unsigned char from[200];
	disped = i = 0;

	memset(&msg,0,sizeof(msg));
	memset(controlVector,0,sizeof(controlVector));
	memset(readBuffer,0,sizeof(readBuffer));
	s_info = NULL;
	iov[0].iov_base = readBuffer;
	iov[0].iov_len = maxread;
	iov[1].iov_base = NULL;
	iov[1].iov_len = 0;
	msg.msg_name = (caddr_t)from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = (caddr_t)controlVector;
	msg.msg_controllen = sizeof(controlVector);
	errno = 0;
	sz = recvmsg(fd,&msg,0);
	printf("Read fd:%d returns %d errno:%d control len is %d msgflg:%x\n",
	       fd,
	       sz,errno,
	       msg.msg_controllen,
	       msg.msg_flags);

	if (msg.msg_flags & MSG_NOTIFICATION) {
		printf("Got a notification\n");
		return(my_handle_notification(fd,readBuffer));
	}else{
		printf("Got data\n");
		return(-1);
	}
}


int
poll_fd(int fd)
{
	int cameup;
	int max,notdone;
	fd_set readfds,writefds,exceptfds;
	struct timeval tv;
	memset(&tv,0,sizeof(tv));
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	cameup = 0;
	max = fd + 1;
	notdone = 1;
	printf("poll_fd\n");
	FD_SET(fd,&readfds);
	select(max,&readfds,&writefds,&exceptfds,NULL);
	notdone = 0;
	if(FD_ISSET(fd,&readfds)){
		printf("Read please\n");
		cameup += my_sctpReadInput(fd,4100);
	}
	return(cameup);
}

static sctp_assoc_t
dig_out_asocid()
{
	struct sctp_sndrcvinfo *s_info;
	struct cmsghdr *cmsg;
	s_info = NULL;
	if(msg.msg_controllen){
		/* parse through and see if we find
		 * the sctp_sndrcvinfo
		 */
		cmsg = (struct cmsghdr *)controlVector;
		while(cmsg){
			if(cmsg->cmsg_level == IPPROTO_SCTP){
				if(cmsg->cmsg_type == SCTP_SNDRCV){
					/* Got it */
					s_info = (struct sctp_sndrcvinfo *)CMSG_DATA(cmsg);
					break;
				}
			}
			cmsg = CMSG_NXTHDR(&msg,cmsg);
		}
	}else{
		printf("No CMSG?\n");
		exit(0);
	}
	if(s_info == NULL){
		printf("No sinfo?\n");
		exit(0);
	}
	return(s_info->sinfo_assoc_id);
}


void
process(int fd,int magic)
{
	int fd1,num_asoc,ret,i;
	sctp_assoc_t asoc;
	struct timespec ts;
	num_asoc = 0;
	i = 1;
	ts.tv_sec = 0;
	ts.tv_nsec = 10000;

	while(i < 4099){
		printf("pass %d\n",i);
		while(num_asoc < 2){
			printf("Have %d asoc's waiting for 2\n", num_asoc);
			ret = poll_fd(fd);
			if(ret >0 ){
				num_asoc += ret;
			}else if(ret == 0){
				sleep(1);
			}else if(ret < 0){
				printf("Got data? %d\n",sz);
				sleep(1);
			}
			printf("asoc count is %d\n",num_asoc);
		}
	again:
		printf("Reading for %d bytes from fd:%d\n",
		       i,fd);
		my_sctpReadInput(fd,i);
		if(sz == i){
			memcpy(buffer1,readBuffer,i);
		}else{
			printf("Huh I am messed up read %d wanted %d\n",
			       sz,i);
			goto again;
		}
		if(msg.msg_flags & MSG_EOR){
			printf("Huh got EOR on paritial read?\n");
			exit(0);
		}
		asoc = dig_out_asocid();
		nanosleep(&ts,NULL);

		printf("Got to peeloff %x\n",(u_int)asoc);
		fd1 = sctp_peeloff(fd,asoc);
		if(fd1 == -1){
			printf("peeloff failed %d/err:%d\n",
			       fd1,errno);
			exit(0);
		}
		my_sctpReadInput(fd1,(4100-i));
		if(sz > 0){
			memcpy(&buffer1[i],readBuffer,sz);
			printf("Copied %d bytes\n",sz);
		}else{
			printf("Huh only read %d\n",sz);
		}
		if(magic >= i){
			my_sctpReadInput(fd,i);
		}else{
			my_sctpReadInput(fd,4100);
		}
		if(sz > 0){
			memcpy(buffer2,readBuffer,sz);
			printf("copied %d bytes\n",sz);
		}else{
			printf("Huh only read %d\n",sz);
		}
		my_sctpReadInput(fd1,4100);
		if(sz > 0){
			memcpy(buffer3,readBuffer,sz);
			printf("copied %d bytes\n",sz);
		}else{
			printf("Huh only read %d\n",sz);
		}
		my_sctpReadInput(fd,4100);
		if(sz > 0){
			memcpy(buffer4,readBuffer,sz);
			printf("copied %d bytes\n",sz);
		}else{
			printf("Huh only read %d\n",sz);
		}
		printf("Check buffers\n");
		if(check_buffers()){
			exit(0);
		}
		close(fd1);
		num_asoc = 0;
		return;
	}
}

int
main(int argc, char **argv)
{
	int i,fd;
	u_int16_t myport=0;
	int magic=0;
	struct sctp_event_subscribe event;
	while((i= getopt(argc,argv,"m:M:")) != EOF){
		switch(i){
		case 'm':
			myport = (u_int16_t)strtol(optarg,NULL,0);
			break;
		case 'M':
			magic = strtol(optarg,NULL,0);
			break;
		};
	}
	/**********************socket 1 *******************/
      	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_SCTP);
	if(fd == -1){
		printf("can't open socket:%d\n",errno);
		return(-1);
	}
	memset(&bindto,0,sizeof(bindto));
	len = bindto.sin_len = sizeof(bindto);
	bindto.sin_family = AF_INET;
	printf("bind port %d\n",myport);
	bindto.sin_port = htons(myport);
	if(bind(fd,(struct sockaddr *)&bindto, len) < 0){
		printf("can't bind a socket:%d\n",errno);
		close(fd);
		return(-1);
	}
	if(getsockname(fd,(struct sockaddr *)&got,&len) < 0){
		printf("get sockname failed err:%d\n",errno);
		close(fd);
		return(-1);
	}	
	printf("fd uses port %d\n",ntohs(got.sin_port));
	listen(fd,100);
	/* enable all event notifications */
	event.sctp_data_io_event = 1;
	event.sctp_association_event = 1;
	event.sctp_address_event = 0;
	event.sctp_send_failure_event = 1;
	event.sctp_peer_error_event = 1;
	event.sctp_shutdown_event = 1;
	event.sctp_partial_delivery_event = 1;
	event.sctp_adaptation_layer_event = 1;
	if (setsockopt(fd, IPPROTO_SCTP, 
		       SCTP_EVENTS, &event, 
		       sizeof(event)) != 0) {
		printf("Gak, can't set events errno:%d\n",errno);
		exit(0);
	}
	printf("to process\n");
	process(fd,magic);
	return(0);
}

