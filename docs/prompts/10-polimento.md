# Etapa 10 — Desempenho, polimento e release

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, manual capítulos 18 a 20.

## Objetivo

Deixar a versão "floresta" estável, rápida e documentada para uma release.

## Tarefas

1. **Medição**: tabela no HISTORICO com FPS, polígonos, `RAM GPU`, RAM total
   e SPU nos 3 piores lugares de cada fase.
2. **Otimizações** onde a medição apontar (LOD, raio de blocos, IA a cada N
   passos, menos divisões, texturas 4 bits).
3. **Robustez**: limites de pools, nenhum `printf` em laço, nada que dependa
   de memória não inicializada; testar em DuckStation e PCSX-Redux.
4. **Salvar progresso** no memory card (receita R10, concluída e validada):
   fase atual, armas, inventário, estatísticas, configurações de volume.
5. **Título e identidade**: proponha um nome para o jogo, tela de título
   nova (com música), créditos lendo o `CREDITS.md`.
6. **Documentação**: README (recursos, controles, limitações), GUIA,
   manual (anotar o que mudou em relação ao PDF), HISTORICO.
7. **Release**: lista de verificação do README; build anexado à Release do GitHub.

## Critérios de aceite

- 30 FPS nos piores pontos medidos; nenhum travamento numa sessão de 30 minutos.
- Salvar e carregar funcionam no emulador com memory card.
- `./dev clean && ./dev build` sem avisos novos.
