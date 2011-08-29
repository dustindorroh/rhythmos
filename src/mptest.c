/*
 *      mptest.c
 *      
 *      Dustin Dorroh <ddorroh@aplopteng.com>
 */

#include "constants.h"
#include "user.h"

#define TAG_TEST           1
#define TAG_PING           2
#define TAG_PONG           3
#define TAG_COMMAND        4
#define TAG_RESPONSE       5
#define TAG_NEIGHBOUR      6
#define TAG_TOKEN          7

/*
 * This macro just makes the rest of the code a bit more concise by avoiding
 * the need to duplicate error detection/reporting logic everywhere. It should
 * only be used for system calls that return an integer >= 0 on success. 
 */
#define try(expr) ({     \
  int _res = (expr);     \
  if (0 > _res) {        \
    perror(#expr);       \
    exit(1);             \
  }                      \
  _res;})

/*============================================================================*/
/*
 * Non-blocking receive - simple test                      
 */

void test_nb_receive_simple()
{
	pid_t child_pid;
	if (0 == (child_pid = try(fork()))) {
		/*
		 * Child 
		 */
		message msg;
		int counter = 0;
		while (1) {
			counter++;
			if (0 == receive(&msg, 0)) {
				printf
				    ("Non-blocking receive: tag %d, size %d, char %d, counter = %d\n",
				     msg.tag, msg.size, ((char *)msg.data)[0],
				     counter);
				counter = 0;
			}
		}
	} else {
		/*
		 * Parent 
		 */
		char c;
		printf
		    ("Press a key to send a message to the child; q to finish\n");
		while ((1 == read(0, &c, 1)) && ('q' != c))
			send(child_pid, TAG_TEST, &c, 1);

		/*
		 * We'r done - kill the child process 
		 */
		try(kill(child_pid));
		try(waitpid(child_pid, NULL, 0));
	}

	printf("Non-blocking receive - simple test: finished\n");
}

/*============================================================================*/

/*
 * Non-blocking receive - advanced test                    
 */
void test_nb_receive_advanced()
{
	message msg;
	int res;
	int err;
	int testval = 1234;

	/*
	 * There should be no messages in the queue yet, because nothing's been sent.
	 * A non-blocking receive call should therefore return -1, with errno set to
	 * EAGAIN 
	 */
	res = receive(&msg, 0);
	err = errno;

	printf("First receive(): res = %d, errno = %d; should be "
	       "-1 and %d (EAGAIN)\n", res, err, EAGAIN);

	/*
	 * Now send a message to ourselves 
	 */
	res = send(getpid(), TAG_TEST, (void *)&testval, sizeof(int));
	printf("First send(): res = %d; should be 0\n", res);

	/*
	 * Try to receive the message we just sent, and check that it has the
	 * right content. 
	 */
	res = receive(&msg, 0);
	printf("Second receive(): res = %d; should be 0\n", res);
	if (0 == res) {
		printf("tag = %d (should be %d)\n", msg.tag, TAG_TEST);
		printf("size = %d (should be %d)\n", msg.size, sizeof(int));
		int r_testval = *(int *)msg.data;
		printf("r_testval = %d (should be %d)\n", r_testval, testval);
	}
	printf("Non-blocking receive - advanced test: finished\n");
}

/*============================================================================*/

/*
 * Non-blocking receive with multiple messages                
 */

void test_nb_multiple_messages()
{
	int nsent = 0;

	/*
	 * See how many messages we can send before running out of memory.
	 * The implementation should enforce some reasonable limit. 
	 */
	printf("Checking how many messages can be outstanding...\n");
	while (nsent < 10000) {
		if (0 != send(getpid(), TAG_TEST, (void *)&nsent, sizeof(int))) {
			break;
		}
		nsent++;
	}

	int err = errno;	/* save; errno may change after printf */
	printf("send() returned error after %d messages\n", nsent);
	printf("errno = %d; should be %d (ENOMEM)\n", err, ENOMEM);

	/*
	 * Receive all the outstanding messages 
	 */
	int i;
	message msg;
	for (i = 0; i < nsent; i++) {
		/*
		 * Receive should not block here, because there should be a message
		 * in the queue 
		 */
		try(receive(&msg, 0));
		assert(TAG_TEST == msg.tag);
		assert(sizeof(int) == msg.size);
		assert(*(int *)msg.data == i);
	}

	printf("Received all messages ok\n");

	/*
	 * Now try another receive; we should get EAGAIN because the queue should
	 * be empty 
	 */
	int res = receive(&msg, 0);
	printf("Final receive() returned %d, errno %d; should be -1 "
	       "and %d (EAGAIN)\n", res, errno, EAGAIN);
	printf("Non-blocking receive with multiple messages: finished\n");
}

