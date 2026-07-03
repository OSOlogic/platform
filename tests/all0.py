import pymysql
import sys

def reset_all_outputs():
    try:
        # Conexión eficiente a la base de datos utilizando la configuración base del PLC
        conn = pymysql.connect(
            host='localhost',
            user='admin',
            password='opi',
            database='PLC',
            autocommit=True, # Auto-commit para que el UPDATE se aplique inmediatamente
            charset='utf8mb4',
            cursorclass=pymysql.cursors.DictCursor
        )
    except Exception as e:
        print(f"Error conectando a la base de datos: {e}")
        sys.exit(1)

    try:
        with conn.cursor() as cursor:
            # 1. Identificar de manera directa los module_id de las 3 tarjetas 32SD (fk_model_id = 20) en los slots 0, 1 y 2
            query_modules = """
                SELECT module_id, module_name, address_on_channel 
                FROM devices 
                WHERE fk_model_id = 20
                  AND address_on_channel IN ('0', '1', '2')
            """
            cursor.execute(query_modules)
            modules = cursor.fetchall()

            if not modules:
                print("No se han encontrado tarjetas '32SD' en los slots 0, 1 o 2.")
                return

            module_ids = [str(m['module_id']) for m in modules]
            
            print(f"Se encontraron {len(modules)} tarjetas 32SD:")
            for m in modules:
                print(f" - ID: {m['module_id']} | Nombre: {m['module_name']} | Slot: {m['address_on_channel']}")

            # 2. Utilizar un solo UPDATE con JOIN (Eficiente) para colocar todos sus outputs a 0.
            # Solo modificamos los registros tipo bit con acceso de escritura para prevenir tocar inputs.
            placeholders = ', '.join(['%s'] * len(module_ids))
            
            query_update = f"""
                UPDATE rtmirror r
                JOIN model_io_definition mid ON r.fk_io_definition_id = mid.io_definition_id
                SET r.net_required_value = 0
                WHERE r.fk_module_id IN ({placeholders})
                  AND mid.io_type = 'bit'
                  AND mid.hardware_access IN ('readwrite', 'writeonly')
            """
            
            # Ejecutamos la consulta pasándole la tupla de module_ids a los placeholders
            affected_rows = cursor.execute(query_update, tuple(module_ids))
            
            print(f"\n¡Operación exitosa! Se han puesto a 0 un total de {affected_rows} outputs.")
            
    except pymysql.MySQLError as e:
        print(f"Ocurrió un error ejecutando en la BD: {e}")
        conn.rollback()
    finally:
        conn.close()

if __name__ == '__main__':
    reset_all_outputs()
