/* SPDX-License-Identifier: GPL-2.0 */

static __read_mostly unsigned int walt_scale_demand_divisor;

static struct sched_cluster *sched_cluster[NR_CPUS];
static cpumask_t __read_mostly **cpu_array;
static int num_sched_clusters;
static int cpu_l2_sibling[NR_CPUS] = {[0 ... NR_CPUS-1] = -1};

enum task_boost_type {
	TASK_BOOST_NONE = 0,
	TASK_BOOST_ON_MID,
	TASK_BOOST_ON_MAX,
	TASK_BOOST_STRICT_MAX,
	TASK_BOOST_END,
};

struct compute_energy_output {
	unsigned long	sum_util[MAX_CLUSTERS];
	unsigned long	max_util[MAX_CLUSTERS];
	u16		cost[MAX_CLUSTERS];
	unsigned int	cluster_first_cpu[MAX_CLUSTERS];
};

/* headers of sysctl table */
extern unsigned int sysctl_panic_on_walt_bug;
extern unsigned int sysctl_sched_suppress_region2;
extern unsigned int sysctl_sched_skip_sp_newly_idle_lb;
extern unsigned int sysctl_sched_asymcap_boost;
extern unsigned int sysctl_walt_low_latency_task_threshold; /* disabled by default */
extern __read_mostly unsigned int sysctl_sched_force_lb_enable;
extern unsigned int sysctl_walt_rtg_cfs_boost_prio;

inline bool task_rtg_high_prio(struct task_struct *p);
