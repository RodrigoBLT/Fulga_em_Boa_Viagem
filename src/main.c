#include <windows.h>
#include <mmsystem.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "jogo.h"

static Jogador jogador = {JOGADOR_INICIO_X, JOGADOR_INICIO_Y, 3, 0, 1};
static ListaObstaculos listaObstaculos = {NULL};
static Pontuacao ranking[TAMANHO_RANKING] = {{0}, {0}, {0}, {0}, {0}};
static int jogoIniciado = 0;
static int jogoEncerrado = 0;
static int pontuacaoRegistrada = 0;
static int telaRegras = 0;
static int ondaOffset = 0;
static const char NOME_CLASSE_JANELA[] = "FugaEmBoaViagemWindow";

static volatile int musicaRodando = 0;
static volatile int estadoMusica = 0;
static HANDLE threadMusica = NULL;

// ----- LISTA ENCADEADA -----

static void inicializarListaObstaculos(ListaObstaculos *lista) {
    lista->inicio = NULL;
}

static int listaObstaculosVazia(ListaObstaculos *lista) {
    return lista->inicio == NULL;
}

static Obstaculo *criarObstaculo(int x, int y, int velocidade, int direcao) {
    Obstaculo *n = (Obstaculo *)malloc(sizeof(Obstaculo));
    if (n == NULL) return NULL;
    n->x = x; n->y = y; n->velocidade = velocidade;
    n->direcao = direcao; n->proximo = NULL;
    return n;
}

static void inserirObstaculo(ListaObstaculos *lista, Obstaculo *novo) {
    if (novo == NULL) return;
    if (lista->inicio == NULL) { lista->inicio = novo; return; }
    Obstaculo *atual = lista->inicio;
    while (atual->proximo != NULL) atual = atual->proximo;
    atual->proximo = novo;
}

static void moverObstaculos(ListaObstaculos *lista) {
    Obstaculo *atual = lista->inicio;
    while (atual != NULL) {
        atual->x += atual->velocidade * atual->direcao;
        atual = atual->proximo;
    }
}

static void removerObstaculosForaDaTela(ListaObstaculos *lista) {
    Obstaculo *atual = lista->inicio;
    while (atual != NULL) {
        if (atual->x < -TAMANHO_CELULA) {
            atual->x = LARGURA_JANELA + (rand() % 120);
            atual->y = INICIO_MAR + (rand() % QUANTIDADE_FAIXAS_TUBARAO) * TAMANHO_CELULA;
        } else if (atual->x > LARGURA_JANELA) {
            atual->x = -TAMANHO_CELULA - (rand() % 120);
            atual->y = INICIO_MAR + (rand() % QUANTIDADE_FAIXAS_TUBARAO) * TAMANHO_CELULA;
        }
        atual = atual->proximo;
    }
}

static void gerarObstaculoSeNecessario(ListaObstaculos *lista, Jogador *j) {
    if (!listaObstaculosVazia(lista)) return;
    int qtd = 2 + j->dificuldade;
    if (qtd > 10) qtd = 10;
    for (int i = 0; i < qtd; i++) {
        int y = INICIO_MAR + (rand() % QUANTIDADE_FAIXAS_TUBARAO) * TAMANHO_CELULA;
        int dir = (i % 2 == 0) ? 1 : -1;
        int x = i * (LARGURA_JANELA / qtd);
        int vel = 3 + j->dificuldade * 2;
        inserirObstaculo(lista, criarObstaculo(x, y, vel, dir));
    }
}

static void liberarListaObstaculos(ListaObstaculos *lista) {
    Obstaculo *atual = lista->inicio;
    while (atual != NULL) {
        Obstaculo *prox = atual->proximo;
        free(atual);
        atual = prox;
    }
    lista->inicio = NULL;
}

static int verificarColisao(ListaObstaculos *lista, Jogador *j) {
    Obstaculo *atual = lista->inicio;
    while (atual != NULL) {
        int ch = j->x < atual->x + TAMANHO_CELULA && j->x + TAMANHO_CELULA > atual->x;
        int cv = j->y < atual->y + TAMANHO_CELULA && j->y + TAMANHO_CELULA > atual->y;
        if (ch && cv) return 1;
        atual = atual->proximo;
    }
    return 0;
}

// ----- MUSICA (waveOut para audio mais alto) -----

