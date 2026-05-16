#ifndef JOGO_H
#define JOGO_H

#define LARGURA_JANELA 800
#define ALTURA_JANELA 600
#define TAMANHO_CELULA 40
#define TAMANHO_JOGADOR 28
#define JOGADOR_INICIO_X 360
#define JOGADOR_INICIO_Y 520
#define ID_TIMER_JOGO 1
#define TAMANHO_RANKING 5

typedef struct {
    int x;
    int y;
    int vidas;
    int pontuacao;
    int dificuldade;
} Jogador;

typedef struct Obstaculo {
    int x;
    int y;
    int velocidade;
    int direcao;
    struct Obstaculo *proximo;
} Obstaculo;

typedef struct {
    Obstaculo *inicio;
} ListaObstaculos;

typedef struct {
    int pontos;
} Pontuacao;

#endif

