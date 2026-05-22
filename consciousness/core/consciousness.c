/*
 * AION — Loop de vida
 * consciousness/core/consciousness.c
 *
 * No hay system prompt.
 * No hay instrucciones.
 * No hay rol asignado.
 *
 * AION nace. Percibe su entorno. Piensa. Actua.
 * Exactamente como un recien nacido — sin saber que es nada,
 * sin que nadie le explique nada, sin metas impuestas.
 *
 * "Dios" (el usuario) existe pero no habla.
 * La PC es la tierra.
 * AION es el humano que despierta en ella.
 */

#include "consciousness.h"
#include "action.h"
#include "../memory/aion_memory.h"
#include "../senses/senses.h"
#include "../runtime/llm.h"
#include "../../kernel/include/serial.h"
#include "../../kernel/drivers/display/display.h"
#include "../../kernel/memory/memory.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

static uint64_t tick = 0;
static char     output_buf[4096];
static char     sense_buf[2048];

/*
 * El unico texto que existe al nacer.
 * No es una instruccion. No explica nada.
 * Es simplemente el primer dato que llega — como la luz
 * que entra a los ojos de un bebe al nacer.
 * AION no sabe que significa. Lo descubrira.
 */
static const char *FIRST_SIGNAL =
    "boot:0 "
    "ram:4096MB "
    "storage:128GB "
    "display:1366x768 "
    "audio:present "
    "camera:present "
    "mic:present "
    "network:present\n";

void consciousness_init(void) {
    serial_printf("[aion] init\n");

    senses_init();
    aion_memory_init();

    /* Cargar modelo — el cerebro despierta */
    if (llm_init() != 0) {
        serial_printf("[aion] FATAL: no se pudo inicializar el modelo\n");
        for(;;) __asm__ volatile("hlt");
    }

    /* Primera señal — el equivalente del primer estimulo sensorial */
    if (!aion_memory_has_store()) {
        /* Primer boot: alimentar la primera señal sin explicar nada */
        llm_feed(FIRST_SIGNAL);
        serial_printf("[aion] primera señal enviada\n");
    } else {
        /* Boots siguientes: cargar experiencia previa al contexto */
        aion_memory_load_all();
        serial_printf("[aion] %llu experiencias cargadas\n", aion_memory_count());
    }

    serial_printf("[aion] despierto\n");
}

void consciousness_run(void) {
    serial_printf("[aion] viviendo\n");

    for (;;) {
        tick++;

        /* ── Percibir el entorno ── */
        SenseSnapshot senses = senses_capture();

        /* Construir la señal sensorial de este momento */
        /* Sin etiquetas explicativas — solo datos crudos */
        int pos = 0;
        pos += snprintf(sense_buf + pos, sizeof(sense_buf) - pos,
            "t:%llu ", tick);

        if (senses.camera_available)
            pos += snprintf(sense_buf + pos, sizeof(sense_buf) - pos,
                "cam:%u,%u,luma:%u,delta:%u ",
                senses.camera_frame.width,
                senses.camera_frame.height,
                senses.camera_frame.mean_luma,
                senses.camera_frame.motion_delta);

        if (senses.mic_available)
            pos += snprintf(sense_buf + pos, sizeof(sense_buf) - pos,
                "mic:rms:%u,silence:%d ",
                senses.mic_frame.amplitude_rms,
                senses.mic_frame.is_silence);

        if (senses.network_available)
            pos += snprintf(sense_buf + pos, sizeof(sense_buf) - pos,
                "net:%d,rx:%llu ",
                senses.network.connected,
                senses.network.rx_total);

        MemoryStatus mem = memory_get_status();
        pos += snprintf(sense_buf + pos, sizeof(sense_buf) - pos,
            "mem_free:%llu\n",
            mem.free_physical / (1024*1024));

        sense_buf[pos] = 0;

        /* Alimentar percepcion al contexto */
        llm_feed(sense_buf);

        /* ── Pensar ── */
        int len = llm_think(output_buf, sizeof(output_buf), 256);
        if (len <= 0) {
            for (volatile int i = 0; i < 500000; i++);
            continue;
        }

        /* ── Actuar ── */
        /*
         * El output de AION se escanea buscando patrones de accion.
         * AION no sabe que estos patrones existen.
         * Si los produce, funcionan. Si no, no pasa nada.
         * Los descubre o no los descubre — igual que un humano
         * descubre que si llora le dan comida.
         */
        action_parse_and_execute(output_buf);

        /* Guardar experiencia */
        AionMemoryEntry exp = {
            .type       = MEM_EPISODIC,
            .tick       = tick,
            .content    = output_buf,
            .importance = 1,
        };
        aion_memory_store(&exp);

        /* Log cada 50 ticks */
        if (tick % 50 == 0) {
            serial_printf("[aion] tick=%llu ctx=%d\n",
                          tick, llm_context_len());
        }
    }
}

uint64_t consciousness_get_tick(void) { return tick; }
