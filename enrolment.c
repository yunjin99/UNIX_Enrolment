#include <stdio.h>
#include <sqlite3.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <glib.h>

#include <signal.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define GPOINTER_TO_INT(p)	((gint) (glong) (p))

//세마포어 생성, 초기화
union semun{
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};

int initsem(key_t semkey){
	union semun semunarg;
	int status = 0, semid;

	semid =semget(semkey, 1, IPC_CREAT | IPC_EXCL | 0600);
	if(semid == -1){
		if(errno == EEXIST)
			semid = semget(semkey, 1, 0);
	}
	else{
		semunarg.val = 1;
		status = semctl(semid, 0, SETVAL, semunarg);
	}

	if(semid == -1 || status == -1){
		perror("initsem");
		return (-1);
	}
	return semid;
}

//세마포어 연산
int semlock(int semid){
	struct sembuf buf;
	buf.sem_num = 0;
	buf.sem_op = -1;
	buf.sem_flg = SEM_UNDO;
	if(semop(semid, &buf, 1) == -1){
		perror("semlock failed");
		exit(1);
	}
	return 0;
}

int semunlock(int semid){
	struct sembuf buf;

	buf.sem_num = 0;
	buf.sem_op = 1;
	buf.sem_flg = SEM_UNDO;
	if(semop(semid, &buf, 1) == -1){
		perror("semunlock failed");
		exit(1);
	}
	return 0;
}