// Gera e toca uma nota seno pura com fade in/out para evitar cliques
static void tocarNota(int freq, int durMs) {
    if (!musicaRodando || freq <= 0 || durMs <= 0) return;

    int n = 44100 * durMs / 1000;
    short *buf = (short *)malloc((size_t)n * 2);
    if (!buf) { Sleep(durMs); return; }

    int fade = 882; // 20ms de fade
    for (int i = 0; i < n; i++) {
        double env = 1.0;
        if (i < fade)       env = (double)i / fade;
        else if (i > n - fade) env = (double)(n - i) / fade;
        buf[i] = (short)(28000.0 * env * sin(6.28318530718 * freq * i / 44100.0));
    }

    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = 1;
    wfx.nSamplesPerSec  = 44100;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = 2;
    wfx.nAvgBytesPerSec = 88200;

    HWAVEOUT hwo = NULL;
    if (waveOutOpen(&hwo, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        free(buf);
        Sleep(durMs);
        return;
    }

    WAVEHDR hdr = {0};
    hdr.lpData        = (LPSTR)buf;
    hdr.dwBufferLength = (DWORD)(n * 2);

    waveOutPrepareHeader(hwo, &hdr, sizeof(hdr));
    waveOutWrite(hwo, &hdr, sizeof(hdr));

    while (!(hdr.dwFlags & WHDR_DONE)) {
        if (!musicaRodando) { waveOutReset(hwo); break; }
        Sleep(5);
    }

    waveOutUnprepareHeader(hwo, &hdr, sizeof(hdr));
    waveOutClose(hwo);
    free(buf);
}

static DWORD WINAPI executarMusica(LPVOID param) {
    (void)param;
    int intervalo = 600;
    int ciclos = 0;

    while (musicaRodando) {
        if (estadoMusica == 1) {
            // Tema do Tubarao: E3 e F3 acelerando
            tocarNota(165, intervalo);
            if (!musicaRodando || estadoMusica != 1) break;
            Sleep(60);
            tocarNota(175, intervalo);
            if (!musicaRodando || estadoMusica != 1) break;
            Sleep(120);
            if (++ciclos % 4 == 0)
                intervalo = (intervalo > 200) ? intervalo - 60 : 600;
        } else if (estadoMusica == 2) {
            // Fanfarra de avanco (C4-E4-G4-C5)
            int notas[] = {262, 330, 392, 523};
            int durs[]  = {150, 150, 150, 350};
            for (int i = 0; i < 4; i++) {
                if (!musicaRodando) return 0;
                tocarNota(notas[i], durs[i]);
                Sleep(30);
            }
            estadoMusica = 1;
            intervalo = 600;
            ciclos = 0;
        } else if (estadoMusica == 3) {
            // Game over (descendente triste)
            int notas[] = {392, 330, 262, 220, 165};
            int durs[]  = {300, 300, 400, 400, 700};
            for (int i = 0; i < 5; i++) {
                if (!musicaRodando) return 0;
                tocarNota(notas[i], durs[i]);
                Sleep(50);
            }
            musicaRodando = 0;
            return 0;
        } else {
            Sleep(50);
        }
    }
    return 0;
}

static void iniciarMusica(int estado) {
    musicaRodando = 0;
    estadoMusica = estado;
    if (threadMusica != NULL) { CloseHandle(threadMusica); threadMusica = NULL; }
    musicaRodando = 1;
    threadMusica = CreateThread(NULL, 0, executarMusica, NULL, 0, NULL);
}

static void encerrarMusica(void) {
    musicaRodando = 0;
    estadoMusica = 0;
    if (threadMusica != NULL) { CloseHandle(threadMusica); threadMusica = NULL; }
}

// ----- LOGICA DO JOGO -----

static void iniciarPartida(Jogador *j, ListaObstaculos *lista) {
    j->x = JOGADOR_INICIO_X; j->y = JOGADOR_INICIO_Y;
    j->vidas = 3; j->pontuacao = 0; j->dificuldade = 1;
    jogoIniciado = 1; jogoEncerrado = 0; pontuacaoRegistrada = 0; telaRegras = 0;
    inicializarListaObstaculos(lista);
    gerarObstaculoSeNecessario(lista, j);
    iniciarMusica(1);
}

static void atualizarDificuldade(Jogador *j) { j->dificuldade++; }
static void calcularPontuacao(Jogador *j) { j->pontuacao += 10; }

static void avancarNivel(Jogador *j, ListaObstaculos *lista) {
    j->pontuacao += 50;
    atualizarDificuldade(j);
    j->x = JOGADOR_INICIO_X; j->y = JOGADOR_INICIO_Y;
    liberarListaObstaculos(lista);
    gerarObstaculoSeNecessario(lista, j);
    iniciarMusica(2);
}

static void trocarPontuacoes(Pontuacao *a, Pontuacao *b) { Pontuacao t = *a; *a = *b; *b = t; }

static int particionarRanking(Pontuacao r[], int ini, int fim) {
    int pivo = r[fim].pontos, idx = ini - 1;
    for (int i = ini; i < fim; i++)
        if (r[i].pontos > pivo) { idx++; trocarPontuacoes(&r[idx], &r[i]); }
    trocarPontuacoes(&r[idx + 1], &r[fim]);
    return idx + 1;
}

static void quickSortRanking(Pontuacao r[], int ini, int fim) {
    if (ini < fim) {
        int p = particionarRanking(r, ini, fim);
        quickSortRanking(r, ini, p - 1);
        quickSortRanking(r, p + 1, fim);
    }
}

static void registrarPontuacao(Pontuacao r[], Jogador *j) {
    if (pontuacaoRegistrada) return;
    r[TAMANHO_RANKING - 1].pontos = j->pontuacao;
    quickSortRanking(r, 0, TAMANHO_RANKING - 1);
    pontuacaoRegistrada = 1;
}

// ----- DESENHO -----

static void desenharRetangulo(HDC hdc, int x, int y, int w, int h, COLORREF cor) {
    HBRUSH b = CreateSolidBrush(cor);
    RECT r = {x, y, x + w, y + h};
    FillRect(hdc, &r, b);
    DeleteObject(b);
}

static void desenharElipse(HDC hdc, int x, int y, int w, int h, COLORREF cor) {
    HBRUSH b = CreateSolidBrush(cor);
    HBRUSH bo = (HBRUSH)SelectObject(hdc, b);
    HPEN po = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, x, y, x + w, y + h);
    SelectObject(hdc, po); SelectObject(hdc, bo); DeleteObject(b);
}

