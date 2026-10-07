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
