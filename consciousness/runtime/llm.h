#pragma once
#include <stdbool.h>
int  llm_init(void);
int  llm_feed(const char *text);
int  llm_think(char *out_buf, int out_sz, int max_tokens);
bool llm_is_ready(void);
int  llm_context_len(void);
void llm_shutdown(void);
