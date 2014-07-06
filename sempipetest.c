/*
 * sempipetest.c
 *
 * A number of producer threads creates items
 * and writes them into a pipe. A number of consumer
 * threads reads the items from the pipe and prints them.
 * The user may choose the corresponding parameters.
 *
 *      Author: Fabian Foerg
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * Item size in bytes.
 */
#define RECORD_SIZE 512

/**
 * Default values, if command line
 * argument not specified.
 */
#define DEFAULT_NUM_PROD 4
#define DEFAULT_NUM_CONS 4
#define DEFAULT_ITEMS_PER_PROD 4
#define DEFAULT_MAX_IN_PIPE 4

/**
 * Semaphores and pipe.
 */
static sem_t slots_available;
static sem_t slots_used;
static sem_t consume_counter;
static sem_t consume_counter_mutex;
static int pipefd[2];

/**
 * Consumer thread argument.
 */
typedef struct __consumer_t {
	size_t num_items_to_consume;
} consumer_t;

/**
 * Producer thread argument.
 */
typedef struct __producer_t {
	int thread_id;
	size_t num_items_per_producer;
} producer_t;

/**
 * Consumer function.
 * Prints the content of the given buffer.
 */
static void consume_item(char buffer[RECORD_SIZE]) {
	printf("%s\n", buffer);
}

/**
 * Consumer thread main function.
 * Reads items from a pipe and consumes them.
 */
static void * consume(void * arg) {
	consumer_t *consumer_arg = (consumer_t *) arg;
	char record[RECORD_SIZE];
	int *return_value = (int *) malloc(sizeof(int));

	*return_value = EXIT_SUCCESS;

	while (1) {
		int consumed;
		size_t read_size;

		// check if there will be any elements to consume
		sem_wait(&consume_counter_mutex);
		sem_post(&consume_counter);
		sem_getvalue(&consume_counter, &consumed);
		sem_post(&consume_counter_mutex);

		if (consumed > consumer_arg->num_items_to_consume) {
			break;
		}

		// start consuming when products become available
		sem_wait(&slots_used);
		read_size = read(pipefd[0], record, RECORD_SIZE);
		sem_post(&slots_available);

		if (read_size != RECORD_SIZE) {
			fprintf(stderr, "Whole record cannot be read at once!\n");
			*return_value = EXIT_FAILURE;
			break;
		}

		consume_item(record);
	}

	return return_value;
}

/**
 * Producer function.
 * Adds information to the given buffer.
 */
static void produce_item(char buffer[RECORD_SIZE], int thread_id,
		int item_number) {
	snprintf(buffer, RECORD_SIZE, "Thread: %d\tItem: %d", thread_id,
			item_number);
}

/**
 * Producer thread main function.
 * Produces items and writes them into a pipe.
 */
static void * produce(void * arg) {
	producer_t * producer_arg = (producer_t *) arg;
	char record[RECORD_SIZE];
	int *return_value = (int *) malloc(sizeof(int));

	for (int i = 0; i < producer_arg->num_items_per_producer; i++) {
		size_t written_size;
		produce_item(record, producer_arg->thread_id, i);

		sem_wait(&slots_available);
		written_size = write(pipefd[1], record, RECORD_SIZE);
		sem_post(&slots_used);

		if (written_size != RECORD_SIZE) {
			*return_value = EXIT_FAILURE;
			return return_value;
		}
	}

	*return_value = EXIT_SUCCESS;
	return return_value;
}

/**
 * Main procedure.
 * Parses command line parameters.
 * Creates producer and consumer threads and waits
 * for them to finish.
 */
