#include <sstream>
#include <cstdlib>
#include <httplib.h>
#include <cJSON.h>
#include <cutils/properties.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>

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
const char *VIRTUAL_DISPLAY_RENDERER_SYSTEM_PROPERTY_KEY = "persist.tesla-android.virtual-display.renderer";
const char *VIRTUAL_DISPLAY_IS_RESPONSIVE_SYSTEM_PROPERTY_KEY = "persist.tesla-android.virtual-display.is_responsive";
const char *VIRTUAL_DISPLAY_IS_H264_SYSTEM_PROPERTY_KEY = "persist.tesla-android.virtual-display.is_h264";
const char *HEADLESS_CONFIG_IS_ENABLED_PROPERTY_KEY = "persist.drm_hwc.headless.is_enabled";
const char *HEADLESS_CONFIG_OVERRIDE_PROPERTY_KEY = "persist.drm_hwc.headless.config";
const char *HEADLESS_CONFIG_LATCH_PROPERTY_KEY = "persist.drm_hwc.latch";
const char *BROWSER_AUDIO_IS_ENABLED_SYSTEM_PROPERTY_KEY = "persist.tesla-android.browser_audio.is_enabled";
const char *BROWSER_AUDIO_VOLUME_SYSTEM_PROPERTY_KEY = "persist.tesla-android.browser_audio.volume";
const char *RELEASE_TYPE_SYSTEM_PROPERTY_KEY = "persist.tesla-android.releasetype";
const char *OTA_URL_SYSTEM_PROPERTY_KEY = "persist.tesla-android.updater.uri";

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

int is_usb_device_present(const char *vendor_id, const char *product_id) {
    const char *usb_devices_path = "/sys/bus/usb/devices/";
    DIR *dir;
    struct dirent *entry;

    dir = opendir(usb_devices_path);
    if (!dir) {
        perror("Could not open /sys/bus/usb/devices");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL) {
        char vendor_file_path[256];
        char product_file_path[256];
        char vid[5], pid[5];

        snprintf(vendor_file_path, sizeof(vendor_file_path), "%s%s/idVendor", usb_devices_path, entry->d_name);
        snprintf(product_file_path, sizeof(product_file_path), "%s%s/idProduct", usb_devices_path, entry->d_name);

        FILE *vendor_file = fopen(vendor_file_path, "r");
        FILE *product_file = fopen(product_file_path, "r");

        if (!vendor_file || !product_file) {
            if (vendor_file) fclose(vendor_file);
            if (product_file) fclose(product_file);
            continue;
        }

        fgets(vid, sizeof(vid), vendor_file);
        fgets(pid, sizeof(pid), product_file);

        fclose(vendor_file);
        fclose(product_file);

        vid[strcspn(vid, "\n")] = 0;
        pid[strcspn(pid, "\n")] = 0;

        if (strcmp(vid, vendor_id) == 0 && strcmp(pid, product_id) == 0) {
            closedir(dir);
            return 1;
        }
    }

    closedir(dir);
    return 0;
}

int is_port_open(const char *ip, int port) {
    struct sockaddr_in address;
    int sockfd, status;
    struct timeval timeout;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    status = connect(sockfd, (struct sockaddr *)&address, sizeof(address));
    close(sockfd);

    return status == 0;
}

