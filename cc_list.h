#ifndef __CC_LIST_H
#define	__CC_LIST_H

struct cc_list_head {
	struct cc_list_head *next, *prev;
};

#define container_of(ptr, type, member) ({  \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

static inline void CC_INIT_LIST_HEAD(struct cc_list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void __cc_list_add(struct cc_list_head *new,
			      struct cc_list_head *prev,
			      struct cc_list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void cc_list_add(struct cc_list_head *new, struct cc_list_head *head)
{
	__cc_list_add(new, head, head->next);
}

static inline void cc_list_add_tail(struct cc_list_head *new, struct cc_list_head *head)
{
	__cc_list_add(new, head->prev, head);
}

static inline void __cc_list_del(struct cc_list_head * prev, struct cc_list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void cc_list_del(struct cc_list_head *entry)
{
	__cc_list_del(entry->prev, entry->next);
	CC_INIT_LIST_HEAD(entry);
}

static inline int cc_list_empty(const struct cc_list_head *head)
{
	return head->next == head;
}

static inline int cc_list_empty_careful(const struct cc_list_head *head)
{
	struct cc_list_head *next = head->next;
	return (next == head) && (next == head->prev);
}

#define cc_list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define cc_list_first_entry(ptr, type, member) \
	cc_list_entry((ptr)->next, type, member)

#define cc_list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define cc_list_for_each_entry(pos, head, member)				\
	for (pos = cc_list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 	\
	     pos = cc_list_entry(pos->member.next, typeof(*pos), member))

#define cc_list_for_each_entry_reverse(pos, head, member)			\
	for (pos = cc_list_entry((head)->prev, typeof(*pos), member);	\
	     &pos->member != (head); 	\
	     pos = cc_list_entry(pos->member.prev, typeof(*pos), member))

#define cc_list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = cc_list_entry((head)->next, typeof(*pos), member),	\
		n = cc_list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = cc_list_entry(n->member.next, typeof(*n), member))

#endif /* __CC_LIST_H */
