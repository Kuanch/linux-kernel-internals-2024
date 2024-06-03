#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#include "queue.h"

/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */


/* Helper functions */
/*
 * e_new(char *s) - Create a new element
 * @s: string would be put into the element
 * Return: the pointer to the new element, NULL for allocation failed
 */
static inline element_t *e_new(char *s)
{
    if (!s)
        return NULL;

    element_t *new_e = malloc(sizeof(element_t));
    if (!new_e)
        return NULL;
    INIT_LIST_HEAD(&new_e->list);

    size_t slen = strlen(s) + 1;
    new_e->value = malloc(slen);
    if (!new_e->value) {
        free(new_e);
        return NULL;
    }
    memcpy(new_e->value, s, slen);

    return new_e;
}

/* Introduce Linux Kernel function
 * list_sort - sort a list
 * @priv: private data, opaque to list_sort(), passed to @cmp
 * @head: the list to sort
 * @cmp: the elements comparison function
 *
 * This function implements "merge sort", which has O(n log n) complexity.
 * The comparison function @cmp must return a negative value if @a < @b,
 * zero if @a = @b, and a positive value if @a > @b.
 *
 * The list is sorted in-place.
 */
__attribute__((nonnull)) struct list_head *merge(struct list_head *a,
                                                 struct list_head *b)
{
    struct list_head *head = NULL, **tail = &head;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (strcmp(list_entry(a, element_t, list)->value,
                   list_entry(b, element_t, list)->value) <= 0) {
            *tail = a;
            tail = &a->next;
            a = a->next;
            if (!a) {
                *tail = b;
                break;
            }
        } else {
            *tail = b;
            tail = &b->next;
            b = b->next;
            if (!b) {
                *tail = a;
                break;
            }
        }
    }
    return head;
}

__attribute__((nonnull)) void merge_final(struct list_head *head,
                                          struct list_head *a,
                                          struct list_head *b)
{
    struct list_head *tail = head;
    uint8_t count = 0;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (strcmp(list_entry(a, element_t, list)->value,
                   list_entry(b, element_t, list)->value) <= 0) {
            tail->next = a;
            a->prev = tail;
            tail = a;
            a = a->next;
            if (!a)
                break;
        } else {
            tail->next = b;
            b->prev = tail;
            tail = b;
            b = b->next;
            if (!b) {
                b = a;
                break;
            }
        }
    }

    /* Finish linking remainder of list b on to tail */
    tail->next = b;
    do {
        /*
         * If the merge is highly unbalanced (e.g. the input is
         * already sorted), this loop may run many iterations.
         * Continue callbacks to the client even though no
         * element comparison is needed, so the client's cmp()
         * routine can invoke cond_resched() periodically.
         */
        if (unlikely(!++count))
            // cppcheck-suppress ignoredReturnValue
            strcmp(list_entry(b, element_t, list)->value,
                   list_entry(b, element_t, list)->value);
        b->prev = tail;
        tail = b;
        b = b->next;
    } while (b);

    /* And the final links to make a circular doubly-linked list */
    tail->next = head;
    head->prev = tail;
}

