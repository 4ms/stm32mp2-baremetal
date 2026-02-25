#pragma once

void sysram_enable();
void sram1_enable();
void sram3_enable();
void retram_enable();
void vderam_enable();
extern "C" void block_ram_enable_el3();
