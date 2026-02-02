#pragma once

#include "drivers/aarch64_system_reg.hh"
#include "print.hh"
#include "stm32mp2xx_hal.h"

struct DmaHelper {

	// Must manually set the following fields:
	// - hdma.Instance
	// - hdma.Init.Request
	// - hdma.Init.Direction
	// - node_conf.SrcAddress
	// - node_conf.DstAddress
	// - node_conf.DataSize
	// Request and Direction must be set BEFORE calling setup_circular
	static void setup_circular(DMA_HandleTypeDef &hdma,
							   DMA_QListTypeDef &queue,
							   DMA_NodeTypeDef &node1,
							   DMA_NodeConfTypeDef &node_conf)
	{
		hdma.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
		hdma.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_WORD;
		hdma.Init.DestDataWidth = DMA_DEST_DATAWIDTH_WORD;
		hdma.Init.Priority = DMA_LOW_PRIORITY_HIGH_WEIGHT;
		hdma.Init.SrcBurstLength = 1;
		hdma.Init.DestBurstLength = 1;
		hdma.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
		hdma.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
		hdma.Init.Mode = DMA_NORMAL;

		if (hdma.Init.Direction == DMA_PERIPH_TO_MEMORY) {
			hdma.Init.SrcInc = DMA_SINC_FIXED;
			hdma.Init.DestInc = DMA_DINC_INCREMENTED;
		} else if (hdma.Init.Direction == DMA_MEMORY_TO_PERIPH) {
			hdma.Init.SrcInc = DMA_SINC_INCREMENTED;
			hdma.Init.DestInc = DMA_DINC_FIXED;
		} else if (hdma.Init.Direction == DMA_MEMORY_TO_MEMORY) {
			hdma.Init.SrcInc = DMA_SINC_INCREMENTED;
			hdma.Init.DestInc = DMA_DINC_INCREMENTED;
		}

		hdma.InitLinkedList.Priority = DMA_LOW_PRIORITY_HIGH_WEIGHT;
		hdma.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
		hdma.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT1;
		hdma.InitLinkedList.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
		hdma.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;

		queue.State = HAL_DMA_QUEUE_STATE_RESET;
		queue.Type = 0;
		queue.Head = NULL;
		queue.FirstCircularNode = NULL;
		queue.NodeNumber = 0;

		node_conf.NodeType = DMA_HPDMA_LINEAR_NODE;
		node_conf.Init.Request = hdma.Init.Request;
		node_conf.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
		node_conf.Init.Direction = hdma.Init.Direction;
		node_conf.Init.SrcInc = hdma.Init.SrcInc;
		node_conf.Init.DestInc = hdma.Init.DestInc;
		node_conf.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_WORD;
		node_conf.Init.DestDataWidth = DMA_DEST_DATAWIDTH_WORD;
		node_conf.Init.SrcBurstLength = 1;
		node_conf.Init.DestBurstLength = 1;
		node_conf.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
		node_conf.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
		node_conf.Init.Mode = DMA_NORMAL;
		node_conf.RepeatBlockConfig.RepeatCount = 1;
		node_conf.RepeatBlockConfig.SrcAddrOffset = 0;
		node_conf.RepeatBlockConfig.DestAddrOffset = 0;
		node_conf.RepeatBlockConfig.BlkSrcAddrOffset = 0;
		node_conf.RepeatBlockConfig.BlkDestAddrOffset = 0;
		node_conf.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
		node_conf.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
		node_conf.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;

		if (HAL_DMAEx_List_BuildNode(&node_conf, &node1) != HAL_OK)
			print("Error: HAL_DMAEx_List_BuildNode\n");

		if (HAL_DMAEx_List_InsertNode_Tail(&queue, &node1) != HAL_OK)
			print("Error: HAL_DMAEx_List_InsertNode\n");

		if (HAL_DMAEx_List_SetCircularMode(&queue) != HAL_OK)
			print("Error: HAL_DMAEx_List_SetCircularMode\n");

		HAL_DMAEx_List_ConvertQToDynamic(&queue);

		if (auto res = HAL_DMAEx_List_Init(&hdma); res != HAL_OK) {
			print("ERROR: HAL_DMA_Init returned ", res, "\n");
		}

		if (HAL_DMAEx_List_LinkQ(&hdma, &queue) != HAL_OK)
			print("Error: HAL_DMAEx_List_LinkQ\n");

		clean_dcache_address((uintptr_t)(&node1));
		clean_dcache_address((uintptr_t)(&queue));

		if (auto res = HAL_DMA_ConfigChannelAttributes(&hdma, DMA_CHANNEL_NPRIV); res != HAL_OK) {
			print("ERROR: HAL_DMA_ConfigChannelAttributes returned ", res, "\n");
		}
	}
};
