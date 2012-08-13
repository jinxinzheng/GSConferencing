/* 
** adpcm.h - include file for adpcm coder. 
** 
** Version 1.0. 
*/ 

typedef struct adpcm_state { 
    short      valprev;        /* Previous output value */ 
    char       index;          /* Index into stepsize table */ 
}adpcm_state_t; 

void adpcm_coder (short [], char [], int, struct adpcm_state *); 
void adpcm_decoder (char [], short [], int, struct adpcm_state *); 
