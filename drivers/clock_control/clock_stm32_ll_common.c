/*
 * Copyright (c) 2017 Linaro Limited.
 * Copyright (c) 2017 RnDity Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <stm32_ll_bus.h>
#include <stm32_ll_pwr.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_system.h>
#include <stm32_ll_utils.h>
#include <drivers/clock_control.h>
#include <sys/util.h>
#include <sys/__assert.h>
#include <drivers/clock_control/stm32_clock_control.h>
#include "clock_stm32_ll_common.h"

/* Macros to fill up prescaler values */
#define fn_ahb_prescaler(v) LL_RCC_SYSCLK_DIV_ ## v
#define ahb_prescaler(v) fn_ahb_prescaler(v)

#define fn_apb1_prescaler(v) LL_RCC_APB1_DIV_ ## v
#define apb1_prescaler(v) fn_apb1_prescaler(v)

#if DT_NODE_HAS_PROP(DT_NODELABEL(rcc), apb2_prescaler)
#define fn_apb2_prescaler(v) LL_RCC_APB2_DIV_ ## v
#define apb2_prescaler(v) fn_apb2_prescaler(v)
#endif

#define fn_mco1_prescaler(v) LL_RCC_MCO1_DIV_ ## v
#define mco1_prescaler(v) fn_mco1_prescaler(v)

#define fn_mco2_prescaler(v) LL_RCC_MCO2_DIV_ ## v
#define mco2_prescaler(v) fn_mco2_prescaler(v)

/* Calculate MSI freq for the given range (at RUN range, not after standby) */
#if !defined(LL_RCC_MSIRANGESEL_RUN)
/* CONFIG_SOC_SERIES_STM32WBX or CONFIG_SOC_SERIES_STM32L0X or CONFIG_SOC_SERIES_STM32L1X */
#define RCC_CALC_MSI_RUN_FREQ() __LL_RCC_CALC_MSI_FREQ(LL_RCC_MSI_GetRange())
#else
/* mainly CONFIG_SOC_SERIES_STM32WLX or CONFIG_SOC_SERIES_STM32L4X or CONFIG_SOC_SERIES_STM32L5X */
#define RCC_CALC_MSI_RUN_FREQ() __LL_RCC_CALC_MSI_FREQ( \
			LL_RCC_MSIRANGESEL_RUN, LL_RCC_MSI_GetRange())
#endif

#if DT_NODE_HAS_PROP(DT_NODELABEL(rcc), ahb4_prescaler)
#define RCC_CALC_FLASH_FREQ __LL_RCC_CALC_HCLK4_FREQ
#define GET_CURRENT_FLASH_PRESCALER LL_RCC_GetAHB4Prescaler
#elif DT_NODE_HAS_PROP(DT_NODELABEL(rcc), ahb3_prescaler)
#define RCC_CALC_FLASH_FREQ __LL_RCC_CALC_HCLK3_FREQ
#define GET_CURRENT_FLASH_PRESCALER LL_RCC_GetAHB3Prescaler
#else
#define RCC_CALC_FLASH_FREQ __LL_RCC_CALC_HCLK_FREQ
#define GET_CURRENT_FLASH_PRESCALER LL_RCC_GetAHBPrescaler
#endif

/* Identify stm32wl dual-core socs by symbol defined in CMSIS dev header file */
#if (defined(CONFIG_SOC_SERIES_STM32WLX) && defined(DUAL_CORE))
#define STM32WL_DUAL_CORE
#endif

static uint32_t get_bus_clock(uint32_t clock, uint32_t prescaler)
{
	return clock / prescaler;
}

