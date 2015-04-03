#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/cn_proc.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/poll.h>

#define TRAC_ADD 1
#define TRAC_REMOVE 2
#define TRAC_LIST 3
#define TRAC_OK 4

#define MAX_PIDS 1024
#define MAX_PIDGROUPS 100

struct process_group {
	char *name;
	int num_pids;
	int *p_list;
};

struct client_msg {
	int msg_type;
	char name;
};

int num_pgroups;
struct process_group process_list[MAX_PIDGROUPS];

void add_pid_group(int pid) {
	process_list[num_pgroups].p_list = calloc(1, MAX_PIDS*sizeof(char *));
	process_list[num_pgroups].p_list[0] = pid;
	process_list[num_pgroups].num_pids = 1;
	num_pgroups++;

	printf("adding group %d\n", num_pgroups-1);
}

void delete_pid_group(int pgroup) {
	free(process_list[pgroup].p_list);
	free(process_list[pgroup].name);
	process_list[pgroup] = process_list[num_pgroups];
	num_pgroups--;

	printf("deleting group %d\n", pgroup);
}

void add_pid_to_list(int ppid, int pid) {
	int i;
	for (i = 0; i < num_pgroups; i++) {
		//printf("add is in group %d\n", i);
		int j;
		for (j = 0; j < process_list[i].num_pids; j++) {
			if (process_list[i].p_list[j] == ppid) {
				//printf("adding pid %d in place %d\n", pid, process_list[i].num_pids);
				process_list[i].p_list[process_list[i].num_pids] = pid;
				process_list[i].num_pids++;
			}
		}
	}
}

void remove_pid_from_list(int pid) {
	int i;
	for (i = 0; i < num_pgroups; i++) {
		//printf("remove is in group %d\n", i);
		int j;
		for (j = 0; j < process_list[i].num_pids; j++) {
			if (process_list[i].p_list[j] == pid) {
				//printf("removing pid %u from place %u  %u\n", pid, j, process_list[i].p_list[process_list[i].num_pids]);
				process_list[i].p_list[j] = process_list[i].p_list[process_list[i].num_pids-1];
				process_list[i].p_list[process_list[i].num_pids-1] = 0;
				process_list[i].num_pids--;
				if (!process_list[i].num_pids)
					delete_pid_group(i);
			}
		}
	}
}

void print_pids(int *list) {
	while (*list) {
		printf("%d\n", *list);
		list++;
	}
}

/*
 * connect to netlink
 * returns netlink socket, or -1 on error
 */
static int nl_connect() {
	int rc;
	int nl_sock;
	struct sockaddr_nl sa_nl;

	nl_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (nl_sock == -1) {
		perror("socket");
		return -1;
	}

	sa_nl.nl_family = AF_NETLINK;
	sa_nl.nl_groups = CN_IDX_PROC;
	sa_nl.nl_pid = getpid();

	rc = bind(nl_sock, (struct sockaddr *)&sa_nl, sizeof(sa_nl));
	if (rc == -1) {
		perror("netlink bind");
		close(nl_sock);
		return -1;
	}

	return nl_sock;
}

/*
 * subscribe on proc events (process notifications)
 */
static int set_proc_ev_listen(int nl_sock, bool enable) {
	int rc;
	struct __attribute__((aligned(NLMSG_ALIGNTO))) {
		struct nlmsghdr nl_hdr;
		struct __attribute__((__packed__)) {
			struct cn_msg cn_msg;
			enum proc_cn_mcast_op cn_mcast;
		};
	} nlcn_msg;

	memset(&nlcn_msg, 0, sizeof(nlcn_msg));
	nlcn_msg.nl_hdr.nlmsg_len = sizeof(nlcn_msg);
	nlcn_msg.nl_hdr.nlmsg_pid = getpid();
	nlcn_msg.nl_hdr.nlmsg_type = NLMSG_DONE;

	nlcn_msg.cn_msg.id.idx = CN_IDX_PROC;
	nlcn_msg.cn_msg.id.val = CN_VAL_PROC;
	nlcn_msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);

	nlcn_msg.cn_mcast = enable ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;

	rc = send(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0);
	if (rc == -1) {
		perror("netlink send");
		return -1;
	}

	return 0;
}