__attribute__((nonnull)) void list_sort(struct list_head *head)
{
    struct list_head *list = head->next, *pending = NULL;
    size_t count = 0; /* Count of pending */

    if (list == head->prev) /* Zero or one elements */
        return;

    /* Convert to a null-terminated singly-linked list. */
    head->prev->next = NULL;

    /*
     * Data structure invariants:
     * - All lists are singly linked and null-terminated; prev
     *   pointers are not maintained.
     * - pending is a prev-linked "list of lists" of sorted
     *   sublists awaiting further merging.
     * - Each of the sorted sublists is power-of-two in size.
     * - Sublists are sorted by size and age, smallest & newest at front.
     * - There are zero to two sublists of each size.
     * - A pair of pending sublists are merged as soon as the number
     *   of following pending elements equals their size (i.e.
     *   each time count reaches an odd multiple of that size).
     *   That ensures each later final merge will be at worst 2:1.
     * - Each round consists of:
     *   - Merging the two sublists selected by the highest bit
     *     which flips when count is incremented, and
     *   - Adding an element from the input as a size-1 sublist.
     */
    do {
        size_t bits;
        struct list_head **tail = &pending;

        /* Find the least-significant clear bit in count */
        for (bits = count; bits & 1; bits >>= 1)
            tail = &(*tail)->prev;
        /* Do the indicated merge */
        if (likely(bits)) {
            struct list_head *a = *tail, *b = a->prev;

            a = merge(b, a);
            /* Install the merged result in place of the inputs */
            a->prev = b->prev;
            *tail = a;
        }

        /* Move one element from input list to pending */
        list->prev = pending;
        pending = list;
        list = list->next;
        pending->next = NULL;
        count++;
    } while (list);

    /* End of input; merge together all the pending lists. */
    list = pending;
    pending = pending->prev;
    for (;;) {
        struct list_head *next = pending->prev;

        if (!next)
            break;
        list = merge(pending, list);
        pending = next;
    }
    /* The final merge, rebuilding prev links */
    merge_final(head, pending, list);
}


/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *new_q = malloc(sizeof(struct list_head));
    if (new_q)
        INIT_LIST_HEAD(new_q);
    return new_q;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (!l)
        return;
    for (struct list_head *node = l->next, *next; node != l; node = next) {
        next = node->next;
        element_t *e = list_entry(node, element_t, list);
        free(e->value);
        free(e);
    }
    free(l);
    return;
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!s)
        return false;
    element_t *new_e = e_new(s);
    if (!new_e)
        return false;
    list_add(&new_e->list, head);
    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!s)
        return false;
    element_t *new_e = e_new(s);
    if (!new_e)
        return false;
    list_add_tail(&new_e->list, head);
    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    element_t *e = list_entry(head->next, element_t, list);
    list_del_init(head->next);
    if (sp && bufsize) {
        size_t min = strlen(e->value) + 1;
        min = min > bufsize ? bufsize : min;
        memcpy(sp, e->value, min);
        sp[min - 1] = '\0';
    }
    return e;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    element_t *e = list_entry(head->prev, element_t, list);
    list_del_init(head->prev);
    if (sp && bufsize) {
        size_t min = strlen(e->value) + 1;
        min = min > bufsize ? bufsize : min;
        memcpy(sp, e->value, min);
        sp[min - 1] = '\0';
    }
    return e;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;
    int qlen = 0;
    for (struct list_head *p = head->next; p != head; p = p->next)
        qlen += 1;
    return qlen;
}

struct list_head *q_find_mid(struct list_head *head)
{
    if (!head || list_empty(head))
        return NULL;
    struct list_head *slow = head->next;
    struct list_head *fast = head->next;
    for (; fast != head && fast->next != head;
         slow = slow->next, fast = fast->next->next)
        ;
    return slow;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    struct list_head *slow = q_find_mid(head);
    list_del(slow);
    element_t *e = list_entry(slow, element_t, list);
    free(e->value);
    free(e);

    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    if (!head || list_empty(head))
        return false;

