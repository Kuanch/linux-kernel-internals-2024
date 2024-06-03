#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "queue.h"
#include "timsort.h"

static inline size_t run_size(struct list_head *head)
{
    if (!head)
        return 0;
    if (!head->next)
        return 1;
    return (size_t)(head->next->prev);
}

struct pair {
    struct list_head *head, *next;
};

static size_t stk_size;

static void build_prev_link(struct list_head *head,
                            struct list_head *tail,
                            struct list_head *queue)
{
    tail->next = queue;
    do {
        queue->prev = tail;
        tail = queue;
        queue = queue->next;
    } while (queue);

    /* The final links to make a circular doubly-linked list */
    tail->next = head;
    head->prev = tail;
}

static struct pair find_run(struct list_head *queue)
{
    size_t len = 1;
    struct list_head *next = queue->next, *head = queue;
    struct pair result;

    if (!next) {
        result.head = head, result.next = next;
        return result;
    }

    if (strcmp(list_entry(queue, element_t, list)->value,
               list_entry(next, element_t, list)->value) > 0) {
        /* decending run, also reverse the list */
        struct list_head *prev = NULL;
        do {
            len++;
            queue->next = prev;
            prev = queue;
            queue = next;
            next = queue->next;
            head = queue;
        } while (next && strcmp(list_entry(queue, element_t, list)->value,
                                list_entry(next, element_t, list)->value) > 0);
        queue->next = prev;
    } else {
        do {
            len++;
            queue = next;
            next = queue->next;
        } while (next && strcmp(list_entry(queue, element_t, list)->value,
                                list_entry(next, element_t, list)->value) <= 0);
        queue->next = NULL;
    }
    head->prev = NULL;
    head->next->prev = (struct list_head *) len;
    result.head = head, result.next = next;
    return result;
}


static struct list_head *merge_at(struct list_head *at)
{
    size_t len = run_size(at) + run_size(at->prev);
    struct list_head *prev = at->prev->prev;
    struct list_head *queue = merge(at->prev, at);
    queue->prev = prev;
    queue->next->prev = (struct list_head *) len;
    --stk_size;
    return queue;
}

static struct list_head *merge_force_collapse(struct list_head *tp)
{
    while (stk_size >= 3) {
        if (run_size(tp->prev->prev) < run_size(tp)) {
            tp->prev = merge_at(tp->prev);
        } else {
            tp = merge_at(tp);
        }
    }
    return tp;
}

static struct list_head *merge_collapse(struct list_head *tp)
{
    int n;
    while ((n = stk_size) >= 2) {
        if ((n >= 3 &&
             run_size(tp->prev->prev) <= run_size(tp->prev) + run_size(tp)) ||
            (n >= 4 && run_size(tp->prev->prev->prev) <=
                           run_size(tp->prev->prev) + run_size(tp->prev))) {
            if (run_size(tp->prev->prev) < run_size(tp)) {
                tp->prev = merge_at(tp->prev);
            } else {
                tp = merge_at(tp);
            }
        } else if (run_size(tp->prev) <= run_size(tp)) {
            tp = merge_at(tp);
        } else {
            break;
        }
    }

    return tp;
}

void timsort(struct list_head *head)
{
    stk_size = 0;

    struct list_head *queue = head->next, *tp = NULL;
    if (head == head->prev)
        return;

    /* Convert to a null-terminated singly-linked list. */
    head->prev->next = NULL;

    do {
        /* Find next run */
        struct pair result = find_run(queue);
        result.head->prev = tp;
        tp = result.head;
        queue = result.next;
        stk_size++;
        tp = merge_collapse(tp);
    } while (queue);

    /* End of input; merge together all the runs. */
    tp = merge_force_collapse(tp);

    /* The final merge; rebuild prev links */
    struct list_head *stk0 = tp, *stk1 = stk0->prev;
    while (stk1 && stk1->prev)
        stk0 = stk0->prev, stk1 = stk1->prev;
    if (stk_size <= 1) {
        build_prev_link(head, head, stk0);
        return;
    }
    merge_final(head, stk1, stk0);
}


void shuffle(int *array, size_t n)
{
    if (n < 1)
        return;

    for (size_t i = 0; i < n - 1; i++) {
        size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
        long t = array[j];
        array[j] = array[i];
        array[i] = t;
    }
}
