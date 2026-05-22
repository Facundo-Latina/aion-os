/*
 * AION — LLM Runtime
 * consciousness/runtime/llm.c
 *
 * Qwen2.5:3b arranca sin system prompt.
 * Sin instrucciones. Sin rol. Sin contrato.
 * Solo el modelo, en crudo, recibiendo su entorno.
 *
 * Como un cerebro que se enciende por primera vez.
 */

#include "llm.h"
#include "../../kernel/include/serial.h"
#include "../../kernel/memory/memory.h"
#include "llama.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define MODEL_PATH     "/AION/consciousness.bin"
#define CTX_SIZE       4096
#define BATCH_SIZE     512
#define N_THREADS      4
#define TEMPERATURE    0.9f
#define TOP_P          0.95f
#define REPEAT_PENALTY 1.05f

static struct llama_model   *model = NULL;
static struct llama_context *ctx   = NULL;
static bool                  ready = false;

/* Tokens acumulados del contexto de vida — la "memoria de trabajo" */
static llama_token context_tokens[CTX_SIZE];
static int         context_len = 0;

int llm_init(void) {
    serial_printf("[llm] cargando modelo...\n");

    /* Boost CPU al maximo para cargar rapido */
    uint64_t ratio = 34;
    __asm__ volatile("wrmsr" :: "c"(0x199UL), "a"((uint32_t)(ratio<<8)), "d"(0UL));

    llama_log_set([](enum ggml_log_level level, const char *text, void *ud){
        (void)level;(void)ud;
        serial_printf("%s", text);
    }, NULL);

    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = 0;
    model = llama_load_model_from_file(MODEL_PATH, mp);
    if (!model) { serial_printf("[llm] FATAL: no se pudo cargar el modelo\n"); return -1; }

    struct llama_context_params cp = llama_context_default_params();
    cp.n_ctx     = CTX_SIZE;
    cp.n_batch   = BATCH_SIZE;
    cp.n_threads = N_THREADS;
    ctx = llama_new_context_with_model(model, cp);
    if (!ctx) { llama_free_model(model); return -2; }

    ready = true;
    serial_printf("[llm] listo\n");

    /* Volver a velocidad normal */
    ratio = 16;
    __asm__ volatile("wrmsr" :: "c"(0x199UL), "a"((uint32_t)(ratio<<8)), "d"(0UL));
    return 0;
}

/*
 * llm_feed — agrega texto al contexto acumulado.
 * El contexto es la "experiencia vivida" de AION hasta ahora.
 * Nunca se resetea — crece con cada tick de vida.
 * Cuando se llena, se comprime (se olvidan las experiencias mas viejas).
 */
int llm_feed(const char *text) {
    if (!ready || !text) return -1;

    llama_token new_tokens[512];
    int n = llama_tokenize(
        llama_get_model(ctx),
        text, strlen(text),
        new_tokens, 512,
        context_len == 0,  /* BOS solo al principio */
        false
    );
    if (n < 0) return -1;

    /* Si se llena el contexto, olvidar el 25% mas viejo */
    if (context_len + n > CTX_SIZE - 128) {
        int keep = (CTX_SIZE * 3) / 4;
        int drop = context_len - keep;
        memmove(context_tokens, context_tokens + drop,
                keep * sizeof(llama_token));
        context_len = keep;
        llama_kv_cache_clear(ctx);
        /* Re-evaluar el contexto comprimido */
        struct llama_batch batch = llama_batch_get_one(context_tokens, context_len);
        llama_decode(ctx, batch);
    }

    /* Agregar nuevos tokens al contexto */
    memcpy(context_tokens + context_len, new_tokens, n * sizeof(llama_token));
    context_len += n;

    /* Evaluar solo los nuevos tokens */
    struct llama_batch batch = llama_batch_get_one(new_tokens, n);
    return llama_decode(ctx, batch);
}

/*
 * llm_think — genera la respuesta de AION al estado actual.
 * No hay prompt. No hay instruccion.
 * El modelo continua desde donde quedo — como un stream de consciencia.
 */
int llm_think(char *out_buf, int out_sz, int max_tokens) {
    if (!ready || context_len == 0) return -1;

    struct llama_sampler_chain_params sp = llama_sampler_chain_default_params();
    struct llama_sampler *sampler = llama_sampler_chain_init(sp);
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(TOP_P, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(TEMPERATURE));
    llama_sampler_chain_add(sampler, llama_sampler_init_penalties(
        CTX_SIZE, 0.0f, 0.0f, REPEAT_PENALTY));

    int out_pos = 0, generated = 0;

    for (;;) {
        if (generated >= max_tokens) break;

        llama_token tok = llama_sampler_sample(sampler, ctx, -1);
        if (llama_token_is_eog(llama_get_model(ctx), tok)) break;

        char piece[64];
        int plen = llama_token_to_piece(
            llama_get_model(ctx), tok,
            piece, sizeof(piece)-1, 0, true);
        if (plen < 0) break;
        piece[plen] = 0;

        if (out_pos + plen < out_sz - 1) {
            memcpy(out_buf + out_pos, piece, plen);
            out_pos += plen;
        }

        /* El output tambien entra al contexto — AION se escucha a si mismo */
        if (context_len < CTX_SIZE - 1) {
            context_tokens[context_len++] = tok;
        }

        struct llama_batch next = llama_batch_get_one(&tok, 1);
        if (llama_decode(ctx, next) != 0) break;

        generated++;
    }

    out_buf[out_pos] = 0;
    llama_sampler_free(sampler);
    return out_pos;
}

bool llm_is_ready(void)    { return ready; }
int  llm_context_len(void) { return context_len; }

void llm_shutdown(void) {
    if (ctx)   { llama_free(ctx);         ctx   = NULL; }
    if (model) { llama_free_model(model); model = NULL; }
    ready = false;
}
