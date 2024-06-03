#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>

struct task_info {
    u64 start_time;
    u64 total_runtime;
    u64 switch_count;
    struct hlist_node hnode;
    pid_t pid;
};

static struct task_info *prev_info;

static int sched_switch_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
    return 0;
}

static void sched_switch_post_handler(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
}

static struct kprobe kp = {
    .symbol_name = "finish_task_switch.isra.0",
    .pre_handler = sched_switch_pre_handler,
    .post_handler = sched_switch_post_handler,
};

static int __init sched_monitor_init(void)
{
    printk(KERN_DEBUG "Hello, world!\n");
    int ret = register_kprobe(&kp);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register kprobe: %d\n", ret);
        return ret;
    }
    printk(KERN_DEBUG "Registered kprobe for finish_task_switch\n");

    prev_info = kmalloc(sizeof(struct task_info), GFP_KERNEL);
    if (!prev_info) {
        printk(KERN_ERR "Failed to allocate memory for prev_info\n");
        unregister_kprobe(&kp);
        return -ENOMEM;
    }
    printk(KERN_DEBUG "Allocated memory for prev_info\n");

    prev_info->start_time = jiffies;
    prev_info->total_runtime = 0;
    prev_info->switch_count = 0;
    prev_info->pid = current->pid;

    printk(KERN_INFO "Scheduler monitor module loaded.\n");
    printk(KERN_INFO "Running on PID: %d, Comm: %s\n", current->pid, current->comm);
    return 0;
}

static void __exit sched_monitor_exit(void)
{
    printk(KERN_INFO "Total runtime of Process %d: %llu\n", prev_info->pid, prev_info->total_runtime);
    kfree(prev_info);
    printk(KERN_INFO "Unregistering kprobe at %p\n", kp.addr);
    unregister_kprobe(&kp);
    printk(KERN_INFO "Scheduler monitor module unloaded.\n");
}

module_init(sched_monitor_init);
module_exit(sched_monitor_exit);

MODULE_LICENSE("Dual MIT/GPL");