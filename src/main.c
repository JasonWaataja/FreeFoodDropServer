/*-
 * Copyright (c) 2016, Jason Waataja
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/queue.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* The file descriptor for the incoming socket.  Being something other than -1
 * means that it hasn't been set yet and doesn't need to be closed.  */
int sockfd = -1;

/* Whether or not the main loop should continue.  */
int should_continue = 1;

enum thread_status { NOT_COMPLETED = 0, COMPLETED };

/* Entry for a pthread list entry. */
struct thread_listent {
	pthread_t id;
	LIST_ENTRY(thread_listent) ents;
};

struct thread_data {
	int sockfd;
};

/* The linked list of threads. Use this to terminate them. */
LIST_HEAD(pthread_list, thread_listent) thread_list;

/* If this gives an error, used -std=gnu99 */
#define 	SERVER_PORT "8110"

/* The amount of connections to keep in the backlog.  */
#define 	BACKLOG 20

static void	close_socket();
static void	handle_connection(int socket);
static void	init_networking();
static void	init_threads();
static void	main_loop();
static void	print_address(FILE *stream, struct sockaddr *sockaddr);
static void	signal_handler(int signum);
static void	terminate_threads();
static void	*handler_function(void *data);

/* 
 * Set up the networking.  Creates the port and sets sockfd to the value.
 * Exits the program if there's an error.
 */
static void
init_networking()
{
	struct addrinfo *host_addr, hints;
	int status;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	status = getaddrinfo(NULL, SERVER_PORT, &hints, &host_addr);

	if (status == -1)
		err(1, "Failed to create address");

	sockfd = socket(host_addr->ai_family, host_addr->ai_socktype,
	    host_addr->ai_protocol);

	if (sockfd == -1)
		err(1, "Failed to create socket");

	status = bind(sockfd, host_addr->ai_addr, host_addr->ai_addrlen);

	if (status == -1)
		err(1, "Failed to bind port %s", SERVER_PORT);

	freeaddrinfo(host_addr);

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	listen(sockfd, BACKLOG);
}

/*
 * This was mainly intended to handle termination signals so that it can exit
 * the infinite loop gracefully.
 */
static void
signal_handler(int signum)
{

	close_socket();
	terminate_threads();

	/* Set the signal back to its original value and use that.  */
	signal(signum, SIG_DFL);
	raise(signum);
}

/* Start accepting connections.  Exists the program if there's an error.  */
static void
main_loop()
{
	struct sockaddr_storage their_sock;
	socklen_t their_socklen;
	int newfd;

	/* Loop forever until should_continue is set to false.  */
	while (should_continue) {
		printf("Accepting connections.\n");

		/* Accept a connection.  */
		newfd = accept (sockfd, (struct sockaddr *)&their_sock,
		    &their_socklen);

		if (newfd == -1)
			warn ("Failed to accept connection");

		printf("Got a connection from ");
		print_address(stdout, (struct sockaddr *)&their_sock);
		printf("\n");

		handle_connection(newfd);
	}

	close_socket();
}

/* Closes the socket, supposed to be on exit.  */
static void
close_socket()
{

	if (sockfd > -1)
		close(sockfd);

	sockfd = -1;
}

/*
 * Prints the address (inet or inet6) to the given stream.  I decided to it
 * this way because returning a char array would mean that it would need to be
 * freed with free() which is annoying and ties it to malloc () (as opposed to
 * c++'s delete.
 */
static void
print_address(FILE *stream, struct sockaddr *sockaddr)
{
	/*
	 * It seems weird to put both of these character declarations here, but
	 * it's to follow the style guide of declaring local variables at the
	 * start and not declaring variables inside of blocks.
	 */
	struct sockaddr_in *as_inet;
	char ip[INET_ADDRSTRLEN];
	char ip6[INET6_ADDRSTRLEN];

	if (sockaddr->sa_family == AF_INET)
	{
		as_inet = (struct sockaddr_in *)sockaddr;
		inet_ntop (AF_INET, &(as_inet->sin_addr), ip, INET_ADDRSTRLEN);
		fprintf (stream, "%s", ip);
	}
	else if (sockaddr->sa_family == AF_INET6)
	{
		struct sockaddr_in6 *as_inet6 =
		    (struct sockaddr_in6 *)sockaddr;

		inet_ntop(AF_INET6, &(as_inet6->sin6_addr), ip6,
		    INET6_ADDRSTRLEN);

		fprintf(stream, "%s", ip6);
	}
}

/* Initialize the list of threads. Other stuff can be put here if necessary. */
static void
init_threads()
{

	LIST_INIT(&thread_list);
}

/* Called to stop all threads and clean up the list. */
static void
terminate_threads()
{
	struct thread_listent *tmp;

	while (!LIST_EMPTY(&thread_list)) {
		tmp = LIST_FIRST(&thread_list);

		pthread_join(tmp->id, NULL);

		/* I think this is the right way to do it, not sure. */
		LIST_REMOVE(tmp, ents);
		free(tmp);
	}
}

/* Stop all threads and remove theme. */

/*
 * Handle the connection already accepted with socket. This function is
 * responsible for closing the socket.
 */
static void
handle_connection(int socket)
{
	struct thread_listent *ent;
	struct thread_data *data;
	int status;
	pthread_t id;

	ent = malloc(sizeof(struct thread_listent));
	if (ent == NULL)
		err(1, NULL);

	data = malloc(sizeof(struct thread_data));
	if (data == NULL)
		err(1, NULL);

	data->sockfd = socket;

	status = pthread_create(&id, NULL, handler_function, data);

	if (status == 0) {
		ent->id = id;
		LIST_INSERT_HEAD(&thread_list, ent, ents);
	} else {
		warn("Failed to create new thread");
		free(ent);
		free(data);
		close(socket);
	}
}

/* The function to handle a socket. */
static void *
handler_function(void *data)
{
	struct thread_data *as_data;

	as_data = (struct thread_data *)data;

	/* TODO: Handle the connection here. */

	close(as_data->sockfd);
	free(data);

	return (NULL);
}

/*
 * TODO: Put the program description here.
 */
int
main(int argc, char *argv[])
{

	init_networking();
	main_loop();
	terminate_threads();
}
