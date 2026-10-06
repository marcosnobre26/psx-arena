# Etapa 04 — Mundo grande: a floresta em blocos

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, manual capítulos 4, 13, 15 e 18.
> Esta é a etapa de maior risco. **Divida em sub-etapas** (04a, 04b, …), com
> um commit funcional e uma medição de FPS ao fim de cada uma.

## Objetivo

Trocar a arena de 32×32 m por uma **floresta grande** (alvo: 128×128 células
= 128×128 m), rodando a 30 FPS, que substitui a arena atual.

## Tarefas

### 04a — Grade grande e geometria sob demanda
- Grade do mapa até 128×128 (constantes `MAP_MAX_W/H`), 1 byte por célula
  (tipo de terreno/objeto) + bits de solidez.
- A geometria **não** é mais montada para o mapa todo: só os blocos (chunks
  de 8×8 células, ajuste se necessário) num raio em volta da câmera ficam
  montados num cache fixo (ex.: 5×5 blocos); ao andar, os blocos que saem são
  reaproveitados para os que entram. Limite de montagem por passo para não
  causar travadinhas.
- Chão com variação: 2–4 texturas de terra/folhas/raízes por célula, escolhidas
  pela grade, e leve variação de cor por vértice.
- Enquanto a geração procedural (etapa 05) não existe, use um **mapa de teste
  feito por código** (ex.: borda de árvores densas, clareiras e trilhas fixas).

### 04b — Árvores e objetos em massa
- Árvores são objetos de cenário em grande quantidade (centenas). Crie 3
  modelos low-poly (20–40 faces) por script em `tools/make_default_assets.py`
  ou no `exemplos.blend` (coleção PSX): pinheiro, árvore seca/retorcida,
  tronco caído. Variação por escala e rotação.
- **LOD**: além de uma distância, a árvore vira um _billboard_ (quad virado
  para a câmera, textura 4 bits com transparência); depois da névoa, não desenha.
- Pool de objetos dimensionado para o mapa grande, guardado por bloco (só os
  objetos dos blocos próximos são percorridos para desenho).

### 04c — Colisão em grade espacial
- `collide_blocked()` hoje percorre todos os objetos. Troque por uma **grade
  espacial**: cada célula sabe quais objetos sólidos e entidades estão nela;
  o teste olha só as células vizinhas. Mantenha a regra "só bloqueia se aproxima".
- Projéteis e linha de visão (para a IA da etapa 07) usam a mesma grade.

### 04d — Câmera na mata
- Árvores entre a câmera e o jogador: desenhar semitransparentes (ou não
  desenhar o tronco) quando cobrirem o jogador; aproxime a câmera em áreas
  densas. Sem atravessar troncos com a câmera.

### 04e — Integração
- A floresta substitui a arena nas fases da etapa 02 (com parâmetros de
  névoa/escuridão da etapa 03). Spawn, inimigos, itens e saída posicionados
  por código no mapa de teste.

## Orçamentos e medições obrigatórias

- 30 FPS no pior ponto; ≤ ~2 500 polígonos/quadro; anote no HISTORICO:
  polígonos, `RAM GPU`, RAM total (`arena.map`).
- Se não couber: reduza raio de blocos, distância do LOD, densidade de
  árvores — nessa ordem — e registre a decisão.

## Fora do escopo

Relevo (altura do terreno), água, geração procedural (etapa 05).

## Critérios de aceite

- Andar de uma ponta à outra da floresta sem travadas visíveis ao trocar blocos.
- Ninguém atravessa árvores; tiros param nelas.
- 2 jogadores: a câmera enquadra os dois sem atravessar troncos.
- `./dev build` limpo; documentação atualizada (GUIA: "Como o mundo é montado").

## Ao terminar

Liste para o Marcos: como andar até as bordas, onde medir FPS, e o que mudou
no formato do mapa (para as próximas etapas).
