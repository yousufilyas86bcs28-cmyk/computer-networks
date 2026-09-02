[24bcs051@mepcolinux ex3new]$cat parentchild.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>

int main(){
   int shmid, *buf;
   int i, num, size = 10;
   
   shmid = shmget(4777, sizeof(int) * size, IPC_CREAT | 0666);
   printf("==========PARENTCHILD PROGRAM=========");
   
   if(shmid == -1){
      perror("shmget");
      return 1;
   }
   
   buf = (int *)shmat(shmid, NULL, 0);
   
   if(buf == (void *)-1){
      perror("shmat");
      return 1;
   }
   
   pid_t pid = fork();
   
   if(pid < 0){
      perror("fork");
      return 1;
   }
   
   if(pid == 0){

      printf("Producer Child Process\n");
      
      for(i = 0; i < size; i++){
	 printf("Enter data %d : ", i + 1);
	 scanf("%d", &num);
	 buf[i] = num;
      }
      
      shmdt(buf);
   }
   
   else{

      wait(NULL);

      printf("Consumer Parent Process\n");
      
      for(i = 0; i < size; i++){
	 printf("data[%d] = %d\n", i + 1, buf[i]);
      }
      
      shmdt(buf);
      shmctl(shmid, IPC_RMID, NULL);

    }

    return 0;
}
[24bcs051@mepcolinux ex3new]$./parent
==========PARENTCHILD PROGRAM=========Producer Child Process
Enter data 1 : 4
Enter data 2 : 5
Enter data 3 : 6
Enter data 4 : 7
Enter data 5 : 8
Enter data 6 : 9
Enter data 7 : 2
Enter data 8 : 3
Enter data 9 : 10
Enter data 10 : 11
==========PARENTCHILD PROGRAM=========Consumer Parent Process
data[1] = 4
data[2] = 5
data[3] = 6
data[4] = 7
data[5] = 8
data[6] = 9
data[7] = 2
data[8] = 3
data[9] = 10
data[10] = 11
[24bcs051@mepcolinux ex3new]$cat producer.c
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<unistd.h>
#include<sys/wait.h>
int main(){
   int shmid,num,*buf,i;
   int size = 10;
   int in = 0;

   shmid = shmget(2462,sizeof(int)*size,IPC_CREAT|00600);

   if(shmid == -1){
      perror("shmget");
      return 1;
   }

   buf = (int*)shmat(shmid,NULL,0);

   if(buf == (int*)-1){
      perror("shmat");
      return 1;
   }
   else{
      for(i = 0; i<size; i++){
	 printf("Enter Data %d:",i+1);
	 scanf("%d",&num);
	 buf[in] = num;
	 in = (in + 1) % size;
      }
   }
   shmdt(buf);
   return 0;
}
[24bcs051@mepcolinux ex3new]$./producer
Enter Data 1:7
Enter Data 2:8
Enter Data 3:9
Enter Data 4:10
Enter Data 5:11
Enter Data 6:1
Enter Data 7:2
Enter Data 8:3
Enter Data 9:4
Enter Data 10:54
[24bcs051@mepcolinux ex3new]$cat consumer.c
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<unistd.h>
#include<sys/wait.h>

int main(){
   int shmid,num,*buf,i;
   int size = 10;
   int out = 0;

   shmid = shmget(2462,sizeof(int)*size,IPC_CREAT|00600);

   if(shmid == -1){
      perror("shmget");
      return 1;
   }

   buf = (int*)shmat(shmid,NULL,0);

   if(buf == (int*)-1){
      perror("shmat");
      return 0;
   }
   else{
      printf("Consumed Data\n");
      for(i = 0; i<size; i++){
	 num = buf[out];
	 printf("%d ",num);
	 out = (out + 1) % size;
      }
      shmdt(buf);
      shmctl(shmid,IPC_RMID,NULL);
   }
return 0;
}
[24bcs051@mepcolinux ex3new]$./consumer
Consumed Data
7 8 9 10 11 1 2 3 4 54 [24bcs051@mepcolinux ex3new]$cat producerbw.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
int main() {
    int shmid, num, *buf;
    int c;
    int size = 10;    
    shmid = shmget(2463,sizeof(int)*(size + 4),IPC_CREAT|00600);
    printf("=========producer starts producing=======\n");
    if(shmid == -1){
       perror("shmget");
       return 1;
    }
    
    buf = (int*)shmat(shmid, NULL, 0);
    if(buf == (int*)-1) {
       perror("shmat");
       return 1;
    }
    
    if(buf[13] != 1){
        buf[10] = 0;
        buf[11] = 0;
    }
    buf[12] = 1;
    
    while(1){
       
       while (((buf[10] + 1) % size) == buf[11]);
       
       printf("Enter Data: ");
       scanf("%d", &num);
       
       buf[buf[10]] = num;
       buf[10] = (buf[10] + 1) % size;

       printf("type 1 to produce or 0 to terminate:");
       scanf("%d",&c);
       if(c != 1){
	  break;
       }
    }
    
    buf[12] = 0;
    
    if(buf[13] == 0){
       shmdt(buf);
       shmctl(shmid, IPC_RMID, NULL);
    } 
    else{
       shmdt(buf);
    }
    
    return 0;
}
[24bcs051@mepcolinux ex3new]$./producerbw
=========producer starts producing=======
Enter Data: 5
type 1 to produce or 0 to terminate:1
Enter Data: 6
type 1 to produce or 0 to terminate:1
Enter Data: 2
type 1 to produce or 0 to terminate:0
[24bcs051@mepcolinux ex3new]$cat consumerbw.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>

