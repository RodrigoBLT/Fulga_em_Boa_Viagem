#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LARGURA_JANELA 800
#define ALTURA_JANELA 600
#define TAMANHO_CELULA 40
#define TAMANHO_JOGADOR 28
#define JOGADOR_INICIO_X 360
#define JOGADOR_INICIO_Y 520

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

static Jogador jogador = {JOGADOR_INICIO_X, JOGADOR_INICIO_Y, 3, 0, 1};
static ListaObstaculos listaObstaculos = {NULL};
static const char NOME_CLASSE_JANELA[] = "FugaEmBoaViagemWindow";

static void inicializarListaObstaculos(ListaObstaculos *lista) {
    lista->inicio = NULL;
}

static int listaObstaculosVazia(ListaObstaculos *lista) {
    return lista->inicio == NULL;
}

static Obstaculo *criarObstaculo(int x, int y, int velocidade, int direcao) {
    Obstaculo *novoObstaculo = (Obstaculo *)malloc(sizeof(Obstaculo));

    if (novoObstaculo == NULL) {
        return NULL;
    }

    novoObstaculo->x = x;
    novoObstaculo->y = y;
    novoObstaculo->velocidade = velocidade;
    novoObstaculo->direcao = direcao;
    novoObstaculo->proximo = NULL;

    return novoObstaculo;
}

static void inserirObstaculo(ListaObstaculos *lista, Obstaculo *novoObstaculo) {
    if (novoObstaculo == NULL) {
        return;
    }

    if (lista->inicio == NULL) {
        lista->inicio = novoObstaculo;
        return;
    }

    Obstaculo *atual = lista->inicio;

    while (atual->proximo != NULL) {
        atual = atual->proximo;
    }

    atual->proximo = novoObstaculo;
}

static void iniciarPartida(Jogador *jogadorAtual, ListaObstaculos *lista) {
    jogadorAtual->x = JOGADOR_INICIO_X;
    jogadorAtual->y = JOGADOR_INICIO_Y;
    jogadorAtual->vidas = 3;
    jogadorAtual->pontuacao = 0;
    jogadorAtual->dificuldade = 1;

    inicializarListaObstaculos(lista);
    inserirObstaculo(lista, criarObstaculo(120, 280, 4, 1));
    inserirObstaculo(lista, criarObstaculo(520, 360, 5, -1));
}

static void desenharRetangulo(HDC hdc, int x, int y, int largura, int altura, COLORREF cor) {
    HBRUSH pincel = CreateSolidBrush(cor);
    RECT area = {x, y, x + largura, y + altura};
    FillRect(hdc, &area, pincel);
    DeleteObject(pincel);
}

static void desenharTexto(HDC hdc, int x, int y, const char *texto, int tamanho, COLORREF cor) {
    HFONT fonte = CreateFontA(
        tamanho, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial"
    );

    HFONT fonteAnterior = (HFONT)SelectObject(hdc, fonte);
    SetTextColor(hdc, cor);
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, x, y, texto, (int)strlen(texto));
    SelectObject(hdc, fonteAnterior);
    DeleteObject(fonte);
}

static void desenharCenario(HDC hdc) {
    desenharRetangulo(hdc, 0, 0, LARGURA_JANELA, 70, RGB(135, 206, 235));
    desenharRetangulo(hdc, 0, 70, LARGURA_JANELA, 250, RGB(238, 203, 145));
    desenharRetangulo(hdc, 0, 250, LARGURA_JANELA, 350, RGB(35, 150, 190));

    for (int y = 70; y < ALTURA_JANELA; y += TAMANHO_CELULA) {
        COLORREF linha = (y < 250) ? RGB(224, 185, 125) : RGB(80, 190, 210);
        desenharRetangulo(hdc, 0, y, LARGURA_JANELA, 2, linha);
    }

    desenharTexto(hdc, 22, 18, "Fuga em Boa Viagem", 26, RGB(20, 70, 90));
    desenharTexto(hdc, 22, 48, "Atravesse a praia e sobreviva aos perigos.", 15, RGB(20, 70, 90));
}

static void desenharJogador(HDC hdc) {
    desenharRetangulo(
        hdc,
        jogador.x + (TAMANHO_CELULA - TAMANHO_JOGADOR) / 2,
        jogador.y + (TAMANHO_CELULA - TAMANHO_JOGADOR) / 2,
        TAMANHO_JOGADOR,
        TAMANHO_JOGADOR,
        RGB(230, 65, 70)
    );
}

