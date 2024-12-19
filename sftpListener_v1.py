import os
import time
import subprocess
import paramiko
from queue import Queue
from threading import Thread

# Configuración del servidor SFTP
SFTP_HOST = "200.16.2.59"      # Dirección IP o nombre del servidor SFTP
SFTP_PORT = 22                # Puerto estándar de SFTP
SFTP_USER = "WS_Monitoreo"    # Nombre de usuario
SFTP_PASS = "patrimonio2018"   
REMOTE_DIR = "/dir/L"  # Directorio en el servidor SFTP
 
#EL ARCHIVO SE GENERA DENTRO DE LA CARPETA EN LA QUE SE EJECUTA EL CODIGO
OUTPUT_DIR = "D:/PUCP/chamba/testFolder/output" # Carpeta para almacenar los archivos procesados
TEMP_DIR = "D:/PUCP/chamba/testFolder" 
LOCAL_DIR = "D:/PUCP/chamba/testFolder"  # Carpeta para almacenar archivos descargados
COLUMNAS = "0,1,2,3,4,5,6,7"

# Ruta al programa en C
C_PROGRAM_PATH = "D:/PUCP/chamba/testFolder/DWDataReader/DWDataReaderAdapted.exe"

# Cola para procesar archivos
file_queue = Queue()
previous_files = set()

def connect_sftp():
    """
    Conecta al servidor SFTP y devuelve una instancia SFTP.
    """
    try:
        transport = paramiko.Transport((SFTP_HOST, SFTP_PORT))
        transport.connect(username=SFTP_USER, password=SFTP_PASS)
        sftp = paramiko.SFTPClient.from_transport(transport)
        return sftp
    except Exception as e:
        print(f"Error conectando al servidor SFTP: {e}")
        raise

def initialize_detected_files(sftp):
    """
    Carga los archivos existentes en el directorio remoto al inicio.
    """
    global previous_files
    try:
        sftp.chdir(REMOTE_DIR)
        previous_files.update(file for file in sftp.listdir() if file.endswith(".dxd"))
        print("Archivos existentes cargados al iniciar:")
        for file in previous_files:
            print(f" - {file}")
    except Exception as e:
        print(f"Error al cargar los archivos existentes: {e}")

def sftp_watchdog():
    """
    Monitorea el servidor SFTP para detectar nuevos archivos y los descarga.
    """
    global previous_files
    try:
        sftp = connect_sftp()
        sftp.chdir(REMOTE_DIR)

        current_files = {file for file in sftp.listdir() if file.endswith(".dxd")}
        new_files = current_files - previous_files

        for file in new_files:
            print(f"Nuevo archivo .dxd detectado: {file}")
            local_path = os.path.join(TEMP_DIR, file)
            sftp.get(file, local_path)  # Descargar el archivo
            print(f"Archivo descargado: {local_path}")
            file_queue.put((local_path, OUTPUT_DIR, COLUMNAS))

        previous_files = current_files
        sftp.close()
    except Exception as e:
        print(f"Error durante la ejecución del watchdog: {e}")

def process_from_queue():
    """
    Consume archivos de la cola, los procesa y elimina después del procesamiento.
    """
    while True:
        task = file_queue.get()
        if task is None:
            break

        input_file, output_folder, columnas = task
        try:
            process_files(input_file, output_folder, columnas)
            # Eliminar el archivo después del procesamiento
            if os.path.exists(input_file):
                os.remove(input_file)
                print(f"Archivo eliminado: {input_file}")
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

        # Generar nombre de archivo procesado
        output_file = os.path.join(output_folder, os.path.basename(input_file).replace(".dxd", "_processed.txt"))

        # Ejecutar programa en C
        print(f"Procesando archivo {input_file}...")
        result = subprocess.run(
            [C_PROGRAM_PATH, input_file, columnas, output_folder],
            capture_output=True,
            text=True
        )

        # Capturar salida estándar y de error
        if result.returncode != 0:
            print(f"Error al ejecutar el programa en C:")
            print(f"Salida estándar:\n{result.stdout}")
            print(f"Salida de error:\n{result.stderr}")  # Imprimir la salida de error
        else:
            print(f"Archivo procesado correctamente: {output_file}")
            print(f"Salida del programa en C:\n{result.stdout}")
    except FileNotFoundError:
        print(f"No se encontró el programa en C en la ruta: {C_PROGRAM_PATH}")
    except Exception as e:
        print(f"Error inesperado: {e}")


if __name__ == "__main__":
    print("Seleccione una opción:")
    print("1. Monitorear el servidor SFTP.")
    print("2. Procesar todos los archivos .dxd en la carpeta local.")
    choice = input("Ingrese su elección (1 o 2): ")

    worker_thread = Thread(target=process_from_queue, daemon=True)
    worker_thread.start()

    if choice == "1":
        try:
            sftp = connect_sftp()
            initialize_detected_files(sftp)
            sftp.close()
        except Exception as e:
            print(f"Error al inicializar el conjunto de archivos detectados: {e}")

        print("Iniciando monitoreo para archivos .dxd en SFTP...")
        try:
            while True:
                sftp_watchdog()
                time.sleep(10)  # Intervalo de monitoreo
        except KeyboardInterrupt:
            print("Deteniendo monitoreo...")
            file_queue.put(None)
            worker_thread.join()
    elif choice == "2":
        print("Procesando archivos locales...")
        local_files = [f for f in os.listdir(LOCAL_DIR) if f.endswith(".dxd")]
        for file in local_files:
            file_path = os.path.join(LOCAL_DIR, file)
            file_queue.put((file_path, OUTPUT_DIR, COLUMNAS))
        file_queue.put(None)
        worker_thread.join()
    else:
        print("Opción inválida. Terminando el programa.")
        file_queue.put(None)
        worker_thread.join()
#sftp bruno@localhost
#pass: 5226013

#linea de codigo para generar el archivo en c
#gcc -o DWDataReaderAdapted.exe DWDataReaderAdapted.c DWLoadLib.c -I. -L. -lDWDataReaderLib64
