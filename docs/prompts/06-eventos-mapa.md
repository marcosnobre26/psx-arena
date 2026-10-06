# Etapa 06 — A floresta que muda: eventos durante a partida

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, etapas 04 e 05.

## Objetivo

A floresta muda enquanto o jogador está nela, de forma **determinística**
(mesma semente + mesmas entradas = mesmos eventos), reforçando o terror.

## Tarefas

1. **Sistema de eventos** (`src/events.c`), orientado a dados por fase:
   gatilho (tempo, entrar numa área, pegar um item, matar N, usar chave) →
   ação. Fila de eventos com prioridade; tudo em `g`.
2. **Ações** iniciais:
   - **Névoa avança**: `fog_far` diminui gradualmente por um tempo.
   - **Árvore cai**: fecha uma trilha (células viram sólidas, tronco caído
     aparece com som e tremor de câmera). Nunca prender o jogador nem tornar
     o objetivo inalcançável (reavaliar com BFS antes de aplicar).
   - **Caminho se abre**: usar uma chave num portão/raízes libera células.
   - **A mata se fecha atrás**: áreas fora da visão (além da névoa e fora do
     cone da câmera) ganham árvores novas — o jogador volta e o caminho mudou.
   - **Onda de inimigos** e **silêncio repentino** (para o ambiente/música).
3. **Reconstrução local**: mudar células reconstrói só os blocos afetados
   (usando o cache da etapa 04) e atualiza a grade de colisão.
4. **Mensagens/pistas** discretas (som, texto curto) quando algo muda.

## Critérios de aceite

- Eventos acontecem e são reprodutíveis com a mesma semente e as mesmas entradas.
- Nenhum evento deixa o objetivo inalcançável (teste automático em debug).
- Sem travadas perceptíveis ao reconstruir blocos.
- `./dev build` limpo; GUIA com "Como criar um evento".

## Ao terminar

Dê ao Marcos uma lista de eventos para provocar e onde conferir cada um.
