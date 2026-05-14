#include <stdio.h>
#include <stdint.h>
#include <im.h>
#include <string.h>

typedef struct {
    char id[16];
} SensorKey;

typedef struct {
    float temperature;
    float humidity;
    uint32_t last_update;
} SensorData;

typedef IndexMap(SensorKey, SensorData) RadarMap;

int main() {
    RadarMap sensors = NULL;

    SensorKey key1 = {"DHT22_ROOM1"};
    SensorData data1 = {24.5f, 40.0f, 1715690000};

    Result(im_dense_idx, IM_Status) res = im_insert(sensors, key1, data1);

    if (is_ok(res)) {
        im_dense_idx pos = unwrap(res);
        printf("Датчик добавлен на позицию: %zu\n", pos);
    } else {
        fprintf(stderr, "Ошибка выделения памяти!\n");
        return 1;
    }

    data1.temperature = 25.1f;
    im_insert(sensors, key1, data1);

    SensorKey search_key = {"DHT22_ROOM1"};
    auto opt = im_get(sensors, search_key);

    if (is_some(opt)) {
        size_t idx = unwrap_opt(opt);
        printf("Температура в %s: %.1f°C\n",
                sensors[idx].key.id,
                sensors[idx].value.temperature);
    }

    printf("\nСписок всех активных датчиков:\n");
    for (size_t i = 0; i < im_len(sensors); i++) {
        printf("[%zu] ID: %s, T: %.1f\n",
               i, sensors[i].key.id, sensors[i].value.temperature);
    }

    SensorKey to_remove;
    memset(&to_remove, 0, sizeof(SensorKey));
    strcpy(to_remove.id, "DHT22_ROOM1");
    typeof(*sensors) removed_entry;


    if (im_remove(sensors, to_remove, &removed_entry)) {
        printf("Удален ключ %s, его значение было %.1f\n",
            removed_entry.key.id,
            removed_entry.value.temperature);
    }

    im_free(sensors);

    return 0;
}