
#include "zigbee.h"

#include "axolotl_state.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_zigbee.h"
#include "ezbee/af.h"
#include "ezbee/bdb.h"
#include "ezbee/zcl/cluster/basic_desc.h"
#include "ezbee/zcl/cluster/custom.h"
#include "ezbee/zcl/cluster/identify_desc.h"
#include "ezbee/zcl/zcl_desc.h"
#include "ezbee/zcl/zcl_type.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "target_state.h"

static const char *TAG = "AXOLOTL-STATE-TEST";

static const AXOLOTL_COLOR mode_to_color[] = {
    [AXO_MODE_WHITE] = WHITE,   [AXO_MODE_YELLOW] = YELLOW,
    [AXO_MODE_ORANGE] = ORANGE, [AXO_MODE_PINK] = PINK,
    [AXO_MODE_BLUE] = BLUE,
};
static bool s_on = false;
static uint8_t s_mode = AXO_MODE_WHITE;

static void axolotl_apply(void) {
  if (!s_on) {
    set_target_state(OFF);
  } else if (s_mode == AXO_MODE_CYCLING) {
    set_target_state(CYCLING);
  } else if (s_mode < AXO_MODE_CYCLING) {
    set_target_state(STATIC);
    set_target_color(mode_to_color[s_mode]);
  }
  ESP_LOGI(TAG, "zigbee -> on=%d mode=%d", s_on, s_mode);
}

static void axolotl_on_attr_write(uint8_t ep_id, uint16_t attr_id,
                                  void *new_value, uint16_t manuf_code) {
  if (ep_id != AXOLOTL_ENDPOINT)
    return;

  if (attr_id == AXOLOTL_ATTR_ON_OFF)
    s_on = *(uint8_t *)new_value;
  else if (attr_id == AXOLOTL_ATTR_MODE)
    s_mode = *(uint8_t *)new_value;
  else
    return;

  axolotl_apply();
}

static void axolotl_cluster_init(uint8_t ep_id) {
  ezb_zcl_custom_cluster_handlers_t h = {
      .cluster_id = AXOLOTL_CLUSTER_ID,
      .cluster_role = EZB_ZCL_CLUSTER_SERVER,
      .write_attr_cb = axolotl_on_attr_write,
  };
  ezb_zcl_custom_cluster_handlers_register(&h);
}

static esp_timer_handle_t s_retry_timer;
static ezb_bdb_comm_mode_mask_t s_retry_mode;

static void commissioning_retry_cb(void *arg) {
  // esp_timer task context -> take Zigbee lock before touching the stack
  esp_zigbee_lock_acquire(portMAX_DELAY);
  ezb_bdb_start_top_level_commissioning(s_retry_mode);
  esp_zigbee_lock_release();
}

static void schedule_commisioning_retry(ezb_bdb_comm_mode_mask_t mode,
                                        uint32_t delay_ms) {
  s_retry_mode = mode;
  if (s_retry_timer == NULL) {
    const esp_timer_create_args_t args = {
        .callback = commissioning_retry_cb,
        .name = "zb_retry",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_retry_timer));
  }
  esp_timer_stop(s_retry_timer); // cancel a pending retry
  ESP_ERROR_CHECK(
      esp_timer_start_once(s_retry_timer, (uint64_t)delay_ms * 1000));
}

