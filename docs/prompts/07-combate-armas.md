# Etapa 07 — Combate de sobrevivência e novas armas

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, manual capítulos 9 a 11
> (receitas R1, R5, R6 servem de base).

## Objetivo

Combate lento e tenso: poucas munições, armas brancas, arco, magias e
inimigos que assustam mais do que enxameiam.

## Tarefas

1. **Tipos de arma** em `WEAPON_DEF` (campo `kind`):
   - `WK_MELEE`: espada (rápida, alcance curto, arco largo), machado (lenta,
     dano alto, também derruba arbustos/obstáculos marcados). Golpe com área
     em arco à frente, duração e janela de acerto; sem projétil.
   - `WK_BOW`: arco e flecha; segurar carrega (mais dano/alcance), flechas
     são munição; flechas podem ser recolhidas no chão às vezes.
   - `WK_MAGIC`: magias gastam energia (mana): bola de fogo (projétil),
     clarão (atordoa e assusta inimigos em volta — reaproveite a ideia da R6),
     cura. As magias substituem o sistema de "poderes" atual (unifique).
   - `WK_GUN`: armas de fogo raras, munição muito escassa, barulhentas.
2. **Munição e recursos**: tipos de munição (flechas, balas), mana, pilhas;
   itens no mapa via orçamento da etapa 05. Ajuste os números para escassez.
3. **Mira**: reduza a mira automática (cone menor) ou deixe só para armas de
   fogo; corpo a corpo acerta pelo arco à frente.
4. **Arma na mão**: modelo da arma desenhado preso à mão do personagem
   (offset + rotação do jogador); trocar de arma troca o modelo.
5. **Inimigos de terror** (repensar `enemy_defs`):
   - Percepção: **visão** limitada pela escuridão/névoa e pela lanterna
     (lanterna ligada torna o jogador mais visível), **audição** (correr, tiro,
     galho quebrando atraem; agachar/andar devagar reduz).
   - Estados: vagar → investigar ruído → perseguir → perder o rastro.
   - 3 tipos iniciais: rastejador lento e resistente; caçador rápido que foge
     da luz; algo grande e raro que só se ouve até estar perto.
   - Use a grade espacial da etapa 04 para linha de visão.
6. **Sons** (etapa 01) para golpes, arco, magias, passos de inimigos, gritos.

## Fora do escopo

Inventário e menu (etapa 08) — por enquanto, troque de arma com o botão atual.

## Critérios de aceite

- Cada arma com sensação distinta; munição acaba; corpo a corpo funciona sem munição.
- Inimigos reagem a barulho e à lanterna de forma perceptível.
- Lógica 100% determinística (só `g.rng`); `./dev build` limpo; 30 FPS.
- `docs/GUIA.md`: "Criar uma arma de cada tipo" e "Criar um inimigo com percepção".

## Ao terminar

Proponha ao Marcos números iniciais de balanceamento numa tabela e peça
para ele jogar uma fase inteira e relatar se faltou ou sobrou munição.
