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
/*	$KAME: sctp6_var.h,v 1.7 2004/08/17 04:06:22 itojun Exp $	*/
#ifndef _NETINET6_SCTP6_VAR_H_
#define _NETINET6_SCTP6_VAR_H_
#ifdef __FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netinet6/sctp6_var.h,v 1.2 2006/11/03 17:21:53 rrs Exp $");
#endif

#if defined(_KERNEL)

#if defined(__FreeBSD__) || (__APPLE__)
SYSCTL_DECL(_net_inet6_sctp6);
extern struct pr_usrreqs sctp6_usrreqs;
int sctp6_ctloutput __P((struct socket *, struct sockopt *));

#else
int sctp6_ctloutput __P((int, struct socket *, int, int, struct mbuf **));

#if defined(__NetBSD__) || defined(__OpenBSD__)
int sctp6_usrreq 
__P((struct socket *, int, struct mbuf *, struct mbuf *,
    struct mbuf *, struct proc *));

#else
int sctp6_usrreq 
__P((struct socket *, int, struct mbuf *, struct mbuf *,
    struct mbuf *));

#endif				/* __NetBSD__ || __OpenBSD__ */
#endif				/* __FreeBSD__ */

#if defined(__APPLE__)
	int sctp6_input __P((struct mbuf **, int *));

#else
	int sctp6_input __P((struct mbuf **, int *, int));

#endif
	int sctp6_output __P((struct sctp_inpcb *, struct mbuf *, struct sockaddr *,
                 struct mbuf *, struct proc *));
	void sctp6_ctlinput __P((int, struct sockaddr *, void *));

#endif				/* _KERNEL */
#endif
