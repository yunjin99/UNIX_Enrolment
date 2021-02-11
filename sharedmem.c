#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

//void handler(int dummy){
//	;
//}

int main(void) {
	key_t key;
	int shmid;
	void *shmaddr;
	sigset_t mask;

	key = ftok("shmfile", 1);
	shmid = shmget(key, sizeof(GQueue*), IPC_CREAT|0666);

	shmaddr = shmat(shmid, NULL, 0);
	shmdt(shmaddr);

	return 0;
}
