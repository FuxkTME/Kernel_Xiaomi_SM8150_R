// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "sched.h"
#include "walt.h"

static int one_thousand = 1000;

/* Keep the same order as in fs/proc/proc_sysctl.c */
#define SYSCTL_ZERO	((void *)&sysctl_vals[0])
#define SYSCTL_ONE	((void *)&sysctl_vals[1])
#define SYSCTL_INT_MAX	((void *)&sysctl_vals[2])

/* shared constants to be used in various sysctls */
const int sysctl_vals[] = { 0, 1, INT_MAX };
EXPORT_SYMBOL(sysctl_vals);

/*
 * CFS task prio range is [100 ... 139]
 * 120 is the default prio.
 * RTG boost range is [100 ... 119] because giving
 * boost for [120 .. 139] does not make sense.
 * 99 means disabled and it is the default value.
 */
static unsigned int min_cfs_boost_prio = 99;
static unsigned int max_cfs_boost_prio = 119;

unsigned int sysctl_panic_on_walt_bug;
unsigned int sysctl_sched_suppress_region2;
unsigned int sysctl_sched_skip_sp_newly_idle_lb = 1;
unsigned int sysctl_sched_asymcap_boost;
unsigned int sysctl_walt_low_latency_task_threshold; /* disabled by default */
unsigned int sysctl_walt_rtg_cfs_boost_prio = 99; /* disabled by default */
__read_mostly unsigned int sysctl_sched_force_lb_enable = 1;

