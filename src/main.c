#include <windows.h>
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

static void gerarObstaculoSeNecessario(ListaObstaculos *lista, Jogador *jogadorAtual) {
    if (!listaObstaculosVazia(lista)) {
        return;
    }

    int quantidade = 2 + jogadorAtual->dificuldade;

    if (quantidade > 10) {
        quantidade = 10;
    }

    for (int i = 0; i < quantidade; i++) {
        int y = INICIO_MAR + (rand() % QUANTIDADE_FAIXAS_TUBARAO) * TAMANHO_CELULA;
        int direcao = (i % 2 == 0) ? 1 : -1;
        int espaco = LARGURA_JANELA / quantidade;
        int x = i * espaco;
        int velocidade = 3 + jogadorAtual->dificuldade * 2;

        inserirObstaculo(lista, criarObstaculo(x, y, velocidade, direcao));
    }
}

static void liberarListaObstaculos(ListaObstaculos *lista) {
    Obstaculo *atual = lista->inicio;

    while (atual != NULL) {
        Obstaculo *proximo = atual->proximo;
        free(atual);
        atual = proximo;
    }

    lista->inicio = NULL;
}

static int verificarColisao(ListaObstaculos *lista, Jogador *jogadorAtual) {
    Obstaculo *atual = lista->inicio;

    while (atual != NULL) {
        int colideHorizontal = jogadorAtual->x < atual->x + TAMANHO_CELULA &&
            jogadorAtual->x + TAMANHO_CELULA > atual->x;
        int colideVertical = jogadorAtual->y < atual->y + TAMANHO_CELULA &&
            jogadorAtual->y + TAMANHO_CELULA > atual->y;

        if (colideHorizontal && colideVertical) {
            return 1;
        }

        atual = atual->proximo;
    }

    return 0;
}

static void iniciarPartida(Jogador *jogadorAtual, ListaObstaculos *lista) {
    jogadorAtual->x = JOGADOR_INICIO_X;
    jogadorAtual->y = JOGADOR_INICIO_Y;
    jogadorAtual->vidas = 3;
    jogadorAtual->pontuacao = 0;
    jogadorAtual->dificuldade = 1;
    jogoIniciado = 1;
    jogoEncerrado = 0;
    pontuacaoRegistrada = 0;

    inicializarListaObstaculos(lista);
    gerarObstaculoSeNecessario(lista, jogadorAtual);
}

static void atualizarDificuldade(Jogador *jogadorAtual) {
    jogadorAtual->dificuldade++;
}

static void calcularPontuacao(Jogador *jogadorAtual) {
    jogadorAtual->pontuacao += 10;
}

static void avancarNivel(Jogador *jogadorAtual, ListaObstaculos *lista) {
    jogadorAtual->pontuacao += 50;
    atualizarDificuldade(jogadorAtual);
    jogadorAtual->x = JOGADOR_INICIO_X;
    jogadorAtual->y = JOGADOR_INICIO_Y;

    liberarListaObstaculos(lista);
    gerarObstaculoSeNecessario(lista, jogadorAtual);
}

static void trocarPontuacoes(Pontuacao *a, Pontuacao *b) {
    Pontuacao temporaria = *a;
    *a = *b;
    *b = temporaria;
}

static int particionarRanking(Pontuacao rankingAtual[], int inicio, int fim) {
    int pivo = rankingAtual[fim].pontos;
    int indiceMenor = inicio - 1;

    for (int i = inicio; i < fim; i++) {
        if (rankingAtual[i].pontos > pivo) {
            indiceMenor++;
            trocarPontuacoes(&rankingAtual[indiceMenor], &rankingAtual[i]);
        }
    }

    trocarPontuacoes(&rankingAtual[indiceMenor + 1], &rankingAtual[fim]);
    return indiceMenor + 1;
}

static void quickSortRanking(Pontuacao rankingAtual[], int inicio, int fim) {
    if (inicio < fim) {
        int indicePivo = particionarRanking(rankingAtual, inicio, fim);

        quickSortRanking(rankingAtual, inicio, indicePivo - 1);
        quickSortRanking(rankingAtual, indicePivo + 1, fim);
    }
}

static void registrarPontuacao(Pontuacao rankingAtual[], Jogador *jogadorAtual) {
    if (pontuacaoRegistrada) {
        return;
    }

    rankingAtual[TAMANHO_RANKING - 1].pontos = jogadorAtual->pontuacao;
    quickSortRanking(rankingAtual, 0, TAMANHO_RANKING - 1);
    pontuacaoRegistrada = 1;
}

static void desenharRetangulo(HDC hdc, int x, int y, int largura, int altura, COLORREF cor) {
    HBRUSH pincel = CreateSolidBrush(cor);
    RECT area = {x, y, x + largura, y + altura};
    FillRect(hdc, &area, pincel);
    DeleteObject(pincel);
}

