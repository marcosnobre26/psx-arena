# Etapa 09 — Floresta viva: céu, pássaros e animais

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, etapas 01, 03 e 04.

## Objetivo

Dar vida e inquietação à floresta com o céu, bandos de pássaros e animais
que cruzam entre as árvores — sem pesar no desempenho.

## Tarefas

1. **Céu**: fundo em degradê escuro (faixa de quads no horizonte ou
   retângulos 2D desenhados no fundo da OT), lua (billboard com brilho
   aditivo), nuvens lentas opcionais. Deve combinar com a névoa da etapa 03.
2. **Pássaros**: bandos de 3–8 pássaros voando alto em trajetórias suaves;
   modelo minúsculo com 2 poses de asa (ou sprite 4 bits com 2 quadros);
   às vezes um bando levanta voo de uma árvore quando o jogador se aproxima
   ou atira (com som). Visíveis no céu mesmo além da névoa (desenho especial).
3. **Animais entre árvores**: eventos de ambiente — um esquilo/criatura
   salta de um tronco para outro num arco, um vulto atravessa a trilha ao
   longe, olhos brilhando na escuridão que somem com a lanterna. Disparados
   por proximidade e tempo, com cooldown e som posicional.
4. **Determinismo**: ambiente puramente visual usa `fx_rng` e **não altera
   `g`**. Se algum animal tiver efeito no jogo (ex.: assustar inimigos),
   ele passa para a lógica com `g.rng`.
5. **Orçamento**: no máximo ~150 polígonos para todo o ambiente; desligar
   pássaros/animais longe da câmera.

## Critérios de aceite

- Em 5 minutos de jogo, o Marcos vê pássaros e pelo menos 2 tipos de evento
  de animal; o FPS não cai mais que 2 em relação à etapa anterior.
- `./dev build` limpo; GUIA: "Criar um evento de ambiente".

## Ao terminar

Liste os eventos de ambiente e como forçá-los (atalho em build de debug).
