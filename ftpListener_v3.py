import os
import time
import subprocess
from ftplib import FTP
from queue import Queue
from threading import Thread

# Configuración del servidor FTP
FTP_HOST = "0.0.0.0"
FTP_USER = "anonymous"
FTP_PASS = ""
REMOTE_DIR = "/"  #Directorio en la servidor ftp
OUTPUT_DIR = "output"  # Carpeta para almacenar los archivos procesados
LOCAL_DIR = "localDir"  # Carpeta para buscar archivos locales (para el procesamiento en masa de archivos)
COLUMNAS = "0,1,2,3,4,5,6,7"

# Cola para procesar archivos
file_queue = Queue()
previous_files = set()

def initialize_detected_files(ftp):
    """
    Carga los archivos existentes en el directorio remoto al inicio.
    """
    global previous_files
    try:
        ftp.cwd(REMOTE_DIR)
        previous_files.update(file for file in ftp.nlst() if file.endswith(".dxd"))
        print("Archivos existentes cargados al iniciar:")
        if not previous_files:
            print("Directorio vacío.")
        else:
            for file in previous_files:
                print(f" - {file}")
    except Exception as e:
        print(f"Error al cargar los archivos existentes: {e}")

def ftp_watchdog():
    """
    Monitorea el servidor FTP para detectar nuevos archivos.
    """
    global previous_files
    try:
        ftp = FTP(FTP_HOST)
        ftp.login(FTP_USER, FTP_PASS)
        ftp.cwd(REMOTE_DIR)

        current_files = {file for file in ftp.nlst() if file.endswith(".dxd")}
        ftp.quit()

        new_files = current_files - previous_files
        for file in new_files:
            print(f"Nuevo archivo .dxd detectado: {file}")
            file_queue.put((file, OUTPUT_DIR, COLUMNAS))

        previous_files = current_files
    except Exception as e:
        print(f"Error durante la ejecución de FTP watchdog: {e}")

def process_local_files():
    """
    Agrega todos los archivos .dxd de LOCAL_DIR a la cola.
    """
    if not os.path.exists(LOCAL_DIR):
        print(f"La carpeta local '{LOCAL_DIR}' no existe.")
        return

    local_files = [f for f in os.listdir(LOCAL_DIR) if f.endswith(".dxd")]
    if not local_files:
        print(f"No se encontraron archivos .dxd en la carpeta '{LOCAL_DIR}'.")
        return

    print(f"Procesando {len(local_files)} archivos .dxd en '{LOCAL_DIR}':")
    for file in local_files:
        file_path = os.path.join(LOCAL_DIR, file)
        print(f"Agregando {file_path} a la cola de procesamiento.")
        file_queue.put((file_path, OUTPUT_DIR, COLUMNAS))

def process_from_queue():
    """
    Consume archivos de la cola y los procesa.
    """
    while True:
        task = file_queue.get()
        if task is None:
            break

        input_file, output_folder, columnas = task
        try:
            process_files(input_file, output_folder, columnas)
        except Exception as e:
            print(f"Error al procesar archivo {input_file}: {e}")
        file_queue.task_done()

def process_files(input_file, output_folder, columnas):
    """
    Procesa un archivo usando un programa en C.
    """
    try:
        if not os.path.exists(output_folder):
            os.makedirs(output_folder)

        c_program_path = "DWDataReader/DWDataReaderAdapted.exe"
        output_file = f"{output_folder}/{os.path.basename(input_file).split('.')[0]}"
        output_file = output_file + "_200.txt"
        result = subprocess.run(
            [c_program_path, input_file, columnas, output_file],
            capture_output=True,
            text=True
        )

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
    print("Seleccione una opción:")
    print("1. Monitorear el servidor FTP.")
    print("2. Procesar todos los archivos .dxd en la carpeta local.")
    choice = input("Ingrese su elección (1 o 2): ")

    worker_thread = Thread(target=process_from_queue, daemon=True)
    worker_thread.start()

    if choice == "1":
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
                time.sleep(1)
        except KeyboardInterrupt:
            print("Deteniendo monitoreo...")
            file_queue.put(None)
            worker_thread.join()
    elif choice == "2":
        process_local_files()
        file_queue.put(None)
        worker_thread.join()
    else:
        print("Opción inválida. Terminando el programa.")
        file_queue.put(None)
        worker_thread.join()