static void desenharElipse(HDC hdc, int x, int y, int largura, int altura, COLORREF cor) {
    HBRUSH pincel = CreateSolidBrush(cor);
    HBRUSH pincelAnterior = (HBRUSH)SelectObject(hdc, pincel);
    HPEN canetaAnterior = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));

    Ellipse(hdc, x, y, x + largura, y + altura);

    SelectObject(hdc, canetaAnterior);
    SelectObject(hdc, pincelAnterior);
    DeleteObject(pincel);
}

static void desenharTriangulo(HDC hdc, POINT pontos[], COLORREF cor) {
    HBRUSH pincel = CreateSolidBrush(cor);
    HBRUSH pincelAnterior = (HBRUSH)SelectObject(hdc, pincel);
    HPEN canetaAnterior = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));

    Polygon(hdc, pontos, 3);

    SelectObject(hdc, canetaAnterior);
    SelectObject(hdc, pincelAnterior);
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
    desenharRetangulo(hdc, 0, 0, LARGURA_JANELA, INICIO_AREIA, RGB(135, 206, 235));
    desenharRetangulo(hdc, 0, INICIO_AREIA, LARGURA_JANELA, INICIO_MAR - INICIO_AREIA, RGB(238, 203, 145));
    desenharRetangulo(hdc, 0, INICIO_MAR, LARGURA_JANELA, ALTURA_JANELA - INICIO_MAR, RGB(35, 150, 190));

    for (int y = INICIO_AREIA; y < ALTURA_JANELA; y += TAMANHO_CELULA) {
        COLORREF linha = (y < INICIO_MAR) ? RGB(224, 185, 125) : RGB(80, 190, 210);
        desenharRetangulo(hdc, 0, y, LARGURA_JANELA, 2, linha);
    }

    desenharTexto(hdc, 22, 18, "Fuga em Boa Viagem", 26, RGB(20, 70, 90));
    desenharTexto(hdc, 22, 48, "Atravesse a praia e sobreviva aos perigos.", 15, RGB(20, 70, 90));
}

static void desenharJogador(HDC hdc) {
    int centroX = jogador.x + TAMANHO_CELULA / 2;
    int topoY = jogador.y + 6;

    desenharElipse(hdc, centroX - 7, topoY, 14, 14, RGB(245, 190, 140));
    desenharRetangulo(hdc, centroX - 8, topoY + 15, 16, 18, RGB(230, 65, 70));
    desenharRetangulo(hdc, centroX - 14, topoY + 18, 6, 14, RGB(245, 190, 140));
    desenharRetangulo(hdc, centroX + 8, topoY + 18, 6, 14, RGB(245, 190, 140));
    desenharRetangulo(hdc, centroX - 8, topoY + 33, 6, 9, RGB(30, 80, 140));
    desenharRetangulo(hdc, centroX + 2, topoY + 33, 6, 9, RGB(30, 80, 140));
}

