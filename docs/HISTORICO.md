# Histórico do projeto e decisões

Resumo da conversa em que o projeto foi criado (06/10/2026), para dar
contexto a quem continuar — pessoa ou agente de IA. Ordem cronológica.

## Objetivo do dono

Marcos quer um **projeto-base de jogo 3D de PS1, de uma fase**, para estudar
mexendo: aplicar modelos próprios feitos no Blender, mapas, poderes, armas,
customizações, e ver tudo funcionando como um jogo de PS1 em emulador ou
console real (CD-R).

## Linha do tempo

1. **Base do jogo (v1).** Escolha do **PSn00bSDK** (livre, moderno, CMake)
   em vez do SDK oficial da Sony (proprietário). Arena 3D com câmera em
   terceira pessoa, inimigos, 4 armas, 3 poderes, gemas, caixas, skins.
   Mapa definido em texto (`level.c`), jogo orientado a dados (`data.c`).
   Exportador do Blender escrito em Python (`tools/blender_export_psx.py`),
   que funciona como biblioteca, operador de menu e CLI.
2. **Validação.** Compilado com o GCC `mipsel-linux-gnu` do Ubuntu. Descoberto
   que esse GCC gera `PT_GNU_STACK` e o `elf2x` produzia um .exe de 2 GB →
   criado `tools/fix_gnu_stack.py`. Testado no **Mednafen** com **OpenBIOS**
   (compilado do repositório `pcsx-redux/nugget`, modo `psexe` com o .exe
   embutido), com capturas de tela automáticas via Xvfb + xdotool.
3. **Ajustes após testes:** câmera colidindo com paredes (aproxima e sobe),
   build `Release` (`-O2`), descarte por frustum, **passo fixo de 60 Hz**
   (o jogo ficava em câmera lenta quando caía para 30 FPS).
4. **Ambiente do Marcos (v2).** Windows + WSL Ubuntu + Docker. Criado o script
   `./dev` (build no Docker com a pasta montada no mesmo caminho — erros
   clicáveis no VS Code), `dev.conf`, configuração do VS Code, e o **pipeline
   automático de assets**: PNG em `assets/textures/` vira TIM com alocação de
   VRAM (`tools/build_textures.py`); `models/*.h` e texturas são declarados em
   `build/gen/assets_gen.h` sem editar o CMake. `./dev models` exporta
   `.blend` em lote usando o Blender do Windows (`tools/export_blend.py`).
   Criado `assets/blender/exemplos.blend` com todos os modelos na coleção PSX.
   Objetos de cenário por tabela (`prop_defs` + dígitos no mapa).
5. **Primeiro uso real.** Build funcionou no WSL do Marcos. O emulador ainda
   não estava instalado; orientado a usar o PCSX-Redux (não precisa de BIOS).
   Observação: a pasta do usuário no Windows é `C:\Users\Marcos  Nobre`
   (**dois espaços**); usar aspas nos caminhos.
6. **Personagens, colisão e 2 jogadores (v3).** Tela de **seleção de
   personagem** (Robô com skins, Cavaleiro, Batedor; atributos em
   `character_defs`). **Colisão por círculos** entre jogadores, inimigos e
   cenário (`collision.c`), com regra de "só bloqueia se aproxima" para nunca
   travar e "pulo passa por cima". **2 jogadores locais** (porta 2), câmera
   que enquadra o grupo, respawn cooperativo.
7. **Online.** Pesquisado: o FAQ de netplay do libretro diz que PS1 não
   funciona; o Batocera não lista núcleos de PS1. Solução documentada:
   **Parsec** (streaming; o convidado vira o controle 2). Modo link pela
   porta serial (SIO1 do PCSX-Redux por TCP / cabo link) ficou como
   **proposta** no README — não implementado porque não dava para testar.
8. **Documentação para o GitHub.** `README.md` técnico, `docs/GUIA.md` com
   receitas, `.gitattributes` (LF obrigatório para o `dev` rodar no Bash).
   Licença ainda **não definida** (README diz "todos os direitos reservados").

## Decisões e motivos

| Decisão | Motivo |
|---|---|
| PSn00bSDK | livre, mantido, CMake, roda em hardware real |
| GCC do Ubuntu no Docker (não `mipsel-none-elf`) | instalável por `apt`, reprodutível; exige o `fix_gnu_stack.py` |
| Modelos como `.h` embutidos | simples para aprender; sem leitura de CD ainda |
| Mapa em texto | editar fase sem ferramenta; geração automática de geometria e colisão |
| Tabelas em `data.c` | mudar o jogo sem mexer no motor |
| Passo fixo 60 Hz | velocidade de jogo independente do FPS; base para um futuro lockstep |
| Colisão por círculos | barata, sem travar, suficiente para arena |
| Docker montando no mesmo caminho do host | caminhos de erro iguais dentro e fora; VS Code abre com clique |

