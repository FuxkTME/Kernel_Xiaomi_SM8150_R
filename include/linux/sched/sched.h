/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _LINUX_SCHED_SCHED_H
#define _LINUX_SCHED_SCHED_H

#define tsk_cpus_allowed(tsk) (&(tsk)->cpus_allowed)

/* cpu_core_energy & cpu_cluster_energy both implmented in topology.c */
extern
const struct sched_group_energy * const cpu_core_energy(int cpu);

extern
const struct sched_group_energy * const cpu_cluster_energy(int cpu);

#ifdef CONFIG_SCHED_TUNE
extern int set_stune_task_threshold(int threshold);
#endif


extern unsigned long capacity_curr_of(int cpu);

/* For multi-scheudling support */
enum SCHED_LB_TYPE {
	SCHED_HMP_LB = 0,
	SCHED_EAS_LB,
	SCHED_HYBRID_LB,
	SCHED_UNKNOWN_LB
};

#endif