static void desenharObstaculos(HDC hdc, ListaObstaculos *lista) {
    if (listaObstaculosVazia(lista)) {
        return;
    }

    Obstaculo *atual = lista->inicio;

    while (atual != NULL) {
        int x = atual->x;
        int y = atual->y;
        int olhandoDireita = atual->direcao == 1;
        POINT cauda[3] = {
            {olhandoDireita ? x : x + TAMANHO_CELULA, y + 20},
            {olhandoDireita ? x - 12 : x + TAMANHO_CELULA + 12, y + 8},
            {olhandoDireita ? x - 12 : x + TAMANHO_CELULA + 12, y + 32}
        };
        POINT barbatana[3] = {
            {x + 18, y + 8},
            {x + 25, y - 8},
            {x + 31, y + 10}
        };

        desenharTriangulo(hdc, cauda, RGB(45, 55, 65));
        desenharElipse(hdc, x, y + 8, TAMANHO_CELULA, 24, RGB(45, 55, 65));
        desenharTriangulo(hdc, barbatana, RGB(35, 45, 55));
        desenharElipse(hdc, olhandoDireita ? x + 29 : x + 8, y + 16, 4, 4, RGB(255, 255, 255));
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

    if (jogoEncerrado) {
        desenharTexto(hdc, 300, 260, "Fim de jogo", 36, RGB(160, 20, 30));
        desenharTexto(hdc, 250, 300, "Pressione R para reiniciar", 20, RGB(160, 20, 30));
        desenharTexto(hdc, 300, 328, "ou ESC para sair", 18, RGB(160, 20, 30));
    }
}

static void exibirRanking(HDC hdc, Pontuacao rankingAtual[], int tamanho) {
    char texto[64];

    desenharTexto(hdc, 318, 365, "Ranking", 22, RGB(20, 70, 90));

    for (int i = 0; i < tamanho; i++) {
        snprintf(texto, sizeof(texto), "%d. %d pontos", i + 1, rankingAtual[i].pontos);
        desenharTexto(hdc, 305, 395 + i * 24, texto, 18, RGB(20, 70, 90));
    }
}

static void exibirMenu(HDC hdc) {
    desenharTexto(hdc, 235, 190, "Fuga em Boa Viagem", 34, RGB(20, 70, 90));
    desenharTexto(hdc, 210, 245, "Atravesse a praia e desvie dos tubaroes", 20, RGB(20, 70, 90));
    desenharTexto(hdc, 275, 310, "Pressione ENTER para jogar", 20, RGB(160, 20, 30));
    desenharTexto(hdc, 310, 340, "Use WASD ou setas", 18, RGB(20, 70, 90));
}

static void desenharTelaJogo(HWND janela) {
    PAINTSTRUCT pintura;
    HDC hdc = BeginPaint(janela, &pintura);

    desenharCenario(hdc);

    if (!jogoIniciado) {
        exibirMenu(hdc);
        EndPaint(janela, &pintura);
        return;
    }

    desenharObstaculos(hdc, &listaObstaculos);
    desenharJogador(hdc);
    desenharHud(hdc);

    if (jogoEncerrado) {
        exibirRanking(hdc, ranking, TAMANHO_RANKING);
    }

    EndPaint(janela, &pintura);
}

static void moverJogador(HWND janela, int movimentoX, int movimentoY) {
    int novoX = jogador.x + movimentoX * TAMANHO_CELULA;
    int novoY = jogador.y + movimentoY * TAMANHO_CELULA;
    int chegouAoFinal = movimentoY < 0 && novoY < INICIO_AREIA;

    if (novoX >= 0 && novoX <= LARGURA_JANELA - TAMANHO_CELULA) {
        jogador.x = novoX;
    }

    if (novoY >= INICIO_AREIA && novoY <= ALTURA_JANELA - TAMANHO_CELULA) {
        jogador.y = novoY;
    }

    if (movimentoY < 0) {
        calcularPontuacao(&jogador);
    }

    if (chegouAoFinal) {
        avancarNivel(&jogador, &listaObstaculos);
    }

    InvalidateRect(janela, NULL, TRUE);
}

static LRESULT CALLBACK processarMensagemJanela(HWND janela, UINT mensagem, WPARAM tecla, LPARAM parametro) {
    switch (mensagem) {
        case WM_KEYDOWN:
            switch (tecla) {
                case VK_RETURN:
                    if (!jogoIniciado) {
                        iniciarPartida(&jogador, &listaObstaculos);
                        InvalidateRect(janela, NULL, TRUE);
                    }
                    break;
                case VK_LEFT:
                case 'A':
                    if (jogoIniciado && !jogoEncerrado) {
                        moverJogador(janela, -1, 0);
                    }
                    break;
                case VK_RIGHT:
                case 'D':
                    if (jogoIniciado && !jogoEncerrado) {
                        moverJogador(janela, 1, 0);
                    }
                    break;
                case VK_UP:
                case 'W':
                    if (jogoIniciado && !jogoEncerrado) {
                        moverJogador(janela, 0, -1);
                    }
                    break;
                case VK_DOWN:
                case 'S':
                    if (jogoIniciado && !jogoEncerrado) {
                        moverJogador(janela, 0, 1);
                    }
                    break;
                case VK_ESCAPE:
                    PostQuitMessage(0);
                    break;
                case 'R':
                    if (jogoEncerrado) {
                        liberarListaObstaculos(&listaObstaculos);
                        iniciarPartida(&jogador, &listaObstaculos);
                        InvalidateRect(janela, NULL, TRUE);
                    }
                    break;
            }
            return 0;

        case WM_PAINT:
            desenharTelaJogo(janela);
            return 0;

        case WM_TIMER:
            if (!jogoIniciado || jogoEncerrado) {
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
                }
            }

            InvalidateRect(janela, NULL, TRUE);
            return 0;

        case WM_DESTROY:
            KillTimer(janela, ID_TIMER_JOGO);
            liberarListaObstaculos(&listaObstaculos);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(janela, mensagem, tecla, parametro);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    srand((unsigned int)time(NULL));
    inicializarListaObstaculos(&listaObstaculos);

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
    SetTimer(janela, ID_TIMER_JOGO, 40, NULL);

    MSG mensagem;
    while (GetMessage(&mensagem, NULL, 0, 0) > 0) {
        TranslateMessage(&mensagem);
        DispatchMessage(&mensagem);
    }

    return 0;
}
