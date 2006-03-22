/*	$KAME: sctp_hashdriver.c,v 1.6 2004/02/24 21:52:26 itojun Exp $	*/

/*
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <netinet/sctp_constants.h>
#ifdef USE_MD5
#include <crypto/md5.h>
#else
#include <netinet/sctp_sha1.h>
#endif /* USE_MD5 */
#include <netinet/sctp_hashdriver.h>

/*
 * Main driver for SCTP's hashing.
 * passing a two pointers and two lengths, returning a digest pointer
 * filled. The md5 code was taken directly from the RFC (2104) so to
 * understand it you may want to go look at the RFC referenced in the
 * SCTP spec. We did modify this code to either user OURs implementation
 * of SLA1 or the MD5 that comes from its RFC. SLA1 may have IPR issues
 * so you need to check in to this if you wish to use it... Or at least
 * that is what the FIP-180.1 web page says.
 */

void sctp_hash_digest(char *key, int key_len, char *text, int text_len,
    unsigned char *digest)
{
#ifdef USE_MD5
	md5_ctxt context;
#else
	struct sha1_context context;
#endif /* USE_MD5 */
	/* inner padding - key XORd with ipad */
	unsigned char k_ipad[65];
	/* outer padding - key XORd with opad */
	unsigned char k_opad[65];
	unsigned char tk[20];
	int i;

	if (key_len > 64) {
#ifdef USE_MD5
		md5_ctxt tctx;
		MD5Init(&tctx);
		MD5Update(&tctx, key, key_len);
		MD5Final(tk, &tctx);
		key = tk;
		key_len = 16;
#else
		struct sha1_context tctx;
		SHA1_Init(&tctx);
		SHA1_Update(&tctx, (unsigned char *)key, key_len);
		SHA1_Final(tk, &tctx);
		key = (char *)tk;
		key_len = 20;
#endif /* USE_MD5 */
	}

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	bzero(k_ipad, sizeof k_ipad);
	bzero(k_opad, sizeof k_opad);
	bcopy(key, k_ipad, key_len);
	bcopy(key, k_opad, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}
	/*
	 * perform inner MD5
	 */
#ifdef USE_MD5
	MD5Init(&context);			/* init context for 1st pass */
	MD5Update(&context, k_ipad, 64);	/* start with inner pad */
	MD5Update(&context, text, text_len);	/* then text of datagram */
	MD5Final(digest, &context);		/* finish up 1st pass */
#else
	SHA1_Init(&context);			/* init context for 1st pass */
	SHA1_Update(&context, k_ipad, 64);	/* start with inner pad */
	SHA1_Update(&context, 
		    (unsigned char *)text, 
		    text_len);	/* then text of datagram */
	SHA1_Final(digest, &context);		/* finish up 1st pass */
#endif /* USE_MD5 */

	/*
	 * perform outer MD5
	 */
#ifdef USE_MD5
	MD5Init(&context);			/* init context for 2nd pass */
	MD5Update(&context, k_opad, 64);	/* start with outer pad */
	MD5Update(&context, digest, 16);	/* then results of 1st hash */
	MD5Final(digest, &context);		/* finish up 2nd pass */
#else
	SHA1_Init(&context);			/* init context for 2nd pass */
	SHA1_Update(&context, k_opad, 64);	/* start with outer pad */
	SHA1_Update(&context, 
		    (unsigned char *)digest, 20);	/* then results of 1st hash */
	SHA1_Final(digest, &context);		/* finish up 2nd pass */
#endif /* USE_MD5 */
}

void sctp_hash_digest_m(char *key, int key_len, struct mbuf *m, int offset,
    unsigned char *digest)
{
	struct mbuf *m_at;
#ifdef USE_MD5
	md5_ctxt context;
#else
	struct sha1_context context;
#endif /* USE_MD5 */
	/* inner padding - key XORd with ipad */
	unsigned char k_ipad[65];
	/* outer padding - key XORd with opad */
	unsigned char k_opad[65];
	unsigned char tk[20];
	int i;

	if (key_len > 64) {
#ifdef USE_MD5
		md5_ctxt tctx;
		MD5Init(&tctx);
		MD5Update(&tctx, key, key_len);
		MD5Final(tk, &tctx);
		key = tk;
		key_len = 16;
#else
		struct sha1_context tctx;
		SHA1_Init(&tctx);
		SHA1_Update(&tctx, (unsigned char *)key, key_len);
		SHA1_Final(tk, &tctx);
		key = tk;
		key_len = 20;
#endif /* USE_MD5 */
	}

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	bzero(k_ipad, sizeof k_ipad);
	bzero(k_opad, sizeof k_opad);
	bcopy(key, k_ipad, key_len);
	bcopy(key, k_opad, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/* find the correct mbuf and offset into mbuf */
	m_at = m;
	while ((m_at != NULL) && (offset > m_at->m_len)) {
		offset -= m_at->m_len;	/* update remaining offset left */
		m_at = m_at->m_next;
	}
	/*
	 * perform inner MD5
	 */
#ifdef USE_MD5
	MD5Init(&context);			/* init context for 1st pass */
	MD5Update(&context, k_ipad, 64);	/* start with inner pad */
	/******/
	while (m_at != NULL) {
		/* then text of datagram... */
		MD5Update(&context, mtod(m_at, char *)+offset,
			  m_at->m_len-offset);
		/* only offset on the first mbuf */
		offset = 0;
		m_at = m_at->m_next;
	}
	/******/
	MD5Final(digest, &context);		/* finish up 1st pass */
#else
	SHA1_Init(&context);			/* init context for 1st pass */
	SHA1_Update(&context, k_ipad, 64);	/* start with inner pad */
	/******/
	while (m_at != NULL) {
		/* then text of datagram */
		SHA1_Update(&context, mtod(m_at, unsigned char *)+offset,
			     m_at->m_len-offset);
		/* only offset on the first mbuf */
		offset = 0;
		m_at = m_at->m_next;
	}
	/******/
	SHA1_Final(digest, &context);             /* finish up 1st pass */
#endif /* USE_MD5 */

	/*
	 * perform outer MD5
	 */
#ifdef USE_MD5
	MD5Init(&context);			/* init context for 2nd pass */
	MD5Update(&context, k_opad, 64);	/* start with outer pad */
	MD5Update(&context, digest, 16);	/* then results of 1st hash */
	MD5Final(digest, &context);		/* finish up 2nd pass */
#else
	SHA1_Init(&context);			/* init context for 2nd pass */
	SHA1_Update(&context, k_opad, 64);	/* start with outer pad */
	SHA1_Update(&context, digest, 20);	/* then results of 1st hash */
	SHA1_Final(digest, &context);		/* finish up 2nd pass */
#endif /* USE_MD5 */
}