int does_interface_exist(const char *interface_name) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ);

    int result = ioctl(sockfd, SIOCGIFINDEX, &ifr);
    close(sockfd);

    return result != -1;
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

  //Disable old overrides
  pid_t pid = fork();
  int status;
  if (pid == -1) {
    perror("fork failed");
    exit(-1);
  } else if (pid == 0) {
    execlp(binaryPath, binaryPath, "size", "reset", NULL);
    perror("execlp failed");
    exit(-1);
  }
  wait(&status);
  printf("child exit status: %d\n", WEXITSTATUS(status));

  // Set density
  pid = fork();
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
  int isHeadless = get_system_property_int(HEADLESS_CONFIG_IS_ENABLED_PROPERTY_KEY);
  if (headlessOverrideValueStr == resolutionStr) {
    printf("Headless override config unchanged");
  } if (isHeadless == 0) {
    printf("Not in headless mode, resize not needed");
  } else {
    printf("Headless override config needs update, triggering the lath");
    system("stop tesla-android-virtual-display");
    property_set(HEADLESS_CONFIG_OVERRIDE_PROPERTY_KEY, resolution);
    property_set(HEADLESS_CONFIG_LATCH_PROPERTY_KEY, "1");
    sleep(1);
    system("start tesla-android-virtual-display");
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

char* get_serial_number() {
    uint32_t serial = 0;
    static char serialStr[17];

    FILE *fp = fopen("/sys/firmware/devicetree/base/serial-number", "r");

    if (fp == NULL) {
        perror("/sys/firmware/devicetree/base/serial-number");
        exit(EXIT_FAILURE);
    }

    if (fscanf(fp, "%x", &serial) != 1) {
        perror("Failed to read serial number");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    fclose(fp);

    snprintf(serialStr, sizeof(serialStr), "%08x", serial);
    return serialStr;
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
  //if(get_system_property_int(IS_ENABLED_SYSTEM_PROPERTY_KEY) == 1) {
    start_softap();
  //}
}

void set_initial_display_resolution() {
  int width = get_system_property_int(VIRTUAL_DISPLAY_RESOLUTION_WIDTH_SYSTEM_PROPERTY_KEY);
  int height = get_system_property_int(VIRTUAL_DISPLAY_RESOLUTION_HEIGHT_SYSTEM_PROPERTY_KEY);
  int density = get_system_property_int(VIRTUAL_DISPLAY_DENSITY_SYSTEM_PROPERTY_KEY);
  set_virtual_display_resolution_and_density(width, height, density);
}

int main() {
  httplib::Server server;

  start_softap_if_enabled();

  set_initial_display_resolution();

  server.Get("/api/openUpdater", [](const httplib::Request& req, httplib::Response& res) {
    system("am start -a android.settings.SYSTEM_UPDATE_SETTINGS");
    res.status = 200;
  });

  server.Options("/api/openUpdater", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Get("/api/deviceInfo", [](const httplib::Request& req, httplib::Response& res) {
    cJSON* json = cJSON_CreateObject();

    int modem_status = (is_port_open("192.168.1.1", 80) || is_port_open("192.168.8.1", 80)) && (does_interface_exist("eth1") || does_interface_exist("eth2"));
    int carplay_status = is_usb_device_present("1314", "1520") || is_usb_device_present("1314", "1521");

    add_number_property(json, "cpu_temperature", get_cpu_temperature(), res);
    add_string_property(json, "serial_number", get_serial_number(), res);
    add_string_property(json, "device_model", get_system_property("ro.product.model"), res);
    add_number_property(json, "is_modem_detected", modem_status, res);
    add_number_property(json, "is_carplay_detected", carplay_status, res);
    add_string_property(json, "release_type", get_system_property(RELEASE_TYPE_SYSTEM_PROPERTY_KEY), res);
    add_string_property(json, "ota_url", get_system_property(OTA_URL_SYSTEM_PROPERTY_KEY), res);

    char* json_str = cJSON_Print(json);

    res.set_header("Content-Type", "application/json");
    res.set_content(json_str, "application/json");
    res.status = 200;

    cJSON_Delete(json);
    free(json_str);
  });

  server.Options("/api/deviceInfo", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });

  server.Get("/api/health", [](const httplib::Request& req, httplib::Response& res) {
    res.status = 200;
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
    add_number_property(json, BROWSER_AUDIO_IS_ENABLED_SYSTEM_PROPERTY_KEY, get_system_property_int(BROWSER_AUDIO_IS_ENABLED_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, BROWSER_AUDIO_VOLUME_SYSTEM_PROPERTY_KEY, get_system_property_int(BROWSER_AUDIO_VOLUME_SYSTEM_PROPERTY_KEY), res);

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
  
    server.Post("/api/overrideReleaseType", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(RELEASE_TYPE_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/overrideReleaseType", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });
  
    server.Post("/api/overrideOtaUrl", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(OTA_URL_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/overrideOtaUrl", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });
    
  server.Post("/api/browserAudioState", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(BROWSER_AUDIO_IS_ENABLED_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/browserAudioState", [](const httplib::Request& req, httplib::Response& res) {
    handle_preflight(res);
  });
    
  server.Post("/api/browserAudioVolume", [](const httplib::Request& req, httplib::Response& res) {
    const char* new_value = req.body.c_str();
    int result = property_set(BROWSER_AUDIO_VOLUME_SYSTEM_PROPERTY_KEY, new_value);
    if (result == 0) {
        handle_post_success(res);
    } else {
        handle_error(res);
    }
  });

  server.Options("/api/browserAudioVolume", [](const httplib::Request& req, httplib::Response& res) {
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
    add_number_property(json, "renderer", get_system_property_int(VIRTUAL_DISPLAY_RENDERER_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, "isResponsive", get_system_property_int(VIRTUAL_DISPLAY_IS_RESPONSIVE_SYSTEM_PROPERTY_KEY), res);
    add_number_property(json, "isH264", get_system_property_int(VIRTUAL_DISPLAY_IS_H264_SYSTEM_PROPERTY_KEY), res);
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
    cJSON* renderer = cJSON_GetObjectItemCaseSensitive(json, "renderer");
    cJSON* isResponsive = cJSON_GetObjectItemCaseSensitive(json, "isResponsive");
    cJSON* isH264 = cJSON_GetObjectItemCaseSensitive(json, "isH264");

    if (!cJSON_IsNumber(width) || !cJSON_IsNumber(height) || !cJSON_IsNumber(density) || !cJSON_IsNumber(lowres) || !cJSON_IsNumber(renderer) || !cJSON_IsNumber(isResponsive) || !cJSON_IsNumber(isH264)) {
        handle_error(res);
        cJSON_Delete(json);
        return;
    }

    int widthSetPropertyResult = property_set(VIRTUAL_DISPLAY_RESOLUTION_WIDTH_SYSTEM_PROPERTY_KEY, std::to_string(width->valueint).c_str());
    int heightSetPropertyResult = property_set(VIRTUAL_DISPLAY_RESOLUTION_HEIGHT_SYSTEM_PROPERTY_KEY, std::to_string(height->valueint).c_str());
    int densitySetPropertyResult = property_set(VIRTUAL_DISPLAY_DENSITY_SYSTEM_PROPERTY_KEY, std::to_string(density->valueint).c_str());
    int lowresSetPropertyResult = property_set(VIRTUAL_DISPLAY_LOWRES_SYSTEM_PROPERTY_KEY, std::to_string(lowres->valueint).c_str());
    int rendererSetPropertyResult = property_set(VIRTUAL_DISPLAY_RENDERER_SYSTEM_PROPERTY_KEY, std::to_string(renderer->valueint).c_str());
    int isResponsiveSetPropertyResult = property_set(VIRTUAL_DISPLAY_IS_RESPONSIVE_SYSTEM_PROPERTY_KEY, std::to_string(isResponsive->valueint).c_str());
    int isH264SetPropertyResult = property_set(VIRTUAL_DISPLAY_IS_H264_SYSTEM_PROPERTY_KEY, std::to_string(isH264->valueint).c_str());

    if (widthSetPropertyResult == 0 && heightSetPropertyResult == 0 && densitySetPropertyResult == 0 && lowresSetPropertyResult == 0 && rendererSetPropertyResult == 0 && isResponsiveSetPropertyResult == 0 && 
isH264SetPropertyResult == 0) {
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