int main(int argc, char *argv[]) {
	// set arguments to default values
	size_t num_producers = DEFAULT_NUM_PROD;
	size_t num_consumers = DEFAULT_NUM_CONS;
	size_t items_per_producer = DEFAULT_ITEMS_PER_PROD;
	size_t max_in_pipe = DEFAULT_MAX_IN_PIPE;
	int opt;
	int exit_status = EXIT_SUCCESS;

	// argument parsing
	while ((opt = getopt(argc, argv, "p:c:i:m:")) != -1) {
		switch (opt) {
		case 'p':
			num_producers = atol(optarg);
			if (num_producers < 1) {
				fprintf(stderr, "number of producers must be at least one!\n");
				exit(EXIT_FAILURE);
			}
			break;

		case 'c':
			num_consumers = atol(optarg);
			if (num_consumers < 1) {
				fprintf(stderr, "number of consumers must be at least one!\n");
				exit(EXIT_FAILURE);
			}
			break;

		case 'i':
			items_per_producer = atol(optarg);
			if (items_per_producer < 1) {
				fprintf(stderr,
						"number of items per producer must be at least one!\n");
				exit(EXIT_FAILURE);
			}
			break;

		case 'm':
			max_in_pipe = atol(optarg);
			if (max_in_pipe < 1) {
				fprintf(stderr,
						"maximum number of items in pipe must be at least one!\n");
				exit(EXIT_FAILURE);
			}
			break;

		default: /* '?' */
			fprintf(stderr,
					"Usage: %s [-p <numproducers>] [-c <numconsumers>] [-i <itemsperproducer>] [-m <maxinpipe>]\n",
					argv[0]);
			exit(EXIT_FAILURE);
			break;
		}
	}

	// init pipe
	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}
	// enable non-blocking read
	fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

	// create and join threads
	{
		pthread_t producers[num_producers];
		pthread_t consumers[num_consumers];
		producer_t producer_arg[num_producers];
		consumer_t consumer_arg[num_consumers];
		int sem_ret;

		sem_ret = sem_init(&slots_available, 0, max_in_pipe);
		if (sem_ret) {
			perror("sem_init");
			exit_status = EXIT_FAILURE;
		}
		sem_ret = sem_init(&slots_used, 0, 0);
		if (sem_ret) {
			perror("sem_init");
			exit_status = EXIT_FAILURE;
		}
		sem_ret = sem_init(&consume_counter, 0, 0);
		if (sem_ret) {
			perror("sem_init");
			exit_status = EXIT_FAILURE;
		}
		sem_ret = sem_init(&consume_counter_mutex, 0, 1);
		if (sem_ret) {
			perror("sem_init");
			exit_status = EXIT_FAILURE;
		}

		if (exit_status == EXIT_SUCCESS) {
			// create threads
			for (int i = 0; i < num_producers; i++) {
				producer_arg[i].thread_id = i;
				producer_arg[i].num_items_per_producer = items_per_producer;
				int creation_fail = pthread_create(producers + i, NULL,
						&produce, &producer_arg[i]);

				if (creation_fail) {
					exit_status = EXIT_FAILURE;
					num_producers = i;
					num_consumers = 0;
					fprintf(stderr,
							"A producer thread was not created successfully!\n");
					break;
				}
			}

			for (int i = 0; i < num_consumers; i++) {
				consumer_arg[i].num_items_to_consume = items_per_producer
						* num_producers;
				int creation_fail = pthread_create(consumers + i, NULL,
						&consume, &consumer_arg[i]);

				if (creation_fail) {
					exit_status = EXIT_FAILURE;
					num_consumers = i;
					fprintf(stderr,
							"A consumer thread was not created successfully!\n");
					break;
				}
			}

			// join threads
			for (int i = 0; i < num_producers; i++) {
				int *ret_val_p;
				int fail;

				fail = pthread_join(producers[i], (void *) &ret_val_p);

				if (fail || (ret_val_p == PTHREAD_CANCELED)
						|| (*ret_val_p != EXIT_SUCCESS)) {
					exit_status = EXIT_FAILURE;
					fprintf(stderr,
							"A producer thread did not exit successfully!\n");
				}

				free(ret_val_p);
			}

			for (int i = 0; i < num_consumers; i++) {
				int *ret_val_p;
				int fail;

				fail = pthread_join(consumers[i], (void *) &ret_val_p);

				if (fail || (ret_val_p == PTHREAD_CANCELED)
						|| (*ret_val_p != EXIT_SUCCESS)) {
					exit_status = EXIT_FAILURE;
					fprintf(stderr,
							"A consumer thread did not exit successfully!\n");
				}

				free(ret_val_p);
			}
		}
	}

	// free resources
	close(pipefd[0]);
	close(pipefd[1]);

	sem_destroy(&slots_available);
	sem_destroy(&slots_used);
	sem_destroy(&consume_counter);
	sem_destroy(&consume_counter_mutex);

	return exit_status;
}