struct ctl_table walt_table[] = {
	{
		.procname       = "panic_on_walt_bug",
		.data           = &sysctl_panic_on_walt_bug,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_INT_MAX,
	},
	{
		.procname       = "sched_suppress_region2",
		.data           = &sysctl_sched_suppress_region2,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname       = "sched_skip_sp_newly_idle_lb",
		.data           = &sysctl_sched_skip_sp_newly_idle_lb,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_asymcap_boost",
		.data		= &sysctl_sched_asymcap_boost,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
      	{
		.procname	= "walt_low_latency_task_threshold",
		.data		= &sysctl_walt_low_latency_task_threshold,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand,
	},
	{
		.procname	= "sched_force_lb_enable",
		.data		= &sysctl_sched_force_lb_enable,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "walt_rtg_cfs_boost_prio",
		.data		= &sysctl_walt_rtg_cfs_boost_prio,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_cfs_boost_prio,
		.extra2		= &max_cfs_boost_prio,
	},
	{ }
};

struct ctl_table walt_base_table[] = {
	{
		.procname	= "walt",
		.mode		= 0555,
		.child		= walt_table,
	},
	{ },
};

static int __init walt_sysctl_init(void)
{
	register_sysctl_init("walt", walt_table);
	return 0;
}
core_initcall(walt_sysctl_init);

static void init_cpu_array(void)
{
	int i;

	cpu_array = kcalloc(num_sched_clusters, sizeof(cpumask_t *),
			GFP_ATOMIC | __GFP_NOFAIL);
	if (!cpu_array)
		SCHED_BUG_ON(1);

	for (i = 0; i < num_sched_clusters; i++) {
		cpu_array[i] = kcalloc(num_sched_clusters, sizeof(cpumask_t),
			GFP_ATOMIC | __GFP_NOFAIL);
		if (!cpu_array[i])
			SCHED_BUG_ON(1);
	}
}

static void build_cpu_array(void)
{
	int i;

	if (!cpu_array)
		SCHED_BUG_ON(1);
	/* Construct cpu_array row by row */
	for (i = 0; i < num_sched_clusters; i++) {
		int j, k = 1;

		/* Fill out first column with appropriate cpu arrays */
		cpumask_copy(&cpu_array[i][0], &sched_cluster[i]->cpus);
		/*
		 * k starts from column 1 because 0 is filled
		 * Fill clusters for the rest of the row,
		 * above i in ascending order
		 */
		for (j = i + 1; j < num_sched_clusters; j++) {
			cpumask_copy(&cpu_array[i][k],
					&sched_cluster[j]->cpus);
			k++;
		}

		/*
		 * k starts from where we left off above.
		 * Fill clusters below i in descending order.
		 */
		for (j = i - 1; j >= 0; j--) {
			cpumask_copy(&cpu_array[i][k],
					&sched_cluster[j]->cpus);
			k++;
		}
	}
}

static void find_cache_siblings(void)
{
	int cpu, cpu2;
	struct device_node *cpu_dev, *cpu_dev2, *cpu_l2_cache_node, *cpu_l2_cache_node2;

	for_each_possible_cpu(cpu) {
		cpu_dev = of_get_cpu_node(cpu, NULL);
		if (!cpu_dev)
			continue;

		cpu_l2_cache_node = of_parse_phandle(cpu_dev, "next-level-cache", 0);
		if (!cpu_l2_cache_node)
			continue;

		for_each_possible_cpu(cpu2) {
			if (cpu == cpu2)
				continue;

			cpu_dev2 = of_get_cpu_node(cpu2, NULL);
			if (!cpu_dev2)
				continue;

			cpu_l2_cache_node2 = of_parse_phandle(cpu_dev2, "next-level-cache", 0);
			if (!cpu_l2_cache_node2)
				continue;

			if (cpu_l2_cache_node == cpu_l2_cache_node2) {
				cpu_l2_sibling[cpu] = cpu2;
				break;
			}
		}
	}
}

static void walt_create_util_to_cost_pd(struct em_perf_domain *pd)
{
	int util, cpu = cpumask_first(to_cpumask(pd->cpus));
	unsigned long fmax;
	unsigned long scale_cpu;
	struct rq *rq = cpu_rq(cpu);
	struct sched_cluster *cluster = rq->cluster;

	fmax = (u64)pd->table[pd->nr_perf_states - 1].frequency;
	scale_cpu = arch_scale_cpu_capacity(NULL, cpu);

	for (util = 0; util < 1024; util++) {
		int j;

		int f = (fmax * util) / scale_cpu;
		struct em_perf_state *ps = &pd->table[0];

		for (j = 0; j < pd->nr_perf_states; j++) {
			ps = &pd->table[j];
			if (ps->frequency >= f)
				break;
		}
		cluster->util_to_cost[util] = ps->cost;
	}
}

void walt_create_util_to_cost(void)
{
	struct perf_domain *pd;
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	for (; pd; pd = pd->next)
		walt_create_util_to_cost_pd(pd->em_pd);
	rcu_read_unlock();
}

static inline void __sched_fork_init(struct task_struct *p)
{
	p->iowaited		= false;
}

static void walt_init_new_task_load(struct task_struct *p)
{
	__sched_fork_init(p);
}

static void walt_init_existing_task_load(struct task_struct *p)
{
	walt_init_new_task_load(p);
	cpumask_copy(&p->cpus_requested, &p->cpus_mask);
}

static void walt_update_cluster_topology(void)
{
	init_cpu_array();
	build_cpu_array();
	find_cache_siblings();
	walt_create_util_to_cost();
}

static int walt_init_stop_handler(void *data)
{
	int cpu;
	struct task_struct *g, *p;

	do_each_thread(g, p) {
		walt_init_existing_task_load(p);
	} while_each_thread(g, p);

	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);

		/* Create task members for idle thread */
		init_new_task_load(rq->idle);

	}

	walt_update_cluster_topology();

	return 0;
}

static void walt_init(struct work_struct *work)
{
	stop_machine(walt_init_stop_handler, NULL, NULL);

}

static DECLARE_WORK(walt_init_work, walt_init);
static int walt_module_init(void)
{
	schedule_work(&walt_init_work);

	return 0;
}

core_initcall(walt_module_init);
