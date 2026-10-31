# OpenDOSPRN

Utilitário Windows (32 bits) para imprimir a partir de programas DOS: captura LPT1–9 / COM1–3, converte o
fluxo (texto + ESC/P) em páginas, mostra pré-visualização e envia para impressora Windows ou PDF.
Reimplementação própria, escrita a partir do comportamento observado do DOSPRN. **Sem registro, trial, chave ou bypass.**

## Compilar
Linux (cross, MinGW-w64): `./build.sh` → `build/OpenDOSPRN.exe`
Windows/MinGW: `cmake -B build -G "MinGW Makefiles" && cmake --build build`

## Instalar
Copie `OpenDOSPRN.exe` e `Portuguese-BR.lng` para uma pasta. Opcional (mesma pasta): `codepages.lst`, `*.cp`, `*.cpx`,
`*.lng`, `ASCII.TBL` do DOSPRN original — os formatos são lidos como estão. Sem eles o programa usa as páginas de
código do Windows e o texto em inglês.

## Como funciona
- Captura: `DefineDosDevice(LPTn -> \Device\NamedPipe\OpenDOSPRN_LPTn)`; um servidor de pipe grava em `%userprofile%\prnspl`.
  **Precisa de administrador** (senão aparece o aviso de conta limitada).
- **Todas as portas ao mesmo tempo**: LPT1–9 e COM1–3 têm captura própria (um pipe, uma thread e um spool `%userprofile%\prnspl_<PORTA>` cada) e **perfil individual** completo (spool, timeout, auto-impressão, orientação, papel, texto, impressora, cópias, RAW, sequências, saída/PDF, fonte, CPI/LPI, margens, página de código). Os botões de porta escolhem qual perfil está sendo editado; `*` = capturando, `!` = job pendente. Registro: `HKCU\Software\OpenDOSPRN\Ports\<PORTA>`; INI: seção `[Port.<PORTA>]`. Idioma, 'iniciar minimizado' e esquemas de sequência são globais. Configuração da versão anterior (porta única) é migrada para todas as portas.
- Auto-impressão: sem dados novos por N segundos, o spool vira um job.
- Emulação ESC/P: negrito, itálico, sublinhado, condensado, largura dupla, 10/12/15 cpi, espaçamento de linha,
  margens, tabulações, form feed, posição absoluta/relativa, imagens bit-image (ESC * K L Y Z), seleção de página de código.
  Esquemas personalizados traduzem as sequências de outra impressora para ESC/P (botão `...`).
- Saída: impressora (GDI) ou modo RAW, e/ou PDF (Courier padrão, caixas/blocos desenhados como vetor).
- Modelo de nome do PDF: `yy yyyy m mm mmm mmmm d dd hh nn ss (literal) <fname> <fext> <uname>`.
- `Serviço`: exporta/importa configurações em INI. Config no registro: `HKCU\Software\OpenDOSPRN`.
- Parâmetro `/show` abre a janela mesmo com "Iniciar minimizado".

## Não implementado (diferenças em relação ao original)
- `DosprnLoader*.exe` / `DosprnIntegration*.dll` (integração com DOSBox/VMs), driver SPOOLPRN (Win9x), fonte MICR.
- Download de caracteres (`ESC &`), comandos `ESC (` (ignorados), proporcional, NLQ.
- PDF sem incorporar fontes (usa Courier padrão; acentos Latin-1/WinAnsi; CJK vira `?`).
- Verificação de nova versão, e-mail de suporte, relatório de bug.
- Captura de COM no Wine falhou (erro 183: o Wine já define COM1); LPT1 e LPT2 simultâneas foram testadas com perfis diferentes. Confirmar COM no Windows real.
- Não testado em Windows real nem com impressora física (testado em Wine: UI, captura por pipe, spool, auto-PDF).