static esp_err_t create_axolotl_device(void) {
  ezb_af_device_desc_t dev = ezb_af_create_device_desc();

  // basic
  ezb_zcl_basic_cluster_server_config_t basic_cfg = {
      .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
      .power_source = EZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,
  };
  ezb_zcl_cluster_desc_t basic =
      ezb_zcl_basic_create_cluster_desc(&basic_cfg, EZB_ZCL_CLUSTER_SERVER);
  ezb_zcl_basic_cluster_desc_add_attr(basic,
                                      EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                      (void *)AXOLOTL_MANUFACTURER_NAME);
  ezb_zcl_basic_cluster_desc_add_attr(basic,
                                      EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                      (void *)AXOLOTL_MODEL_IDENTIFIER);

  // identify
  ezb_zcl_identify_cluster_server_config_t id_cfg = {
      .identify_time = EZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,
  };
  ezb_zcl_cluster_desc_t identify =
      ezb_zcl_identify_create_cluster_desc(&id_cfg, EZB_ZCL_CLUSTER_SERVER);

  // custom axolotl cluster
  ezb_zcl_custom_cluster_config_t cc = {.cluster_id = AXOLOTL_CLUSTER_ID,
                                        .init_func = axolotl_cluster_init,
                                        .deinit_func = NULL};
  ezb_zcl_cluster_desc_t custom =
      ezb_zcl_custom_create_cluster_desc(&cc, EZB_ZCL_CLUSTER_SERVER);
  uint8_t on_def = 0, mode_def = AXO_MODE_WHITE;
  ezb_zcl_custom_cluster_desc_add_attr(
      custom, AXOLOTL_ATTR_ON_OFF, EZB_ZCL_ATTR_TYPE_BOOL,
      EZB_ZCL_ATTR_ACCESS_READ_WRITE | EZB_ZCL_ATTR_ACCESS_REPORTING, &on_def);
  ezb_zcl_custom_cluster_desc_add_attr(
      custom, AXOLOTL_ATTR_MODE, EZB_ZCL_ATTR_TYPE_ENUM8,
      EZB_ZCL_ATTR_ACCESS_READ_WRITE | EZB_ZCL_ATTR_ACCESS_REPORTING,
      &mode_def);

  // endpoint
  ezb_af_ep_config_t ep_cfg = {
      .ep_id = AXOLOTL_ENDPOINT,
      .app_profile_id = EZB_AF_HA_PROFILE_ID,
      .app_device_id = AXOLOTL_DEVICE_ID,
      .app_device_version = 0,
  };
  ezb_af_ep_desc_t ep = ezb_af_create_endpoint_desc(&ep_cfg);
  ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep, basic));
  ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep, identify));
  ESP_ERROR_CHECK(ezb_af_endpoint_add_cluster_desc(ep, custom));

  ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev, ep));
  ESP_ERROR_CHECK(ezb_af_device_desc_register(dev));
  return ESP_OK;
}

static bool app_signal_handler(const ezb_app_signal_t *app_signal) {
  ezb_app_signal_type_t signal_type = ezb_app_signal_get_type(app_signal);
  ezb_bdb_comm_status_t status;

  switch (signal_type) {
  case EZB_ZDO_SIGNAL_SKIP_STARTUP: // initialization
    ESP_LOGI(TAG, "Initialize Zigbee stack");
    ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
    break;

  case EZB_BDB_SIGNAL_DEVICE_FIRST_START: // has no saved network
  case EZB_BDB_SIGNAL_DEVICE_REBOOT:      // has saved network
  {
    status = *((ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal));
    if (status == EZB_BDB_STATUS_SUCCESS) {
      ESP_LOGI(TAG, "Device started up in%s factory-reset mode",
               ezb_bdb_is_factory_new() ? "" : " non");

      if (ezb_bdb_is_factory_new()) {
        ESP_LOGI(TAG, "Start network steering");
        ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
      } else {
        ESP_LOGI(TAG, "Device reboot");
      }
    } else {
      ESP_LOGW(TAG, "commissioning init failed (0x%02x), retrying", status);
      schedule_commisioning_retry(EZB_BDB_MODE_INITIALIZATION, 1000);
    }
  } break;

  case EZB_BDB_SIGNAL_STEERING: // Result of network steering
  {
    status = *((ezb_bdb_comm_status_t *)ezb_app_signal_get_params(app_signal));
    if (status == EZB_BDB_STATUS_SUCCESS) {
      ESP_LOGI(TAG, "Network steering completed");
    } else {
      ESP_LOGW(TAG, "network steering failed (0x%02x), retrying", status);
      schedule_commisioning_retry(EZB_BDB_MODE_NETWORK_STEERING, 1000);
    }
  } break;

  default:
    ESP_LOGI(TAG, "Zigbee APP Signal: %s(type: 0x%02x)",
             ezb_app_signal_to_string(signal_type), signal_type);
    break;
  }
  return true;
}

static esp_err_t setup_commissioning(void) {
  ezb_aps_secur_enable_distributed_security(false);
  ESP_ERROR_CHECK(ezb_bdb_set_primary_channel_set(ZIGBEE_CHANNEL_MASK));
  ESP_ERROR_CHECK(ezb_bdb_set_secondary_channel_set(ZIGBEE_CHANNEL_MASK));
  ESP_ERROR_CHECK(ezb_app_signal_add_handler(app_signal_handler));

  return ESP_OK;
}

static void zigbee_task(void *args) {
  esp_zigbee_config_t config = AXOLOTL_ZB_CONFIG();
  ESP_ERROR_CHECK(esp_zigbee_init(&config));
  ESP_ERROR_CHECK(setup_commissioning());
  ESP_ERROR_CHECK(create_axolotl_device());
  ESP_ERROR_CHECK(esp_zigbee_start(false));
  esp_zigbee_launch_mainloop();
  esp_zigbee_deinit();
  vTaskDelete(NULL);
}

void init_zigbee(void) {
  // init flash storage
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // create task
  xTaskCreate(zigbee_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