static void desenharTriangulo(HDC hdc, POINT pts[], COLORREF cor) {
    HBRUSH b = CreateSolidBrush(cor);
    HBRUSH bo = (HBRUSH)SelectObject(hdc, b);
    HPEN po = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    Polygon(hdc, pts, 3);
    SelectObject(hdc, po); SelectObject(hdc, bo); DeleteObject(b);
}

static void desenharPoligono(HDC hdc, POINT pts[], int n, COLORREF cor) {
    HBRUSH b = CreateSolidBrush(cor);
    HBRUSH bo = (HBRUSH)SelectObject(hdc, b);
    HPEN po = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    Polygon(hdc, pts, n);
    SelectObject(hdc, po); SelectObject(hdc, bo); DeleteObject(b);
}

static void desenharTexto(HDC hdc, int x, int y, const char *txt, int sz, COLORREF cor) {
    HFONT f = CreateFontA(sz, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT fo = (HFONT)SelectObject(hdc, f);
    SetTextColor(hdc, cor); SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, x, y, txt, (int)strlen(txt));
    SelectObject(hdc, fo); DeleteObject(f);
}

static void desenharTextoCentrado(HDC hdc, int cx, int y, int w, const char *txt, int sz, COLORREF cor) {
    HFONT f = CreateFontA(sz, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    HFONT fo = (HFONT)SelectObject(hdc, f);
    SetTextColor(hdc, cor); SetBkMode(hdc, TRANSPARENT);
    RECT r = {cx - w / 2, y, cx + w / 2, y + sz + 8};
    DrawTextA(hdc, txt, -1, &r, DT_CENTER | DT_SINGLELINE);
    SelectObject(hdc, fo); DeleteObject(f);
}

// Coracao (icone de vida)
static void desenharCoracao(HDC hdc, int x, int y, COLORREF cor) {
    desenharElipse(hdc, x,     y, 10, 10, cor);
    desenharElipse(hdc, x + 6, y, 10, 10, cor);
    POINT pts[3] = {{x, y + 6}, {x + 16, y + 6}, {x + 8, y + 17}};
    desenharTriangulo(hdc, pts, cor);
    desenharRetangulo(hdc, x, y + 3, 16, 5, cor);
}

// Nuvem decorativa no ceu
static void desenharNuvem(HDC hdc, int x, int y) {
    desenharElipse(hdc, x,      y + 5, 30, 16, RGB(255, 255, 255));
    desenharElipse(hdc, x + 8,  y,     24, 18, RGB(255, 255, 255));
    desenharElipse(hdc, x + 22, y + 4, 22, 14, RGB(255, 255, 255));
}

// Placa de aviso de tubarao igual a de Boa Viagem (sem texto de alerta simples)
static void desenharPlacaTubarao(HDC hdc) {
    int sx = 44, sy = 76, sw = 122, sh = 100;
    int cx = sx + sw / 2;  // 105

    // Poste de madeira
    desenharRetangulo(hdc, cx - 4, sy + sh - 8, 8, 52, RGB(120, 80, 45));
    // Sombra do poste
    desenharRetangulo(hdc, cx + 2, sy + sh - 8, 3, 52, RGB(90, 55, 25));

    // Fundo branco do quadro
    desenharRetangulo(hdc, sx, sy, sw, sh, RGB(245, 245, 240));

    // Cabecalho vermelho: "PERIGO"
    desenharRetangulo(hdc, sx, sy, sw, 24, RGB(200, 20, 20));
    desenharTextoCentrado(hdc, cx, sy + 5, sw, "PERIGO", 14, RGB(255, 255, 255));

    // Sub-cabecalho vermelho escuro: area de tubarao
    desenharRetangulo(hdc, sx, sy + 24, sw, 15, RGB(165, 12, 12));
    desenharTextoCentrado(hdc, cx, sy + 26, sw, "AREA SUJEITA A TUBARAO", 9, RGB(255, 220, 180));

    // Faixa amarela com losango e silhueta de tubarao
    desenharRetangulo(hdc, sx, sy + 39, sw, 36, RGB(255, 200, 0));

    // Losango amarelo escuro dentro da faixa
    POINT losango[4] = {
        {cx,          sy + 41},
        {sx + sw - 8, sy + 57},
        {cx,          sy + 73},
        {sx + 8,      sy + 57}
    };
    desenharPoligono(hdc, losango, 4, RGB(240, 180, 0));

    // Silhueta do tubarao (preta) dentro do losango
    int bx = cx - 26, by = sy + 51;
    desenharElipse(hdc,  bx,      by,     52, 12, RGB(15, 15, 15));  // corpo
    POINT fin[3]  = {{bx + 22, by}, {bx + 28, by - 8}, {bx + 36, by + 1}};
    POINT tail[3] = {{bx,      by + 4}, {bx - 9, by - 2}, {bx - 9, by + 10}};
    desenharTriangulo(hdc, fin,  RGB(15, 15, 15));
    desenharTriangulo(hdc, tail, RGB(15, 15, 15));

    // Faixa branca: "DANGER - SHARK ZONE"
    desenharRetangulo(hdc, sx, sy + 75, sw, 13, RGB(255, 255, 255));
    desenharTextoCentrado(hdc, cx, sy + 77, sw, "DANGER - SHARK ZONE", 9, RGB(190, 0, 0));

    // Rodape vermelho: "EVITE BANHO DE MAR"
    desenharRetangulo(hdc, sx, sy + 88, sw, 12, RGB(200, 20, 20));
    desenharTextoCentrado(hdc, cx, sy + 90, sw, "EVITE BANHO DE MAR", 9, RGB(255, 255, 255));

    // Borda escura do quadro
    HPEN bpen = CreatePen(PS_SOLID, 2, RGB(70, 35, 5));
    HPEN bpOld = (HPEN)SelectObject(hdc, bpen);
    HBRUSH bnOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, sx, sy, sx + sw, sy + sh);
    SelectObject(hdc, bpOld); SelectObject(hdc, bnOld); DeleteObject(bpen);
}