static inline int stm32_clock_control_on(const struct device *dev,
					 clock_control_subsys_t sub_system)
{
	struct stm32_pclken *pclken = (struct stm32_pclken *)(sub_system);

	ARG_UNUSED(dev);

	switch (pclken->bus) {
	case STM32_CLOCK_BUS_AHB1:
		LL_AHB1_GRP1_EnableClock(pclken->enr);
		break;
#if defined(CONFIG_SOC_SERIES_STM32L4X) || \
	defined(CONFIG_SOC_SERIES_STM32L5X) || \
	defined(CONFIG_SOC_SERIES_STM32F4X) && defined(RCC_AHB2_SUPPORT) || \
	defined(CONFIG_SOC_SERIES_STM32F7X) || \
	defined(CONFIG_SOC_SERIES_STM32F2X) || \
	defined(CONFIG_SOC_SERIES_STM32WBX) || \
	defined(CONFIG_SOC_SERIES_STM32WLX) || \
	defined(CONFIG_SOC_SERIES_STM32G4X)
	case STM32_CLOCK_BUS_AHB2:
		LL_AHB2_GRP1_EnableClock(pclken->enr);
		break;
#endif /* CONFIG_SOC_SERIES_STM32_* */
#if defined(CONFIG_SOC_SERIES_STM32L4X) || \
	defined(CONFIG_SOC_SERIES_STM32L5X) || \
	defined(CONFIG_SOC_SERIES_STM32F4X) && defined(RCC_AHB3_SUPPORT) || \
	defined(CONFIG_SOC_SERIES_STM32F7X) || \
	defined(CONFIG_SOC_SERIES_STM32F2X) || \
	defined(CONFIG_SOC_SERIES_STM32WBX) || \
	defined(CONFIG_SOC_SERIES_STM32WLX) || \
	defined(CONFIG_SOC_SERIES_STM32G4X)
	case STM32_CLOCK_BUS_AHB3:
		LL_AHB3_GRP1_EnableClock(pclken->enr);
		break;
#endif /* CONFIG_SOC_SERIES_STM32_* */
	case STM32_CLOCK_BUS_APB1:
		LL_APB1_GRP1_EnableClock(pclken->enr);
		break;
#if defined(CONFIG_SOC_SERIES_STM32L4X) || \
	defined(CONFIG_SOC_SERIES_STM32L5X) || \
	defined(CONFIG_SOC_SERIES_STM32F0X) || \
	defined(CONFIG_SOC_SERIES_STM32WBX) || \
	defined(CONFIG_SOC_SERIES_STM32WLX) || \
	defined(CONFIG_SOC_SERIES_STM32G4X)
	case STM32_CLOCK_BUS_APB1_2:
		LL_APB1_GRP2_EnableClock(pclken->enr);
		break;
#endif /* CONFIG_SOC_SERIES_STM32_* */
#if !defined(CONFIG_SOC_SERIES_STM32F0X)
	case STM32_CLOCK_BUS_APB2:
		LL_APB2_GRP1_EnableClock(pclken->enr);
		break;
#endif
#if defined(CONFIG_SOC_SERIES_STM32WLX)
	case STM32_CLOCK_BUS_APB3:
		LL_APB3_GRP1_EnableClock(pclken->enr);
		break;
#endif
#if defined (CONFIG_SOC_SERIES_STM32L0X) || \
	defined (CONFIG_SOC_SERIES_STM32G0X)
	case STM32_CLOCK_BUS_IOP:
		LL_IOP_GRP1_EnableClock(pclken->enr);
		break;
#endif
	default:
		return -ENOTSUP;
	}

	return 0;
}


