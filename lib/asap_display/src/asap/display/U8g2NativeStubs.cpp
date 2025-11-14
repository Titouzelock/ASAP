#ifdef ASAP_NATIVE
#include <stdint.h>
extern "C" {
#include <u8x8.h>
}

extern "C" uint8_t u8x8_gpio_and_delay_arduino(u8x8_t* u8x8,
                                               uint8_t msg,
                                               uint8_t arg_int,
                                               void* arg_ptr)
{
  (void)u8x8; (void)msg; (void)arg_int; (void)arg_ptr;
  return 1;
}

extern "C" uint8_t u8x8_byte_arduino_hw_spi(u8x8_t* u8x8,
                                            uint8_t msg,
                                            uint8_t arg_int,
                                            void* arg_ptr)
{
  (void)u8x8; (void)msg; (void)arg_int; (void)arg_ptr;
  return 1;
}
#endif