// Cena de praia: guarda-sol listrado + 2 pessoas
static void desenharCenaPraia(HDC hdc, int cx, COLORREF cor1, COLORREF cor2) {
    int ay = INICIO_AREIA;

    POINT meiaEsq[3] = {{cx - 28, ay + 35}, {cx, ay + 5},  {cx,      ay + 35}};
    POINT meiaDrt[3] = {{cx,      ay + 35}, {cx, ay + 5},  {cx + 28, ay + 35}};
    desenharTriangulo(hdc, meiaEsq, cor1);
    desenharTriangulo(hdc, meiaDrt, cor2);
    desenharRetangulo(hdc, cx - 2, ay + 35, 4, 52, RGB(160, 120, 70));

    // Pessoa 1 (esquerda)
    desenharElipse(hdc,  cx - 18, ay + 66, 11, 11, RGB(220, 175, 120));
    desenharRetangulo(hdc, cx - 19, ay + 77, 13, 14, RGB(200, 80,  80));
    desenharRetangulo(hdc, cx - 21, ay + 89, 17,  6, RGB(200, 80,  80));
    desenharRetangulo(hdc, cx - 23, ay + 91, 20,  5, RGB(255, 200, 120));

    // Pessoa 2 (direita)
    desenharElipse(hdc,  cx +  7, ay + 66, 11, 11, RGB(240, 200, 150));
    desenharRetangulo(hdc, cx +  6, ay + 77, 13, 14, RGB(50, 100, 200));
    desenharRetangulo(hdc, cx +  5, ay + 89, 17,  6, RGB(50, 100, 200));
    desenharRetangulo(hdc, cx +  4, ay + 91, 20,  5, RGB(100, 200, 255));
}

static void desenharCenario(HDC hdc) {
    // Ceu
    desenharRetangulo(hdc, 0, 0,           LARGURA_JANELA, INICIO_AREIA,               RGB(100, 185, 228));
    // Gradiente visual do ceu: faixa mais clara no topo
    desenharRetangulo(hdc, 0, 0,           LARGURA_JANELA, 20,                          RGB(160, 215, 240));
    // Areia
    desenharRetangulo(hdc, 0, INICIO_AREIA, LARGURA_JANELA, INICIO_MAR - INICIO_AREIA,  RGB(238, 203, 145));
    // Mar
    desenharRetangulo(hdc, 0, INICIO_MAR,  LARGURA_JANELA, ALTURA_JANELA - INICIO_MAR,  RGB(30, 140, 185));

    // Sol com brilho
    desenharElipse(hdc, 700, 10, 50, 50, RGB(255, 230, 80));
    desenharElipse(hdc, 706, 16, 38, 38, RGB(255, 245, 140));

    // Nuvens
    desenharNuvem(hdc, 190, 8);
    desenharNuvem(hdc, 450, 14);

    // Linhas de grade apenas na areia
    for (int y = INICIO_AREIA; y < INICIO_MAR; y += TAMANHO_CELULA)
        desenharRetangulo(hdc, 0, y, LARGURA_JANELA, 2, RGB(220, 180, 118));

    // Animacao de ondas no mar (dois padroes defasados)
    for (int y = INICIO_MAR + ondaOffset; y < ALTURA_JANELA; y += 70)
        desenharRetangulo(hdc, 0, y, LARGURA_JANELA, 5, RGB(80, 175, 210));
    for (int y = INICIO_MAR + (ondaOffset + 35) % 70; y < ALTURA_JANELA; y += 70)
        desenharRetangulo(hdc, 0, y, LARGURA_JANELA, 2, RGB(130, 205, 225));

    // Espuma na beira do mar
    for (int x = 0; x < LARGURA_JANELA; x += 24) {
        int off = ((x / 24 + ondaOffset / 10) % 3) * 2;
        desenharElipse(hdc, x, INICIO_MAR + off, 16, 6, RGB(210, 238, 248));
    }

    // Placa oficial de aviso de tubarao
    desenharPlacaTubarao(hdc);

    // Tres cenas de praia tipicas de Boa Viagem
    desenharCenaPraia(hdc, 210, RGB(230, 50, 50), RGB(255, 255, 255));
    desenharCenaPraia(hdc, 395, RGB(30, 100, 200), RGB(255, 220, 50));
    desenharCenaPraia(hdc, 590, RGB(40, 160, 60), RGB(255, 220, 50));

    // Coqueiro tipico de Boa Viagem (canto direito)
    desenharRetangulo(hdc, 763, INICIO_AREIA + 12, 7, 98, RGB(139, 100, 60));
    POINT f1[3] = {{766, INICIO_AREIA + 12}, {800, INICIO_AREIA},      {785, INICIO_AREIA + 38}};
    POINT f2[3] = {{766, INICIO_AREIA + 12}, {728, INICIO_AREIA + 5},  {748, INICIO_AREIA + 38}};
    POINT f3[3] = {{766, INICIO_AREIA + 12}, {800, INICIO_AREIA + 32}, {788, INICIO_AREIA + 52}};
    desenharTriangulo(hdc, f1, RGB(55, 130, 45));
    desenharTriangulo(hdc, f2, RGB(65, 145, 50));
    desenharTriangulo(hdc, f3, RGB(50, 120, 40));
    desenharElipse(hdc, 770, INICIO_AREIA + 20, 9, 9, RGB(130, 100, 50));
    desenharElipse(hdc, 759, INICIO_AREIA + 24, 8, 8, RGB(110, 85,  40));

    // Titulo no ceu
    desenharTexto(hdc, 22, 16, "Fuga em Boa Viagem",                       26, RGB(10, 55, 80));
    desenharTexto(hdc, 22, 45, "Atravesse a praia e sobreviva aos perigos.", 14, RGB(10, 55, 80));
}

