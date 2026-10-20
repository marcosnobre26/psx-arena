# Continuidade do projeto (atualizado em 2026-10-20)

Arquivo para qualquer agente ou pessoa retomar o MATA ESCURA de onde parou.

## Onde estamos

- Etapas 00 a 10 concluídas e na `main`. **v0.9.0 publicada** no GitHub
  (releases/tag/v0.9.0).
- Testado no DuckStation (Linux, no WSL) e no PCSX-Redux (sem janela,
  `./dev redux-test`: os 30 hashes do replay iguais).
- Equilíbrio medido com o robô (skill 50, 240 partidas): 223 vitórias,
  8 mortes. O GRANDE é quem mais mata.

## Primeira partida do Marcos (2026-10-12)

Log: `logs/partida-20261012-1531.txt` (analise com `./dev analisar-partida`).
Personagem ROBO, morreu na fase 1 aos 3:45 para o GRANDE.

- Lanterna: a pilha zerou, mas ele "não lembra de ter visto a lanterna ligada"
  (o cone e o indicador passam despercebidos; a pilha dura ~100 s ligada).
- GRANDE: ele achou que dava para vencer; nada no jogo sugere fugir
  (0 fugas no log). O GRANDE tem 25 de vida (o Caçador tem 3).
- Armas: "vi bem poucas"; 2 armas ficaram no mapa, só usou a espada.
- Dificuldade: "eu tentaria de novo". Ou seja, manter a dificuldade e melhorar
  a comunicação.

## Próximos passos, nesta ordem

1. **Etapa 10b, primeira impressão.** O prompt completo está abaixo.
2. **Etapa 01b, música por CD-DA.** Ver `docs/prompts/01-som.md`, seção 4.
   O ponto de entrada já existe em `src/sound.c`: `music_title_start()`,
   `music_stop()`, e o volume via `SpuSetCommonCDVolume` na aba Opções.
   Músicas provisórias geradas por script. Repetição com `CdlSetloc` +
   `CdlPlay` sem número de faixa, com o fim detectado por `CdlGetlocP`.
   A música só toca rodando o `.cue`. Depois disso: **v1.0**.
3. **Etapa 11, modo link** (SIO1, lockstep). Ver `docs/prompts/11-modo-link.md`.
4. **Depois, se o Marcos quiser:** etapa 12, personagens em peças rígidas no
   padrão PS1, gerados por script no Blender, e uma ferramenta de foto para
   personagem (só com foto do próprio Marcos ou de quem autorizar; a foto
   original nunca vai para o git). Está adiada.

Depois de cada etapa, o Marcos faz o teste final pelo `docs/TESTE-FINAL.md`.

## Prompt da etapa 10b

```
Etapa 10b: o Marcos jogou a primeira partida (logs/partida-20261012-1531.txt; rode ./dev analisar-partida nele). Morreu na fase 1 aos 3:45 para o GRANDE, a pilha da lanterna zerou e ele "não lembra de ter visto a lanterna ligada", achou que dava para vencer o GRANDE e viu "bem poucas" armas. Disse que tentaria de novo, então a dificuldade fica; o problema é o jogo comunicar. Corrija:

1. Lanterna perceptível: cone mais visível (contraste maior no chão, sem estourar o orçamento), indicador de pilha claro no HUD, dica "SELECT: LANTERNA" no início da fase 1, mensagens "PILHA FRACA" e "SEM PILHA". Revise BATTERY_DRAIN/BATTERY_PICKUP: com a lanterna sempre ligada e as pilhas do mapa, um jogador médio não deve ficar no escuro antes de ~5 min na fase 1. Confirme com ./dev economia.
2. GRANDE comunicado: pisadas audíveis antes de ele aparecer, mensagem na primeira vez que ele surge (algo como "ALGO ENORME... MELHOR FUGIR"), tremor de câmera só visual, e um golpe com preparação visível (o jogador consegue esquivar se prestar atenção). Ele continua podendo ser morto.
3. Itens visíveis no escuro: armas e itens dão um brilho/reflexo periódico (só visual, fx_range), mais forte quando a lanterna passa por eles. Na fase 1, garanta uma arma extra num raio curto do início (mapgen).
4. Ao terminar: robô 240 partidas (skill 50) e compare com antes (223/240). A vitória do robô não deve passar de ~95%. Bench dentro do limite. Se a lógica mudar, explique o motivo e regrave as referências com replay-test --save e robo-test --save. Atualize capturas, HISTORICO e TESTE-FINAL.
Antes de implementar, me mostre o plano.
```

## Ambiente (máquina do Marcos)

- Projeto em `~/psx-arena` no WSL Ubuntu. Usuário `marcos_nobre`.
- `dev.conf` é local (fora do git):
  `EMULATOR="/home/marcos_nobre/emuladores/duckstation/AppRun"`,
  `EMULATOR_ARGS="-fastboot"`, `RUN_FILE="cue"`.
- PCSX-Redux em `~/emuladores/pcsx-redux/squashfs-root/AppRun`.
- Os emuladores do Windows são bloqueados pelo Smart App Control. Controle USB
  não funciona no WSL: o Marcos joga pelo teclado.
- Teclado no DuckStation: setas = direcional, K = X, J = Quadrado,
  I = Triângulo, L = Bola, Q/E = L1/R1, 1 = L2, 3 = R2,
  Backspace = SELECT (lanterna), Enter = START.
