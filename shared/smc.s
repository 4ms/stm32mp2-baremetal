.global __arm_smccc_smc;
.type __arm_smccc_smc_function;
.align 6;

	.cfi_startproc
	smc	#0
	ldr	x4, [sp]
	stp	x0, x1, [x4, #ARM_SMCCC_RES_X0_OFFS]
	stp	x2, x3, [x4, #ARM_SMCCC_RES_X2_OFFS]
	ldr	x4, [sp, #8]
	cbz	x4, 1f /* no quirk structure */
	ldr	x9, [x4, #ARM_SMCCC_QUIRK_ID_OFFS]
	cmp	x9, #ARM_SMCCC_QUIRK_QCOM_A6
	b.ne	1f
	str	x6, [x4, ARM_SMCCC_QUIRK_STATE_OFFS]
1:	ret
	.cfi_endproc


.type __arm_smccc_smc 2
.size __arm_smccc_smc, .-__arm_smccc_smc


