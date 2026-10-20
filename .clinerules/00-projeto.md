# MATA ESCURA — regras para o agente (Cline)

Você está continuando um projeto que foi desenvolvido até aqui com outro agente.
Todo o conhecimento do projeto está nos arquivos do repositório; nada depende
de conversas anteriores.

## Antes de qualquer tarefa

1. Leia `CLAUDE.md` inteiro. Ele é o manual do projeto (regras, arquitetura,
   comandos, armadilhas). O nome do arquivo é histórico: vale para você também.
2. Leia `docs/CONTINUIDADE.md` (estado atual e próximos passos).
3. Para entender uma decisão antiga, procure em `docs/HISTORICO.md`
   (use busca por palavra; não leia o arquivo inteiro, ele é longo).
4. O prompt de cada etapa está em `docs/prompts/NN-*.md`; o plano geral em
   `docs/ROADMAP.md`.

## Como trabalhar

- Converse em português do Brasil. Texto na tela do jogo: ASCII sem acentos.
- Use o modo Plan primeiro: mostre o plano e espere o Marcos aprovar antes de
  editar arquivos.
- Leia arquivos grandes por partes (ex.: `src/main.c` tem ~2500 linhas): use
  busca (`grep -n`) e leia só o trecho necessário.
- Rode tudo pelo script `./dev` no terminal do WSL (Ubuntu). Nunca compile
  fora dele: o compilador fica dentro do Docker (imagem `psx-arena-sdk`).
- Trabalhe numa branch da etapa (`etapa-XX-nome`), nunca direto na `main`.
  Faça commits pequenos, um por passo que funciona.

## Regras que não podem ser quebradas

- Determinismo (o jogo é preparado para modo link/lockstep): a lógica só usa
  `g.in[]`, `g.rng` e passos fixos de 60 Hz. Efeitos só visuais ou sonoros
  usam `fx_range` e nunca mudam `g` nem chamam `g.rng`.
- Não use `__builtin_bswap32`, `__builtin_popcount` nem outros builtins que
  puxem a libgcc: o jogo trava.
- Não edite `models/*.h` à mão (são gerados por `./dev models`).
- Orçamentos: pior quadro <= 450 linhas (480 em eventos); RAM e SPU dentro do
  que o `CLAUDE.md` define.

## Conferência obrigatória antes de dizer que terminou

```bash
./dev build          # sem warnings novos
./dev replay-test    # deve dar IGUAL
./dev robo-test      # deve dar IGUAL
./dev bench          # pior quadro dentro do limite
```

Se a lógica do jogo mudou de propósito, explique o motivo e só então regrave
as referências com `./dev replay-test --save` e `./dev robo-test --save`.
Para mudanças de equilíbrio rode também `./dev robo 240` e `./dev economia`.
Para mudanças visuais rode `./dev capturas` e confira as imagens em
`docs/capturas/`.

## Ao terminar uma etapa

- Atualize `docs/HISTORICO.md` (data, o que mudou, medições),
  `docs/TESTE-FINAL.md` (o que o Marcos deve conferir) e `docs/CONTINUIDADE.md`.
- Diga ao Marcos exatamente o que mudou e o que ele pode olhar no jogo.
- Commit em português, sem linhas de coautoria de outros agentes.
