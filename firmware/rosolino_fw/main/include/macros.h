#ifndef MACROS_H
#define MACROS_H

// esp headers
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RCCHECK(fn)                                                            \
  {                                                                            \
    rcl_ret_t temp_rc = fn;                                                    \
    if ((temp_rc != RCL_RET_OK)) {                                             \
      printf("Failed status on %s:%d: %d. Aborting.\n", __PRETTY_FUNCTION__,   \
             __LINE__, (int)temp_rc);                                          \
      vTaskDelete(NULL);                                                       \
    }                                                                          \
  }

#define RCSOFTCHECK(fn)                                                        \
  {                                                                            \
    rcl_ret_t temp_rc = fn;                                                    \
    if ((temp_rc != RCL_RET_OK)) {                                             \
      printf("Failed status on %s:%d: %d. Continuing.\n", __PRETTY_FUNCTION__, \
             __LINE__, (int)temp_rc);                                          \
    }                                                                          \
  }

#endif // MACROS_H