/*============================================================================*/

/*
 * Blocking receive - simple test                        
 */

void test_b_receive_simple()
{
	pid_t child_pid;
	if (0 == (child_pid = try(fork()))) {
		/*
		 * Child 
		 */
		message msg;
		while (1) {
			try(receive(&msg, 1));
			printf("Blocking receive: tag %d, size %d, char %d\n",
			       msg.tag, msg.size, ((char *)msg.data)[0]);
		}
	} else {
		/*
		 * Parent 
		 */
		char c;
		printf
		    ("Press a key to send a message to the child; q to finish\n");
		while ((1 == read(0, &c, 1)) && ('q' != c))
			send(child_pid, TAG_TEST, &c, 1);

		/*
		 * We'r done - kill the child process 
		 */
		try(kill(child_pid));
		try(waitpid(child_pid, NULL, 0));
	}
	printf("Blocking receive - simple test: finished\n");
}

/*============================================================================*/

/*
 * Ping-pong                                  
 */

void ping(pid_t pong_pid)
{
	message msg;
	int count;
	for (count = 1; count <= 100; count++) {
		try(send(pong_pid, TAG_PING, NULL, 0));
		try(receive(&msg, 1));
		printf("ping: received pong %d\n", count);
	}
}

void pong()
{
	int count;
	for (count = 1; count <= 100; count++) {
		message msg;
		try(receive(&msg, 1));
		printf("pong: received ping %d\n", count);
		try(send(msg.from, TAG_PONG, NULL, 0));
	}
}

void test_ping_pong()
{
	pid_t pong_pid;
	pid_t ping_pid;

	if (0 == (pong_pid = try(fork()))) {
		/*
		 * First child process - pong 
		 */
		pong();
		exit(0);
	} else if (0 == (ping_pid = try(fork()))) {
		/*
		 * Second child process - ping 
		 */
		ping(pong_pid);
		exit(0);
	} else {
		/*
		 * Both processes launched... wait for them to complete 
		 */
		try(waitpid(pong_pid, NULL, 0));
		try(waitpid(ping_pid, NULL, 0));
		printf("Ping-pong: finished\n");
	}
}

/*============================================================================*/

/*
 * Broadcast                                  
 */

#define NSLAVES 6

void slave()
{
	/*
	 * Receive a message from the master 
	 */
	message msg;
	try(receive(&msg, 1));

	/*
	 * Send a reply, which includes the message content just received 
	 */
	char str[100];
	snprintf(str, 100, "I am process %d, and got \"%s\"", getpid(),
		 (char *)msg.data);
	send(msg.from, TAG_RESPONSE, str, strlen(str) + 1);
}

void test_broadcast()
{
	pid_t slave_pids[NSLAVES];
	int i = 0;

	/*
	 * Start up each of the slave processes 
	 */
	for (i = 0; i < NSLAVES; i++) {
		if (0 == (slave_pids[i] = try(fork()))) {
			slave();
			exit(0);
		}
	}

	/*
	 * Send a message out to all of them 
	 */
	for (i = 0; i < NSLAVES; i++) {
		char str[100];
		snprintf(str, 100, "You are process %d", slave_pids[i]);
		send(slave_pids[i], TAG_COMMAND, str, strlen(str) + 1);
	}

	/*
	 * Get a response from each 
	 */
	int responses;
	for (responses = 0; responses < NSLAVES; responses++) {
		message msg;
		try(receive(&msg, 1));
		printf("Slave %d said: %s\n", msg.from, (char *)msg.data);
	}

	/*
	 * Wait for all the slaves to finish 
	 */
	for (i = 0; i < NSLAVES; i++)
		try(waitpid(slave_pids[i], NULL, 0));

	printf("Broadcast: finished\n");
}

/*============================================================================*/

/*
 * Ring communication                            
 */

#define RING_SIZE 4
#define RING_MAX_PASSES 100

