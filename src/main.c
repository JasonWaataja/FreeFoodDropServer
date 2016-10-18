

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>

/* The file descriptor for the incoming socket.  Being something other than -1
 * means that it hasn't been set yet and doesn't need to be closed.  */
int sockfd = -1;

/* Whether or not the main loop should continue.  */
int should_continue = 1;

/* If this gives an error, used -std=gnu99 */
#define SERVER_PORT "8110"

/* The amount of connections to keep in the backlog.  */
#define BACKLOG 20

/* Set up the networking.  Creates the port and sets sockfd to the value.
 * Exits the program if there's an error.  */
void
init_networking ();

/* Start accepting connections.  Exists the program if there's an error.  */
void
main_loop ();

/* Closes the socket, supposed to be on exit.  */
void
close_socket ();

/* Prints the address (inet or inet6) to the given stream.  I decided to it this
 * way because returning a char array would mean that it would need to be freed
 * with free () which is annoying and ties it to malloc () (as opposed to c++'s
 * delete.  */
void
print_address (FILE *stream, struct sockaddr *sockaddr);

void
handle_connection (int socket);

void
signal_handler (int signum);

void
init_networking ()
{
  struct addrinfo *host_addr = NULL;

  struct addrinfo hints;
  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status;
  status = getaddrinfo (NULL, SERVER_PORT, &hints, &host_addr);

  if (status == -1)
	err (EXIT_FAILURE, "Failed to create address");

  sockfd = socket (host_addr->ai_family,
				   host_addr->ai_socktype,
				   host_addr->ai_protocol);

  if (sockfd == -1)
	err (EXIT_FAILURE, "Failed to create socket");

  status = bind (sockfd, host_addr->ai_addr, host_addr->ai_addrlen);

  if (status == -1)
	err (EXIT_FAILURE, "Failed to bind port %s", SERVER_PORT);

  freeaddrinfo (host_addr);

  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);

  listen (sockfd, BACKLOG);
}

void
signal_handler (int signum)
{
  close_socket ();
  /* Do a bunch of stuff to terminate the worker threads here.  */

  /* Set the signal back to its original value and use that.  */
  signal (signum, SIG_DFL);
  raise (signum);
}

void
main_loop ()
{
  /* Loop forever until should_continue is set to false.  */
  while (should_continue)
	{
	  /* The file descriptor for the new connection.  */
	  int newfd;

	  struct sockaddr_storage their_sock;
	  socklen_t their_socklen;

          printf ("Accepting connections.\n");

	  /* Accept a connection.  */
	  newfd = accept (sockfd,
					  (struct sockaddr *) &their_sock,
					  &their_socklen);

	  if (newfd == -1)
		warn ("Failed to accept connection");

	  printf ("Got a connection from ");
	  print_address (stdout, (struct sockaddr *) &their_sock);
	  printf ("\n");

	  handle_connection (newfd);
	}

  close_socket ();
}

void
close_socket ()
{
  if (sockfd > -1)
	close (sockfd);

  sockfd = -1;
}

void
print_address (FILE *stream, struct sockaddr *sockaddr)
{
  if (sockaddr->sa_family == AF_INET)
	{
	  struct sockaddr_in *as_inet = (struct sockaddr_in *) sockaddr;
	  char ip[INET_ADDRSTRLEN];

	  inet_ntop (AF_INET, &(as_inet->sin_addr), ip, INET_ADDRSTRLEN);

	  fprintf (stream, "%s", ip);
	}
  else if (sockaddr->sa_family == AF_INET6)
	{
	  struct sockaddr_in6 *as_inet6 = (struct sockaddr_in6 *) sockaddr;
	  char ip[INET6_ADDRSTRLEN];

	  inet_ntop (AF_INET6, &(as_inet6->sin6_addr), ip, INET6_ADDRSTRLEN);

	  fprintf (stream, "%s", ip);
	}
}

void
handle_connection (int socket)
{
  /* Handle connection here.  Do some threading stuff.  */
  close (socket);
}
int
main (int argc, char *argv[])
{
  init_networking ();
  main_loop ();
}
