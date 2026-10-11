#!/usr/bin/env python3
"""
bench_settings.py - Monta o settings.ini do DuckStation usado pelo ./dev bench.

Uso: python3 tools/bench_settings.py [--jogar] <settings.ini do usuário ou ""> <saída> <pasta da BIOS>

--jogar (./dev run, etapa 07b): só liga o log da TTY no console (o ./dev run
grava em logs/); controles, velocidade, tela: tudo como na sua instalação.

Parte da configuração do usuário (controles, BIOS etc.) e muda só o que o
benchmark precisa, numa CÓPIA (a configuração original não é tocada):
  - log da TTY ligado e mandado para o console (é por ali que chegam as
    linhas BENCH ... do printf do jogo);
  - velocidade ilimitada (o jogo mede FPS pelos retraços emulados, então o
    resultado não muda; só termina mais rápido);
  - sem confirmação ao fechar, sem pausar ao perder o foco, sem salvar estado.
"""
import configparser
import os
import sys

args = sys.argv[1:]
play = bool(args) and args[0] == "--jogar"
if play:
    args = args[1:]
src, dst, bios = args[0], args[1], args[2]
ini = configparser.ConfigParser(interpolation=None, strict=False)
ini.optionxform = str                       # mantém maiúsculas das chaves
if src and os.path.isfile(src):
    ini.read(src, encoding="utf-8")

def put(section, key, value):
    if not ini.has_section(section):
        ini.add_section(section)
    ini.set(section, key, value)

put("BIOS", "TTYLogging", "true")
put("BIOS", "SearchDirectory", bios)
put("Logging", "LogToConsole", "true")
put("Logging", "LogLevel", "Info")
put("Logging", "LogTimestamps", "false")
if play:                                    # jogar: o resto fica como o seu
    put("Main", "SetupWizardIncomplete", "false")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "w", encoding="utf-8") as f:
        ini.write(f, space_around_delimiters=True)
    sys.exit(0)

put("Main", "EmulationSpeed", "0")          # 0 = ilimitada
put("Main", "ConfirmPowerOff", "false")
put("Main", "PauseOnFocusLoss", "false")
put("Main", "SaveStateOnExit", "false")
put("Main", "SetupWizardIncomplete", "false")
put("Main", "StartFullscreen", "false")
put("BIOS", "TTYLogging", "true")
put("BIOS", "SearchDirectory", bios)
put("BIOS", "PatchFastBoot", "true")
put("Logging", "LogToConsole", "true")
put("Logging", "LogLevel", "Info")
put("Logging", "LogTimestamps", "false")
put("Display", "ShowFPS", "false")

os.makedirs(os.path.dirname(dst), exist_ok=True)
with open(dst, "w", encoding="utf-8") as f:
    ini.write(f, space_around_delimiters=True)