    for (struct list_head *p = head->next, *p_next; p != head; p = p_next) {
        bool found_dup = false;
        p_next = p->next;
        element_t *e = list_entry(p, element_t, list);
        for (struct list_head *q = p->next, *q_next, *q_prev; q != head;
             q = q_next) {
            element_t *e2 = list_entry(q, element_t, list);
            q_next = q->next;
            q_prev = q->prev;
            if (strcmp(e->value, e2->value) == 0) {
                q->next->prev = q_prev;
                q_prev->next = q_next;
                q_release_element(e2);
                found_dup = true;
            }
        }
        if (found_dup) {
            p_next = p->next;
            p->prev->next = p_next;
            p_next->prev = p->prev;
            q_release_element(e);
        }
    }
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    for (struct list_head *i = head->next; i != head && i->next != head;
         i = i->next)
        list_move_tail(i->next, i);
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    for (struct list_head *p = head->next, *next, *prev; p != head; p = next) {
        next = p->next;
        prev = p->prev;
        p->prev = next;
        p->next = prev;
    }
    struct list_head *next = head->next;
    head->next = head->prev;
    head->prev = next;
}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    int q_remain = q_size(head);
    if (!head || list_empty(head) || list_is_singular(head) || k < 2 ||
        k > q_remain)
        return;
    struct list_head *tmp_head = head;
    while (q_remain >= k) {
        int count = 0;
        struct list_head *next = NULL;
        struct list_head *prev = NULL;
        for (struct list_head *p = tmp_head->next; count < k && p != head;
             p = next, count++) {
            next = p->next;
            prev = p->prev;
            p->prev = next;
            p->next = prev;
        }
        // connect between two reversed list
        struct list_head *tmp_node = tmp_head->next;  // 1
        tmp_head->next->next = next;                  // 1 -> 4
        tmp_head->next = next->prev;                  // head -> 3
        next->prev->prev = tmp_head;                  // head <- 3
        next->prev = tmp_node;                        // 4 -> 1
        // let the tail of the reversed list be the new head
        tmp_head = tmp_node;
        q_remain -= k;
    }
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;

    LIST_HEAD(new_head);
    struct list_head *mid = q_find_mid(head);
    list_cut_position(&new_head, head, mid->prev);

    q_sort(head, descend);
    q_sort(&new_head, descend);

    struct list_head *temp;
    struct list_head *node1 = head->next;
    struct list_head *node2 = new_head.next;
    while (node1 != head && node2 != &new_head) {
        element_t *e1 = list_entry(node1, element_t, list);
        element_t *e2 = list_entry(node2, element_t, list);
        if (descend ? strcmp(e1->value, e2->value) > 0
                    : strcmp(e1->value, e2->value) < 0) {
            node1 = node1->next;
        } else {
            temp = node2->next;
            list_del(node2);
            node1->prev->next = node2;
            node2->prev = node1->prev;
            node1->prev = node2;
            node2->next = node1;
            node2 = temp;
        }
    }

    if (node1 == head) {
        list_splice_tail_init(&new_head, head);
    }
}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    for (struct list_head *p = head->next, *next; p != head; p = next) {
        element_t *e = list_entry(p, element_t, list);
        next = p->next;
        for (struct list_head *q = p->next; q != head; q = q->next) {
            element_t *e2 = list_entry(q, element_t, list);
            if (strcmp(e->value, e2->value) > 0) {
                list_del(p);
                free(e->value);
                free(e);
                break;
            }
        }
    }
    return 0;
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    for (struct list_head *p = head->next, *next; p != head; p = next) {
        element_t *e = list_entry(p, element_t, list);
        next = p->next;
        for (struct list_head *q = p->next; q != head; q = q->next) {
            element_t *e2 = list_entry(q, element_t, list);
            if (strcmp(e->value, e2->value) < 0) {
                list_del(p);
                free(e->value);
                free(e);
                break;
            }
        }
    }
    return 0;
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    if (!head || list_empty(head)) {
        return 0;
    }
    if (list_is_singular(head)) {
        return list_entry(head->next, queue_contex_t, chain)->size;
    }
    queue_contex_t *merged_list = list_entry(head->next, queue_contex_t, chain);
    for (struct list_head *p = head->next->next; p != head; p = p->next) {
        queue_contex_t *node = list_entry(p, queue_contex_t, chain);
        list_splice_init(node->q, merged_list->q);
    }
    q_sort(merged_list->q, descend);

    return q_size(merged_list->q);
}

void swap2nodes(struct list_head *a, struct list_head *b)
{
    struct list_head *a_prev = a->prev;
    struct list_head *b_prev = b->prev;
    if (a->prev != b)
        list_move(b, a_prev);
    list_move(a, b_prev);
}

bool q_shuffle(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return false;

    int qlen = q_size(head);
    for (struct list_head *old = head->prev, *old_next, *new;
         old != head &&qlen; old = old_next, qlen--) {
        old_next = old->prev;
        int rand_index = rand() % qlen;
        new = head->next;
        while (rand_index--)
            new = new->next;
        if (old == new)
            continue;
        swap2nodes(old, new);
    }
    return true;
}
