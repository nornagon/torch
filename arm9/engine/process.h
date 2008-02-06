#ifndef PROCESS_H
#define PROCESS_H 1

struct Process;
typedef void (*process_func)(Process *process);

struct Process {
	process_func process, end;
	void *data;
	u16 counter;
};

#endif /* PROCESS_H */