int main(int argc, char **argv){
	sqlite3* db;
	char* err_msg = 0;
	sqlite3_stmt* res;
	char* sql;
	int step;

	int shared = sqlite3_enable_shared_cache(1);
	int rc = sqlite3_open("sangmyung.db", &db);

	int flag = 1;
	int option;
	int cnum;

	int semid;

	if((semid = initsem(1) < 0))
		exit(1);

	if(rc != SQLITE_OK){
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}
	int sno;
	char password[20];
	int size;

	//수강신청 가능 과목 수 알아내기
	sql = "SELECT count(*) FROM course";
	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	
	if (rc != SQLITE_OK){
		fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                sqlite3_close (db);
                return 1;
        }
        step = sqlite3_step(res);

        if(step == SQLITE_ROW){
		size = sqlite3_column_int(res, 0);
		//printf("%d\n", size);
	}

	
	//공유메모리
        key_t key;
        int shmid;
	int count = 0;
        GQueue* queue[size];
	for(int i = 0; i < size; i++){
		GQueue* q = g_queue_new();
		queue[i] = q;
        }



        void *shmaddr;

        key = ftok("shmfile", 1);

        /*if((shmid=shmget(key, 1024, IPC_CREAT|0666)) == -1){
                printf("shmget failed\n");
                exit(0);
        }

        if((shmaddr=shmat(shmid, NULL, 0)) == (void*)-1){
                printf("shmat failed\n");
                exit(0);
        }*/

	shmid = shmget(key, sizeof(GQueue*), 0);


	printf("-------------------상명대학교 수강신청--------------------\n");
	printf("==========================================================\n");

	while(1){
		//로그인

		//학번 입력받기
		printf(">>> 학번을 입력해주세요: ");
		scanf("%d", &sno);

		//비밀번호 입력받기
		printf(">>> 비밀번호를 입력해주세요: ");
		scanf("%s", password);
		
		//학생 테이블에서 등록된 정보인지 확인
		sql = "SELECT sname FROM student WHERE sno = ? AND password = ?";
		rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

		if (rc != SQLITE_OK){
			fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
			sqlite3_close (db);
			return 1;
		}

		sqlite3_bind_int(res, 1, sno);
		sqlite3_bind_text(res, 2, password, -1, SQLITE_TRANSIENT);
		step = sqlite3_step(res);
		
		//로그인에 성공할 경우
		if(step == SQLITE_ROW){
			printf("==========================================================\n");

			//학생 이름 출력
			printf("%s님으로 로그인 되었습니다.\n", sqlite3_column_text(res, 0));
			printf("==========================================================\n");
			break;
		}
		//로그인에 실패할 경우
		printf("학번 또는 비밀번호가 잘못 입력되었습니다\n\n");
	}
	
	while(flag){
		//옵션
		printf("1. 수강신청\n");
		printf("2. 수강신청 취소\n");
		printf("3. 수강목록 확인\n");
		printf("0. 종료\n\n");

		//사용자로부터 옵션 입력받기
		printf(">>> 옵션 1, 2, 3, 0 중에 하나를 선택해주세요: ");
		scanf("%d", &option);
		printf("\n");
		printf("==========================================================\n");

		switch(option){
			//수강신청
			case 1: 
				printf("-------------------------수강신청-------------------------\n\n");
				//과목 테이블의 모든 과목 출력해서 보여주기
				sql = "SELECT * FROM course";
				rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

				if(rc != SQLITE_OK){
					fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
					sqlite3_close(db);
					return 1;
				}

				while(sqlite3_step(res) == SQLITE_ROW){
					printf("과목번호: %d	교과목명: %s	담당교수: %s\n", sqlite3_column_int(res, 0), sqlite3_column_text(res, 1), sqlite3_column_text(res, 2));
				}
				printf("\n");

				//사용자로부터 수강신청할 과목 입력받기
				printf(">>> 수강신청할 과목의 번호를 선택해주세요: ");
				scanf("%d", &cnum);
				printf("\n");

				//이미 수강신청된 과목인 경우
				sql = "SELECT * FROM enrol WHERE sno = ? AND cno = ?";
				rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

				if (rc != SQLITE_OK){
					fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
					sqlite3_close (db);
					return 1;
				}

				sqlite3_bind_int(res, 1, sno);
				sqlite3_bind_int(res, 2, cnum);
				
				if(sqlite3_step(res) == SQLITE_ROW){
					printf("이미 수강신청된 과목입니다.\n\n");
					printf("===========================================================\n");
					break;
				}

				//해당 과목의 정원이 초과되었는지 확인하기
				sql = "SELECT scount, capacity FROM course WHERE cno = ?";
				rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

				if (rc != SQLITE_OK){
                                        fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                                        sqlite3_close (db);
                                        return 1;
                                }

				sqlite3_bind_int(res, 1, cnum);
				step = sqlite3_step(res);

				//정원이 초과된 경우
				if(sqlite3_column_int(res, 0) >= sqlite3_column_int(res, 1)){
					//학생의 학번을 예비번호 큐에 넣기
					
					//공유메모리 동기화
					//세마포어 잠금 함수 호출
					semlock(semid);

					shmaddr = shmat(shmid, NULL, 0);
					//*queue = (GQueue*)shmaddr;
					g_queue_push_head((GQueue*)shmaddr, &sno);
					
					printf("정원이 초과되어 신청할 수 없습니다. 예비번호는 %d번 입니다\n\n", g_queue_get_length ((GQueue*)shmaddr));
					printf("===========================================================\n");

					shmdt(shmaddr);

					//세마포어 잠금해제 함수 호출
                                        semunlock(semid);
					
				}
				
				//정원이 초과되지 않은 경우. 수강신청 진행
				else{
					//과목 테이블의 해당 과목 인원수 1 증가
					sql = "UPDATE course SET scount = scount+1 WHERE cno = ?";
                                	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

                                	if (rc != SQLITE_OK){
                                        	fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                                        	sqlite3_close (db);
                                        	return 1;
                                	}

                                	sqlite3_bind_int(res, 1, cnum);
                                	step = sqlite3_step(res);

					//등록 테이블에 수강신청한 학생의 학번과 과목번호 삽입
                                	sql = "INSERT INTO enrol(sno, cno) VALUES(?, ?)";
                                	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

                                	if (rc != SQLITE_OK){
                                        	fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                                        	sqlite3_close (db);
                                        	return 1;
                                	}

                                	sqlite3_bind_int(res, 1, sno);
                                	sqlite3_bind_int(res, 2, cnum);
                                	step = sqlite3_step(res);

					sqlite3_finalize(res);
					sqlite3_close(db);

					printf("신청되었습니다.\n\n");
					printf("==========================================================\n");
				}

				break;

			//수강신청 취소
			case 2:
				//수강신청한 과목의 목록 출력해서 보여주기
				printf("------------------------수강신청 취소------------------------\n\n");
                                sql = "SELECT course.cno, course.cname, course.prname FROM student, course, enrol WHERE student.sno = enrol.sno AND course.cno = enrol.cno AND student.sno = ?";
                                rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

                                if(rc != SQLITE_OK){
                                        fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                                        sqlite3_close (db);
                                        return 1;
                                }

				sqlite3_bind_int(res, 1, sno);

                                while(sqlite3_step(res) == SQLITE_ROW){
                                        printf("과목번호: %d	교과목명: %s        담당교수: %s\n", sqlite3_column_int(res, 0), sqlite3_column_text(res, 1), sqlite3_column_text(res, 2));
                                }
                                printf("\n");


                                //사용자로부터 수강신청을 취소할 과목 입력받기
                                printf(">>> 수강신청을 취소할 과목의 번호를 선택해주세요: ");
                                scanf("%d", &cnum);
                                printf("\n");

                                //해당 과목의 정원이 초과되었는지 확인하기
                                sql = "SELECT scount, capacity FROM course WHERE cno = ?";
                                rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

                                if (rc != SQLITE_OK){
                                        fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                                        sqlite3_close (db);
                                        return 1;
                                }

                                sqlite3_bind_int(res, 1, cnum);
                                step = sqlite3_step(res);

				//정원이 초과된 경우
                                if(sqlite3_column_int(res, 0) >= sqlite3_column_int(res, 1)){
                                        //큐에서 학생 한 명을 뽑아서 등록하기
					
					//공유메모리 동기화
					//세마포어 잠금 함수 호출
					semlock(semid);

					shmaddr = shmat(shmid, NULL, 0);
                                        //queue = *(GQueue*)shmaddr;
					
					int addsno = GPOINTER_TO_INT(g_queue_pop_tail((GQueue*)shmaddr));

					//예비 번호를 받은 학생이 있으면
					if(!g_queue_is_empty((GQueue*)shmaddr)){
						//과목 테이블의 해당 과목 인원수 1 증가
                                        	sql = "UPDATE course SET scount = scount+1 WHERE cno = ?";
                                        	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

                                        	if (rc != SQLITE_OK){
                                                	fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                                                	sqlite3_close (db);
                                                	return 1;
						}

                                        	sqlite3_bind_int(res, 1, cnum);
                                        	step = sqlite3_step(res);

                                        	//등록 테이블에 예비번호를 받은 학생의 학번과 과목번호 삽입
                                        	sql = "INSERT INTO enrol(sno, cno) VALUES(?, ?)";
                                        	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

                                        	if (rc != SQLITE_OK){
                                                	fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                                                	sqlite3_close (db);
                                                	return 1;
						}

                                        	sqlite3_bind_int(res, 1, addsno);
                                        	sqlite3_bind_int(res, 2, cnum);
                                        	step = sqlite3_step(res);

                                        	sqlite3_finalize(res);
                                        	sqlite3_close(db);

                                        	//printf("이 과목의 예비번호 %d번 학생이 수강 신청되었습니다.\n\n", g_queue_get_length ((GQueue*)shmaddr));
                                        	printf("==========================================================\n");

						shmdt(shmaddr);

						//세마포어 잠금해제 함수 호출
                                        	semunlock(semid);

                                	}
				}
				
				//과목 테이블에 해당 과목 인원수 1 감소
				sql = "UPDATE course SET scount = scount-1 WHERE cno = ?";
                                rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

                                if (rc != SQLITE_OK){
                                        fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                                        sqlite3_close (db);
                                        return 1;
                                }
                                sqlite3_bind_int(res, 1, cnum);
                                step = sqlite3_step(res);

                                //등록 테이블에 수강신청한 학생의 학번과 과목번호 삭제
                                sql = "DELETE FROM enrol WHERE cno = ?";
                                rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

                                if (rc != SQLITE_OK){
                                        fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                                        sqlite3_close (db);
                                        return 1;
                                }

                                sqlite3_bind_int(res, 1, cnum);
                                step = sqlite3_step(res);

                                sqlite3_finalize(res);
                                sqlite3_close(db);

                                printf("수강신청이 취소되었습니다.\n\n");
                                printf("==========================================================\n");

				break;


			//수강목록 확인
			case 3:
				//수강신청한 과목의 목록 출력해서 보여주기
                                printf("------------------------수강목록 확인------------------------\n\n");
                                sql = "SELECT course.cno, course.cname, course.prname FROM student, course, enrol WHERE student.sno = enrol.sno AND course.cno = enrol.cno AND student.sno = ?";
                                rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);

                                if(rc != SQLITE_OK){
                                        fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
                                        sqlite3_close (db);
                                        return 1;
                                }

                                sqlite3_bind_int(res, 1, sno);

                                while(sqlite3_step(res) == SQLITE_ROW){
                                        printf("과목번호: %d    교과목명: %s        담당교수: %s\n", sqlite3_column_int(res, 0), sqlite3_column_text(res, 1), sqlite3_column_text(res, 2));
                                }
                                printf("\n");
				printf("==========================================================\n");

				break;

			//종료
			case 0:
				flag = 0;
				break;
		}

	}
	
	//공유메모리 연결 해제 및 삭제
	//shmdt(shmaddr);
	shmctl(shmid, IPC_RMID, NULL);

	sqlite3_finalize(res);
	sqlite3_close(db);
	
	return 0;
}
