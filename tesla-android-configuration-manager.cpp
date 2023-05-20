#include <cstdlib>
#include <httplib.h>
#include <cJSON.h>
#include <cutils/properties.h>

const char *BAND_TYPE_SYSTEM_PROPERTY_KEY = "persist.tesla-android.softap.band_type";
const char *CHANNEL_SYSTEM_PROPERTY_KEY = "persist.tesla-android.softap.channel";
const char *CHANNEL_WIDTH_SYSTEM_PROPERTY_KEY = "persist.tesla-android.softap.channel_width";
const char *IS_ENABLED_SYSTEM_PROPERTY_KEY = "persist.tesla-android.softap.is_enabled";
const char *OFFLINE_MODE_IS_ENABLED_SYSTEM_PROPERTY_KEY = "persist.tesla-android.offline-mode.is_enabled";
const char *OFFLINE_MODE_TELEMETRY_IS_ENABLED_SYSTEM_PROPERTY_KEY = "persist.tesla-android.offline-mode.telemetry.is_enabled";
const char *OFFLINE_MODE_TESLA_FIRMWARE_DOWNLOADS_IS_ENABLED_SYSTEM_PROPERTY_KEY = "persist.tesla-android.offline-mode.tesla-firmware-downloads";
 
int get_system_property_int(const char* prop_name) {
  char prop_value[PROPERTY_VALUE_MAX];
  if (property_get(prop_name, prop_value, nullptr) > 0) {
    return atoi(prop_value);
  } else {
    return -1;
  }
}

const char* get_system_property(const char* prop_name) {
  static char prop_value[PROPERTY_VALUE_MAX];
  if (property_get(prop_name, prop_value, nullptr) > 0) {
    return prop_value;
  } else {
    return nullptr;
  }
}

void handle_post_success(httplib::Response& res) {
  res.status = 200;
  res.set_content("OK", "text/plain");
}

void handle_preflight(httplib::Response& res) {
  res.set_header("Access-Control-Max-Age", "1728000");
  res.set_header("Content-Length", "0");
  res.set_header("Content-Type", "text/plain; charset=utf-8");
  res.status = 204;
}

void handle_error(httplib::Response& res) {
  res.status = 500;
  res.set_content("Internal Server Error", "text/plain");
}

void add_string_property(cJSON* json, const char* prop_name, const char* prop_value, httplib::Response& res) {
  if (prop_value == NULL) {
    handle_error(res);
    return;
  }
  cJSON_AddStringToObject(json, prop_name, prop_value);
}

void add_number_property(cJSON* json, const char* prop_name, int prop_value, httplib::Response& res) {
  cJSON_AddNumberToObject(json, prop_name, prop_value);
}

void start_softap() {
  system("cmd wifi start-softap-with-existing-config");
}

void stop_softap() {
  system("cmd wifi stop-softap");
}

void start_softap_if_enabled() {
  if(get_system_property_int(IS_ENABLED_SYSTEM_PROPERTY_KEY) == 1) {
    start_softap();
  }
}

int main() {
  httplib::Server server;

  start_softap_if_enabled();

  server.Get("/health", [](const httplib::Request& req, httplib::Response& res) {
    res.set_content("OK", "text/plain");
  });

  server.Get("/configuration", [](const httplib::Request& req, httplib::Response& res) {
    cJSON* json = cJSON_CreateObject();

    add_number_property(json, BAND_TYPE_SYSTEM_PROPERTY_KEY, get_system_property_int(BAND_TYPE_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, CHANNEL_SYSTEM_PROPERTY_KEY, get_system_property_int(CHANNEL_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, CHANNEL_WIDTH_SYSTEM_PROPERTY_KEY, get_system_property_int(CHANNEL_WIDTH_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, IS_ENABLED_SYSTEM_PROPERTY_KEY, get_system_property_int(IS_ENABLED_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, OFFLINE_MODE_IS_ENABLED_SYSTEM_PROPERTY_KEY, get_system_property_int(OFFLINE_MODE_IS_ENABLED_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, OFFLINE_MODE_TELEMETRY_IS_ENABLED_SYSTEM_PROPERTY_KEY, get_system_property_int(OFFLINE_MODE_TELEMETRY_IS_ENABLED_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, OFFLINE_MODE_TESLA_FIRMWARE_DOWNLOADS_IS_ENABLED_SYSTEM_PROPERTY_KEY, get_system_property_int(OFFLINE_MODE_TESLA_FIRMWARE_DOWNLOADS_IS_ENABLED_SYSTEM_PROPERTY_KEY), res);

    char* json_str = cJSON_Print(json);

    res.set_header("Content-Type", "application/json");
    res.set_content(json_str, "application/json");

    cJSON_Delete(json);
    free(json_str);
  });

  server.Post("/softApBand", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(BAND_TYPE_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/softApBand", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/softApChannel", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(CHANNEL_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/softApChannel", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/softApChannelWidth", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(CHANNEL_WIDTH_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/softApChannelWidth", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/softApState", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(IS_ENABLED_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/softApState", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });
  
  server.Post("/offlineModeState", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(OFFLINE_MODE_IS_ENABLED_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/offlineModeState", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/offlineModeTelemetryState", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(OFFLINE_MODE_TELEMETRY_IS_ENABLED_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/offlineModeTelemetryState", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/offlineModeTeslaFirmwareDownloads", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(OFFLINE_MODE_TESLA_FIRMWARE_DOWNLOADS_IS_ENABLED_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/offlineModeTeslaFirmwareDownloads", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
   });

  server.set_post_routing_handler([](const auto& req, auto& res) {
    res.set_header("Allow", "GET, POST, HEAD, OPTIONS");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Headers", "X-Requested-With, Content-Type, Accept, Origin, Authorization");
    res.set_header("Access-Control-Allow-Methods", "OPTIONS, GET, POST, HEAD");
  });

  server.listen("0.0.0.0", 8081);
  return 0;
}
