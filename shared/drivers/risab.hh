#include "rcc.hh"
#include "stm32mp2xx.h"

void full_access_risab(RISAB_TypeDef *risab)
{
	risab->CR = risab->CR & ~(RISAB_CR_SRWIAD);

	for (auto i = 0; i < 32; i++) {
		risab->PGSECCFGR[i] = 0UL;
		risab->PGPRIVCFGR[i] = 0UL;
		risab->PGCIDCFGR[i] = 0UL;
	}
	for (auto cid = 0; cid < 7; cid++) {
		risab->CID[cid].PRIVCFGR = 0;
		risab->CID[cid].RDCFGR = 0;
		risab->CID[cid].WRCFGR = 0;
	}
}

void full_access_sysram()
{
	RCC_Enable::SYSRAM_::set();
	full_access_risab(RISAB1);
	full_access_risab(RISAB2);
}

void full_access_sram1()
{
	RCC_Enable::SRAM1_::set();
	full_access_risab(RISAB3);
}

void full_access_sram2()
{
	RCC_Enable::SRAM2_::set();
	full_access_risab(RISAB4);
}

void full_access_retram()
{
	RCC_Enable::RETRAM_::set();
	full_access_risab(RISAB5);
}

void full_access_vderam()
{
	RCC_Enable::VDERAM_::set();
	full_access_risab(RISAB6);
}
