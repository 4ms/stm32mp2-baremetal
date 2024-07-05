#include <cstdint>

int main() {
  volatile uint32_t *uart = reinterpret_cast<volatile uint32_t *>(0x400E0028);
  *uart = 'A';
  *uart = 'B';
  *uart = 'C';

  while (true) {
    *uart = 'D';
  }
}
