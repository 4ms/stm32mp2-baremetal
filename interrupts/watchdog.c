void watchdog(void)
{
	*(volatile uint32_t *)(0x44010000) = 0x0000AAAA;
	*(volatile uint32_t *)(0x44020000) = 0x0000AAAA;
	*(volatile uint32_t *)(0x44030000) = 0x0000AAAA;
	*(volatile uint32_t *)(0x44040000) = 0x0000AAAA;
}