static inline int stm32_clock_control_off(const struct device *dev,
					  clock_control_subsys_t sub_system)
{
	struct stm32_pclken *pclken = (struct stm32_pclken *)(sub_system);

	ARG_UNUSED(dev);

	switch (pclken->bus) {
	case STM32_CLOCK_BUS_AHB1:
		LL_AHB1_GRP1_DisableClock(pclken->enr);
		break;
#if defined(CONFIG_SOC_SERIES_STM32L4X) || \
	defined(CONFIG_SOC_SERIES_STM32L5X) || \
	defined(CONFIG_SOC_SERIES_STM32F4X) && defined(RCC_AHB2_SUPPORT) || \
	defined(CONFIG_SOC_SERIES_STM32F7X) || \
	defined(CONFIG_SOC_SERIES_STM32F2X) || \
	defined(CONFIG_SOC_SERIES_STM32WBX) || \
	defined(CONFIG_SOC_SERIES_STM32WLX) || \
	defined(CONFIG_SOC_SERIES_STM32G4X)
	case STM32_CLOCK_BUS_AHB2:
		LL_AHB2_GRP1_DisableClock(pclken->enr);
		break;
#endif /* CONFIG_SOC_SERIES_STM32_* */
#if defined(CONFIG_SOC_SERIES_STM32L4X) || \
	defined(CONFIG_SOC_SERIES_STM32L5X) || \
	defined(CONFIG_SOC_SERIES_STM32F4X) && defined(RCC_AHB3_SUPPORT) || \
	defined(CONFIG_SOC_SERIES_STM32F7X) || \
	defined(CONFIG_SOC_SERIES_STM32F2X) || \
	defined(CONFIG_SOC_SERIES_STM32WBX) || \
	defined(CONFIG_SOC_SERIES_STM32WLX) || \
	defined(CONFIG_SOC_SERIES_STM32G4X)
	case STM32_CLOCK_BUS_AHB3:
		LL_AHB3_GRP1_DisableClock(pclken->enr);
		break;
#endif /* CONFIG_SOC_SERIES_STM32_* */
	case STM32_CLOCK_BUS_APB1:
		LL_APB1_GRP1_DisableClock(pclken->enr);
		break;
#if defined(CONFIG_SOC_SERIES_STM32L4X) || \
	defined(CONFIG_SOC_SERIES_STM32L5X) || \
	defined(CONFIG_SOC_SERIES_STM32F0X) || \
	defined(CONFIG_SOC_SERIES_STM32WBX) || \
	defined(CONFIG_SOC_SERIES_STM32WLX) || \
	defined(CONFIG_SOC_SERIES_STM32G4X)
	case STM32_CLOCK_BUS_APB1_2:
		LL_APB1_GRP2_DisableClock(pclken->enr);
		break;
#endif /* CONFIG_SOC_SERIES_STM32_* */
#if !defined(CONFIG_SOC_SERIES_STM32F0X)
	case STM32_CLOCK_BUS_APB2:
		LL_APB2_GRP1_DisableClock(pclken->enr);
		break;
#endif
#if defined(CONFIG_SOC_SERIES_STM32WLX)
	case STM32_CLOCK_BUS_APB3:
		LL_APB3_GRP1_DisableClock(pclken->enr);
		break;
#endif
#if defined (CONFIG_SOC_SERIES_STM32L0X) || \
	defined (CONFIG_SOC_SERIES_STM32G0X)
	case STM32_CLOCK_BUS_IOP:
		LL_IOP_GRP1_DisableClock(pclken->enr);
		break;
#endif
	default:
		return -ENOTSUP;
	}

	return 0;
}


