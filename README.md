# Fuga em Boa Viagem

Jogo em C inspirado em Crossy Road, ambientado na Praia de Boa Viagem, em Recife.

O jogador precisa atravessar a praia e o mar desviando dos tubaroes. Ao chegar ao final da tela, ele volta para o inicio, ganha pontos extras e o nivel de dificuldade aumenta. Com isso, os tubaroes ficam mais rapidos e aparecem em maior quantidade.

## Dependencias

Para compilar e rodar o jogo, e necessario usar Windows com MinGW/GCC instalado.

- Windows
- MinGW com `gcc`
- `mingw32-make`

As bibliotecas usadas pelo jogo sao do proprio Windows/MinGW:

- `gdi32`: interface grafica
- `winmm`: audio/musica
- `m`: funcoes matematicas

Elas ja sao chamadas no `Makefile`, entao nao precisa instalar nenhuma biblioteca extra alem do MinGW.

Para verificar se o MinGW esta instalado corretamente, rode:

```bash
gcc --version
mingw32-make --version
```

Se algum comando nao for reconhecido, instale o MinGW ou adicione a pasta `bin` do MinGW ao `PATH` do Windows.

## Como executar

No terminal, dentro da pasta do projeto, rode:

```bash
mingw32-make
```

Esse comando compila o jogo e gera o executavel em:

```text
build/fuga-em-bv.exe
```

Para compilar e abrir o jogo direto:

```bash
mingw32-make run
```

Para limpar os arquivos gerados:

```bash
mingw32-make clean
```

## Controles

- Enter: iniciar a partida na tela inicial
- I: abrir a tela de regras
- Setas ou WASD: mover o jogador
- R: reiniciar a partida apos o fim de jogo
- Esc: sair ou voltar da tela de regras

## Regras principais

- O jogador comeca com 3 vidas.
- Cada colisao com um tubarao remove 1 vida.
- A pontuacao aumenta quando o jogador avanca.
- Ao chegar ao final da tela, o jogador sobe de nivel e volta para o inicio.
- A dificuldade aumenta a velocidade e a quantidade de tubaroes.
- Quando as vidas acabam, o jogo exibe o ranking de pontuacoes.

## Estrutura de dados

A estrutura de dados principal do jogo e uma lista encadeada de obstaculos.

Cada no da lista representa um tubarao ativo na tela, armazenando posicao, velocidade, direcao e o ponteiro para o proximo obstaculo. O jogo percorre essa lista para desenhar os tubaroes, mover cada obstaculo e verificar colisoes com o jogador.

Quando um tubarao sai da tela, ele e reposicionado no lado oposto, mantendo o fluxo continuo de obstaculos. Ao reiniciar a partida ou avancar de nivel, a lista e liberada e criada novamente de acordo com a dificuldade atual.

## Ordenacao

O ranking de pontuacoes utiliza o algoritmo QuickSort.

Ao final da partida, a pontuacao do jogador e registrada no vetor de ranking. Em seguida, o QuickSort ordena as pontuacoes em ordem decrescente, mostrando os melhores resultados primeiro.

## Organizacao do projeto

```text
.
|-- Makefile
|-- README.md
`-- src
    |-- jogo.h
    `-- main.c
```

- `src/jogo.h`: constantes e estruturas principais do jogo.
- `src/main.c`: logica da partida, interface grafica, lista encadeada e ranking.

