#include "DWLoadLib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_CHANNELS 100
#define MAX_CHANNEL_NAME 100

// Función para normalizar la ruta (convertir \ a /)
void normalize_path(char *path) {
    for (int i = 0; path[i]; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <input_file.dxd> <channel_indices> <output_folder>\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    const char *channel_indices = argv[2];
    const char *output_folder = argv[3];
    char output_file[512]; // Buffer para el nombre del archivo de salida

    // Normalizar la ruta del archivo de entrada
    normalize_path((char *)input_file);

    // Validar que la carpeta de salida existe o intentar crearla
    struct stat st = {0};
    if (stat(output_folder, &st) == -1) {
#ifdef _WIN32
        if (mkdir(output_folder) != 0) {
#else
        if (mkdir(output_folder, 0755) != 0) {
#endif
            perror("Error creando carpeta de salida");
            return 1;
        }
    }

    if (!LoadDWDLL("DWDataReaderLib64.dll")) {
        printf("Could not load DWDataReaderLib64.dll\n");
        return 1;
    }

    if (DWInit() != DWSTAT_OK) {
        printf("DWInit() failed\n");
        return 1;
    }

    struct DWFileInfo file_info;
    if (DWOpenDataFile((char *)input_file, &file_info) != DWSTAT_OK) {
        printf("Failed to open file: %s\n", input_file);
        return 1;
    }

    int num_channels = DWGetChannelListCount();
    if (num_channels <= 0) {
        printf("No channels found\n");
        return 1;
    }

    struct DWChannel *channels = malloc(sizeof(struct DWChannel) * num_channels);
    if (!channels || DWGetChannelList(channels) != DWSTAT_OK) {
        printf("Failed to get channel list\n");
        return 1;
    }

    printf("Total channels: %d\n", num_channels);
    int selected_indices[MAX_CHANNELS] = {0};
    int selected_count = 0;
    char channel_names[512] = ""; // Buffer para los nombres de los canales seleccionados

    // Parse the channel indices from the input
    char *token = strtok((char *)channel_indices, ",");
    while (token) {
        int index = atoi(token);
        if (index >= 0 && index < num_channels) {
            selected_indices[selected_count++] = index;
            strcat(channel_names, channels[index].name);
            token = strtok(NULL, ",");
            if (token) strcat(channel_names, "_"); // Separador entre nombres
        } else {
            printf("Invalid channel index: %d\n", index);
        }
    }

    // Extraer el nombre base del archivo de entrada (sin ruta ni extensión)
    char *filename = strrchr(input_file, '/');
    if (!filename) filename = strrchr(input_file, '\\'); // Manejar rutas en Windows
    if (filename) {
        filename++; // Saltar la barra
    } else {
        filename = (char *)input_file; // Usar el input_file completo si no hay barra
    }

    // Eliminar la extensión del nombre base
    char base_name[256];
    strncpy(base_name, filename, sizeof(base_name) - 1);
    base_name[sizeof(base_name) - 1] = '\0';
    char *dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    // Generar el nombre del archivo de salida
    snprintf(output_file, sizeof(output_file), "%s/%s_%s.txt", output_folder, base_name, channel_names);

    // Depuración: imprimir el nombre del archivo de salida
    printf("Nombre del archivo de salida: %s\n", output_file);

    // Allocate memory for storing data for all selected channels
    double **data = malloc(selected_count * sizeof(double *));
    __int64 sample_count = 0;

    for (int i = 0; i < selected_count; i++) {
        int channel_index = selected_indices[i];
        printf("Processing channel: %s\n", channels[channel_index].name);

        sample_count = DWGetScaledSamplesCount(channels[channel_index].index);
        if (sample_count <= 0) {
            printf("Failed to get sample count for channel: %d\n", channel_index);
            free(data);
            free(channels);
            return 1;
        }

        data[i] = malloc(sample_count * sizeof(double));
        if (!data[i]) {
            printf("Memory allocation failed\n");
            free(data);
            free(channels);
            return 1;
        }

        if (DWGetScaledSamples(channels[channel_index].index, 0, sample_count, data[i], NULL) != DWSTAT_OK) {
            printf("Failed to get scaled samples for channel: %s\n", channels[channel_index].name);
            free(data[i]);
            free(channels);
            return 1;
        }
    }

    // Open the output file
    FILE *out_file = fopen(output_file, "w");
    if (!out_file) {
        printf("Failed to open output file: %s\n", output_file);
        for (int i = 0; i < selected_count; i++) {
            free(data[i]);
        }
        free(data);
        free(channels);
        return 1;
    }

    // Write data in tabular format (without headers)
    for (__int64 j = 0; j < sample_count; j++) {
        for (int i = 0; i < selected_count; i++) {
            fprintf(out_file, "%.12f", data[i][j]);
            if (i < selected_count - 1) {
                fprintf(out_file, " "); // Espacio entre columnas
            }
        }
        fprintf(out_file, "\n"); // Nueva línea después de cada fila
    }

    // Free allocated memory and close file
    for (int i = 0; i < selected_count; i++) {
        free(data[i]);
    }
    free(data);
    free(channels);
    fclose(out_file);

    DWCloseDataFile();
    DWDeInit();

    printf("Data exported to %s\n", output_file);
    return 0;
}