static int stm32_clock_control_get_subsys_rate(const struct device *clock,
						clock_control_subsys_t sub_system,
						uint32_t *rate)
{
	struct stm32_pclken *pclken = (struct stm32_pclken *)(sub_system);
	/*
	 * Get AHB Clock (= SystemCoreClock = SYSCLK/prescaler)
	 * SystemCoreClock is preferred to CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC
	 * since it will be updated after clock configuration and hence
	 * more likely to contain actual clock speed
	 */
	uint32_t ahb_clock = SystemCoreClock;
	uint32_t apb1_clock = get_bus_clock(ahb_clock, STM32_APB1_PRESCALER);
#if DT_NODE_HAS_PROP(DT_NODELABEL(rcc), apb2_prescaler)
	uint32_t apb2_clock = get_bus_clock(ahb_clock, STM32_APB2_PRESCALER);
#endif
#if DT_NODE_HAS_PROP(DT_NODELABEL(rcc), ahb3_prescaler)
	uint32_t ahb3_clock = get_bus_clock(ahb_clock * STM32_CPU1_PRESCALER,
					    STM32_AHB3_PRESCALER);
#endif

	ARG_UNUSED(clock);

	switch (pclken->bus) {
	case STM32_CLOCK_BUS_AHB1:
	case STM32_CLOCK_BUS_AHB2:
#if !defined(CONFIG_SOC_SERIES_STM32WLX)
	case STM32_CLOCK_BUS_AHB3:
#endif
#if defined (CONFIG_SOC_SERIES_STM32L0X) || \
	defined (CONFIG_SOC_SERIES_STM32G0X)
	case STM32_CLOCK_BUS_IOP:
#endif
		*rate = ahb_clock;
		break;
	case STM32_CLOCK_BUS_APB1:
#if defined(CONFIG_SOC_SERIES_STM32L4X) || \
	defined(CONFIG_SOC_SERIES_STM32L5X) || \
	defined(CONFIG_SOC_SERIES_STM32F0X) || \
	defined(CONFIG_SOC_SERIES_STM32WBX) || \
	defined(CONFIG_SOC_SERIES_STM32WLX) || \
	defined(CONFIG_SOC_SERIES_STM32G4X)
	case STM32_CLOCK_BUS_APB1_2:
#endif /* CONFIG_SOC_SERIES_STM32_* */
#if defined(CONFIG_SOC_SERIES_STM32G0X)
	case STM32_CLOCK_BUS_APB2:
		/*
		 * STM32G0x only has one APB, but two reset/clock enable
		 * registers for peripherals, so return the APB1 clock rate here
		 */
#endif /* CONFIG_SOC_SERIES_STM32G0X */
		*rate = apb1_clock;
		break;
#if !defined (CONFIG_SOC_SERIES_STM32F0X) && \
	!defined (CONFIG_SOC_SERIES_STM32G0X)
	case STM32_CLOCK_BUS_APB2:
		*rate = apb2_clock;
		break;
#endif
#if defined(CONFIG_SOC_SERIES_STM32WLX)
	case STM32_CLOCK_BUS_AHB3:
	case STM32_CLOCK_BUS_APB3:
		/* AHB3 and APB3 share the same clock and prescaler. */
		*rate = ahb3_clock;
		break;
#endif
	default:
		return -ENOTSUP;
	}

	return 0;
}

static struct clock_control_driver_api stm32_clock_control_api = {
	.on = stm32_clock_control_on,
	.off = stm32_clock_control_off,
	.get_rate = stm32_clock_control_get_subsys_rate,
};

/*
 * Unconditionally switch the system clock source to HSI.
 */
__unused
static void stm32_clock_switch_to_hsi(uint32_t new_ahb_prescaler)
{
	/* Enable HSI if not enabled */
	if (LL_RCC_HSI_IsReady() != 1) {
		/* Enable HSI */
		LL_RCC_HSI_Enable();
		while (LL_RCC_HSI_IsReady() != 1) {
		/* Wait for HSI ready */
		}
	}

	/* Set HSI as SYSCLCK source */
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
	LL_RCC_SetAHBPrescaler(new_ahb_prescaler);
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI) {
	}
}

/*
 * MCO configure doesn't active requested clock source,
 * so please make sure the clock source was enabled.
 */
static inline void stm32_clock_control_mco_init(void)
{
#ifndef CONFIG_CLOCK_STM32_MCO1_SRC_NOCLOCK
	LL_RCC_ConfigMCO(MCO1_SOURCE,
			 mco1_prescaler(CONFIG_CLOCK_STM32_MCO1_DIV));
#endif /* CONFIG_CLOCK_STM32_MCO1_SRC_NOCLOCK */

#ifndef CONFIG_CLOCK_STM32_MCO2_SRC_NOCLOCK
	LL_RCC_ConfigMCO(MCO2_SOURCE,
			 mco2_prescaler(CONFIG_CLOCK_STM32_MCO2_DIV));
#endif /* CONFIG_CLOCK_STM32_MCO2_SRC_NOCLOCK */
}

