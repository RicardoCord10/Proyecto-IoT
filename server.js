// API REST

// Importar dependencias

// Permite crear el servidor y definir las rutas de la API
const express = require("express");

// Configura la comunicación entre distintos sitios web
const cors = require("cors");

// Permite conectar Node.js con Supabase
const { createClient } = require("@supabase/supabase-js");

// Carga las variables de configuración del archivo .env
require("dotenv").config();

// Configurar el servidor

const app = express();

// Permite la comunicación con distintos dispositivos
app.use(cors());

// Interpreta los datos recibidos en formato JSON
app.use(express.json());

app.use((req, res, next) => {
    console.log("Solicitud recibida:", req.method, req.url);
    next();
});

// Establecer conexión con Supabase

const supabase = createClient(
    process.env.SUPABASE_URL,
    process.env.SUPABASE_SECRET_KEY
);

const PORT = process.env.PORT || 3000;

// Validar las variables de configuración

if (
    !process.env.SUPABASE_URL ||
    !process.env.SUPABASE_SECRET_KEY ||
    !process.env.DEVICE_TOKEN
) {
    throw new Error("Faltan variables de configuración en .env");
}

// Prueba de funcionamiento: solicitud GET

app.get("/", (req, res) => {
    res.json({
        mensaje: "API del sistema funcionando",
        estado: "Activo"
    });
});


// Recibir y guardar mediciones

app.post("/api/readings", async (req, res) => {
    console.log("POST RECIBIDO:", req.body);
    try {

        // Validar dispositivo
        const token = req.headers["x-device-token"];

        if (token !== process.env.DEVICE_TOKEN) {
            return res.status(401).json({
                error: "Dispositivo no autorizado"
            });
        }


        // Obtener datos enviados por el ESP8266

        const {
            device_id,
            distance_cm,
            water_level_cm,
            level_percent,
            level_change_rate_cm_min,
            water_status,
            possible_leak
        } = req.body;


        // Validar campos obligatorios

        const required = [
            distance_cm,
            water_level_cm,
            level_percent,
            water_status
        ];

        if (required.some(value => value === undefined || value === null)) {
            return res.status(400).json({
                error: "Faltan datos obligatorios"
            });
        }


        // Validar datos numéricos

        const numbers = [
            distance_cm,
            water_level_cm,
            level_percent,
            level_change_rate_cm_min ?? 0
        ];

        if (!numbers.every(value =>
            typeof value === "number" && Number.isFinite(value)
        )) {
            return res.status(400).json({
                error: "Las mediciones deben ser números válidos"
            });
        }

        // Validar rangos de las mediciones

        if (
            distance_cm < 0 ||
            water_level_cm < 0 ||
            level_percent < 0 ||
            level_percent > 100
        ) {
            return res.status(400).json({
                error: "Las mediciones están fuera de rango"
            });
        }

        // Validar estado del agua

        if (!["bajo", "medio", "alto"].includes(water_status)) {
            return res.status(400).json({
                error: "Estado de agua inválido"
            });
        }

        // Validar posible fuga

        if (typeof (possible_leak ?? false) !== "boolean") {
            return res.status(400).json({
                error: "possible_leak debe ser de tipo boolean"
            });
        }

        // Preparar registro para Supabase
        // Los nombres corresponden a las columnas
        // de la tabla mediciones_agua

        const reading = {
            id_dispositivo: device_id || "ESP8266_01",
            distancia_cm: distance_cm,
            nivel_agua_cm: water_level_cm,
            nivel_porcentaje: level_percent,
            velocidad_cambio_cm_min: level_change_rate_cm_min ?? 0,
            estado_agua: water_status,
            posible_fuga: possible_leak ?? false
        };

        // Insertar los datos en Supabase

        const { data, error } = await supabase
            .from("mediciones_agua")
            .insert([reading])
            .select()
            .single();

        if (error) {
            console.error("Error de Supabase:", error);

            return res.status(500).json({
                error: "No se pudo guardar la medición",
                detalle: error.message,
                codigo: error.code
            });
        }

        // Confirmar registro exitoso

        res.status(201).json({
            mensaje: "Medición guardada correctamente",
            datos: data
        });

    } catch (error) {

        console.error("Error interno:", error);

        res.status(500).json({
            error: "Error interno del servidor",
            detalle: error.message
        });

    }
});

// Consultar datos históricos

app.get("/api/readings", async (req, res) => {
    try {

        // Definir cantidad de registros a consultar

        const requestedLimit = Number(req.query.limit || 50);

        // Validar límite de consulta

        if (
            !Number.isInteger(requestedLimit) ||
            requestedLimit < 1 ||
            requestedLimit > 200
        ) {
            return res.status(400).json({
                error: "El límite debe estar entre 1 y 200"
            });
        }

        // Consultar la tabla de Supabase

        const { data, error } = await supabase
            .from("mediciones_agua")
            .select("*")
            .order("fecha_medicion", { ascending: false })
            .limit(requestedLimit);


        if (error) {
            console.error("Error de Supabase:", error);

            return res.status(500).json({
                error: "No se pudo consultar el historial",
                detalle: error.message,
                codigo: error.code
            });
        }

        // Enviar resultados de la consulta

        res.json({
            total: data.length,
            mediciones: data
        });

    } catch (error) {

        console.error("Error al consultar mediciones:", error);

        res.status(500).json({
            error: "Error al consultar mediciones",
            detalle: error.message
        });

    }
});

// Iniciar servidor

app.listen(PORT, "0.0.0.0", () => {

    console.log("API de monitoreo de agua");
    console.log(`Servidor activo en puerto ${PORT}`);
    console.log(`URL local: http://localhost:${PORT}`);

});

