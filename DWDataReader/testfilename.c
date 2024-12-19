#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_PATH 512

void sanitize_filename(char *filename) {
    for (int i = 0; filename[i]; i++) {
        if (!isalnum(filename[i]) && filename[i] != '_' && filename[i] != '.') {
            filename[i] = '_';  // Reemplazar caracteres inválidos por '_'
        }
    }
}

int main(int argc, char *argv[]) {
    char input_filename[MAX_PATH];
    char channel_names[MAX_PATH];
    char output_file[MAX_PATH];
    FILE *output;

    // Verificar argumentos
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <input_file> <channel_names>\n", argv[0]);
        return -1;
    }

    strncpy(input_filename, argv[1], MAX_PATH - 1);
    strncpy(channel_names, argv[2], MAX_PATH - 1);

    // Limpiar nombres de archivo y canales
    sanitize_filename(input_filename);
    sanitize_filename(channel_names);

    // Generar el nombre del archivo de salida
    snprintf(output_file, sizeof(output_file), "%s_%s.txt", input_filename, channel_names);
    printf("Nombre del archivo de salida: %s\n", output_file);

    // Intentar abrir el archivo de salida
    output = fopen(output_file, "w");
    if (!output) {
        perror("Error abriendo archivo de salida");
        return -1;
    }

    // Escribir datos de prueba
    fprintf(output, "Archivo generado correctamente.\n");

    fclose(output);
    printf("Archivo generado y cerrado correctamente.\n");

    return 0;
}