int handle_ipc(int ipc_fd) {
	struct sockaddr_un client_name;
	socklen_t client_name_len = sizeof(client_name);
	int client_socket_fd;
	client_socket_fd = accept(ipc_fd, (struct sockaddr *) &client_name, &client_name_len);

	int msg_type;
	read(client_socket_fd, &msg_type, sizeof(msg_type));

	if (msg_type==TRAC_ADD) {
		int pid;
		read(client_socket_fd, &pid, sizeof(pid));
		add_pid_group(pid);

		int name_len;
		process_list[num_pgroups-1].name = calloc(1, 256);
		read(client_socket_fd, &name_len, sizeof(int));
		read(client_socket_fd, process_list[num_pgroups-1].name, name_len);
		process_list[num_pgroups-1].name[255] = 0;

		printf("added group %d with name %s\n", num_pgroups-1, process_list[num_pgroups-1].name);

		int ret = TRAC_OK;
		write(client_socket_fd, &ret, sizeof(ret));

	} else if (msg_type==TRAC_LIST) {

		int name_len;
		read(client_socket_fd, &name_len, sizeof(int));

		char *name;
		name = calloc(1, name_len+1);
		read(client_socket_fd, name, name_len);

		int i, found;
		for (i=0; i<num_pgroups; i++) {
			if (!strcmp(name, process_list[i].name)) {
				printf("%s\n", process_list[i].name);
				write(client_socket_fd, &process_list[i].num_pids, sizeof(int));
				write(client_socket_fd, process_list[i].p_list, process_list[i].num_pids*sizeof(int));
				found=1;
			}
		}
		if (!found) {
			int zero = 0;
			write(client_socket_fd, &zero, sizeof(int));
		}
		//printf("")
	}

	close(client_socket_fd);
	return 0;
}

static volatile bool done = false;
static int handle_proc_ev(int nl_sock) {
	int rc;
	struct __attribute__((aligned(NLMSG_ALIGNTO))) {
		struct nlmsghdr nl_hdr;
		struct __attribute__((__packed__)) {
			struct cn_msg cn_msg;
			struct proc_event proc_ev;
		};
	} nlcn_msg;

	rc = recv(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0);
	if (rc == 0) {
		/* shutdown? */
		return 0;
	} else if (rc == -1) {
		//if (errno == EINTR) continue;
		perror("netlink recv");
		return -1;
	}
	switch (nlcn_msg.proc_ev.what) {
	case PROC_EVENT_NONE:
		printf("set mcast listen ok\n");
		break;
	case PROC_EVENT_FORK:
		add_pid_to_list(nlcn_msg.proc_ev.event_data.fork.parent_pid,
		        nlcn_msg.proc_ev.event_data.fork.child_pid);
		break;
	case PROC_EVENT_EXIT:
		remove_pid_from_list(nlcn_msg.proc_ev.event_data.exit.process_pid);
		break;
	default:
		break;
	}

	return 0;
}

static void on_sigint(int unused) {
	print_pids(process_list[0].p_list);
	done = true;
}

int main(int argc, char *const argv[]) {

	signal(SIGINT, &on_sigint);
	siginterrupt(SIGINT, true);

	//netlink stuff
	int nl_sock;
	int rc;

	nl_sock = nl_connect();
	if (nl_sock == -1)
		exit(EXIT_FAILURE);
	rc = set_proc_ev_listen(nl_sock, true);
	if (rc == -1) {
		rc = EXIT_FAILURE;
		//goto out;
	}

	//domain socket stuff
	char trac_socket_name[] = "/dev/shm/trac";
	int socket_fd;
	struct sockaddr_un trac_sockaddr;

	socket_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
	trac_sockaddr.sun_family = AF_LOCAL;
	strcpy(trac_sockaddr.sun_path, trac_socket_name);
	bind(socket_fd, (struct sockaddr *) &trac_sockaddr, SUN_LEN(&trac_sockaddr));
	listen(socket_fd, 5);

	//poll the stuff
	struct pollfd pollfds[2];
	pollfds[0].fd = socket_fd;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = nl_sock;
	pollfds[1].events = POLLIN;

	do {

		poll(pollfds, 2, -1);
		if (pollfds[0].revents & POLLIN)
			handle_ipc(socket_fd);
		//done = handle_ipc(socket_fd);

		if (pollfds[1].revents & POLLIN)
			handle_proc_ev(nl_sock);

	} while (!done);

	close(socket_fd);
	close(nl_sock);
	unlink(trac_socket_name);
	return 0;
}








