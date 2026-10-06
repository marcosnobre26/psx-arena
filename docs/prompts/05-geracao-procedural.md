# Etapa 05 — Geração procedural da floresta

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, etapa 04 (formato da grade).

## Objetivo

Cada partida gera uma floresta diferente a partir de `g.seed`, sempre
jogável (tudo alcançável) e com a mesma semente gerando o mesmo mapa.

## Tarefas

1. **Gerador** em `src/mapgen.c`, usando **apenas** `RNG` com a semente da
   fase (`g.seed ^ numero_da_fase`), sem `float`:
   - Densidade da mata com ruído de valor em inteiros (2–3 oitavas).
   - Clareiras (círculos/blobs), trilhas ligando pontos de interesse
     (caminhada aleatória com tendência ao alvo ou A* sobre a grade com custo).
   - Pontos de interesse: spawn, saída/objetivo, cabana/ruína, itens-chave.
2. **Garantia de jogabilidade**: BFS a partir do spawn; tudo que importa
   (saída, itens de objetivo, chaves) precisa ser alcançável. Se não for,
   abra caminho (derrube árvores na rota) ou gere de novo com `seed+1`
   (limite de tentativas).
3. **Distribuição** de inimigos, munição, cura, pilhas por orçamento da fase
   (`LEVEL_DEF`: tamanho, densidade, orçamento de inimigos/itens, dificuldade),
   com distância mínima do spawn e mais perigo longe dele.
4. **Parâmetros por fase**: a `LEVEL_DEF` passa a descrever o gerador em vez
   de um mapa de texto (mantenha suporte a mapa fixo para fases especiais).
5. **Tempo de geração**: medir; se passar de ~0,5 s, gerar durante a tela de
   introdução (etapa 02) e mostrar "..." animado.
6. **Depuração**: semente no overlay L2 e na tela de introdução; build de
   debug permite fixar a semente (constante em `config.h`).

## Fora do escopo

Eventos que mudam o mapa durante o jogo (etapa 06).

## Critérios de aceite

- Mesma semente → mesmo mapa (teste: gerar duas vezes e comparar um hash da grade).
- 50 sementes seguidas geradas num teste de inicialização (build de debug)
  sem nenhum mapa impossível; registre os resultados.
- `./dev build` limpo; ~30 FPS mantidos.

## Ao terminar

Diga ao Marcos como forçar uma semente para reproduzir um mapa e como ajustar
densidade/tamanho por fase.