static void desenharJogador(HDC hdc) {
    int cx = jogador.x + TAMANHO_CELULA / 2;
    int ty = jogador.y + 6;
    POINT cabelo[3] = {{cx - 9, ty + 4}, {cx, ty - 3}, {cx + 9, ty + 4}};

    desenharElipse(hdc, cx - 9, ty - 2, 18, 10, RGB(80, 45, 25));
    desenharElipse(hdc, cx - 7, ty,     14, 14, RGB(245, 190, 140));
    desenharTriangulo(hdc, cabelo, RGB(80, 45, 25));
    desenharElipse(hdc, cx - 3, ty + 6, 2, 2, RGB(30, 30, 30));
    desenharElipse(hdc, cx + 3, ty + 6, 2, 2, RGB(30, 30, 30));
    desenharRetangulo(hdc, cx - 9,  ty + 15, 18, 16, RGB(230, 65, 70));
    desenharRetangulo(hdc, cx - 11, ty + 20, 22,  4, RGB(255, 235, 120));
    desenharRetangulo(hdc, cx - 15, ty + 18,  6, 15, RGB(245, 190, 140));
    desenharRetangulo(hdc, cx + 9,  ty + 18,  6, 15, RGB(245, 190, 140));
    desenharRetangulo(hdc, cx - 8,  ty + 31,  7, 11, RGB(30, 80, 140));
    desenharRetangulo(hdc, cx + 1,  ty + 31,  7, 11, RGB(30, 80, 140));
}

static void desenharObstaculos(HDC hdc, ListaObstaculos *lista) {
    if (listaObstaculosVazia(lista)) return;
    Obstaculo *atual = lista->inicio;
    while (atual != NULL) {
        int x = atual->x, y = atual->y;
        int dr = atual->direcao == 1;
        POINT cauda[3]   = {{dr ? x : x+TAMANHO_CELULA, y+20},{dr ? x-12 : x+TAMANHO_CELULA+12, y+8},{dr ? x-12 : x+TAMANHO_CELULA+12, y+32}};
        POINT barbatana[3] = {{x+18, y+8},{x+25, y-8},{x+31, y+10}};
        POINT boca[3]    = {{dr ? x+35 : x+5, y+23},{dr ? x+27 : x+13, y+19},{dr ? x+27 : x+13, y+27}};
        desenharTriangulo(hdc, cauda, RGB(45, 55, 65));
        desenharElipse(hdc, x, y+8, TAMANHO_CELULA, 24, RGB(45, 55, 65));
        desenharTriangulo(hdc, barbatana, RGB(35, 45, 55));
        desenharElipse(hdc, x+9, y+23, 22, 5, RGB(245, 245, 245));
        desenharTriangulo(hdc, boca, RGB(210, 40, 45));
        desenharElipse(hdc, dr ? x+29 : x+8, y+16, 4, 4, RGB(255, 255, 255));
        desenharElipse(hdc, dr ? x+31 : x+10, y+17, 2, 2, RGB(0, 0, 0));
        atual = atual->proximo;
    }
}

