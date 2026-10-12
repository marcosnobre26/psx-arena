# MATA ESCURA v0.9.0 — "floresta"

Terror de sobrevivência numa floresta escura, para PlayStation 1. Primeira
versão jogável completa: 4 fases geradas a cada partida, a floresta que
muda enquanto você joga, combate de sobrevivência, inventário, menu,
memory card. A **v1.0** vem com a música (CD-DA, etapa 01b).

## Como jogar

Abra `mata-escura-v0.9.0.cue` no **DuckStation** (precisa de BIOS) ou em
outro emulador de PS1; ou grave `.cue/.bin` num CD-R para um console. O
`.exe` serve para emuladores que carregam PS-EXE direto.

| Botão | Ação |
|---|---|
| direcional | andar (relativo à câmera) |
| X | pular / confirmar |
| QUADRADO | atacar (arco: segure para carregar) |
| TRIÂNGULO | trocar de arma |
| CÍRCULO | cura rápida (bandagem, depois kit médico) |
| R2 (segure) | andar devagar e em silêncio |
| L1 / R1 | girar a câmera |
| SELECT | lanterna |
| START | menu: mapa, objetivo, estatísticas, armas, itens, opções (o jogo para) |

**2 jogadores:** aperte START no controle 2 na tela de título.

## Novidades

- **4 fases** (Caçada, Travessia, Relíquias, Cerco) com mapas gerados pela
  semente e **eventos** que mudam a floresta durante a partida.
- **Combate de sobrevivência:** espada, machado, arco, pistola, espingarda e
  magias (bola de fogo, clarão, cura); inimigos que veem e ouvem.
- **Inventário de 8 espaços**, cura rápida no Círculo.
- **Menu do START** com mapa da névoa de exploração e estatísticas da fase e
  da campanha; **opções** com o volume dos efeitos.
- **Memory card:** salva o começo da fase (sozinho ao vencer e pelo menu);
  save inválido ou de outra versão é recusado sem travar.
- **Floresta viva:** céu, lua, pássaros, olhos no escuro, vulto, bichos.
- **Tela de título e créditos** novos.

## Qualidade (medido antes da release)

| O quê | Resultado |
|---|---|
| Desempenho (bench, 3 piores lugares de cada fase) | pior quadro 434 linhas (normal) e 451 (evento), sempre **30 FPS** |
| Maratona (robô, 2 h de jogo desenhando) | sem travar; 41 fases; pools com folga; pilha ~1,3 KB; 0 quadros abaixo de 30 FPS |
| Memory card (`./dev save-test`) | carregar dá o **mesmo estado** (hash igual); save corrompido/de outra versão recusado |
| Lógica (`replay-test`, `robo-test`) | iguais às referências |
| RAM / SPU | ~977 KB de 2 MB (orçamento 1,2 MB) / ~127 KB de 450 KB |

## Limitações conhecidas

- Sem música (v1.0). O save guarda o **começo** da fase atual.
- Sem recorte de polígonos no plano próximo; ordenação por profundidade média
  (sobreposições ocasionais, típicas do PS1).
- Testado no DuckStation; **não** testado no PCSX-Redux nem em console real.
- Modo link (2 consoles) ainda não existe.

## Publicar no GitHub (para o Marcos)

Depois de fazer o merge da `etapa-10-polimento` na `main`:

```bash
cd ~/psx-arena
git switch main && git pull
./dev release
gh release create v0.9.0 \
  dist/mata-escura-v0.9.0.zip dist/mata-escura-v0.9.0.cue \
  dist/mata-escura-v0.9.0.bin dist/mata-escura-v0.9.0.exe \
  --target main --title "MATA ESCURA v0.9.0 (floresta)" \
  --notes-file docs/NOTAS-v0.9.md
```
