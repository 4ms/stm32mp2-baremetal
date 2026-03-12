#ifndef _LINUX_WORKQUEUE_H
#define _LINUX_WORKQUEUE_H

/* Bare-metal workqueue shim: simple pending-flag mechanism. */

typedef struct work_struct {
	void (*func)(struct work_struct *work);
	int pending;
} work_struct_t;

#define INIT_WORK(w, f) \
	do { (w)->func = (f); (w)->pending = 0; } while (0)

#define schedule_work(w) \
	do { (w)->pending = 1; } while (0)

/* Call this in your main loop to drain pending work */
static inline void run_pending_work(work_struct_t *w)
{
	if (w->pending) {
		w->pending = 0;
		w->func(w);
	}
}

#endif /* _LINUX_WORKQUEUE_H */