static void desenharHud(HDC hdc) {
    // Painel do HUD
    desenharRetangulo(hdc, 607, 5, 188, 72, RGB(240, 250, 248));
    desenharRetangulo(hdc, 607, 5, 188,  3, RGB(35, 150, 190));

    // Corações (cheios = vidas restantes, vazios = vidas perdidas)
    for (int i = 0; i < 3; i++) {
        COLORREF cor = (i < jogador.vidas) ? RGB(215, 35, 55) : RGB(185, 185, 185);
        desenharCoracao(hdc, 614 + i * 22, 12, cor);
    }

    char txt[64];
    snprintf(txt, sizeof(txt), "Pontos: %d", jogador.pontuacao);
    desenharTexto(hdc, 614, 36, txt, 16, RGB(15, 60, 85));

    snprintf(txt, sizeof(txt), "Nivel: %d", jogador.dificuldade);
    desenharTexto(hdc, 614, 54, txt, 16, RGB(15, 60, 85));

    // Overlay de fim de jogo (centralizado e alinhado)
    if (jogoEncerrado) {
        int ox = 200, oy = 208, ow = 400, oh = 165;
        // Sombra
        desenharRetangulo(hdc, ox + 4, oy + 4, ow, oh, RGB(80, 80, 80));
        // Painel principal
        desenharRetangulo(hdc, ox, oy, ow, oh, RGB(252, 238, 205));
        // Barra superior vermelha
        desenharRetangulo(hdc, ox, oy, ow, 7, RGB(160, 20, 30));
        // Barra inferior vermelha
        desenharRetangulo(hdc, ox, oy + oh - 7, ow, 7, RGB(160, 20, 30));
        // Titulo
        desenharTextoCentrado(hdc, ox + ow/2, oy + 18, ow, "FIM DE JOGO", 34, RGB(155, 18, 28));
        // Pontuacao
        char score[64];
        snprintf(score, sizeof(score), "Pontuacao final:  %d pts", jogador.pontuacao);
        desenharTextoCentrado(hdc, ox + ow/2, oy + 72, ow, score, 18, RGB(15, 60, 85));
        // Instrucoes (centralizadas)
        desenharTextoCentrado(hdc, ox + ow/2, oy + 104, ow, "R  para reiniciar", 17, RGB(15, 60, 85));
        desenharTextoCentrado(hdc, ox + ow/2, oy + 128, ow, "ESC  para sair",    17, RGB(15, 60, 85));
    }
}

static void exibirRanking(HDC hdc, Pontuacao r[], int tam) {
    char txt[64];
    int rx = 248, ry = 388, rw = 304, rh = 175;

    // Sombra
    desenharRetangulo(hdc, rx + 3, ry + 3, rw, rh, RGB(80, 80, 80));
    // Painel
    desenharRetangulo(hdc, rx, ry, rw, rh, RGB(232, 248, 244));
    desenharRetangulo(hdc, rx, ry, rw,   5, RGB(35, 150, 190));

    desenharTextoCentrado(hdc, rx + rw/2, ry + 10, rw, "RANKING", 20, RGB(15, 60, 85));

    // Linha divisora
    desenharRetangulo(hdc, rx + 15, ry + 36, rw - 30, 2, RGB(35, 150, 190));

    for (int i = 0; i < tam; i++) {
        // Ouro / Prata / Bronze para top 3
        COLORREF cor;
        if      (i == 0) cor = RGB(180, 140, 0);
        else if (i == 1) cor = RGB(120, 130, 145);
        else if (i == 2) cor = RGB(150, 90, 50);
        else             cor = RGB(15, 60, 85);

        snprintf(txt, sizeof(txt), "%d.   %d pts", i + 1, r[i].pontos);
        desenharTexto(hdc, rx + 32, ry + 44 + i * 24, txt, 17, cor);
    }
}

// Menu inicial com itens centralizados
static void exibirMenu(HDC hdc) {
    int px = 155, py = 155, pw = 490, ph = 250;
    int cx = px + pw / 2;  // 400

    // Sombra
    desenharRetangulo(hdc, px + 5, py + 5, pw, ph, RGB(60, 80, 90));
    // Painel
    desenharRetangulo(hdc, px, py, pw, ph, RGB(252, 238, 205));
    desenharRetangulo(hdc, px, py, pw,   8, RGB(35, 150, 190));

    // Titulo
    desenharTextoCentrado(hdc, cx, py + 22, pw, "Fuga em Boa Viagem",           36, RGB(15, 60, 85));
    // Linha divisora
    desenharRetangulo(hdc, px + 20, py + 68, pw - 40, 2, RGB(35, 150, 190));
    // Subtitulo
    desenharTextoCentrado(hdc, cx, py + 78, pw, "Atravesse a praia e desvie dos tubaroes!", 16, RGB(40, 85, 110));

    // Botao ENTER
    int bx = px + 80, by = py + 112, bw = 330, bh = 40;
    desenharRetangulo(hdc, bx + 3, by + 3, bw, bh, RGB(15, 90, 130));  // sombra botao
    desenharRetangulo(hdc, bx, by, bw, bh, RGB(35, 150, 190));
    desenharTextoCentrado(hdc, cx, by + 10, bw, "ENTER  para jogar", 20, RGB(255, 255, 255));

    // Link regras
    desenharTextoCentrado(hdc, cx, py + 170, pw, "I  para ver as regras", 17, RGB(15, 60, 85));

    // Dica controles
    desenharTextoCentrado(hdc, cx, py + 202, pw, "WASD  ou  setas  para  mover", 14, RGB(75, 115, 135));

    // Top score se existir
    if (ranking[0].pontos > 0) {
        char score[64];
        snprintf(score, sizeof(score), "Recorde: %d pts", ranking[0].pontos);
        desenharTextoCentrado(hdc, cx, py + 226, pw, score, 13, RGB(160, 120, 0));
    }
}

