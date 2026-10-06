# Etapa 08 — Inventário e menu do START

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, manual capítulo 14 (estados, R12).

## Objetivo

Ao apertar START, abrir um menu (pausando o jogo) com abas: **Mapa**,
**Objetivo**, **Estatísticas**, **Armas** e **Itens**, onde o jogador
consulta tudo e usa/equipa itens.

## Tarefas

1. **Inventário** (`src/inventory.c`): itens com tipo, quantidade e pilha
   máxima; capacidade limitada (ex.: 8 espaços — escassez é parte do gênero).
   Tipos iniciais: kit médico, bandagem, pilha, flechas, balas, chave,
   item de objetivo, poção de mana. Usar consome e aplica o efeito.
2. **Estado `STATE_MENU`** (substitui a pausa atual) com abas navegáveis
   por L1/R1 (ou esquerda/direita) e X para confirmar, CÍRCULO para voltar:
   - **Mapa**: minimapa 2D da grade com **névoa de exploração** (bitset de
     células vistas, 128×128 bits = 2 KB em `g`), posição e direção dos
     jogadores, saída/objetivo quando descobertos.
   - **Objetivo**: texto do objetivo atual e progresso (ex.: 2/3 itens).
   - **Estatísticas**: tempo, inimigos derrotados por tipo, precisão,
     dano recebido, itens usados, distância percorrida, semente do mapa.
   - **Armas**: armas possuídas, munição, equipar.
   - **Itens**: lista do inventário, usar/descartar.
   - **Opções** (opcional): volume de efeitos e música (etapa 01).
3. **Visual do menu**: painéis semitransparentes (`render_rect`), ícones
   simples (texturas 4 bits 16×16) e, se valer a pena, uma **fonte própria**
   em textura (o texto continua ASCII sem acentos). O mundo continua
   desenhado ao fundo, escurecido.
4. **Atalho rápido**: um botão para usar o item de cura mais fraco sem abrir o menu.
5. **2 jogadores**: qualquer um abre o menu e o jogo pausa para os dois; cada
   jogador navega o próprio inventário (abas por jogador ou alternância).
   No futuro modo link o START é sincronizado, então a pausa vale para ambos.

## Critérios de aceite

- Menu abre/fecha sem afetar a simulação (o tempo não corre no menu).
- Mapa mostra só o explorado; itens usados aplicam o efeito e somem.
- `./dev build` limpo; texto cabe na tela 320×240.

## Ao terminar

Peça ao Marcos para revisar textos e layout com capturas de cada aba.
