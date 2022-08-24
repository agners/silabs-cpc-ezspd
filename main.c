#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

#include "sl_cpc.h"

#define EZSP_BUFFER_SIZE 4096
#define ZIGBEE_CPC_TRANSMIT_WINDOW 1

int ezsp_socket = -1;
struct in6_addr ezsp_listen_address = IN6ADDR_ANY_INIT;
uint16_t ezsp_port = 9999;

static cpc_handle_t zigbee_cpc_handle;
static char *zigbee_cpc_instance_name = NULL;
static cpc_endpoint_t zigbee_cpc_endpoint;

static const int max_restart_attempts = 3;

static void reset_crash_callback(void)
{
	int ret = 0;
	int attempts = 0;
	// Reset cpc communication if daemon signals
	do {
		//Try to restart CPC
		ret = cpc_restart(&zigbee_cpc_handle);
		//Mark how many times the restart was attempted
		attempts++;
		//Continue to try and restore CPC communication until we
		//have exhausted the retries or restart was successful
	} while ((ret != 0) && (attempts < max_restart_attempts));

	if (ret < 0) {
		perror("reset error");
		exit(EXIT_FAILURE);
	}
}

int cpc_start(void)
{
	int ret;
	ret = cpc_init(&zigbee_cpc_handle,
			zigbee_cpc_instance_name, // if NULL, uses default instance name (cpcd_0)
			false, // no debug traces in stderr
			reset_crash_callback);

	if (ret) {
		printf("Failed to connect to %s\n",
			zigbee_cpc_instance_name
			? zigbee_cpc_instance_name
			: "default CPCd instance");
		return -1;
	}


	ret = cpc_open_endpoint(zigbee_cpc_handle,
				&zigbee_cpc_endpoint,
				SL_CPC_ENDPOINT_ZIGBEE,
				ZIGBEE_CPC_TRANSMIT_WINDOW);

	printf("Connected to CPC daemon, endpoint %d: %s (errno %d)\n",
		SL_CPC_ENDPOINT_ZIGBEE,
		ret >= 0 ? "OK" : "ERROR",
		errno);
	if (ret < 0)
		return -1;

	return ret;
}

int socket_start(void)
{
	int server_fd, socket_fd;
	struct sockaddr_in6 address;
	int addrlen = sizeof(address);
	int opt = 1;
	int ret;

	server_fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (server_fd < 0) {
		perror("socket()");
		return -1;
	}

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt()");
		return -1;
	}

	address.sin6_family = AF_INET6;
	address.sin6_addr = ezsp_listen_address;
	address.sin6_port = htons(ezsp_port);

	ret = bind(server_fd, (struct sockaddr*)&address, sizeof(address));
	if (ret < 0) {
		perror("bind()");
		return -1;
	}

	printf("Listening on port %d for connection...\r\n", ezsp_port);
	if (listen(server_fd, 3) < 0) {
		perror("listen()");
		return -1;
	}

	printf("Accepting connection.\n");
	if ((socket_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
		perror("accept()");
		return -1;
	}
	printf("Accepted connection %d.\n", socket_fd);

	//close(server_fd);

	int flags;
	flags = fcntl(socket_fd, F_GETFL, 0);
	fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);

	return socket_fd;
}


void sigint_handler(int sig)
{
	int ret;

	printf("Shutting down ezspd\n");
	ret = cpc_close_endpoint(&zigbee_cpc_endpoint);

	if (ret < 0)
		exit(1);

	exit(0);
}

int main(int argc, char *argv[])
{
	char *buf, *tmp;
	ssize_t count;
	int ret;
	fd_set rfds;
	int cpc_fd, ezsp_fd, max_fd;

	printf("Starting ezspd\n");

	ezsp_fd = socket_start();
	if (ezsp_fd < 0)
		exit(EXIT_FAILURE);

	cpc_fd = cpc_start();
	if (cpc_fd < 0)
		exit(EXIT_FAILURE);

	signal(SIGINT, sigint_handler);

	buf = malloc(EZSP_BUFFER_SIZE);

	while (true)
	{
		FD_ZERO(&rfds);
		FD_SET(ezsp_fd, &rfds);
		FD_SET(cpc_fd, &rfds);

		max_fd = ezsp_fd < cpc_fd ? cpc_fd : ezsp_fd;

		ret = select(max_fd + 1, &rfds, NULL, NULL, NULL);
		printf("select ret %d\n", ret);
		if (ret == -1)
			perror("pselect()");
		else if (ret) {
			if (FD_ISSET(ezsp_fd, &rfds)) {
				count = read(ezsp_fd, buf, EZSP_BUFFER_SIZE);
				printf("EZSP -> CPC %d bytes\n", count);
				if (count == 0) {
					printf("Connection closed\n");
					exit(EXIT_SUCCESS);
				}

				/* Guarantees to write count */
				ret = cpc_write_endpoint(zigbee_cpc_endpoint,
						         buf, count, 0); 
				if (ret < 0) {
					perror("Error writing to CPC\n");
					exit(EXIT_FAILURE);
				}
			}
			if (FD_ISSET(cpc_fd, &rfds)) {
				count = cpc_read_endpoint(zigbee_cpc_endpoint,
							  buf, EZSP_BUFFER_SIZE, SL_CPC_FLAG_NON_BLOCK);
				printf("CPC -> EZSP %d bytes\n", count);

				tmp = buf;
				while (count) {
					ret = write(ezsp_fd, tmp, count);
					if (ret < 0) {
						perror("Error writing to CPC\n");
						exit(EXIT_FAILURE);
					}
					tmp += ret;
					count -= ret;
				}
			}
		}
		printf("end, ret %d\n", ret);
	}

	return EXIT_SUCCESS;
}
