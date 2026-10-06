# Roteiro: PS1 Arena → survival horror na floresta

Plano de evolução do jogo, decidido em 06/10/2026. Cada etapa tem um prompt
pronto em `docs/prompts/` para ser executado pelo agente de IA (Claude Code)
no VS Code, uma etapa por vez, cada uma numa branch própria.

## Visão do jogo

Um **survival horror lento** numa **floresta grande, escura e gerada a cada
partida**. Pouca munição, lanterna com bateria, sons que denunciam o que não
se vê. O jogador sobrevive com armas brancas (espada, machado), arco e
flechas, magias e poucas armas de fogo. A floresta **muda durante a partida**:
a névoa avança, árvores caem e fecham caminhos, trilhas se abrem com chaves.
Pássaros cruzam o céu e, de vez em quando, um animal salta de uma árvore para
outra.

### Decisões já tomadas

| Tema | Decisão |
|---|---|
| Gênero | survival horror lento: recursos escassos, lanterna, tensão > ação |
| Cenário | a **floresta substitui a arena** atual |
| Mapas | **gerados a cada partida** (semente) **e** mudam durante o jogo (eventos) |
| Armas | espada, machado (corpo a corpo), arco e flecha (munição), magias (energia), armas de fogo raras |
| Online | **Parsec** agora; **modo link** (lockstep pela serial, receita R11 do manual) por último |
| Som | efeitos em VAG no SPU; música por faixa de CD-DA; som posicional |

### Pilares (use para decidir quando houver dúvida)

1. **Ver pouco, ouvir muito.** Escuridão e névoa limitam a visão; o som avisa.
2. **Cada recurso conta.** Munição, bateria da lanterna e cura são raras.
3. **A floresta é viva e hostil.** Coisas se movem, mudam e reagem ao jogador.
4. **Roda num PS1 real.** 30 FPS estáveis, 2 MB de RAM, sem atalhos que só funcionam em emulador.

## Ordem das etapas

| # | Etapa | Prompt | Depende de | Risco |
|---|---|---|---|---|
| 00 | Preparação (regras, créditos, ferramentas) | `00-preparacao.md` | — | baixo |
| 01 | Som: efeitos, passos, música, som posicional | `01-som.md` | 00 | baixo |
| 02 | Fases e objetivos | `02-fases-objetivos.md` | 00 | baixo |
| 03 | Clima: escuridão, névoa, lanterna | `03-clima-lanterna.md` | 02 | médio |
| 04 | Mundo grande: floresta em blocos | `04-mundo-grande.md` | 03 | **alto** |
| 05 | Geração procedural da floresta | `05-geracao-procedural.md` | 04 | médio |
| 06 | Floresta que muda (eventos) | `06-eventos-mapa.md` | 05 | médio |
| 07 | Combate de sobrevivência e novas armas | `07-combate-armas.md` | 02 | médio |
| 08 | Inventário e menu do START | `08-menu-inventario.md` | 06, 07 | médio |
| 09 | Floresta viva: céu, pássaros, animais | `09-ambiente-vivo.md` | 04, 01 | médio |
| 10 | Desempenho, polimento, release | `10-polimento.md` | todas | médio |
| 11 | Modo link (2 jogadores pela serial) | `11-modo-link.md` | todas | médio |

## Regras que valem para TODAS as etapas

Além das regras do `CLAUDE.md`:

- **Determinismo (preparação para o modo link).** Toda a lógica que muda `g`
  depende só de `g.in[]` e de geradores aleatórios com semente conhecida.
  - O mapa procedural e os eventos usam um **gerador próprio com semente**
    (`g.seed`), nunca valores locais (tempo, VSync).
  - `rand()`/PRNG do jogo **só na lógica** (`*_update`, `game_tick`). O
    desenho e os efeitos puramente visuais (pássaros, partículas) usam um
    gerador **separado**, que não afeta `g`.
  - Nada de ler o controle direto do hardware dentro da lógica.
  - Todo campo novo é inicializado em `*_spawn`/reset (o `memset` de `g` zera o resto).
- **Desempenho medido.** Toda etapa visual termina com uma medição no overlay
  L2 (FPS e `RAM GPU`) no pior ponto do mapa, anotada no `HISTORICO.md`.
- **Orçamentos.** 30 FPS; até ~2 500 polígonos por quadro; `PACKET_LEN` ≤ 128 KB
  por buffer; RAM total do executável ≤ 1,2 MB; SPU ≤ 450 KB de amostras.
- **Assets com licença.** Todo som, textura ou modelo de terceiros entra no
  `CREDITS.md` (fonte, autor, licença, link). Nada extraído de jogos comerciais.
- **Uma etapa = uma branch = um PR/merge.** Commits pequenos e frequentes.

## Como usar os prompts

```bash
cd ~/psx-arena
git switch main && git pull
git switch -c etapa-01-som
code .
```

No painel do Claude Code:

> Leia `docs/ROADMAP.md` e `docs/prompts/01-som.md`. Antes de alterar
> qualquer arquivo, me mostre o seu plano em passos e as dúvidas que tiver.

Depois de aprovar o plano, deixe-o trabalhar. Ao final de cada etapa:

1. `./dev build` sem avisos novos e `./dev run` — teste o que o agente pediu.
2. Se estiver bom: `git push -u origin etapa-01-som`, abra o PR no GitHub e faça o merge.
3. Volte para a `main` e comece a próxima etapa.

Se uma etapa ficar grande demais, peça ao agente para dividi-la em partes
(ex.: `04a`, `04b`) e faça uma branch por parte.
