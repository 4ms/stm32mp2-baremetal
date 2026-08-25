#pragma once
#include <cstdint>

// NV3052C configuration over 9-bit SPI, bit-banged on GPIOs.
// For simplicity, pins are hardwired in nv3052c.cc.
uint32_t nv3052c_init();

void nv3052c_write_cmd(uint8_t cmd);
void nv3052c_write_data(uint8_t data);
void nv3052c_write_reg(uint8_t reg, uint8_t val); // cmd + one data byte

uint8_t nv3052c_read_reg(uint8_t reg);
