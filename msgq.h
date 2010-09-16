/* $Id$ */
/* Simple message queue implementation using UNIX domain socket.
 * Copyright (C) 2010  Seong-Kook Shin <cinsky@gmail.com>
 */

#ifndef MSGQ_H__
#define MSGQ_H__

/*
 * 'msgq' module provides a message queue that can be used for the
 * inter-process communciation.
 *
 * Since it is designed to use local(unix) domain datagram socket, you
 * can't use 'msgq' for networked environment.
 *
 * 'msgq' is designed to be thread-safe.   It is safe to wait for
 * messages from multiple threads at the same time.
 *
 * It is possbile to send messages from multiple threads at the same
 * time.  I don't think that we need a locking mechanism for this.
 * See msgq_send() for more.  On some Internet article, it is safe if
 * the packet is less than 4K.
 *
 * It is not safe to call any packet-related function on multiple threads
 * for one packet at the same time.
 *
 * The maximum lenght of a packet is determined by OS, not msgq module.
 * It may be adjustable via SO_SNDBUF/SO_RCVBUF socket options.    It is
 * still affected via system-wide limits.  On Linux, you can get/set the
 * syste-wide limit via "/proc/sys/net/core/[rw]mem_max".
 */

/*
 * To compile it, you may need to define _GNU_SOURCE.
 * msgq module needs "-lpthread".
 *
 * $ cc -D_GNU_SOURCE your-source.c msgq.c -lpthread -lrt
 *
 * msgq module provides a simple test server, which can be compiled by:
 *
 * $ cc -D_GNU_SOURCE -DTEST_MSGQ msgq.c -lpthread -lrt
 *
 * See the comments for main() in msgq.c for using the test server.
 */

/*
 * If you want broadcasting functions, define MSGQ_BROADCAST to compile it.
 * you need sglob module to buid it.  For example:
 *
 * $ cc -D_GNU_SOURCE -DMSGQ_BROADCAST your-source msgq.c sglob.c -lpthread -lrt
 *
 * For the test server:
 *
 * $ cc -D_GNU_SOURCE -DMSGQ_BROADCAST -DTEST_MSGQ msgq.c sglob.c -lpthread -lrt
 *
 */
/* This indirect using of extern "C" { ... } makes Emacs happy */
#ifndef BEGIN_C_DECLS
# ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
# else
#  define BEGIN_C_DECLS
#  define END_C_DECLS
# endif
#endif /* BEGIN_C_DECLS */

#include <stddef.h>             /* required for 'size_t' */

BEGIN_C_DECLS

#define MSGQ_TMP_TEMPLATE       "/tmp/msgq-XXXXXX"
#define MSGQ_MSG_MAX    4096

struct msgq_packet {
  void *container;              /* internal purpose only.  Do not change. */
  size_t size;                  /* size of data in bytes */
  char data[0];
};

//struct msgq_;
typedef struct msgq_ MSGQ;

/*
 * Create new message queue.
 *
 * The message queue address, ADDRESS should be an absolute pathname.
 * The file in pathname should not exist.   If ADDRESS is NULL, msgq_open()
 * will create one.
 *
 * On error, msgq_open() returns NULL.  Otherwise, a valid pointer to MSGQ
 * type will be returned.
 */
extern MSGQ *msgq_open(const char *address);

/*
 * Destroy given message queue.
 *
 * Currently, once msgq_close() is called, all queued packets that are not
 * yet passed to the user are destroyed.
 */
extern void msgq_close(MSGQ *msgq);

/*
 * Send a packet to the remote.
 *
 * RECEIVER is the remote address.
 * PACKET is the packet data, and SIZE is the length of PACKET.
 *
 * On success, returns zero.  Otherwise -1.
 */
extern int msgq_send(MSGQ *msgq, const char *receiver,
                     const void *packet, size_t size);

/*
 * Send a string to the remote.
 *
 * RECEIVER is the remote address.
 * FORMAT and rest arguments are printf-like arguments.
 *
 * On success, returns zero.  Otherwise -1.
 */
extern int msgq_send_string(MSGQ *msgq, const char *receiver,
                            const char *format, ...)
  __attribute__ ((format (printf, 3, 4)));