__unused
static int set_up_plls(void)
{
#if defined(STM32_PLL_ENABLED)
	int r;

	/*
	 * Case of chain-loaded applications:
	 * Switch to HSI and disable the PLL before configuration.
	 * (Switching to HSI makes sure we have a SYSCLK source in
	 * case we're currently running from the PLL we're about to
	 * turn off and reconfigure.)
	 *
	 */
	if (LL_RCC_GetSysClkSource() == LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {
		stm32_clock_switch_to_hsi(LL_RCC_SYSCLK_DIV_1);
	}
	LL_RCC_PLL_Disable();

#ifdef CONFIG_SOC_SERIES_STM32F7X
	/* Assuming we stay on Power Scale default value: Power Scale 1 */
	if (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC > 180000000) {
		LL_PWR_EnableOverDriveMode();
		while (LL_PWR_IsActiveFlag_OD() != 1) {
		/* Wait for OverDrive mode ready */
		}
		LL_PWR_EnableOverDriveSwitching();
		while (LL_PWR_IsActiveFlag_ODSW() != 1) {
		/* Wait for OverDrive switch ready */
		}
	}
#endif

#if STM32_PLL_Q_DIVISOR
	MODIFY_REG(RCC->PLLCFGR, RCC_PLLCFGR_PLLQ,
		   STM32_PLL_Q_DIVISOR
		   << RCC_PLLCFGR_PLLQ_Pos);
#endif

	r = config_pll_sysclock();
	if (r < 0) {
		return -ENOTSUP;
	}

	/* Enable PLL */
	LL_RCC_PLL_Enable();
	while (LL_RCC_PLL_IsReady() != 1U) {
	/* Wait for PLL ready */
	}

#endif /* STM32_PLL_ENABLED */

	return 0;
}

static void set_up_fixed_clock_sources(void)
{

#if STM32_HSE_ENABLED
#ifdef CONFIG_SOC_SERIES_STM32WLX
	if (IS_ENABLED(STM32_HSE_TCXO)) {
		LL_RCC_HSE_EnableTcxo();
	}
#elif !defined(CONFIG_SOC_SERIES_STM32WBX)
	/* Check if need to enable HSE bypass feature or not */
	if (IS_ENABLED(STM32_HSE_BYPASS)) {
		LL_RCC_HSE_EnableBypass();
	} else {
		LL_RCC_HSE_DisableBypass();
	}
#endif
	/* Enable HSE */
	LL_RCC_HSE_Enable();
	while (LL_RCC_HSE_IsReady() != 1) {
	/* Wait for HSE ready */
	}
#endif /* STM32_HSE_ENABLED */

#if STM32_HSI_ENABLED
	/* Enable HSI if not enabled */
	if (LL_RCC_HSI_IsReady() != 1) {
		/* Enable HSI */
		LL_RCC_HSI_Enable();
		while (LL_RCC_HSI_IsReady() != 1) {
		/* Wait for HSI ready */
		}
	}
#endif /* STM32_HSI_ENABLED */

#if STM32_MSI_ENABLED
	/* Set MSI Range */
#if defined(RCC_CR_MSIRGSEL)
	LL_RCC_MSI_EnableRangeSelection();
#endif /* RCC_CR_MSIRGSEL */

#if defined(CONFIG_SOC_SERIES_STM32L0X) || defined(CONFIG_SOC_SERIES_STM32L1X)
	LL_RCC_MSI_SetRange(STM32_MSI_RANGE << RCC_ICSCR_MSIRANGE_Pos);
#else
	LL_RCC_MSI_SetRange(STM32_MSI_RANGE << RCC_CR_MSIRANGE_Pos);
#endif /* CONFIG_SOC_SERIES_STM32L0X || CONFIG_SOC_SERIES_STM32L1X */

#if STM32_MSI_PLL_MODE
	/* Enable MSI hardware auto calibration */
	LL_RCC_MSI_EnablePLLMode();
#endif
	LL_RCC_MSI_SetCalibTrimming(0);

	/* Enable MSI if not enabled */
	if (LL_RCC_MSI_IsReady() != 1) {
		/* Enable MSI */
		LL_RCC_MSI_Enable();
		while (LL_RCC_MSI_IsReady() != 1) {
			/* Wait for MSI ready */
		}
	}
#endif /* STM32_MSI_ENABLED */

}

/**
 * @brief Initialize clocks for the stm32
 *
 * This routine is called to enable and configure the clocks and PLL
 * of the soc on the board. It depends on the board definition.
 * This function is called on the startup and also to restore the config
 * when exiting for low power mode.
 *
 * @param dev clock device struct
 *
 * @return 0
 */
int stm32_clock_control_init(const struct device *dev)
{
	uint32_t new_hclk_freq;
	uint32_t old_flash_freq;
	uint32_t new_flash_freq;
	int r;

	ARG_UNUSED(dev);


	/* Some clocks would be activated by default */
	config_enable_default_clocks();

	old_flash_freq = RCC_CALC_FLASH_FREQ(HAL_RCC_GetSysClockFreq(),
					       GET_CURRENT_FLASH_PRESCALER());

	new_hclk_freq = get_bus_clock(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC,
				      STM32_CORE_PRESCALER);

	new_flash_freq = RCC_CALC_FLASH_FREQ(new_hclk_freq,
				      STM32_FLASH_PRESCALER);

	/* If freq increases, set flash latency before any clock setting */
	if (new_flash_freq > old_flash_freq) {
		LL_SetFlashLatency(new_flash_freq);
	}

	/* Set up indiviual enabled clocks */
	set_up_fixed_clock_sources();

	/* Set up PLLs */
	r = set_up_plls();
	if (r < 0) {
		return r;
	}

#if STM32_SYSCLK_SRC_PLL
	/* Set PLL as System Clock Source */
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {
	}
#elif STM32_SYSCLK_SRC_HSE
	/* Set HSE as SYSCLCK source */
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSE);
	LL_RCC_SetAHBPrescaler(STM32_CORE_PRESCALER);
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSE) {
	}