static void desenharObstaculos(HDC hdc, ListaObstaculos *lista) {
    if (listaObstaculosVazia(lista)) {
        return;
    }

    Obstaculo *atual = lista->inicio;

    while (atual != NULL) {
        desenharRetangulo(hdc, atual->x, atual->y, TAMANHO_CELULA, TAMANHO_CELULA, RGB(45, 55, 65));
        desenharTexto(hdc, atual->x + 8, atual->y + 8, "T", 20, RGB(255, 255, 255));
        atual = atual->proximo;
    }
}

static void desenharHud(HDC hdc) {
    char texto[128];

    snprintf(texto, sizeof(texto), "Vidas: %d", jogador.vidas);
    desenharTexto(hdc, 620, 18, texto, 18, RGB(20, 70, 90));

    snprintf(texto, sizeof(texto), "Pontos: %d", jogador.pontuacao);
    desenharTexto(hdc, 620, 40, texto, 18, RGB(20, 70, 90));

    snprintf(texto, sizeof(texto), "Nivel: %d", jogador.dificuldade);
    desenharTexto(hdc, 620, 62, texto, 18, RGB(20, 70, 90));
}

static void desenharTelaJogo(HWND janela) {
    PAINTSTRUCT pintura;
    HDC hdc = BeginPaint(janela, &pintura);

    desenharCenario(hdc);
    desenharObstaculos(hdc, &listaObstaculos);
    desenharJogador(hdc);
    desenharHud(hdc);

    EndPaint(janela, &pintura);
}

static void moverJogador(HWND janela, int movimentoX, int movimentoY) {
    int novoX = jogador.x + movimentoX * TAMANHO_CELULA;
    int novoY = jogador.y + movimentoY * TAMANHO_CELULA;

    if (novoX >= 0 && novoX <= LARGURA_JANELA - TAMANHO_CELULA) {
        jogador.x = novoX;
    }

    if (novoY >= 70 && novoY <= ALTURA_JANELA - TAMANHO_CELULA) {
        jogador.y = novoY;
    }

    if (movimentoY < 0) {
        jogador.pontuacao += 10;
        jogador.dificuldade = 1 + jogador.pontuacao / 100;
    }

    InvalidateRect(janela, NULL, TRUE);
}

static LRESULT CALLBACK processarMensagemJanela(HWND janela, UINT mensagem, WPARAM tecla, LPARAM parametro) {
    switch (mensagem) {
        case WM_KEYDOWN:
            switch (tecla) {
                case VK_LEFT:
                case 'A':
                    moverJogador(janela, -1, 0);
                    break;
                case VK_RIGHT:
                case 'D':
                    moverJogador(janela, 1, 0);
                    break;
                case VK_UP:
                case 'W':
                    moverJogador(janela, 0, -1);
                    break;
                case VK_DOWN:
                case 'S':
                    moverJogador(janela, 0, 1);
                    break;
                case VK_ESCAPE:
                    PostQuitMessage(0);
                    break;
            }
            return 0;

        case WM_PAINT:
            desenharTelaJogo(janela);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(janela, mensagem, tecla, parametro);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    iniciarPartida(&jogador, &listaObstaculos);

    WNDCLASSA classeJanela = {0};
    classeJanela.lpfnWndProc = processarMensagemJanela;
    classeJanela.hInstance = hInstance;
    classeJanela.lpszClassName = NOME_CLASSE_JANELA;
    classeJanela.hCursor = LoadCursor(NULL, IDC_ARROW);
    classeJanela.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&classeJanela)) {
        MessageBoxA(NULL, "Nao foi possivel registrar a janela.", "Erro", MB_ICONERROR);
        return 1;
    }

    HWND janela = CreateWindowExA(
        0,
        NOME_CLASSE_JANELA,
        "Fuga em Boa Viagem",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        LARGURA_JANELA,
        ALTURA_JANELA,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (janela == NULL) {
        MessageBoxA(NULL, "Nao foi possivel criar a janela.", "Erro", MB_ICONERROR);
        return 1;
    }

    ShowWindow(janela, nCmdShow);
    UpdateWindow(janela);

    MSG mensagem;
    while (GetMessage(&mensagem, NULL, 0, 0) > 0) {
        TranslateMessage(&mensagem);
        DispatchMessage(&mensagem);
    }

    return 0;
}