/*
 * Send a packet to the remote.
 *
 * This is the most efficient, the lowest-level function among
 * msgq_send*() family.
 *
 * You need to construct struct msgq_packet by yourself.
 * You need to fill only 'size' and 'data' members of struct msgq_packet.
 *
 * On success, returns zero, otherwise -1.
 */
extern int msgq_send_(MSGQ *msgq, const char *receiver,
               const struct msgq_packet *packet);


#ifdef MSGQ_BROADCAST
/*
 * Broadcast a packet to the addresses that satisfies the given filename
 * wildcard.
 *
 * Note that msgq_broadcast_wildcard() does not care for success of
 * sending a packet.  If this function fails to allocate/prepare for
 * broadcasting, it returns -1.  Otherwise returns zero.
 */
extern int msgq_broadcast_wildcard(MSGQ *msgq, const char *pattern,
                                   const struct msgq_packet *packet);

/*
 * String version of msgq_broadcast_wildcard().
 */
extern int msgq_broadcast_string_wildcard(MSGQ *msgq, const char *pattern,
                                          const char *fmt, ...);
#endif  /* MSGQ_BROADCAST */


/*
 * Get a packet from the message queue.
 *
 * If there is a packet received, msgq_recv() returns immediately.
 * Otherwise, returns NULL.
 *
 * The returned packet is in the struct msgq_packet type.  In the
 * returned value, the 'data' member contains the actual packet data.
 * Its length is specified in 'size' member.  You may change the
 * packet data on the fly.  However, you MUST not change its
 * 'container' member, which is used internally.
 *
 * To get the sender, you may call msgq_pkt_sender().  Once the packet
 * is no longer used, you should call msgq_pkt_delete().
 */
extern struct msgq_packet *msgq_recv(MSGQ *msgq);


/*
 * Get a packet from the message queue or wait for one.
 *
 * This is the same as msgq_recv() except this function will wait
 * until ABSTIME if there is no packet received.   If ABSTIME is NULL,
 * this function behaves exactly the same as msgq_recv_wait().
 *
 * Returns a pointer to the packet if found.
 * Retruns NULL on timeout (i.e. ABSTIME is not NULL and ABSTIME is passed.)
 *
 * Returns NULL on any internal error.  (Sets errno)
 *
 * If a UNIX signal is delivered to the caller thread, upon return
 * from the signal handler this function keeps waiting for the message
 * until ABSTIME.  If ABSTIME is NULL, the behavior is the same as
 * msgq_recv_wait().
 */
extern struct msgq_packet *msgq_recv_timedwait(MSGQ *msgq,
                                               struct timespec *abstime);

/*
 * Get a packet from the message queue or wait for one.
 *
 * This is the same as msgq_recv() except this function will wait
 * permanently if there is no packet received.
 *
 * Note that this function may return NULL on any internal error with
 * appropriate errno.  Users of this function should check
 * the return value in case of NULL.
 *
 * If a UNIX signal is delivered to the caller thread, upon return
 * from the signal handler this function resumes waiting for a message
 * or, it shall return NULL due to spurious wakeup.
 */
extern struct msgq_packet *msgq_recv_wait(MSGQ *msgq);



/*
 * Returns the number of received packets which is not processed.
 *
 * Note that in a multi-threaded environments, if you have several
 * threads that call msgq_recv() or msgq_recv_wait(), the return value
 * from this function is not quite precise.  For example, even if this
 * function tells that there is one packet, before you call any other
 * function, another thread may interfere by calling msgq_recv().
 */
extern int msgq_message_count(MSGQ *msgq);

/*
 * Returns a sender address of given PACKET.
 */
extern const char *msgq_pkt_sender(struct msgq_packet *packet);

/*
 * Delete the PACKET.
 *
 * Note that this function should be called for packets that is acquired
 * via msgq_recv() or msgq_recv_wait().
 *
 * If you construct struct msgq_packet instance by yourself, DO NOT
 * call this function.
 */
extern int msgq_pkt_delete(struct msgq_packet *packet);

END_C_DECLS

#endif  /* MSGQ_H__ */