#elif STM32_SYSCLK_SRC_MSI
	/* Set MSI as SYSCLCK source */
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_MSI);
	LL_RCC_SetAHBPrescaler(STM32_CORE_PRESCALER);
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_MSI) {
	}
#elif STM32_SYSCLK_SRC_HSI
	stm32_clock_switch_to_hsi(STM32_CORE_PRESCALER);
#endif /* STM32_SYSCLK_SRC_... */

	/* If freq not increased, set flash latency after all clock setting */
	if (old_flash_freq >= CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC) {
		LL_SetFlashLatency(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC);
	}

	SystemCoreClock = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;

	/* Set bus prescalers prescaler */
	LL_RCC_SetAPB1Prescaler(apb1_prescaler(STM32_APB1_PRESCALER));
#if DT_NODE_HAS_PROP(DT_NODELABEL(rcc), apb2_prescaler)
	LL_RCC_SetAPB2Prescaler(apb2_prescaler(STM32_APB2_PRESCALER));
#endif
#if DT_NODE_HAS_PROP(DT_NODELABEL(rcc), cpu2_prescaler)
	LL_C2_RCC_SetAHBPrescaler(ahb_prescaler(STM32_CPU2_PRESCALER));
#endif
#if DT_NODE_HAS_PROP(DT_NODELABEL(rcc), ahb3_prescaler)
	LL_RCC_SetAHB3Prescaler(ahb_prescaler(STM32_AHB3_PRESCALER));
#endif
#if DT_NODE_HAS_PROP(DT_NODELABEL(rcc), ahb4_prescaler)
	LL_RCC_SetAHB4Prescaler(ahb_prescaler(STM32_AHB4_PRESCALER));
#endif

	/* configure MCO1/MCO2 based on Kconfig */
	stm32_clock_control_mco_init();

	return 0;
}

/**
 * @brief RCC device, note that priority is intentionally set to 1 so
 * that the device init runs just after SOC init
 */
DEVICE_DT_DEFINE(DT_NODELABEL(rcc),
		    &stm32_clock_control_init,
		    NULL,
		    NULL, NULL,
		    PRE_KERNEL_1,
		    CONFIG_CLOCK_CONTROL_INIT_PRIORITY,
		    &stm32_clock_control_api);
