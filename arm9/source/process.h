#ifndef PROCESS_H
#define PROCESS_H 1

struct process_s;
typedef struct process_s process_t;

struct process_s {
	void (*process)(process_t *process, map_t *map);
	void (*end)(process_t *process);
	void *data;
};

#endif /* PROCESS_H */
