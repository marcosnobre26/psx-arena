# Teste final do Marcos (tudo o que ficou para jogar de verdade)

Lista única de tudo o que as etapas 02–10 deixaram para o seu teste. Os
números do robô e do bench já estão no `docs/HISTORICO.md`; aqui é o que
**só jogando** dá para decidir. Depois de jogar, me diga as respostas das
perguntas (pode ser só "ok" ou "não") e eu proponho os ajustes.

## Como jogar

```bash
cd ~/psx-arena
./dev run              # jogo COM registro de partida (logs/partida-*.txt)
./dev analisar-partida # depois de fechar o emulador: resumo do que aconteceu
```

**Teclado** (o mapeamento da sua instalação do DuckStation):

| Tecla | Botão | Tecla | Botão |
|---|---|---|---|
| setas | direcional | Enter | START |
| K | X (pular / confirmar) | Backspace | SELECT (lanterna) |
| J | QUADRADO (atacar; arco: segure) | Q / E | L1 / R1 (câmera; abas do menu) |
| I | TRIÂNGULO (trocar arma) | 1 | L2 (overlay de depuração) |
| L | CÍRCULO (cura rápida; sair do menu) | 3 | R2 (andar devagar, em silêncio) |

Atalhos de depuração: **1** uma vez abre o overlay (FPS, polígonos,
memória); **1 duas vezes** abre a página AMBIENTE; com o overlay aberto,
**1 + Enter** pula de fase (para testar fases avançadas sem jogar tudo).

---

## 1. Primeira impressão (título, memory card, créditos)

- [ ] **Título MATA ESCURA:** o logotipo, a frase e o fundo combinam?
  *Pergunta:* o nome e o logotipo ficam? A versão "V0.9.0" no canto incomoda?
- [ ] **Novo jogo → jogar a f1 até vencer → aparece "JOGO SALVO".** Feche o
  emulador, abra de novo (`./dev run`), **CONTINUAR**: começa a f2 com o que
  você tinha (armas, itens, vida, pontos)?
  *Pergunta:* faltou alguma coisa no que voltou?
- [ ] **CREDITOS:** o texto rola e volta com X ou O. *Pergunta:* "CRIADO POR
  MARCOS NOBRE" está certo como você quer aparecer?
- [ ] **Sem música** (vem na v1.0). *Pergunta:* o silêncio do título incomoda
  até lá?

## 2. Dificuldade e balanceamento (o robô aponta, você decide)

- [ ] **Batedor na f3 (Relíquias):** antes do inventário (etapa 08) o robô
  morria em ~45% das vezes com ele; hoje vence 20/20, mas **raspando** (vida
  mínima média 26 de 80). Jogue a f3 com o Batedor.
  *Pergunta:* a tensão está boa ou ele morre/escapa fácil demais?
- [ ] **Dificuldade geral:** o robô "mediano" vence 223 de 240 partidas (fase
  avulsa, começando do zero). *Pergunta:* para você o jogo está fácil, no
  ponto ou difícil? (Se fácil: menos itens, inimigos mais atentos ou mais.)
- [ ] **O GRANDE** (dano 22, devagar, você o ouve antes de ver).
  *Pergunta:* dá medo sem ser injusto? Bater e recuar funciona para você?
- [ ] **Itens de cura talvez fartos demais:** o robô costuma terminar as fases
  com bandagens, poções e pilhas sobrando. *Pergunta:* você sentiu falta de
  cura alguma vez? Se nunca, posso cortar itens (H/N/L) nas fases.
- [ ] **Caçador** (rápido, foge da lanterna). *Pergunta:* ele assusta ou é
  inofensivo com a lanterna sempre ligada?
- [ ] **Munição:** balas aparecem só com a pistola chegando pelo W (arco →
  pistola → machado → espingarda). *Pergunta:* a pistola fez diferença?
- [ ] **Campanha seguida é mais fácil que fase avulsa:** na maratona o robô
  venceu 41 fases e morreu 1 vez levando o progresso de uma fase para outra.
  *Pergunta:* a partir da f3 ficou fácil demais com o que você acumulou?

## 3. A floresta

- [ ] **Floresta rala ou densa?** (você pediu mata alta na 05). Ande pelas 4
  fases. *Pergunta:* a densidade está boa ou quer mais mata/menos clareiras?
- [ ] **Eventos** (a cada 45–90 s: névoa avança, árvore cai, mata fecha,
  onda, silêncio; chave abre a barreira de raízes). *Pergunta:* a frequência
  está boa? Algum evento irrita mais do que assusta?
- [ ] **Árvores que tapam você ficam a 50%.** *Pergunta:* alguma vez você
  perdeu o personagem de vista atrás de árvore ou mata?

## 4. Ambiente (só visual)

- [ ] **Vulto:** figura pálida que desliza de lado olhando para você por
  ~1,2 s. *Pergunta:* está claro demais (parece fantasma de desenho) ou no
  ponto? (Para ver na hora: 1 duas vezes, segure 1 e aperte K.)
- [ ] **Olhos no escuro** somem com a lanterna (às vezes encaram antes).
  *Pergunta:* assustam? (1 duas vezes, segure 1 e aperte L.)
- [ ] **Pássaros:** no horizonte e rasantes pela lanterna; um bando levanta
  voo quando você ataca perto de árvores. *Pergunta:* você notou sem procurar?
- [ ] **Galho estalando atrás de você** (só som). *Pergunta:* deu vontade de
  virar a câmera?

## 5. Menu e inventário

- [ ] Enter abre o menu e o jogo **para** (inimigos congelados).
- [ ] Abas com Q/E: MAPA (só o que você viu), OBJET., ESTAT. (fase e total),
  ARMAS (K equipa), ITENS (K usa, J larga), OPCOES (setas: volume; K em SALVAR).
- [ ] **Inventário cheio** (8 espaços): o item fica no chão com "INVENTARIO
  CHEIO". *Pergunta:* 8 espaços é pouco, muito ou certo?
- [ ] **Volume dos efeitos** muda na hora e volta igual depois do CONTINUAR.

## 6. Dois jogadores (economia para depois)

- [ ] Com o 2º controle (Start no controle 2 no título): jogue uma fase junto.
  A munição e a mana são de cada um; os inimigos são os mesmos.
  *Pergunta:* faltou munição/cura dividindo? (A economia com 2 jogadores
  ainda não foi calculada; com a sua resposta eu faço.)

## 7. Desempenho e console

- [ ] Overlay **1**: FPS 30 nas partes mais densas de cada fase.
  *Pergunta:* viu queda, "câmera lenta" ou tremida em algum lugar? Anote a
  fase e a SEED que aparece no overlay.
- [ ] **PCSX-Redux:** já testado sem janela (replay com hashes iguais,
  save-test OK, telas iguais). Se quiser ver com os seus olhos: abra
  `~/emuladores/pcsx-redux/PCSX-Redux-HEAD-x86_64.AppImage` e carregue o
  `dist/mata-escura-v0.9.0.cue`. *Pergunta:* os controles e o som estão ok nele?
- [ ] **Console real (CD-R),** se tiver: o mesmo `.cue/.bin`.
