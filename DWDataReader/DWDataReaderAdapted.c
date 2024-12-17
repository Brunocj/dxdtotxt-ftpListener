#include "DWLoadLib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHANNELS 100
#define MAX_CHANNEL_NAME 100
#define OUTPUT_FILE "output.txt"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <input_file.dxd> <channel_indices> <output_file>\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    const char *channel_indices = argv[2];
    const char *output_file = argv[3];

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

    // Parse the channel indices from the input
    char *token = strtok((char *)channel_indices, ",");
    while (token) {
        int index = atoi(token);
        if (index >= 0 && index < num_channels) {
            selected_indices[selected_count++] = index;
        } else {
            printf("Invalid channel index: %d\n", index);
        }
        token = strtok(NULL, ",");
    }

    // Allocate memory for storing data for all selected channels
    double **data = malloc(selected_count * sizeof(double *));
    __int64 sample_count = 0;

    for (int i = 0; i < selected_count; i++) {
        int channel_index = selected_indices[i];
        printf("Processing channel: %s\n", channels[channel_index].name);

        // Get the number of samples for the channel
        sample_count = DWGetScaledSamplesCount(channels[channel_index].index);
        if (sample_count <= 0) {
            printf("Failed to get sample count for channel: %d\n", channel_index);
            free(data);
            free(channels);
            return 1;
        }

        // Allocate memory for data of the channel
        data[i] = malloc(sample_count * sizeof(double));
        if (!data[i]) {
            printf("Memory allocation failed\n");
            free(data);
            free(channels);
            return 1;
        }

        // Read scaled samples for the channel
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

    // Write the names of the sensors in the first row
    for (int i = 0; i < selected_count; i++) {
        fprintf(out_file, "%s", channels[selected_indices[i]].name);
        if (i < selected_count - 1) {
            fprintf(out_file, " "); // Add space between column names
        }
    }
    fprintf(out_file, "\n"); // Add a newline after the header row

    // Write data in tabular format
    for (__int64 j = 0; j < sample_count; j++) {
        for (int i = 0; i < selected_count; i++) {
            fprintf(out_file, "%.12f", data[i][j]);
            if (i < selected_count - 1) {
                fprintf(out_file, " "); // Add space between columns
            }
        }
        fprintf(out_file, "\n"); // Add a newline after each row
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
