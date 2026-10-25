/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef GPU_UNLOCK_ARCH_610_H
#define GPU_UNLOCK_ARCH_610_H

enum gpu_unlock_booter_arch {
	GU_BOOTER_GA10X = 0,
	GU_BOOTER_AD10X = 1,
};

/* Exact experimental AD103 canary; other Ada PCI IDs are not admitted. */
#define GU_AD103_RTX_4070_TI_SUPER 0x2705u
/* Experimental AD103 target; validated on one modified 32 GiB card only. */
#define GU_AD103_RTX_4080 0x2704u
/* Exact experimental AD104 target; one RTX 4070 validated on Linux. */
#define GU_AD104_RTX_4070 0x2786u
/* RTX 4070 on the AD103 die (10de:2709); same opt-in as the 2705. */
#define GU_AD103_RTX_4070 0x2709u
/* Exact experimental AD106 targets, each validated on one card. */
#define GU_AD106_RTX_4060_TI 0x2803u
#define GU_AD106_RTX_4060_TI_16GB 0x2805u
/* Exact experimental AD107 target, untested on silicon. */
#define GU_AD107_RTX_4060_MOBILE 0x28e0u
#define GU_AD107_RTX_4050_MOBILE 0x28e1u
/* Exact experimental AD104 target. */
#define GU_AD104_RTX_4070_TI 0x2782u
/* Exact experimental AD107 target, untested on silicon. */
#define GU_AD107_RTX_4060_LAPTOP 0x28a0u

/* RTX 4090 D uses the AD102 firmware path; gated as experimental. */
#define GU_AD102_RTX_4090_D 0x2685u


/* RTX 4070 Ti SUPER on a cut-down AD102 die (not the AD103 0x2705). Exact
 * experimental target, untested on silicon. */
#define GU_AD102_RTX_4070_TI_SUPER 0x2689u

#endif