int main() {
    int shmid, num, *buf, i, c;
    int size = 10;    
    shmid = shmget(2463,sizeof(int)*(size + 4),IPC_CREAT|00600);
    printf("========consumer starts==========\n");
    if(shmid == -1){
       perror("shmget");
       return 1;
    }
    
    buf = (int*)shmat(shmid, NULL, 0);
    if(buf == (int*)-1){
       perror("shmat");
       return 1;
    }

    if(buf[12] != 1){
       buf[10] = 0;
       buf[11] = 0;
    }
    
    buf[13] = 1;
    
    while(1){
       while (buf[11] == buf[10]);
       
       num = buf[buf[11]];
       printf("Consumed Data: %d\n", num);
       
       buf[11] = (buf[11] + 1) % size;
       
       printf("type 1 for consume 0 to exit:");
       scanf("%d",&c);
       if(c != 1){
	  break;
       }
    }
    
    buf[13] = 0;
    
    if(buf[12] == 0){
       shmdt(buf);
       shmctl(shmid, IPC_RMID, NULL);
    } 
    else{
       shmdt(buf);
    }
    
    return 0;
}
[24bcs051@mepcolinux ex3new]$./consumerbw
========consumer starts==========
5
enter 1 to consume:1
6
enter 1 to consume:1
2
^C
[24bcs051@mepcolinux ex3new]$cat msgq.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>

// Define the message structure
struct msg_buffer {
    long msg_type;      // Must be > 0
    int data;           // The actual payload (item produced)
};

int main() {
    key_t key;
    int msqid;
    pid_t pid;

    // 1. Generate a unique key using ftok
    key = ftok("progfile", 65);
    if (key == -1) {
        perror("ftok failed");
        exit(1);
    }

    // 2. Create the message queue (IPC_CREAT creates it, 0666 sets read/write permissions)
    msqid = msgget(key, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("msgget failed");
        exit(1);
    }

    // 3. Fork the process into Producer and Consumer
    pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    } 
    else if (pid == 0) {
        // ====CONSTRUCTING THE CONSUMER (Child Process) ======
        struct msg_buffer message;
        
        for (int i = 1; i <= 5; i++) {
            // msgrcv blocks automatically if the queue is empty
            if (msgrcv(msqid, &message, sizeof(message.data), 1, 0) == -1) {
                perror("msgrcv failed");
                exit(1);
            }
            printf("[Consumer] Consumed item: %d\n", message.data);
            sleep(2); // Simulate time taken to consume the item
        }
        exit(0);
    } 
    else {
        // ====== CONSTRUCTING THE PRODUCER (Parent Process) =======
        struct msg_buffer message;
        message.msg_type = 1; // Message type must be positive

        for (int i = 1; i <= 5; i++) {
            message.data = i * 10; // Produce an item (e.g., 10, 20, 30...)
            printf("[Producer] Produced item: %d\n", message.data);

            // msgsnd sends the message to the queue
            if (msgsnd(msqid, &message, sizeof(message.data), 0) == -1) {
                perror("msgsnd failed");
                exit(1);
            }
            sleep(1); // Simulate time taken to produce the next item
        }

        // Wait for the consumer child process to finish
        wait(NULL);

        // 4. Clean up and delete the message queue from the kernel
        msgctl(msqid, IPC_RMID, NULL);
        printf("Message queue destroyed. Program finished.\n");
    }

    return 0;
}
[24bcs051@mepcolinux ex3new]$./msg
[Producer] Produced item: 10
[Consumer] Consumed item: 30
[Producer] Produced item: 20
[Producer] Produced item: 30
[Consumer] Consumed item: 10
[Producer] Produced item: 40
[Producer] Produced item: 50
[Consumer] Consumed item: 20
[Consumer] Consumed item: 30
[Consumer] Consumed item: 40
Message queue destroyed. Program finished.
[24bcs051@mepcolinux ex3new]$exit
exit
