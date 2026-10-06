# Etapa 11 — Modo link: 2 jogadores em consoles/emuladores separados

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, manual capítulo 17 inteiro
> (receita R11: protocolo, código e testes).

## Objetivo

Jogar a dois com cada pessoa no próprio PC (PCSX-Redux ligados pela SIO1 via
TCP) ou console (cabo link), em lockstep.

## Tarefas

1. Implementar a receita R11 (`net.c`, `net.h`, integração no laço,
   `LINK_MODE` em `config.h`), adaptada ao código atual:
   - a **semente** do handshake vira `g.seed` (mapa procedural igual nos dois);
   - estado fora de `g` que afete a lógica precisa ser zerado no início da partida;
   - o menu do START (etapa 08) pausa os dois lados (START é sincronizado).
2. **Hash de estado** mais completo (jogadores, inimigos, projéteis, itens,
   `g.rng`, eventos) e log de dessincronia por `printf` (TTY).
3. **Auditoria de determinismo**: procurar usos de `rand()`, `fx_rng` na
   lógica, leituras de hardware fora de `input_update`, timers por quadro.
4. **Teste com dois PCSX-Redux** na mesma máquina (manual 17.7) e depois pela
   internet via VPN; ajustar `NET_DELAY`.
5. **Tela de conexão** não bloqueante (handshake em etapas por quadro, com
   "Aguardando..." animado e cancelar com CÍRCULO).
6. Documentar no GUIA o passo a passo para jogar com um amigo.

## Critérios de aceite

- 15 minutos de partida a dois sem "DESSINCRONIZADO" no mesmo PC.
- Partida pela internet (VPN) jogável com `NET_DELAY` documentado.
- Sem `LINK_MODE`, o jogo continua idêntico ao modo local.
