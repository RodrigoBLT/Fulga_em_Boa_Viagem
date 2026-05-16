# Fuga em Boa Viagem

Jogo em C inspirado em Crossy Road, ambientado na Praia de Boa Viagem.

O objetivo e atravessar a praia desviando dos tubaroes. Ao chegar ao final da tela, o jogador volta para o inicio e o nivel aumenta, deixando os tubaroes mais rapidos e em maior quantidade.

## Como executar

Requisitos:

- Windows
- GCC/MinGW instalado
- `mingw32-make`

Comandos:

```bash
mingw32-make
mingw32-make run
```

## Controles

- Setas ou WASD: mover o jogador
- R: reiniciar a partida apos o fim de jogo
- Esc: sair

## Regras principais

- O jogador comeca com 3 vidas.
- Cada colisao com um tubarao remove 1 vida.
- A pontuacao aumenta quando o jogador avanca.
- Ao chegar ao final da praia, o nivel aumenta.
- Quando as vidas acabam, o jogo exibe o ranking.

## Estrutura de dados

A estrutura principal do jogo e uma lista encadeada de obstaculos. Cada no representa um tubarao ativo na tela, com posicao, velocidade, direcao e ponteiro para o proximo obstaculo.

Essa lista e usada para inserir tubaroes, mover obstaculos, verificar colisoes, remover obstaculos fora da tela e liberar memoria ao encerrar a partida.

## Ordenacao

O ranking de pontuacoes utiliza QuickSort para ordenar os resultados em ordem decrescente.