// Tela de regras do jogo
static void exibirRegras(HDC hdc) {
    int px = 125, py = 68, pw = 550, ph = 460;
    int cx = px + pw / 2;  // 400

    // Sombra
    desenharRetangulo(hdc, px + 5, py + 5, pw, ph, RGB(60, 80, 90));
    // Painel
    desenharRetangulo(hdc, px, py, pw, ph, RGB(252, 242, 218));
    desenharRetangulo(hdc, px, py, pw,   8, RGB(35, 150, 190));

    desenharTextoCentrado(hdc, cx, py + 18, pw, "Regras do Jogo", 28, RGB(15, 60, 85));
    desenharRetangulo(hdc, px + 20, py + 55, pw - 40, 2, RGB(35, 150, 190));

    desenharTexto(hdc, px + 28, py + 64,  "Objetivo:",                                          18, RGB(15, 60, 85));
    desenharTexto(hdc, px + 28, py + 86,  "Atravesse o mar, desvie dos tubaroes e chegue",      15, RGB(55, 95, 115));
    desenharTexto(hdc, px + 28, py + 104, "ao topo da tela para completar o nivel!",             15, RGB(55, 95, 115));
    desenharRetangulo(hdc, px + 20, py + 126, pw - 40, 2, RGB(195, 215, 205));

    desenharTexto(hdc, px + 28, py + 134, "Controles:",                                         18, RGB(15, 60, 85));
    desenharTexto(hdc, px + 44, py + 156, "W  ou  seta cima    ->  mover para cima",             15, RGB(55, 95, 115));
    desenharTexto(hdc, px + 44, py + 174, "S  ou  seta baixo   ->  mover para baixo",            15, RGB(55, 95, 115));
    desenharTexto(hdc, px + 44, py + 192, "A  ou  seta esq.    ->  mover para esquerda",         15, RGB(55, 95, 115));
    desenharTexto(hdc, px + 44, py + 210, "D  ou  seta dir.    ->  mover para direita",          15, RGB(55, 95, 115));
    desenharRetangulo(hdc, px + 20, py + 232, pw - 40, 2, RGB(195, 215, 205));

    desenharTexto(hdc, px + 28, py + 240, "Pontuacao:",                                         18, RGB(15, 60, 85));
    desenharTexto(hdc, px + 44, py + 262, "10 pontos por linha avancada",                        15, RGB(55, 95, 115));
    desenharTexto(hdc, px + 44, py + 280, "50 pontos bonus ao completar um nivel",               15, RGB(55, 95, 115));
    desenharRetangulo(hdc, px + 20, py + 302, pw - 40, 2, RGB(195, 215, 205));

    desenharTexto(hdc, px + 28, py + 310, "Vidas e perigos:",                                   18, RGB(15, 60, 85));
    desenharTexto(hdc, px + 44, py + 332, "Voce tem 3 vidas - cada tubarao tira uma",            15, RGB(55, 95, 115));
    desenharTexto(hdc, px + 44, py + 350, "A dificuldade aumenta a cada nivel",                  15, RGB(55, 95, 115));
    desenharTexto(hdc, px + 44, py + 368, "No ranking ficam as 5 melhores pontuacoes",           15, RGB(55, 95, 115));

    // Botao voltar
    int bbx = cx - 130, bby = py + 408;
    desenharRetangulo(hdc, bbx + 3, bby + 3, 260, 36, RGB(15, 90, 130));
    desenharRetangulo(hdc, bbx, bby, 260, 36, RGB(35, 150, 190));
    desenharTextoCentrado(hdc, cx, bby + 8, 260, "ESC  para voltar", 18, RGB(255, 255, 255));
}

// Double buffering: desenha em bitmap fora da tela, depois copia para evitar piscar
static void desenharTelaJogo(HWND janela) {
    PAINTSTRUCT ps;
    HDC hdcTela = BeginPaint(janela, &ps);

    HDC hdc = CreateCompatibleDC(hdcTela);
    HBITMAP hbm = CreateCompatibleBitmap(hdcTela, LARGURA_JANELA, ALTURA_JANELA);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdc, hbm);

    // Desenha tudo no buffer
    desenharCenario(hdc);

    if (!jogoIniciado) {
        if (telaRegras) exibirRegras(hdc);
        else            exibirMenu(hdc);
    } else {
        desenharObstaculos(hdc, &listaObstaculos);
        desenharJogador(hdc);
        desenharHud(hdc);
        if (jogoEncerrado) exibirRanking(hdc, ranking, TAMANHO_RANKING);
    }

    // Copia buffer para a tela (sem flickering)
    BitBlt(hdcTela, 0, 0, LARGURA_JANELA, ALTURA_JANELA, hdc, 0, 0, SRCCOPY);

    SelectObject(hdc, hbmOld);
    DeleteObject(hbm);
    DeleteDC(hdc);
    EndPaint(janela, &ps);
}

