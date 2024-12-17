import os
import time
import subprocess
from ftplib import FTP
from queue import Queue
from threading import Thread

# Configuración del servidor FTP
FTP_HOST = "192.168.18.39"
FTP_USER = "anonymous"
FTP_PASS = ""
REMOTE_DIR = "/"
OUTPUT_DIR = "./output"  # Carpeta para almacenar los archivos procesados
COLUMNAS="0,1,2,3,4,5,6,7"
# Cola para procesar archivos
file_queue = Queue()
previous_files = set()


def initialize_detected_files(ftp):
    """Carga los archivos existentes en el directorio remoto al inicio."""
    global previous_files
    try:
        ftp.cwd(REMOTE_DIR)
        previous_files.update(file for file in ftp.nlst() if file.endswith(".dxd"))
        print("Archivos existentes cargados al iniciar el script:")
        if len(previous_files) == 0:
            print("Directorio vacío")
        else:
            for file in previous_files:
                print(f" - {file}")
    except Exception as e:
        print(f"Error al cargar los archivos existentes: {e}")


def ftp_watchdog():
    """Monitorea el servidor FTP para detectar nuevos archivos."""
    global previous_files

    try:
        ftp = FTP(FTP_HOST)
        ftp.login(FTP_USER, FTP_PASS)
        ftp.cwd(REMOTE_DIR)

        # Listar y filtrar archivos .dxd
        current_files = {file for file in ftp.nlst() if file.endswith(".dxd")}
        ftp.quit()

        # Detectar archivos nuevos
        new_files = current_files - previous_files
        for file in new_files:
            print(f"Nuevo archivo .dxd detectado: {file}")
            file_queue.put((file, OUTPUT_DIR, COLUMNAS))

        # Actualizar la lista de archivos detectados
        previous_files = current_files

    except Exception as e:
        print(f"Error durante la ejecución de FTP watchdog: {e}")
        
        
def process_from_queue():
    """Consume archivos de la cola y los pasa a process_files."""
    while True:
        # Obtener archivo y parámetros de la cola
        task = file_queue.get()
        if task is None:  # Señal de terminación
            break

        # Desempaquetar los parámetros
        input_file, output_folder, columnas = task

        # Llamar a process_files con los argumentos desempaquetados
        try:
            process_files(input_file, output_folder, columnas)
        except Exception as e:
            print(f"Error al procesar archivo {input_file}: {e}")

        file_queue.task_done()  # Marcar el archivo como procesado


def process_files(input_file, output_folder, columnas):
    try:
        # Crear la carpeta de salida si no existe
        if not os.path.exists(output_folder):
            os.makedirs(output_folder)

        # Ruta al programa en C compilado
        c_program_path = "D:/PUCP/chamba/testFolder/DWDataReader/DWDataReaderAdapted.exe"

        # Archivo de salida basado en el nombre del archivo de entrada
        output_file = f"{output_folder}/{input_file.split('/')[-1].split('.')[0]}.txt"

        # Ejecutar el programa en C
        result = subprocess.run(
            [c_program_path, input_file, columnas, output_file],
            capture_output=True,  # Captura stdout y stderr
            text=True             # Decodifica la salida como texto
        )

        # Verificar si ocurrió algún error
        if result.returncode != 0:
            print(f"Error al ejecutar el programa en C:")
            print(f"Salida estándar:\n{result.stdout}")
            print(f"Salida de error:\n{result.stderr}")
        else:
            print(f"Archivo procesado correctamente: {output_file}")
            print(f"Salida del programa en C:\n{result.stdout}")

    except FileNotFoundError:
        print(f"No se encontró el programa en C en la ruta: {c_program_path}")
    except Exception as e:
        print(f"Error inesperado: {e}")

        
    


if __name__ == "__main__":
    # Iniciar el hilo de procesamiento
    worker_thread = Thread(target=process_from_queue, daemon=True)
    worker_thread.start()

    # Conectarse al servidor para inicializar el conjunto de archivos
    try:
        ftp = FTP(FTP_HOST)
        ftp.login(FTP_USER, FTP_PASS)
        initialize_detected_files(ftp)
        ftp.quit()
    except Exception as e:
        print(f"Error al inicializar el conjunto de archivos detectados: {e}")

    print("Iniciando monitoreo para archivos .dxd en FTP...")
    try:
        while True:
            ftp_watchdog()
            time.sleep(1)  # Intervalo de monitoreo
    except KeyboardInterrupt:  # Detener el programa
        print("Deteniendo monitoreo...")
        file_queue.put(None)
        worker_thread.join()
