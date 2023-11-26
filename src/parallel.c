// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_list.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* sum_lock -> no threads will try to add to the sum simultaneously */
pthread_mutex_t sum_lock;
/* graph task argument. */
typedef struct {
	int node_id;
} graph_task_arg_t;

static void process_neighbours(void *arg)
{
	int index = ((graph_task_arg_t *)arg)->node_id;
	os_node_t *node = graph->nodes[index];

	//if the node is being processed by another thread, we don't do anything
	if (graph->visited[index] == PROCESSING) {
		return;
	}

	// if the node is not being processed, we mark it as processing
	graph->visited[index] = PROCESSING;

	//we enqueue its neighbours
	for (unsigned int i = 0; i < node->num_neighbours; i++) {
		int neighbour_id = node->neighbours[i];
		if (graph->visited[neighbour_id] == NOT_VISITED) {
			//we create a task for each neighbour
			graph_task_arg_t *new_arg = malloc(sizeof(graph_task_arg_t));

			DIE(!new_arg, "malloc");
			new_arg->node_id = neighbour_id;
			os_task_t *task = create_task((void (*)(void *))process_neighbours, new_arg, free);

			enqueue_task(tp, task);
		}
	}

	// we process the node
	pthread_mutex_lock(&sum_lock);
	sum += node->info;
	pthread_mutex_unlock(&sum_lock);
}

static void process_node(int index)
{
	graph_task_arg_t *arg = malloc(sizeof(*arg));

	DIE(!arg, "malloc");
	arg->node_id = index;
	os_task_t *task = create_task((void (*)(void *))process_neighbours, arg, free);

	enqueue_task(tp, task);
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* TODO: Initialize graph synchronization mechanisms. */
	pthread_mutex_init(&sum_lock, NULL);
	tp = create_threadpool(NUM_THREADS);

	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	// we destroy the mutex after we are done with it
	pthread_mutex_destroy(&sum_lock);

	printf("%d", sum);

	return 0;
}