## Como foi testado (e como testar sem o Windows)

No ambiente onde o projeto foi criado: `mednafen` (apt) + Xvfb + OpenBIOS
compilado com o mesmo GCC MIPS (`make PREFIX=mipsel-linux-gnu
FORMAT=elf32-tradlittlemips BOOT_MODE=psexe EMBED_PSEXE=arena.exe` em
`nugget/openbios`, após trocar `elf32-littlemips` por `elf32-tradlittlemips`
nos Makefiles), copiado como `scph5501.bin` em `~/.mednafen/firmware/`,
rodando `mednafen -force_module psx -psx.bios_sanity 0 build/arena.cue`,
teclas enviadas com `xdotool` e capturas com F9. Útil se alguém quiser
automatizar testes visuais.

## Ideias pendentes (do roteiro de estudo)

- Segunda fase; som (`psxspu` / CD-DA); assets lidos do
  CD (`psxcd`); animação de personagem; recorde no memory card; modo link
  pela serial; CI no GitHub Actions; arquivo `LICENSE`.

## Registro de mudanças

- 2026-10-06 — v1 a v3 e documentação, como descrito acima.
- 2026-10-06 — Inimigo que atira (`ATIRADOR`, letra `A`): `BULLET.owner`,
  tabela `enemy_weapon_defs` separada (para não cair nos itens `W`), colunas
  `arma`/`alcance` em `enemy_defs`, IA com alcance, recuo, linha de visão e
  aviso piscando de 20 passos antes do tiro. Dois `A` no mapa da fase 1.
- 2026-10-06 — Etapa 00 (preparação): `src/rng.c` (xorshift32) com `g.seed`/
  `g.rng` para a lógica e `fx_range` para efeitos; `rand()` da libc removido;
  `game_reset(seed)` semeia antes do `level_load` (o `memset` apagava
  `g.frame`); `DEBUG_FIXED_SEED` em `config.h`; overlay L2 com `SEED` e
  `POLIS`; `CREDITS.md`; seção "Roteiro" e regras de determinismo no `CLAUDE.md`.
- 2026-10-06 — Etapa 01a (efeitos sonoros): `tools/wav2vag.py` (encoder
  SPU-ADPCM em Python puro, `--selftest` com SNR), pipeline
  `assets/sounds/*.wav` → `sfx_<nome>` no CMake, `src/sound.c` (vozes 0–1
  para loops, 2–23 com prioridade de `sound_defs`, som posicional pela
  câmera), sons provisórios por `tools/make_placeholder_sounds.py`, passos
  sincronizados com o balanço do modelo (~341 unidades, não 256), rosnado de
  inimigo fora da tela, vento e grilos em loop, `SPU xxxK/512K` no L2.
  **Correção do manual (R8):** a RAM do SPU gasta **~12,6 KB por segundo a
  22 050 Hz** (16 bytes a cada 28 amostras), não 8 KB/s; os 450 KB do
  orçamento dão ~35 s a 22 kHz ou ~70 s a 11 kHz. Música (CD-DA) fica para a
  etapa 01b. Obs.: o inimigo `ATIRADOR` registrado acima não está no código
  atual (`enemies.c` não atira), então só o tiro do jogador tem som.
- 2026-10-06 — Etapa 02 (fases e objetivos): `LEVEL_DEF` em `src/levels.c`
  (R3 ampliada: texturas, céu, faixa de música sem reprodução, objetivo,
  parâmetro, `max_enemies`, texto); `src/objective.c` com `OBJ_KILL_ALL`,
  `OBJ_REACH_EXIT`, `OBJ_COLLECT`, `OBJ_SURVIVE`; letras novas `X` (saída:
  pilar + anel, abre ao cumprir o objetivo), `Q` (item de missão) e `S`
  (reforço do SURVIVE: a cada 8 s, até `max_enemies` vivos, longe dos
  jogadores e fora da câmera); `STATE_INTRO` (R12) e `STATE_END`;
  progressão leva pontos, vida, energia, armas e poder (caído volta com
  50%); game over recomeça a fase; `DEBUG_START_LEVEL`. 4 fases de teste
  (ARENA, CORREDORES, RELIQUIAS, CERCO). Música CD-DA (01b) adiada para
  depois da etapa 10. Medição L2 pendente (teste do Marcos).
