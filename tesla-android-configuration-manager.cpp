#include <sstream>
#include <cstdlib>
#include <httplib.h>
#include <cJSON.h>
#include <cutils/properties.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

const char *BAND_TYPE_SYSTEM_PROPERTY_KEY = "persist.tesla-android.softap.band_type";
const char *CHANNEL_SYSTEM_PROPERTY_KEY = "persist.tesla-android.softap.channel";
const char *CHANNEL_WIDTH_SYSTEM_PROPERTY_KEY = "persist.tesla-android.softap.channel_width";
const char *IS_ENABLED_SYSTEM_PROPERTY_KEY = "persist.tesla-android.softap.is_enabled";
const char *OFFLINE_MODE_IS_ENABLED_SYSTEM_PROPERTY_KEY = "persist.tesla-android.offline-mode.is_enabled";
const char *OFFLINE_MODE_TELEMETRY_IS_ENABLED_SYSTEM_PROPERTY_KEY = "persist.tesla-android.offline-mode.telemetry.is_enabled";
const char *OFFLINE_MODE_TESLA_FIRMWARE_DOWNLOADS_IS_ENABLED_SYSTEM_PROPERTY_KEY = "persist.tesla-android.offline-mode.tesla-firmware-downloads";
const char *VIRTUAL_DISPLAY_RESOLUTION_WIDTH_SYSTEM_PROPERTY_KEY = "persist.tesla-android.virtual-display.resolution.width";
const char *VIRTUAL_DISPLAY_RESOLUTION_HEIGHT_SYSTEM_PROPERTY_KEY = "persist.tesla-android.virtual-display.resolution.height";
const char *VIRTUAL_DISPLAY_DENSITY_SYSTEM_PROPERTY_KEY = "persist.tesla-android.virtual-display.density";
const char *VIRTUAL_DISPLAY_LOWRES_SYSTEM_PROPERTY_KEY = "persist.tesla-android.virtual-display.lowres";
const char *HEADLESS_CONFIG_IS_ENABLED_PROPERTY_KEY = "persist.drm_hwc.headless.is_enabled";
const char *HEADLESS_CONFIG_OVERRIDE_PROPERTY_KEY = "persist.drm_hwc.headless.config";
const char *HEADLESS_CONFIG_LATCH_PROPERTY_KEY = "persist.drm_hwc.latch";

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

void set_virtual_display_resolution_and_density(int width, int height, int density) {
  const char* binaryPath = "/system/bin/wm";

  std::ostringstream resolutionStream, densityStream;
  resolutionStream << width << "x" << height << "@30";

  densityStream << density;

  std::string resolutionStr = resolutionStream.str();
  std::string densityStr = densityStream.str();

  const char* resolution = resolutionStr.c_str();
  const char* densityCStr = densityStr.c_str();

  // Set density
  pid_t pid = fork();
  int status;
  if (pid == -1) {
    perror("fork failed");
    exit(-1);
  } else if (pid == 0) {
    execlp(binaryPath, binaryPath, "density", densityCStr, NULL);
    perror("execlp failed");
    exit(-1);
  }
  wait(&status);
  printf("child exit status: %d\n", WEXITSTATUS(status));

  // Unload virtual touchscreen module
  const char* rmmodPath = "/vendor/bin/rmmod";
  pid = fork();
  if (pid == -1) {
    perror("fork failed");
    exit(-1);
  } else if (pid == 0) {
    execlp(rmmodPath, rmmodPath, "virtual_touchscreen", NULL);
    perror("execlp failed");
    exit(-1);
  }
  wait(NULL);

  // Load virtual touchscreen
  const char* modprobePath = "/vendor/bin/modprobe";

  std::ostringstream absXMaxStream, absYMaxStream;
  absXMaxStream << "abs_x_max_param=" << width;
  absYMaxStream << "abs_y_max_param=" << height;
  std::string absXMaxParam = absXMaxStream.str();
  std::string absYMaxParam = absYMaxStream.str();

  const char* absXMaxCStr = absXMaxParam.c_str();
  const char* absYMaxCStr = absYMaxParam.c_str();

  pid = fork();
  if (pid == -1) {
    perror("fork failed");
    exit(-1);
  } else if (pid == 0) {
    execlp(modprobePath, modprobePath, "-d", "/vendor/lib/modules", "-a", "virtual_touchscreen", absXMaxCStr, absYMaxCStr, NULL);
    perror("execlp failed");
    exit(-1);
  }
  wait(&status);
  printf("child exit status: %d\n", WEXITSTATUS(status));

  // Check current headless resolution
  std::string headlessOverrideValueStr = std::string(get_system_property(HEADLESS_CONFIG_OVERRIDE_PROPERTY_KEY));
  if (headlessOverrideValueStr == resolutionStr) {
    printf("Headless override config unchanged");
  } else {
    printf("Headless override config needs update, triggering the lath");
    property_set(HEADLESS_CONFIG_OVERRIDE_PROPERTY_KEY, resolution);
    property_set(HEADLESS_CONFIG_LATCH_PROPERTY_KEY, "1");
  }
}

