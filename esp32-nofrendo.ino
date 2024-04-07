/* Arduino Nofrendo
 * Please check hw_config.h and display.cpp for configuration details
 */


#define USB_DISK
//for usb mass storage
#ifdef USB_DISK
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"
#include "ffconf.h"
#include "esp_vfs.h"
#include "errno.h"
#include "hal/usb_hal.h"
#include "driver/gpio.h"
#include <esp_vfs_fat.h>
#include <dirent.h>
#include <sys/types.h>

#define USB_DISCONNECT_PIN  GPIO_NUM_10

#define READY_TO_UNINSTALL (HOST_NO_CLIENT | HOST_ALL_FREE)

    typedef enum { ///
        MSC_DEVICE_CONNECTED,       /**< MSC device has been connected to the system.*/
        MSC_DEVICE_DISCONNECTED,    /**< MSC device has been disconnected from the system.*/
    } event; ///

typedef enum {
    HOST_NO_CLIENT = 0x1,
    HOST_ALL_FREE = 0x2,
    DEVICE_CONNECTED = 0x4,
    DEVICE_DISCONNECTED = 0x8,
    DEVICE_ADDRESS_MASK = 0xFF0,
} app_event_t;
#endif
//end
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <FFat.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <M5Cardputer.h>
#include "hw_config.h"

extern "C"
{
#include <nofrendo.h>
}

int16_t bg_color;
//extern Arduino_TFT *gfx;
extern void display_begin();
#ifdef USB_DISK
static EventGroupHandle_t usb_flags;

static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    if (event->event == MSC_DEVICE_CONNECTED) {
    M5Cardputer.Display.println ( "MSC device connected");
        // Obtained USB device address is placed after application events
        xEventGroupSetBits(usb_flags, DEVICE_CONNECTED | (event->device.address << 4));
    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        xEventGroupSetBits(usb_flags, DEVICE_DISCONNECTED);
        M5Cardputer.Display.println ( "MSC device disconnected");
    }
}

static void print_device_info(msc_host_device_info_t *info)
{
    const size_t megabyte = 1024 * 1024;
    uint64_t capacity = ((uint64_t)info->sector_size * info->sector_count) / megabyte;

    M5Cardputer.Display.println("Device info:\n");
    M5Cardputer.Display.printf("\t Capacity: %llu MB\n", capacity);
    M5Cardputer.Display.printf("\t Sector size: %"PRIu32"\n", info->sector_size);
    M5Cardputer.Display.printf("\t Sector count: %"PRIu32"\n", info->sector_count);
M5Cardputer.Display.printf("\t PID: 0x%4X \n", info->idProduct);
M5Cardputer.Display.printf("\t VID: 0x%4X \n", info->idVendor);
    wprintf(L"\t iProduct: %S \n", info->iProduct);
    wprintf(L"\t iManufacturer: %S \n", info->iManufacturer);
    wprintf(L"\t iSerialNumber: %S \n", info->iSerialNumber);
}

static bool file_exists(const char *file_path)
{
    struct stat buffer;
    return stat(file_path, &buffer) == 0;
}

static void file_operations(void)
{
    //const char *directory = "/usb/esp";
    
    char* _dirpath = "/usb";
    char* dirpath = "/usb/";
    DIR *dir = opendir(_dirpath);

    // /* Retrieve the base path of file storage to construct the full path */
    // strlcpy(entrypath, dirpath, sizeof(entrypath));
    char entrypath[256];
    char entrysize[32];
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;
    const size_t dirpath_len = strlen(dirpath);
    strlcpy(entrypath, dirpath, sizeof(entrypath));
    char *argv[1];
    if (!dir)
    {
        ESP_LOGE("TAG", "Failed to stat dir : %s", _dirpath);
    }
    else
    {
        entry = readdir(dir);
        while (entry != NULL)
        {
            entrytype = (entry->d_type == DT_DIR ? "directory" : "file");
            if(entry->d_type==DT_REG ){

             char *filename = (char *)entry->d_name;
                int8_t len = strlen(filename);
                if (strstr(strlwr(filename + (len - 4)), ".nes"))
                {
                
                    char fullFilename[256];
                    sprintf(fullFilename, "%s/%s", _dirpath, filename);
                    Serial.println(fullFilename);
                    argv[0] = fullFilename;
                    break;
                }
           // strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
            }
            entry = readdir(dir);
        }
        closedir(dir);
    }
    nofrendo_main(1,argv);
}

// Handles common USB host library events
static void handle_usb_events(void *args)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        // Release devices once all clients has deregistered
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
            xEventGroupSetBits(usb_flags, HOST_NO_CLIENT);
        }
        // Give ready_to_uninstall_usb semaphore to indicate that USB Host library
        // can be deinitialized, and terminate this task.
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            xEventGroupSetBits(usb_flags, HOST_ALL_FREE);
        }
    }

    vTaskDelete(NULL);
}

static uint8_t wait_for_msc_device(void)
{
    EventBits_t event;

    M5Cardputer.Display.println ("Waiting for USB stick to be connected");
    event = xEventGroupWaitBits(usb_flags, DEVICE_CONNECTED | DEVICE_ADDRESS_MASK,
                                pdTRUE, pdFALSE, portMAX_DELAY);
    M5Cardputer.Display.println ("connection...");
    // Extract USB device address from event group bits
    return (event & DEVICE_ADDRESS_MASK) >> 4;
}

