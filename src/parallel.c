// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;

/* TODO: Define graph synchronization mechanisms. */
static pthread_mutex_t graph_lock;


/* TODO: Define graph task argument. */
typedef struct {
    unsigned int node_idx;
} graph_task_arg_t;

/* My TODO: helper function */
static void task_function(void *arg)
{
    graph_task_arg_t *task_arg = (graph_task_arg_t *) arg;
    unsigned int idx = task_arg->node_idx;

    pthread_mutex_lock(&graph_lock);
    if (graph->visited[idx] != NOT_VISITED) {
        pthread_mutex_unlock(&graph_lock);
		// Don't use free() here!
		return;
    }

    graph->visited[idx] = PROCESSING;
    sum += graph->nodes[idx]->info;
    pthread_mutex_unlock(&graph_lock);

    for (unsigned int i = 0; i < graph->nodes[idx]->num_neighbours; i++) {
        unsigned int neigh = graph->nodes[idx]->neighbours[i];

        pthread_mutex_lock(&graph_lock);
        int already = (graph->visited[neigh] != NOT_VISITED);
        pthread_mutex_unlock(&graph_lock);

        if (!already) {
            graph_task_arg_t *new_arg = malloc(sizeof(*new_arg));
            DIE(new_arg == NULL, "malloc");
            new_arg->node_idx = neigh;

            os_task_t *t = create_task(task_function, new_arg, free);
            enqueue_task(tp, t);
        }
    }

    pthread_mutex_lock(&graph_lock);
    graph->visited[idx] = DONE;
    pthread_mutex_unlock(&graph_lock);

    // Don't free(task_arg); destroy_task will handle it
}



static void process_node(unsigned int idx)
{
	/* TODO: Implement thread-pool based processing of graph. */
   	graph_task_arg_t *arg = malloc(sizeof(*arg));
    DIE(arg == NULL, "malloc");
    arg->node_idx = idx;

    os_task_t *t = create_task(task_function, arg, free);
    enqueue_task(tp, t);
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
	pthread_mutex_init(&graph_lock, NULL);

	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	/* My TODO: Cleanup graph synchronization mechanisms. */
	pthread_mutex_destroy(&graph_lock);

	printf("%d", sum);

	return 0;
}
