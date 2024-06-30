Performance Counters
====================

FFTW measures execution time in the planning stage, optionally taking advantage
of hardware performance counters. This document describes the supported
counters and additional steps needed to enable each on different architectures.

See `./configure --help` for flags for enabling each supported counter.
See [kernel/cycle.h](kernel/cycle.h) for the code that accesses the counters.

ARMv7-A (armv7a)
================

`CNTVCT`: Virtual Count Register in VMSA
--------------------------------------

A 64-bit counter part of Virtual Memory System Architecture.
Section B4.1.34 in ARM Architecture Reference Manual ARMv7-A/ARMv7-R

For access from user mode, requires `CNTKCTL.PL0VCTEN == 1`, which must
be set in kernel mode on each CPU:

        #define CNTKCTL_PL0VCTEN 0x2 /* B4.1.26 in ARM Architecture Rreference */
        uint32_t r;
        asm volatile("mrc p15, 0, %0, c14, c1, 0" : "=r"(r)); /* read */
        r |= CNTKCTL_PL0VCTEN;
        asm volatile("mcr p15, 0, %0, c14, c1, 0" :: "r"(r)); /* write */

Kernel module source *which can be patched with the above code* available at:
https://github.com/thoughtpolice/enable_arm_pmu

`PMCCNTR`: Performance Monitors Cycle Count Register in VMSA
----------------------------------------------------------

A 32-bit counter part of Virtual Memory System Architecture.
Section B4.1.113 in ARM Architecture Reference Manual ARMv7-A/ARMv7-R

For access from user mode, requires user-mode access to PMU to be enabled
(`PMUSERENR.EN == 1`), which must be done from kernel mode on each CPU:

        #define PERF_DEF_OPTS (1 | 16)
        /* enable user-mode access to counters */
        asm volatile("mcr p15, 0, %0, c9, c14, 0" :: "r"(1));
        /* Program PMU and enable all counters */
        asm volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(PERF_DEF_OPTS));
        asm volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(0x8000000f));

Kernel module source with the above code available at:
[GitHub thoughtpolice/enable\_arm\_pmu](https://github.com/thoughtpolice/enable_arm_pmu)

More information:
http://neocontra.blogspot.com/2013/05/user-mode-performance-counters-for.html

ARMv8-A (aarch64)
=================

`CNTVCT_EL0`: Counter-timer Virtual Count Register
------------------------------------------------

A 64-bit counter, part of Generic Registers.
Section D8.5.17 in ARM Architecture Reference Manual ARMv8-A

For user-mode access, requires `CNTKCTL_EL1.EL0VCTEN == 1`, which
must be set from kernel mode for each CPU:

        #define CNTKCTL_EL0VCTEN 0x2
        uint32_t r;
        asm volatile("mrs %0, CNTKCTL_EL1" : "=r"(r)); /* read */
        r |= CNTKCTL_EL0VCTEN;
        asm volatile("msr CNTKCTL_EL1, %0" :: "r"(r)); /* write */

*WARNING*: Above code was not tested.

`PMCCNTR_EL0`: Performance Monitors Cycle Count Register
------------------------------------------------------

A 64-bit counter, part of Performance Monitors.
Section D8.4.2 in ARM Architecture Reference Manual ARMv8-A

For access from user mode, requires user-mode access to PMU (`PMUSERENR_EL0.EN
== 1`), which must be set from kernel mode for each CPU:

        #define PERF_DEF_OPTS (1 | 16)
        /* enable user-mode access to counters */
        asm volatile("msr PMUSERENR_EL0, %0" :: "r"(1));
        /* Program PMU and enable all counters */
        asm volatile("msr PMCR_EL0, %0" :: "r"(PERF_DEF_OPTS));
        asm volatile("msr PMCNTENSET_EL0, %0" :: "r"(0x8000000f));
        asm volatile("msr PMCCFILTR_EL0, %0" :: "r"(0));

Kernel module source with the above code available at:
[GitHub rdolbeau/enable\_arm\_pmu](https://github.com/rdolbeau/enable_arm_pmu)
or in [Pull Request #2 at thoughtpolice/enable\_arm\_pmu](https://github.com/thoughtpolice/enable_arm_pmu/pull/2)