static bool wait_for_event(EventBits_t event, TickType_t timeout)
{
    return xEventGroupWaitBits(usb_flags, event, pdTRUE, pdTRUE, timeout) & event;
}
/*
extern "C" {
void display_printf(const char *format, ...){
    va_list arg;
    va_list copy;
     va_start(arg, format);
     va_copy(copy,arg);
    M5Cardputer.Display.printf(format,copy);
    va_end(copy);
    va_end(arg);
}


}

*/
static void usb_setup(void)
{
    #define dprintln M5Cardputer.Display.println
    msc_host_device_handle_t msc_device;
    msc_host_vfs_handle_t vfs_handle;
    msc_host_device_info_t info;
    BaseType_t task_created;
//M5Cardputer.begin();
//M5Cardputer.Display.setRotation(1);
    const gpio_config_t input_pin = {
        .pin_bit_mask = BIT64(USB_DISCONNECT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK( gpio_config(&input_pin) );

    usb_flags = xEventGroupCreate();
    assert(usb_flags);

    const usb_host_config_t host_config = { .intr_flags = ESP_INTR_FLAG_LEVEL1 };
    ESP_ERROR_CHECK( usb_host_install(&host_config) );
    task_created = xTaskCreate(handle_usb_events, "usb_events", 2048, NULL, 2, NULL);
    assert(task_created);

    const msc_host_driver_config_t msc_config = {
        .create_backround_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .callback = msc_event_cb,
    };
    ESP_ERROR_CHECK( msc_host_install(&msc_config) );

    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3,
        .allocation_unit_size = 1024,
    };

    do {
        uint8_t device_address = wait_for_msc_device();

        ESP_ERROR_CHECK( msc_host_install_device(device_address, &msc_device) );

        msc_host_print_descriptors(msc_device);

        ESP_ERROR_CHECK( msc_host_get_device_info(msc_device, &info) );
        print_device_info(&info);
      //  dprintln("vfs begin");
         ESP_ERROR_CHECK(msc_host_vfs_register(msc_device, "/usb", &mount_config, &vfs_handle));
        //dprintln(esp_err_to_name(err));
        //ESP_ERROR_CHECK( err);
        
        //dprintln("vfs end");
        while (!wait_for_event(DEVICE_DISCONNECTED, 200)) {
          //  dprintln("file begin");
            file_operations();
           // dprintln("file end");
        }

        xEventGroupClearBits(usb_flags, READY_TO_UNINSTALL);
        ESP_ERROR_CHECK( msc_host_vfs_unregister(vfs_handle) );
        ESP_ERROR_CHECK( msc_host_uninstall_device(msc_device) );

    } while (gpio_get_level(USB_DISCONNECT_PIN) != 0);

   //M5Cardputer.Display.println( "Uninitializing USB ...");
    ESP_ERROR_CHECK( msc_host_uninstall() );
    wait_for_event(READY_TO_UNINSTALL, portMAX_DELAY);
    ESP_ERROR_CHECK( usb_host_uninstall() );
    //M5Cardputer.Display.println("Done");
}

#endif
void setup()
{
      auto cfg=M5.config();
    cfg.external_speaker.hat_spk=true;
    M5Cardputer.begin(cfg);
Serial.begin(115200);

    // turn off WiFi
    esp_wifi_deinit();

    // disable Core 0 WDT
    TaskHandle_t idle_0 = xTaskGetIdleTaskHandleForCPU(0);
    esp_task_wdt_delete(idle_0);

    // start display
    display_begin();

    
#ifdef USB_DISK
usb_setup();

#else
  
    // filesystem defined in hw_config.h
    FILESYSTEM_BEGIN

    // find first rom file (*.nes)
    File root = filesystem.open("/");
    char *argv[1];
    if (!root)
    {
        Serial.println("Filesystem mount failed! Please check hw_config.h settings.");
       M5Cardputer.Display.println("Filesystem mount failed! Please check hw_config.h settings.");
    }
    else
    {
        bool foundRom = false;

        File file = root.openNextFile();
        while (file)
        {
            if (file.isDirectory())
            {
                // skip
            }
            else
            {
                char *filename = (char *)file.name();
                int8_t len = strlen(filename);
                if (strstr(strlwr(filename + (len - 4)), ".nes"))
                {
                    foundRom = true;
                    char fullFilename[256];
                    sprintf(fullFilename, "%s/%s", FSROOT, filename);
                    Serial.println(fullFilename);
                    argv[0] = fullFilename;
                    break;
                }
            }

            file = root.openNextFile();
        }

        if (!foundRom)
        {
            Serial.println("Failed to find rom file, please copy rom file to data folder and upload with \"ESP32 Sketch Data Upload\"");
            argv[0] = "/";
        }

        Serial.println("NoFrendo start!\n");
        nofrendo_main(1, argv);
        Serial.println("NoFrendo end!\n");
    }
    #endif
}

void loop()
{
//    M5Cardputer.update();
}
