#include "nvs_memory.h"

static char namespaceForMmtPeriods[] = "storage";

void nvsInit(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
}


uint8_t nvsWriteItem(char* key, uint32_t item)
{
    if (strlen(key) > NVS_MAX_KEY_LEN)
    {
        printf("[NVS] Write Item ERROR, key too long.\r\n");
        return EXIT_FAILURE;
    }

    // Open NVS namespace
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open(namespaceForMmtPeriods, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK)
    {
        printf("[NVS] Error opening namespace: %s\n", esp_err_to_name(err));
        nvs_close(nvsHandle);
        return EXIT_FAILURE;
    }

    // Write value into NVS
    err = nvs_set_u32(nvsHandle, key, item);
    if (err != ESP_OK)
    {
        printf("[NVS] Error writing value: %s\n", esp_err_to_name(err));
        nvs_close(nvsHandle);
        return EXIT_FAILURE;
    }

    printf("[NVS] Writing value: [%d] with key: [%s] success\n", item, key);

    nvs_close(nvsHandle);
    return EXIT_SUCCESS;
}

uint8_t nvsReadItem(char* key, uint32_t* item)
{
    if (strlen(key) > NVS_MAX_KEY_LEN)
    {
        printf("[NVS] Read Item ERROR, key too long.\r\n");
        return EXIT_FAILURE;
    }

    // Open NVS namespace
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open(namespaceForMmtPeriods, NVS_READONLY, &nvsHandle);
    if (err != ESP_OK)
    {
        printf("[NVS] Error opening namespace: %s\n", esp_err_to_name(err));
        nvs_close(nvsHandle);
        return EXIT_FAILURE;
    }

    // Write value from NVS
    err = nvs_get_u32(nvsHandle, key, item);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        printf("[NVS] Key [%s] is not stored in NVS yet.\n", key);
        nvs_close(nvsHandle);
        return EXIT_FAILURE;
    }
    else if (err != ESP_OK)
    {
        printf("[NVS] Error reading value from NVS: %s\n", esp_err_to_name(err));
        nvs_close(nvsHandle);
        return EXIT_FAILURE;
    }

    printf("[NVS] Reading from key: [%s], value: [%d] success\n", key, *item);

    nvs_close(nvsHandle);
    return EXIT_SUCCESS;
}

// This fcn erases all data in the given namespace
void nvsEraseAll(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);
    nvs_handle_t nvsHandle;

    err = nvs_open(namespaceForMmtPeriods, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK)
    {
        printf("[NVS] Error opening namespace: %s\n", esp_err_to_name(err));
        return;
    }

    // Erase an the namespace
    err = nvs_erase_all(nvsHandle);
    if (err != ESP_OK)
    {
        printf("[NVS] Error erasing namespace: %s\n", esp_err_to_name(err));
        nvs_close(nvsHandle);
        return;
    }

    err = nvs_commit(nvsHandle);
    if (err != ESP_OK)
    {
        printf("[NVS] Error committing changes: %s\n", esp_err_to_name(err));
        nvs_close(nvsHandle);
        return;
    }

    nvs_close(nvsHandle);
    printf("[NVS] Namespace cleared successfully.\n");
}