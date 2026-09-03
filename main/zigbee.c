
#include "zigbee.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_zigbee.h"
#include "ezbee/bdb.h"
#include "ezbee/zha.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"

static const char *TAG = "AXOLOTL-STATE-TEST";

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
static void axolotl_zcl_action_handler(ezb_zcl_core_action_callback_id_t cb_id,
                                       void *message) {
  if (cb_id == EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID) {
    ezb_zcl_set_attr_value_message_t *m = message;
    if (m->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF) {
      bool on = *(uint8_t *)m->in.attribute.data.value;
      ESP_LOGI(TAG, "On/Off command: %d", on);
      // TODO: map to axolotl state (on -> non-OFF, off -> OFF)
    }
  }
}

static esp_err_t create_axolotl_device(void) {
  ezb_af_device_desc_t dev_desc =
      ezb_af_create_device_desc(); // container for endpoints
  ezb_zha_on_off_light_config_t light_cfg = EZB_ZHA_ON_OFF_LIGHT_CONFIG();
  ezb_af_ep_desc_t ep_desc =
      ezb_zha_create_on_off_light(AXOLOTL_ENDPOINT, &light_cfg);

  ezb_zcl_cluster_desc_t basic_desc = ezb_af_endpoint_get_cluster_desc(
      ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  ezb_zcl_basic_cluster_desc_add_attr(basic_desc,
                                      EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                      (void *)AXOLOTL_MANUFACTURER_NAME);
  ezb_zcl_basic_cluster_desc_add_attr(basic_desc,
                                      EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                      (void *)AXOLOTL_MODEL_IDENTIFIER);
  ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(dev_desc, ep_desc));
  ESP_ERROR_CHECK(ezb_af_device_desc_register(dev_desc));

  ezb_zcl_core_action_handler_register(axolotl_zcl_action_handler);

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