static void moverJogador(HWND janela, int mx, int my) {
    int nx = jogador.x + mx * TAMANHO_CELULA;
    int ny = jogador.y + my * TAMANHO_CELULA;
    int chegou = my < 0 && ny < INICIO_AREIA;

    if (nx >= 0 && nx <= LARGURA_JANELA - TAMANHO_CELULA) jogador.x = nx;
    if (ny >= INICIO_AREIA && ny <= ALTURA_JANELA - TAMANHO_CELULA) jogador.y = ny;
    if (my < 0) calcularPontuacao(&jogador);
    if (chegou) avancarNivel(&jogador, &listaObstaculos);

    InvalidateRect(janela, NULL, FALSE);
}

static LRESULT CALLBACK processarMensagemJanela(HWND janela, UINT msg, WPARAM tecla, LPARAM param) {
    switch (msg) {
        case WM_ERASEBKGND:
            // Suprime o apagamento de fundo (double buffer cuida disso)
            return 1;

        case WM_KEYDOWN:
            switch (tecla) {
                case VK_RETURN:
                    if (!jogoIniciado && !telaRegras) {
                        iniciarPartida(&jogador, &listaObstaculos);
                        InvalidateRect(janela, NULL, FALSE);
                    }
                    break;
                case 'I':
                    if (!jogoIniciado && !telaRegras) {
                        telaRegras = 1;
                        InvalidateRect(janela, NULL, FALSE);
                    }
                    break;
                case VK_LEFT: case 'A':
                    if (jogoIniciado && !jogoEncerrado) moverJogador(janela, -1, 0);
                    break;
                case VK_RIGHT: case 'D':
                    if (jogoIniciado && !jogoEncerrado) moverJogador(janela, 1, 0);
                    break;
                case VK_UP: case 'W':
                    if (jogoIniciado && !jogoEncerrado) moverJogador(janela, 0, -1);
                    break;
                case VK_DOWN: case 'S':
                    if (jogoIniciado && !jogoEncerrado) moverJogador(janela, 0, 1);
                    break;
                case VK_ESCAPE:
                    if (telaRegras) { telaRegras = 0; InvalidateRect(janela, NULL, FALSE); }
                    else PostQuitMessage(0);
                    break;
                case 'R':
                    if (jogoEncerrado) {
                        liberarListaObstaculos(&listaObstaculos);
                        iniciarPartida(&jogador, &listaObstaculos);
                        InvalidateRect(janela, NULL, FALSE);
                    }
                    break;
            }
            return 0;

        case WM_PAINT:
            desenharTelaJogo(janela);
            return 0;

        case WM_TIMER:
            // Sempre anima ondas (mesmo no menu e game over)
            ondaOffset = (ondaOffset + 2) % 70;

            if (!jogoIniciado || jogoEncerrado) {
                InvalidateRect(janela, NULL, FALSE);
                return 0;
            }

            moverObstaculos(&listaObstaculos);
            removerObstaculosForaDaTela(&listaObstaculos);
            gerarObstaculoSeNecessario(&listaObstaculos, &jogador);

            if (verificarColisao(&listaObstaculos, &jogador)) {
                jogador.vidas--;
                jogador.x = JOGADOR_INICIO_X;
                jogador.y = JOGADOR_INICIO_Y;

                if (jogador.vidas <= 0) {
                    jogoEncerrado = 1;
                    registrarPontuacao(ranking, &jogador);
                    iniciarMusica(3);
                }
            }

            InvalidateRect(janela, NULL, FALSE);
            return 0;

        case WM_DESTROY:
            KillTimer(janela, ID_TIMER_JOGO);
            liberarListaObstaculos(&listaObstaculos);
            encerrarMusica();
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(janela, msg, tecla, param);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine;

    srand((unsigned int)time(NULL));
    inicializarListaObstaculos(&listaObstaculos);

    WNDCLASSA cls = {0};
    cls.lpfnWndProc   = processarMensagemJanela;
    cls.hInstance     = hInstance;
    cls.lpszClassName = NOME_CLASSE_JANELA;
    cls.hCursor       = LoadCursor(NULL, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);  // sem apagamento automatico

    if (!RegisterClassA(&cls)) {
        MessageBoxA(NULL, "Nao foi possivel registrar a janela.", "Erro", MB_ICONERROR);
        return 1;
    }

    HWND janela = CreateWindowExA(0, NOME_CLASSE_JANELA, "Fuga em Boa Viagem",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, LARGURA_JANELA, ALTURA_JANELA,
        NULL, NULL, hInstance, NULL);

    if (janela == NULL) {
        MessageBoxA(NULL, "Nao foi possivel criar a janela.", "Erro", MB_ICONERROR);
        return 1;
    }

    ShowWindow(janela, nCmdShow);
    UpdateWindow(janela);
    SetTimer(janela, ID_TIMER_JOGO, 40, NULL);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return 0;
}