void ring(int my_index, pid_t main_pid)
{
	message msg;
	try(receive(&msg, 1));
	assert(TAG_NEIGHBOUR == msg.tag);
	assert(sizeof(pid_t) == msg.size);
	pid_t next = *(pid_t *) msg.data;

	while (1) {
		try(receive(&msg, 1));
		assert(TAG_TOKEN == msg.tag);
		assert(sizeof(int) == msg.size);
		int token = *(int *)msg.data;
		token++;
		int next_index = (my_index + 1) % RING_SIZE;
		if (token < RING_MAX_PASSES) {
			printf("%d: got token (value = %d), passing to %d\n",
			       my_index, token, next_index);
			try(send(next, TAG_TOKEN, (void *)&token, sizeof(int)));
		} else {
			printf
			    ("%d: got token (value = %d), passing back to main process\n",
			     my_index, token);
			/*
			 * Send back to main process 
			 */
			send(main_pid, TAG_TOKEN, (void *)&token, sizeof(int));
		}
	}
}

void test_ring()
{
	pid_t main_pid = getpid();

	/*
	 * Start up each of the processes in the ring 
	 */
	pid_t pids[RING_SIZE];
	int i;
	for (i = 0; i < RING_SIZE; i++) {
		if (0 == (pids[i] = try(fork()))) {
			ring(i, main_pid);
			exit(0);
		}
	}

	/*
	 * Send each process the pid of its neighbour 
	 */
	for (i = 0; i < RING_SIZE; i++)
		send(pids[i], TAG_NEIGHBOUR, &pids[(i + 1) % RING_SIZE],
		     sizeof(pid_t));

	/*
	 * Send the token to the first process 
	 */
	int token = 0;
	send(pids[0], TAG_TOKEN, (void *)&token, sizeof(int));

	/*
	 * Wait for the token to be sent back to us after it has finished
	 * being passed around 
	 */
	message msg;
	try(receive(&msg, 1));
	assert(TAG_TOKEN == msg.tag);
	assert(sizeof(int) == msg.size);
	token = *(int *)msg.data;
	printf("Main process received token: value = %d\n", token);

	/*
	 * Kill all processes in the ring 
	 */
	for (i = 0; i < RING_SIZE; i++) {
		try(kill(pids[i]));
		try(waitpid(pids[i], NULL, 0));
	}

	printf("Ring: finished\n");
}

/*============================================================================*/


/*
 * Error handling                               
 */
void test_error_handling()
{
	int res;
	char data[MAX_MESSAGE_SIZE + 1];

	printf("Testing send() with invalid size...\n");
	res = send(getpid(), TAG_TEST, data, MAX_MESSAGE_SIZE + 1);
	printf("res = %d, err = %d; should be -1 and %d (EINVAL)\n", res, errno,
	       EINVAL);

	printf("Testing send() with invalid pointer...\n");
	res = send(getpid(), TAG_TEST, (void *)0xD12F301A, 20);
	printf("res = %d, err = %d; should be -1 and %d (EFAULT)\n", res, errno,
	       EFAULT);

	printf("Testing send() with non-existant process...\n");
	res = send(99, TAG_TEST, data, 1);
	printf("res = %d, err = %d; should be -1 and %d (ESRCH)\n", res, errno,
	       ESRCH);

	printf("Testing receive() with invalid pointer...\n");
	res = receive((message *) 0xD12F301A, 0);
	printf("res = %d, err = %d; should be -1 and %d (EFAULT)\n", res, errno,
	       EFAULT);
}

/*============================================================================*/

int main(int argc, char **argv)
{
	printf("\n");
	printf("Message passing tests\n");
	printf("---------------------\n");
	while (1) {
		printf("\n");
		printf("Enter test to run:\n");
		printf("1. Non-blocking receive - simple test\n");
		printf("2. Non-blocking receive - advanced test\n");
		printf("3. Non-blocking receive with multiple messages\n");
		printf("4. Blocking receive - simple test\n");
		printf("5. Ping-pong\n");
		printf("6. Broadcast\n");
		printf("7. Ring\n");
		printf("8. Error handling\n");
		printf("q. Quit test program\n");
		char c;
		if (1 == read(0, &c, 1)) {
			printf("\n");
			switch (c) {
			case '1':
				test_nb_receive_simple();
				break;
			case '2':
				test_nb_receive_advanced();
				break;
			case '3':
				test_nb_multiple_messages();
				break;
			case '4':
				test_b_receive_simple();
				break;
			case '5':
				test_ping_pong();
				break;
			case '6':
				test_broadcast();
				break;
			case '7':
				test_ring();
				break;
			case '8':
				test_error_handling();
				break;
			case 'q':
				exit(0);
				break;
			}
		}
	}
	return 0;
}
