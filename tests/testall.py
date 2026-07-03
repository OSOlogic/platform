import pymysql
import sys
import time

# --- Configuración de la Base de Datos ---
DB_CONFIG = {
    'user': 'admin',
    'password': 'opi',
    'host': 'localhost', # Adaptado a localhost ya que se ejecuta en el PLC
    'database': 'PLC',
    'port': 3306,
    'charset': 'utf8mb4',
    'autocommit': True
}

def obtener_puntos_logicos():
    """
    Adaptación: Extraemos dinámicamente los module_id buscando fk_model_id = 20 (32SD).
    Devuelve la lista de tuplas (module_id, logical_address).
    """
    puntos = []
    try:
        conn = pymysql.connect(**DB_CONFIG)
        with conn.cursor() as cursor:
            # Seleccionamos salidas disponibles para las 3 tarjetas
            query = """
                SELECT r.fk_module_id, mid.logical_address
                FROM rtmirror r
                JOIN model_io_definition mid ON r.fk_io_definition_id = mid.io_definition_id
                JOIN devices d ON r.fk_module_id = d.module_id
                WHERE d.fk_model_id = 20
                  AND d.address_on_channel IN ('0', '1', '2')
                  AND mid.io_type = 'bit'
                  AND mid.hardware_access IN ('readwrite', 'writeonly')
                ORDER BY d.address_on_channel, CAST(mid.logical_address AS UNSIGNED)
            """
            cursor.execute(query)
            for row in cursor.fetchall():
                puntos.append((row[0], row[1]))
    except pymysql.MySQLError as err:
        print(f"Error obteniendo configuración de puntos: {err}")
        sys.exit(1)
    finally:
        if 'conn' in locals() and conn.open:
            conn.close()
    return puntos

# Poblamos la lista automáticamente
ALL_LOGICAL_POINTS = obtener_puntos_logicos()

def ejecutar_sql_batch_logical(valor_a_escribir, points, quiet=False):
    """
    Actualiza el valor usando (module_id, logical_address).
    Adaptación: En OSOlogic seteamos net_required_value (y evitamos required_value puro).
    Adaptación: Usamos pymysql igual que el backend para compatibilidad absoluta sin requerir virtual envs extra.
    """
    if not points:
        return

    # Construimos la query con JOIN para filtrar por logical_address
    query = """
    UPDATE rtmirror rt
    JOIN model_io_definition mid ON rt.fk_io_definition_id = mid.io_definition_id
    SET rt.net_required_value = %s
    WHERE (rt.fk_module_id, mid.logical_address) IN ({})
    """.format(','.join(['(%s, %s)'] * len(points)))
    
    try:
        conn = pymysql.connect(**DB_CONFIG)
        cursor = conn.cursor()
        
        # Parámetros: valor + lista aplanada de (mod, addr)
        flat_points = []
        for p in points:
            flat_points.extend(p)
            
        params = [valor_a_escribir] + flat_points
        cursor.execute(query, params)
        # conn.commit() # autocommit está en True, pero no hace daño
        
        rows_affected = cursor.rowcount
        
        if not quiet:
            print(f"    -> {rows_affected} puntos actualizados a {valor_a_escribir}")

    except pymysql.MySQLError as err:
        print(f"    -> ¡ERROR! {err}")

    finally:
        if 'conn' in locals() and conn.open:
            cursor.close()
            conn.close()

# --- Sequential Test Loop ---
if __name__ == "__main__":
    if not ALL_LOGICAL_POINTS:
        print("No se encontraron outputs para testear (revisa que el fk_model_id=20 y los slots estén ok).")
        sys.exit(1)

    print(f"--- Sequential Test (Logical Address) - Modules 32SD (Total {len(ALL_LOGICAL_POINTS)} outputs) ---")
    print("Press CTRL+C to stop.\n")
    
    # Ensure all are OFF initially
    ejecutar_sql_batch_logical(0, ALL_LOGICAL_POINTS, quiet=True)

    try:
        while True:
            for mod_id, log_addr in ALL_LOGICAL_POINTS:
                print(f"Activating Mod:{mod_id} Addr:{log_addr}...\t\t", end='\r', flush=True)
                # Turn ON current output
                ejecutar_sql_batch_logical(1, [(mod_id, log_addr)], quiet=True)
                time.sleep(0.25) # O el tiempo que se necesite
                
                # Turn OFF current output
                ejecutar_sql_batch_logical(0, [(mod_id, log_addr)], quiet=True)
                time.sleep(0.1) # O el tiempo que se necesite

                
            print("\nCycle finished. Restarting in 1s...")
            time.sleep(1)

    except KeyboardInterrupt:
        print("\n\nStopping! Turning OFF all targets...")
        ejecutar_sql_batch_logical(0, ALL_LOGICAL_POINTS, quiet=False)
        print("Done. Exiting.")
        sys.exit()