int get_cpu_temperature() {
    FILE* file = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (file == NULL) {
        printf("Failed to open the file.\n");
        return -1;
    }

    int temperature;
    fscanf(file, "%d", &temperature);

    fclose(file);

    int temperatureCelsius = temperature / 1000;
    return temperatureCelsius;
}

void start_softap() {
  // Switch from channel 36 to 44
  int channel = get_system_property_int(CHANNEL_SYSTEM_PROPERTY_KEY);
  if(channel == 36) {
    property_set(CHANNEL_SYSTEM_PROPERTY_KEY, "44");
  }
  // Global channels are used, region is set only to enable Wi-Fi 5
  system("iw reg set US");
  sleep(1);
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

  server.Get("/api/health", [](const httplib::Request& req, httplib::Response& res) {
    cJSON* json = cJSON_CreateObject();

    add_number_property(json, "cpu_temperature", get_cpu_temperature(),  res);

    char* json_str = cJSON_Print(json);

    res.set_header("Content-Type", "application/json");
    res.set_content(json_str, "application/json");
    res.status = 200;

    cJSON_Delete(json);
    free(json_str);
  });

  server.Options("/api/health", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Get("/api/configuration", [](const httplib::Request& req, httplib::Response& res) {
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
    res.status = 200;

    cJSON_Delete(json);
    free(json_str);
  });

  server.Options("/api/configuration", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/api/softApBand", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(BAND_TYPE_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/softApBand", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/api/softApChannel", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(CHANNEL_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/softApChannel", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/api/softApChannelWidth", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(CHANNEL_WIDTH_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/softApChannelWidth", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/api/softApState", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(IS_ENABLED_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/softApState", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/api/offlineModeState", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(OFFLINE_MODE_IS_ENABLED_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/offlineModeState", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/api/offlineModeTelemetryState", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(OFFLINE_MODE_TELEMETRY_IS_ENABLED_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/offlineModeTelemetryState", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Post("/api/offlineModeTeslaFirmwareDownloads", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(OFFLINE_MODE_TESLA_FIRMWARE_DOWNLOADS_IS_ENABLED_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/offlineModeTeslaFirmwareDownloads", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
   });

  server.Get("/api/displayState", [](const httplib::Request& req, httplib::Response& res) {
    cJSON* json = cJSON_CreateObject();

    add_number_property(json, "width", get_system_property_int(VIRTUAL_DISPLAY_RESOLUTION_WIDTH_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, "height", get_system_property_int(VIRTUAL_DISPLAY_RESOLUTION_HEIGHT_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, "density", get_system_property_int(VIRTUAL_DISPLAY_DENSITY_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, "lowres", get_system_property_int(VIRTUAL_DISPLAY_LOWRES_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, "isHeadless", get_system_property_int(HEADLESS_CONFIG_IS_ENABLED_PROPERTY_KEY), res);

    char* json_str = cJSON_Print(json);

    res.set_header("Content-Type", "application/json");
    res.set_content(json_str, "application/json");
    res.status = 200;

    cJSON_Delete(json);
    free(json_str);
  });

  server.Post("/api/displayState", [](const httplib::Request& req, httplib::Response& res) {
    cJSON* json = cJSON_Parse(req.body.c_str());

    if (json == nullptr) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != nullptr) {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        handle_error(res);
        return;
    }

    cJSON* width = cJSON_GetObjectItemCaseSensitive(json, "width");
    cJSON* height = cJSON_GetObjectItemCaseSensitive(json, "height");
    cJSON* density = cJSON_GetObjectItemCaseSensitive(json, "density");
    cJSON* lowres = cJSON_GetObjectItemCaseSensitive(json, "lowres");

    if (!cJSON_IsNumber(width) || !cJSON_IsNumber(height) || !cJSON_IsNumber(density) || !cJSON_IsNumber(lowres)) {
        handle_error(res);
        cJSON_Delete(json);
        return;
    }

    int widthSetPropertyResult = property_set(VIRTUAL_DISPLAY_RESOLUTION_WIDTH_SYSTEM_PROPERTY_KEY, std::to_string(width->valueint).c_str());
    int heightSetPropertyResult = property_set(VIRTUAL_DISPLAY_RESOLUTION_HEIGHT_SYSTEM_PROPERTY_KEY, std::to_string(height->valueint).c_str());
    int densitySetPropertyResult = property_set(VIRTUAL_DISPLAY_DENSITY_SYSTEM_PROPERTY_KEY, std::to_string(density->valueint).c_str());
    int lowresSetPropertyResult = property_set(VIRTUAL_DISPLAY_LOWRES_SYSTEM_PROPERTY_KEY, std::to_string(lowres->valueint).c_str());

    if (widthSetPropertyResult == 0 && heightSetPropertyResult == 0 && densitySetPropertyResult == 0 && lowresSetPropertyResult == 0) {
        handle_post_success(res);
        set_virtual_display_resolution_and_density(width->valueint, height->valueint, density->valueint);
    } else {
        handle_error(res);
    }

    cJSON_Delete(json);
  });

  server.Options("/api/displayState", [](const httplib::Request& req, httplib::Response& res) {
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
