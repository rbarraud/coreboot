/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2014 Google Inc.
 * Copyright (C) 2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Helper functions for dealing with power management registers
 * and the differences between PCH variants.
 */

#define __SIMPLE_DEVICE__

#include <arch/io.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_def.h>
#include <console/console.h>
#include <intelblocks/pmclib.h>
#include <halt.h>
#include <rules.h>
#include <stdlib.h>
#include <soc/gpe.h>
#include <soc/gpio.h>
#include <soc/iomap.h>
#include <soc/lpc.h>
#include <soc/pci_devs.h>
#include <soc/pm.h>
#include <soc/smbus.h>
#include <timer.h>
#include "chip.h"

/*
 * SMI
 */

const char *const *soc_smi_sts_array(size_t *a)
{
	static const char *const smi_sts_bits[] = {
		[BIOS_STS_BIT] = "BIOS",
		[LEGACY_USB_STS_BIT] = "LEGACY_USB",
		[SMI_ON_SLP_EN_STS_BIT] = "SLP_SMI",
		[APM_STS_BIT] = "APM",
		[SWSMI_TMR_STS_BIT] = "SWSMI_TMR",
		[PM1_STS_BIT] = "PM1",
		[GPE0_STS_BIT] = "GPE0",
		[GPIO_STS_BIT] = "GPI",
		[MCSMI_STS_BIT] = "MCSMI",
		[DEVMON_STS_BIT] = "DEVMON",
		[TCO_STS_BIT] = "TCO",
		[PERIODIC_STS_BIT] = "PERIODIC",
		[SERIRQ_SMI_STS_BIT] = "SERIRQ_SMI",
		[SMBUS_SMI_STS_BIT] = "SMBUS_SMI",
		[PCI_EXP_SMI_STS_BIT] = "PCI_EXP_SMI",
		[MONITOR_STS_BIT] = "MONITOR",
		[SPI_SMI_STS_BIT] = "SPI",
		[GPIO_UNLOCK_SMI_STS_BIT] = "GPIO_UNLOCK",
		[ESPI_SMI_STS_BIT] = "ESPI_SMI",
	};

	*a = ARRAY_SIZE(smi_sts_bits);
	return smi_sts_bits;
}

/*
 * TCO
 */

const char *const *soc_tco_sts_array(size_t *a)
{
	static const char *const tco_sts_bits[] = {
		[0] = "NMI2SMI",
		[1] = "SW_TCO",
		[2] = "TCO_INT",
		[3] = "TIMEOUT",
		[7] = "NEWCENTURY",
		[8] = "BIOSWR",
		[9] = "DMISCI",
		[10] = "DMISMI",
		[12] = "DMISERR",
		[13] = "SLVSEL",
		[16] = "INTRD_DET",
		[17] = "SECOND_TO",
		[18] = "BOOT",
		[20] = "SMLINK_SLV"
	};

	*a = ARRAY_SIZE(tco_sts_bits);
	return tco_sts_bits;
}

/*
 * GPE0
 */

const char *const *soc_gpe_sts_array(size_t *a)
{
	static const char *const gpe_sts_bits[] = {
		[1] = "HOTPLUG",
		[2] = "SWGPE",
		[6] = "TCO_SCI",
		[7] = "SMB_WAK",
		[9] = "PCI_EXP",
		[10] = "BATLOW",
		[11] = "PME",
		[12] = "ME",
		[13] = "PME_B0",
		[14] = "eSPI",
		[15] = "GPIO Tier-2",
		[16] = "LAN_WAKE",
		[18] = "WADT"
	};

	*a = ARRAY_SIZE(gpe_sts_bits);
	return gpe_sts_bits;
}

int acpi_sci_irq(void)
{
	int scis = pci_read_config32(PCH_DEV_PMC, ACTL) & SCI_IRQ_SEL;
	int sci_irq = 9;

	/* Determine how SCI is routed. */
	switch (scis) {
	case SCIS_IRQ9:
	case SCIS_IRQ10:
	case SCIS_IRQ11:
		sci_irq = scis - SCIS_IRQ9 + 9;
		break;
	case SCIS_IRQ20:
	case SCIS_IRQ21:
	case SCIS_IRQ22:
	case SCIS_IRQ23:
		sci_irq = scis - SCIS_IRQ20 + 20;
		break;
	default:
		printk(BIOS_DEBUG, "Invalid SCI route! Defaulting to IRQ9.\n");
		sci_irq = 9;
		break;
	}

	printk(BIOS_DEBUG, "SCI is IRQ%d\n", sci_irq);
	return sci_irq;
}

uint8_t *pmc_mmio_regs(void)
{
	uint32_t reg32;

	reg32 = pci_read_config32(PCH_DEV_PMC, PWRMBASE);

	return (void *)(uintptr_t)ALIGN_DOWN(reg32, 4 * KiB);
}

uint16_t smbus_tco_regs(void)
{
	uint16_t reg16;

	reg16 = pci_read_config16(PCH_DEV_SMBUS, TCOBASE);

	return ALIGN_DOWN(reg16, 0x20);
}

uint32_t soc_reset_tco_status(void)
{
	u16 tco1_sts;
	u16 tco2_sts;
	u16 tcobase;

	tcobase = smbus_tco_regs();

	/* TCO Status 2 register */
	tco2_sts = inw(tcobase + TCO2_STS);
	tco2_sts |= TCO2_STS_SECOND_TO;
	outw(tco2_sts, tcobase + TCO2_STS);

	/* TCO Status 1 register */
	tco1_sts = inw(tcobase + TCO1_STS);

	/* Clear SECOND_TO_STS bit */
	if (tco2_sts & TCO2_STS_SECOND_TO)
		outw(tco2_sts & ~TCO2_STS_SECOND_TO, tcobase + TCO2_STS);

	return (tco2_sts << 16) | tco1_sts;
}

uintptr_t soc_read_pmc_base(void)
{
	return (uintptr_t)pmc_mmio_regs();
}

void soc_get_gpe_configs(uint8_t *dw0, uint8_t *dw1, uint8_t *dw2)
{
	DEVTREE_CONST struct soc_intel_cannonlake_config *config;

	/* Look up the device in devicetree */
	DEVTREE_CONST struct device *dev = dev_find_slot(0, PCH_DEVFN_PMC);
	if (!dev || !dev->chip_info) {
		printk(BIOS_ERR, "BUG! Could not find SOC devicetree config\n");
	}
	config = dev->chip_info;

	/* Assign to out variable */
	*dw0 = config->gpe0_dw0;
	*dw1 = config->gpe0_dw1;
	*dw2 = config->gpe0_dw2;
}
