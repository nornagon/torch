#ifndef PROCESS_H
#define PROCESS_H 1

struct map_s;
struct process_s;
typedef struct process_s process_t;

typedef void (*process_func)(process_t *process, struct map_s *map);

struct process_s {
	process_func process, end;
	void *data;
};

#endif /* PROCESS_H */