- 2026-10-06 — Etapa 03 (clima e lanterna): névoa por profundidade da GTE
  (DQA/DQB por macro própria, `ncds`/`dpcs`, faces além do `fog_far`
  descartadas, cor da névoa = céu), escuridão por fase (`fog_near/far` e luz
  ambiente em `LEVEL_DEF`, lua azulada em `config.h`), lanterna no Select
  (bateria 0–1000, pisca e apaga, pilha `L` +400 e 10% de drop dos
  inimigos; cone `POLY_G3` aditivo entre `DR_TPAGE`; objetos no cone com
  névoa 1,6× mais longe), overlay `FOG near/far`, `DEBUG_FOG_TUNING`,
  L2 + START pula de fase (o overlay passou a alternar ao soltar o L2).
  Clarear vértices do chão (opcional) não foi feito: o chão é um quad de cor
  única por célula. Desempenho no pior ponto de cada fase (L2):

  | Fase | Antes: FPS / POLIS / RAM GPU | Depois: FPS / POLIS / RAM GPU |
  |---|---|---|
  | 1 ARENA | 60 / 354 / 14 584 | a medir |
  | 2 CORREDORES | 30 / 937 / 35 464 | a medir |
  | 3 RELIQUIAS | 30 / 518 / 20 796 | a medir |
  | 4 CERCO | 30 / 531 / 22 056 | a medir |
- 2026-10-07 — Etapa 04a (grade grande e blocos sob demanda): grade de
  até 128×128 (1 byte por célula: `CELL_*` + `CELL_SOLID`); geometria em
  blocos de 8×8 num cache de 36 — o 3×3 em volta de cada jogador é montado
  no mesmo quadro, o resto do 5×5 do grupo numa fila de 1 bloco/quadro,
  cache centrado no jogador (girar a câmera não muda nada); `render_chunk`
  com tabela única de 405 vértices, luz embutida nas cores e névoa com
  `dpct`/`dpcs` (`POLY_GT4`); variação de cor por vértice e textura girada
  por hash; texturas terra/folhas/raízes/mata (`make_forest_textures.py`);
  mata densa com 3 m; fase 5 `FLORESTA (TESTE)` feita por código (validada
  compilando o gerador no host: tudo alcançável, ≤ 208 quads por bloco);
  L2 + direcional teleporta para 4 pontos fixos; cone da lanterna em
  coordenadas relativas (o mapa passa de 32 767). RAM: 707 KB = 723 750 bytes
  (text 82 KB, data 115 KB, bss 510 KB); antes 532 KB. Medições (L2, lanterna ligada):

  | Ponto (fase 5) | FPS | POLIS | RAM GPU | BLOCOS |
  |---|---|---|---|---|
  | ← início | a medir | | | |
  | → cruzamento | a medir | | | |
  | ↑ canto denso | a medir | | | |
  | ↓ clareira | a medir | | | |
  | fase 2 CORREDORES (pior ponto da etapa 03) | a medir | | | |
- 2026-10-07 — Correção da 04a: blocos desenhados na origem, desencontrados
  da colisão. As macros de carga da GTE leem memória dentro de um `asm` sem
  declarar isso; em `load_translation` o GCC apagou a soma da translação e
  todos os blocos (e o cone da lanterna) iam para a origem do mundo.
  Confirmado no assembly (`ctc2 $5..$7` carregando a translação da câmera
  sem a origem do bloco). Correção: `GTE_BARRIER()` (asm com clobber
  `"memory"`) antes das cargas de dados recém-escritos (`load_translation`,
  cone, `render_set_light`, cor em `render_mesh`) e `R·p` em inteiros no
  lugar do `ApplyMatrixLV`. Dados verificados compilando `level.c` no host:
  nas 5 fases todo quad de chão cai em célula livre e todo topo em célula
  sólida. Novo atalho L2 + Select: marcadores da grade de colisão.
- 2026-10-07 — Etapa 04b (árvores em massa): `tools/make_tree_models.py`
  gera pinheiro (22 faces, 3,5 m), árvore seca (24) e tronco caído (22, 3 m),
  e as imagens planas 32×64 de 4 bits renderizadas dos próprios modelos.
  Árvores fora de `g`, 8 bytes cada, ordenadas por bloco; desenho só no 5×5
  do grupo: modelo até profundidade 1800 (`TREE_LOD_DIST`), imagem plana
  além, nada depois da névoa (`render_billboard`, com `GTE_BARRIER`).
  Colisão provisória: o tronco marca a célula sólida (tronco caído: 3
  células, deitado em X ou Z). Fase 5: moitas de mata viram árvores (5%,
  16% no canto nordeste), fileira na frente da borda, 15 troncos; hash com
  semente (`FOREST_TEST_SEED`; etapa 05 deriva de `g.seed`). Validado no
  host: 883 árvores, tudo alcançável, trilhas e pontos de teleporte livres.
  RAM: 736 KB. Medições (L2, lanterna ligada, fase 5):

  | Ponto | FPS | POLIS | RAM GPU | ARVORES modelo/plana |
  |---|---|---|---|---|
  | ← início | a medir | | | |
  | → cruzamento | a medir | | | |
  | ↑ canto denso | a medir | | | |
  | ↓ clareira | a medir | | | |
