#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define TRAC_ADD 1
#define TRAC_REMOVE 2
#define TRAC_LIST 3
#define TRAC_OK 4

#define MAX_PIDS 1024

char exec_name[255];
const char * process_name(pid) {
        int exec_len;
        sprintf(exec_name, "/proc/%d/exe", pid);
        exec_len = readlink(exec_name, exec_name, 255);
        exec_name[exec_len] = 0;
        return exec_name;
}

int main(int argc, char *const argv[], char *envp[]) {

	char trac_socket_name[] = "/dev/shm/trac";

	int socket_fd;
	struct sockaddr_un trac_sockaddr;
	/* Create the socket. */
	socket_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
	/* Store the server's name in the socket address. */
	trac_sockaddr.sun_family = AF_LOCAL;
	strcpy(trac_sockaddr.sun_path, trac_socket_name);
	/* Connect the socket. */
	connect(socket_fd, &trac_sockaddr, SUN_LEN(&trac_sockaddr));

	if (!strcmp(argv[1],"add")) {
		int msg_type = TRAC_ADD;
		write(socket_fd, &msg_type, sizeof(msg_type));

		int pid = getpid();
		write(socket_fd, &pid, sizeof(pid));

		int name_len=strlen(argv[2]);
		write(socket_fd, &name_len, sizeof(int));
		write(socket_fd, argv[2], name_len);

		int ret;
		read(socket_fd, &ret, sizeof(ret));
		//int err = execve(argv[2], &argv[2], &envp[0]);
		execve(argv[3], &argv[3], &envp[0]);

	}

	if (!strcmp(argv[1],"list")) {
		int msg_type = TRAC_LIST;
		write(socket_fd, &msg_type, sizeof(msg_type));

		int name_len = strlen(argv[2]);
		write(socket_fd, &name_len, sizeof(int));
		write(socket_fd, argv[2], name_len);

		int num_pids;
		read(socket_fd, &num_pids, sizeof(int));
		if (num_pids==0)
			return 0;

		int *pids;
		pids = calloc(1, num_pids*sizeof(int));
		read(socket_fd, pids, num_pids*sizeof(int));

		int i;
		for (i=0; i<num_pids; i++) {
			printf("%u: %s\n", *pids, process_name(*pids));
			pids++;
		}
		free(pids-num_pids);
	}

	close(socket_fd);
	return 0;
}