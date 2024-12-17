import os
import time
from ftplib import FTP
from queue import Queue
from threading import Thread

# Configuración del servidor FTP
FTP_HOST = "192.168.18.39"
FTP_USER = "anonymous"
FTP_PASS = ""
REMOTE_DIR = "/"

# Cola para procesar archivos
file_queue = Queue()
previous_files = set()


def initialize_detected_files(ftp):
    """Carga los archivos existentes en el directorio remoto al inicio."""
    global previous_files
    try:
        ftp.cwd(REMOTE_DIR)
        # Agregar todos los archivos actuales al conjunto previous_files
        previous_files.update(file for file in ftp.nlst() if file.endswith(".dxd"))
        print("Archivos existentes cargados al iniciar el script:")
        if(len(previous_files)== 0):
            print("Directorio vacío")
        else:
            for file in previous_files:
                print(f" - {file}")
    except Exception as e:
        print(f"Error al cargar los archivos existentes: {e}")


def ftp_watchdog():
    global previous_files

    try:
        # Conectarse al servidor FTP
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
            file_queue.put(file)  # Agregar archivo a la cola

        # Actualizar la lista de archivos detectados
        previous_files = current_files

    except Exception as e:
        print(f"Error durante la ejecución de FTP watchdog: {e}")


def process_files():
    while True:
        # Esperar un archivo en la cola
        file = file_queue.get()
        if file is None:
            break  # Señal de terminación
        # Simular procesamiento del archivo
        print(f"Procesando archivo: {file}...")
        time.sleep(10)  # Simula 10 segundos de procesamiento
        print(f"Archivo procesado: {file}")
        file_queue.task_done()  # Marcar el archivo como procesado


if __name__ == "__main__":
    # Iniciar el hilo de procesamiento
    worker_thread = Thread(target=process_files, daemon=True)
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
            time.sleep(1)  # Intervalo (MODIFICAR)
    except KeyboardInterrupt: #Detener el programa
        print("Deteniendo monitoreo...")
        file_queue.put(None)
        worker_thread.join()
