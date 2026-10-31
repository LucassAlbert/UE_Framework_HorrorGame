# Validação da base — 12/10/2026

Engine verificada: Unreal 5.8.3, Windows.

## Verificações concluídas

| Verificação | Resultado | Alcance |
| --- | --- | --- |
| `HorrorGameFrameworkEditor`, Development/Win64 | Passou | Build completo após mudanças em headers refletidos e dependências. |
| `HorrorGameFramework`, Development/Win64 | Passou | Compilação e link do target de runtime; não equivale a teste de jogo empacotado. |
| `HorrorFramework.Save.ComponentRoundTrip` | Passou, sem avisos | Captura/serialização/restauração em memória de posição e componente; preservação de propriedades transitórias; rejeição de mapa incorreto, ator ausente, IDs duplicados e slots fora de 0–4. Não grava slots do usuário. |
| Compilação pelo MCP de Blueprints consumidores | Passou | Player, controller, game modes de gameplay/boot, menu, pausa, HUD, BP_InteractiveItem e BP_Door. |
| PIE em BootScene após corrigir o controller de boot | Passou na inicialização | Login do jogador, PlayerState válido, cursor visível e logs de conclusão do boot. A sessão iniciada para validação foi encerrada. |
| Cook de Windows dos mapas de boot/gameplay | Passou, sem erros ou avisos no resumo | Cook inicial completo e revalidação incremental depois de salvar BP_BootGameMode. Não foi feito staging/empacotamento. |
| `git diff --check` nos fontes alterados | Passou | Verificação de whitespace. |

O GameMode de boot usava `NoPawnPlayerController`, cuja implementação não cria PlayerState. A configuração foi alterada e salva para usar o controller Blueprint do framework, mantendo `DefaultPawnClass=None`. O último build também inclui a proteção para não fechar a tela raiz do menu quando não há gameplay por baixo.

Os builds emitiram avisos sobre a versão do MSVC instalada não ser a preferida da engine e depreciações em headers da própria engine. Esses avisos não bloquearam os builds finais.

## Ainda pendente

- Testar a navegação visual completa, gamepad, pausa sobre pausa, retorno de configurações e HUD. A captura nativa de janelas falhou neste ambiente; a captura de viewport pelo MCP mostrou somente a cena do editor.
- Gravar/restaurar um slot em disco, fechar/reabrir o jogo e restaurar após morte.
- Persistir/verificar GUIDs de instâncias colocadas antes de validar saves em jogo empacotado.
- Implementar os sistemas e o cenário completo definidos em `FrameworkRoadmap.md`.

Não foram executados testes de inventário, inspeção, IA ou do cenário completo na etapa 1. Os resultados do inventário estão registrados abaixo.

## Etapa 2 — inventário — 13/10/2026

Engine: Unreal 5.8.3, Windows. O mapa novo é `/Game/Maps/FrameworkInventory_Demo`; o mapa original e os assets originais do HorrorCore foram preservados.

| Verificação | Resultado e alcance |
| --- | --- |
| Builds completos Editor e runtime Development/Win64 | Passaram após as correções finais. A leitura de diff Git de arquivos gerados foi interrompida durante o link para liberar os arquivos mapeados; nenhum arquivo foi removido. |
| Automação `Inventory.Transactions` | Passou: empilhamento, capacidade, rollback de combinação sem espaço, dois ingredientes do mesmo stack, equipamento e descarte desabilitado. |
| Automação `Inventory.WorldSavePhysics` | Passou: inventário, equipamento, reconstrução do drop com velocidades, restauração de pickup/porta e coleta real pelo scanner ativo. |
| Automação `Save.ComponentRoundTrip` | Passou: inclui restauração de valores iguais ao padrão da classe. |
| Automação `Save.RuntimeActorReconstruction` | Passou: gravação em slot isolado, identidade estável, recriação, remoção de atores posteriores e rejeição de IDs duplicados. |
| `Inventory.ColdRestart`, duas execuções independentes | Escrita e leitura passaram: arquivo real, dependências inicialmente descarregadas, load assíncrono, inventário equipado e drop físico reconstruído. O slot temporário de teste foi removido pelo próprio teste. Nenhum slot do usuário foi sobrescrito. |
| PIE com teclado e captura visual | Coleta dos quatro tipos por E, inventário com cinco slots/quantidades, uso da chave consumindo uma unidade e fechando a tela, combinação A+B, equipamento da lanterna à esquerda e drop da lanterna equipada liberando o slot e removendo o ator visual. Escape retoma gameplay. |
| Cook Windows | Passou: boot, mapa original e novo mapa de inventário; resumo com zero erros e zero avisos. Não houve staging/empacotamento. |
| `git diff --check` em fontes/documentação | Passou; apenas avisos de conversão LF/CRLF do Git. |

A execução geral teve quatro testes efetivos e uma entrada `ColdRestart` que apenas informa a necessidade das duas execuções especiais. O reinício real foi validado separadamente, não inferido dessa entrada.

Os testes de UI reposicionaram exclusivamente atores da sessão PIE para controlar distância e visibilidade; essas posições não foram salvas no mapa. Os meshes e a tela continuam sendo placeholders.

Durante a queda física foi observado atravessamento do piso BSP por um item pequeno. Foi habilitada colisão contínua nos drops. Em nova sessão PIE com a DLL atualizada, a lanterna caiu de Z=152 para Z=7,90 e permaneceu apoiada no piso. A mensagem de seleção do segundo ingrediente também foi substituída por confirmação após combinar com sucesso.

Continuam pendentes gamepad completo, inspeção com rotação/zoom, UI funcional de seleção dos cinco saves, restauração após morte/transição, load entre mapas/streaming e teste de jogo empacotado. O save ainda não oferece rollback integral após uma falha tardia de desserialização.

## Etapa 3 — inspeção — 13/10/2026

Implementação e configuração: [InspectionImplementation.md](InspectionImplementation.md). A inspeção com rotação/zoom está implementada; o item é apresentado em uma cena temporária independente, sem alterar o inventário ou o mundo persistente.

| Verificação | Resultado e alcance |
| --- | --- |
| Build completo `HorrorGameFrameworkEditor`, Development/Win64 | Passou, incluindo headers refletidos, novo plugin e adaptadores. Uma repetição encontrou cache mapeado pela leitura de diff Git; a leitura foi interrompida durante o build e a validação completa passou. |
| Build `HorrorGameFramework`, Development/Win64 | Passou. O plugin usa somente dependências de runtime. |
| `HorrorFramework.Inspection.View` | Passou sem avisos: rotação por todos os lados, normalização após milhares de ajustes, rejeição de input não finito, limites/reset de zoom e enquadramento de mesh com pivot deslocado. |
| `HorrorFramework.Inspection.Lifecycle` | Passou sem avisos na revalidação: erro explícito para item inválido, criação do preview/render target, ajustes, fechamento repetido e três reaberturas. O primeiro teste emitia aviso por contexto ausente no mundo do próprio teste; o preparo foi corrigido. |
| Regressões de inventário/save | Transactions, WorldSavePhysics, ComponentRoundTrip e RuntimeActorReconstruction passaram. A entrada ColdRestart da execução geral apenas informa o protocolo; o teste real em dois processos permanece registrado na etapa 2. |
| Blueprints consumidores | BP_InventoryPlayer, BP_InventoryController e BP_InventoryGameMode compilaram com warnings-as-errors. Nenhum ficou dirty; não foi necessário salvar assets binários nesta etapa. |
| PIE com captura pelo MCP | Chave coletada por E, botão Inspecionar disponível, item renderizado com iluminação própria, nome/descrição, reset por R, retorno por Esc ao inventário com seleção/quantidade preservadas e gameplay retomado ao fechar o inventário. Reabertura e interrupção do PIE com inspeção aberta também foram exercitadas. |
| Cook de Windows | Passou para BootScene, FrameworkHorror_Test_Map e FrameworkInventory_Demo; resumo com zero erros e zero avisos. Não houve staging/empacotamento. |

A captura nativa de janelas do Windows retornou `FrameArrived timed out`/`window capture timed out`. A imagem da apresentação foi verificada pelo MCP e a navegação pelo teclado. O arraste/roda reais do mouse e os analógicos/gatilhos de um gamepad físico **não foram validados em PIE**; a matemática de rotação/zoom foi validada por automação. Esses testes de dispositivos, perda de foco durante arraste e baixa taxa de frames continuam pendentes.

O MCP também emitiu erros de GetCurrentLevel ao reposicionar um ator exclusivamente da sessão PIE. A ação ocorreu e a coleta foi confirmada; esses logs pertencem à ferramenta de editor, não à sessão de inspeção. As posições de teste não foram salvas. O Editor foi deixado no mapa de demonstração, com o PIE encerrado.

A integração fecha os modais associados na restauração de save, mas a combinação save/load com a inspeção aberta ainda precisa de validação específica em PIE. Inspeção não adiciona estado persistente. Hotspots/pistas interativas, documentos, diálogos e o cenário completo continuam nas etapas seguintes.

## Etapa 4a — puzzles, portas e objetivos — 13/10/2026

Implementação e configuração: [ProgressionImplementation.md](ProgressionImplementation.md). Engine Unreal 5.8.3, Windows. Mapa `/Game/Maps/FrameworkProgression_Demo`, assets em `/Game/_Project/Progression`.

| Verificação | Resultado e alcance |
| --- | --- |
| Build completo Editor Development/Win64 | Passou após os ajustes finais de ciclo de vida e fechamento das telas. Foi corrigida a proteção ausente de inclusão em `InputCharacter.h`, exposta pelo build completo. |
| Build runtime Development/Win64 | Passou; os dois plugins novos usam somente módulos de runtime. |
| `HorrorFramework.Progression.WorldSave` | Passou: fatos anteriores ao carregamento, duplicatas rejeitadas, consumo de uma unidade empilhada, item errado preservado, preparo/cancelamento, bloqueio de save durante transação, puzzle parcial/completo, abertura explícita, objetivos encadeados e entrada na área. Captura, serialização em memória e restauração recuperaram inventário, meshes, porta, fatos e visita. Overlaps durante restauração não produziram progresso. |
| Suíte `HorrorFramework` após o código final | As oito entradas passaram sem erros/avisos dos testes. Sete executam validações; `Inventory.ColdRestart` apenas informa o protocolo de dois processos, já registrado na etapa 2. Não representa novo teste de reinício desta etapa. |
| Blueprints | BP_ProgressionPlayer, BP_ProgressionGameMode e os três consumidores anteriores do inventário compilaram com warnings-as-errors. A lista de objetivos permaneceu configurada após compilar/salvar. |
| PIE | Botão Objetivos no inventário, lista inicial, retorno com seleção preservada e pausa mantida. Coleta por E, seleção por interação, mensagem para item incompatível sem consumo, inserção correta/fechamento da seleção, porta destrancada ainda fechada, abertura com E e aviso de conclusão da área confirmados. |
| Persistência dos assets | Após reabrir o Editor e encerrar os testes, o mapa continuou salvo no estado inicial: porta trancada/fechada, encaixes vazios e área não visitada. As posições de teste foram aplicadas somente aos atores PIE. |
| Cook Windows | BootScene, FrameworkHorror_Test_Map, FrameworkInventory_Demo e FrameworkProgression_Demo passaram com zero erros e zero avisos. O recook do mapa novo após o ajuste final de iluminação também passou com zero erros e zero avisos. Não houve staging/empacotamento. |

O fixture inicial de áreas não iniciava o mundo e não criava PlayerState. Essas condições foram corrigidas no teste antes da aprovação da suíte; não foi desativada nenhuma funcionalidade de gameplay. Uma repetição de build encontrou o cache gerado mapeado pelo diff Git; somente os processos de leitura daquele diff foram interrompidos para permitir a compilação, sem excluir/reverter arquivos.

Capturas nativas do Windows continuaram retornando `FrameArrived timed out`; as imagens foram verificadas pelo MCP e as ações pelo teclado. Reposicionar atores PIE também gerou erros de GetCurrentLevel da ferramenta, embora a alteração transitória tenha ocorrido.

**Incidente na validação manual:** a mira mudou após fechar uma seleção; a seleção seguinte não abriu. A navegação de teclado alcançou o botão Save do HUD existente e confirmou uma gravação em `Saved/SaveGames/SaveSlot_0.sav` às 22:07:38, sobrescrevendo o slot 0 com um estado parcial da demonstração. Não foi encontrada uma cópia anterior recuperável no projeto nem nas cópias temporárias verificadas. O incidente foi informado ao usuário. Os testes seguintes passaram a conferir a imagem da seleção antes da confirmação. Nenhum asset de mapa foi salvo com estado de PIE.

Continuam pendentes gamepad físico, baixa taxa de frames/perda de foco, save/load com objetivos aberto sobre inventário em PIE e reinício real de processo com o estado novo de progressão. A restauração de estado desta etapa foi validada por automação com snapshot serializado. Load entre mapas/streaming, migração de conteúdo alterado, rollback integral de restauração e jogo empacotado mantêm as limitações existentes. Documentos, diálogos, IA e sustos não fazem parte desta etapa.

## Etapa 4b — documentos — 14/10/2026

Implementação e configuração: [DocumentsImplementation.md](DocumentsImplementation.md). Unreal 5.8.3, Windows. Mapa `/Game/Maps/FrameworkDocuments_Demo`, assets em `/Game/_Project/Documents`.

| Verificação | Resultado e alcance |
| --- | --- |
| Builds completos Editor e runtime Development/Win64 | Passaram após a última alteração, incluindo o plugin HorrorDocuments e o bloqueio de ações do HUD sob modais. |
| `Documents.Collection` | Passou: definição inválida, páginas sem texto, aquisição sem leitura, duplicatas por referência/ID, documento não adquirido e leitura emitida somente uma vez. |
| `Documents.WorldSave` | Passou: interação real com distância/trace, ocultação, inventário preservado, fatos de objetivos, snapshots anteriores à coleta/coletados não lidos/lidos e serialização em memória. A restauração não reemitiu fatos de coleta/leitura. |
| Suíte final `HorrorFramework` | Onze entradas com sucesso. Nove executam validações; as duas entradas ColdRestart apenas descrevem os respectivos protocolos quando chamadas sem flags especiais. |
| `Documents.ColdRestart`, dois processos | Escrita e leitura passaram em execuções independentes: arquivo real exclusivo, definição inicialmente descarregada no leitor, dependências carregadas assincronamente, coleção/estado lido/pickup/fatos restaurados. O teste removeu somente seu próprio slot temporário. |
| Blueprints | Os novos BP_DocumentsPlayer/BP_DocumentsGameMode e os consumidores de progressão/inventário compilaram com warnings-as-errors. Os novos foram recompilados após reabrir o Editor. |
| PIE e teclado | E abre automaticamente; pickup oculto, objetivo de leitura concluído e objetivo do puzzle ativado. Duas páginas, imagem opcional, limites de paginação, Page Up/Page Down, cinco slots vazios e coleção com estado Lido confirmados. Reabertura começa na primeira página, sem novo aviso de leitura. Esc retorna leitor → coleção → inventário → gameplay; HUD desabilitado sob modais e habilitado ao retomar. Interrupção com leitor aberto também exercitada. |
| Estado autorado | Após reabrir o Editor e encerrar o PIE, pickup permaneceu não coletado e visível no mapa salvo. Nenhum estado de PIE foi salvo em assets. |
| Cook Windows | BootScene e as quatro demonstrações passaram com zero erros e zero avisos. Não houve staging/empacotamento. |
| Preservação dos saves | Uma cópia foi feita antes dos testes. O SHA256 de SaveSlot_0.sav permaneceu igual à cópia; nenhum slot do jogador foi sobrescrito. |

Logs e relatórios desta validação estão na pasta temporária do usuário: `HorrorDocumentsFinalTests`, `HorrorDocumentsWriteTests`, `HorrorDocumentsReadTests` e `HorrorDocumentsCook.log`. O slot exclusivo foi `HorrorFramework_Documents_Test_20261014_1552`.

O Output Log registrou a tentativa de restauração do slot 0 durante a inicialização de ambas as sessões PIE, antes dos inputs de teste. O save pertence a outro mapa e o SaveSystem recusou a restauração, conforme a limitação existente de load entre mapas. Não houve gravação ou restauração parcial por essa tentativa. Também houve um erro de sessão MCP antiga após reabrir o Editor; a conexão foi restabelecida.

Gamepad físico, baixa taxa de frames/perda de foco, imagem ausente e fechamento durante um carregamento de imagem ainda pendente não foram reproduzidos manualmente. O fechamento com leitor aberto foi testado; isso não equivale a testar uma requisição lenta. Save/load com coleção aberta em PIE, migração de saves antigos/conteúdo alterado, streaming e jogo empacotado continuam pendentes. A interface e os meshes são placeholders. Diálogos, IA e sustos ficam para as próximas etapas.

## Etapa 4c — diálogos sequenciais com NPCs — 15/10/2026

Configuração e arquitetura: [DialogueImplementation.md](DialogueImplementation.md). Engine Unreal 5.8.3, Windows. Mapa `/Game/Maps/FrameworkDialogue_Demo`, conteúdo próprio em `/Game/_Project/Dialogue`.

| Verificação | Resultado e alcance |
| --- | --- |
| Builds completos Editor e runtime Development/Win64 | Passaram após a revisão final de input/foco. Live Coding não foi utilizado como substituto. |
| `Dialogue.Session` | Passou: definições/condições inválidas, sequência, confirmação da última fala, sessão exclusiva, cancelamento sem conclusão, reinício na primeira fala, repetição e identidade estável. |
| `Dialogue.WorldSave` | Passou: fatos exigidos/bloqueantes, integração desativada, cancelamento sem progresso, conclusão atualizando objetivo, repetição sem reemitir fato, serialização em memória, rewind e restauração com sessão ativa. O load encerrou a sessão e recuperou o histórico sem repetir conclusão. |
| Suíte final `HorrorFramework` | Quatorze entradas passaram. Onze executam validações; as três entradas ColdRestart, sem flags, somente informam seus protocolos especiais. |
| `Dialogue.ColdRestart`, dois processos | Escrita e leitura passaram em processos independentes. O histórico e o fato persistiram; a conversa única continuou indisponível e nenhuma sessão foi retomada pela metade. O arquivo exclusivo foi removido pelo teste. |
| Blueprints | BP_DialoguePlayer/BP_DialogueGameMode, consumidores anteriores de documentos/progressão/inventário, WBP_Gameplay e BP_HorrorPlayerController compilaram com warnings-as-errors. A lista de quatro objetivos da demonstração foi conferida após compilar. |
| PIE — sequência e controles | E abriu e avançou três falas; a confirmação final concluiu o objetivo, e a conversa repetível reabriu na fala 1. Esc cancelou preservando o PIE; prompt e HUD voltaram. W não alterou a posição durante o diálogo; após cancelar, o comando voltou a deslocar o jogador. Inventário bloqueado durante a conversa, disponível depois, com pausa/retomada normais. |
| PIE — câmera e mundo ativo | O NPC foi reposicionado somente em PIE e a câmera acompanhou seu LookTarget com a conversa aberta. Em outro teste, InitialLifeSpan temporário de 12 segundos expirou durante a conversa: o NPC deixou a sessão, a apresentação fechou e o HUD foi reabilitado. Isso também confirma que o timer do mundo continuou ativo. |
| Limpeza e assets | PIE encerrado também com conversa aberta. InitialLifeSpan do NPC autorado foi restaurado a zero e salvo apenas no mapa novo; uma nova sessão manteve o NPC. As posições transitórias não foram salvas. |
| Cook Windows | BootScene e as cinco demonstrações passaram com zero erros e zero avisos. Sem staging/empacotamento. |
| Saves existentes | Cópia antes dos testes em `HorrorDialogueSaveBackup-20261015-195926`, na pasta temporária do usuário. SaveSlot_0.sav manteve o SHA256 `993620832BA65AE506C1FA11AB8E7552859E606C5FE7C2344F00C6A335A20134`. |

Logs e relatórios na pasta temporária: `HorrorDialogueFinalTests`, `HorrorDialogueWriteTests`, `HorrorDialogueReadTests` e `HorrorDialogueCook.log`. O slot exclusivo de reinício foi `HorrorFramework_Dialogue_Test_20261015_2005`.

A primeira apresentação usava input GameOnly; Esc foi capturado pelo atalho do Editor e encerrou o PIE. A política foi corrigida para GameAndUI com foco na tela: Voltar é consumido pela UI e ações não tratadas seguem ao Enhanced Input. O cancelamento, o avanço e o retorno ao inventário foram revalidados com essa versão. Os conflitos de compilação iniciais com nomes herdados de componentes/widgets e uma declaração duplicada foram corrigidos antes dos resultados acima.

O MCP recusou a remoção direta de um ator durante PIE. O teste de interrupção foi realizado pela vida útil temporária do NPC, sem exclusão de conteúdo autorado. O Output Log também contém a sessão MCP antiga após reabrir o Editor e a recusa existente de restauração do slot 0 pertencente a outro mapa; essas tentativas não gravaram o slot nem restauraram parcialmente o mundo.

Permanecem pendentes gamepad físico, remapeamento exercitado em dispositivo, perda de foco/baixa taxa de frames, troca efetiva de pawn, game over sobre a conversa e save/load com a apresentação aberta em PIE. A automação valida o histórico/restauração em mundo de teste, não substitui esses cenários visuais. Migração de IDs/saves antigos, load entre mapas/streaming e jogo empacotado mantêm as limitações existentes. Falas devem ser divididas em trechos adequados à apresentação; o NPC e a UI são placeholders. Não há dublagem, ramificações, animação de fala ou IA de NPC nesta etapa.

## Consolidação após a etapa 4c — 17/10/2026

Arquitetura, controles e reutilização: [FrameworkConsolidation.md](FrameworkConsolidation.md). Engine confirmada: Unreal 5.8.3; Windows Development/Win64.

| Verificação | Resultado e alcance |
| --- | --- |
| Build completo Editor | Passou após o código final e o teste dos assets de input. O primeiro build de testes encontrou a flag inexistente ApplicationContextMask; corrigida para EditorContext/ClientContext antes da execução. |
| Suíte final HorrorFramework | 17 entradas com sucesso, zero falhas e zero avisos de teste. 14 executam validações; três ColdRestart, sem flags especiais, apenas informam seus protocolos. Não houve nova execução dos protocolos de dois processos nesta consolidação. |
| Consolidation.UIOwnership | Passou: reaplicação sem acúmulo, pausa e bloqueios externos preservados, liberação sem controller atual e transferência entre controllers. |
| Consolidation.PlayerLifecycle | Passou: posse real entre dois pawns, cancelamento do timer de esconderijo, saída sem ultrapassar modal, liberação de apresentação, velocidade/entrada transitórias limpas no callback de restore, view target recuperado e comandos bloqueados em GameOver/Loading. |
| Consolidation.GamepadMappings | Passou: contextos efetivamente salvos, comandos aprovados, View no contexto da demonstração, drift filtrado, sinal de câmera e rotação acumulada equivalente em 15/30/120 FPS. É teste da cadeia de modificadores, não medição de input físico nem desempenho. |
| Blueprints | BP_InventoryController compilou com warnings-as-errors após os mapeamentos. O cook compilou/carregou os consumidores incluídos nos seis mapas sem erro/aviso de cook. Não substitui conferência visual de todos os gráficos. |
| Build runtime, cook, stage, pak e archive | BuildCookRun passou, ExitCode=0. Cook com zero erros e zero avisos, 714 pacotes finais. BootScene e as cinco demonstrações estão configurados em MapsToCook. Não foi produzido build Shipping. |
| Testes no executável empacotado | As três entradas Consolidation passaram com zero falhas/avisos de teste, executadas em NullRHI. Isso confirma carregamento dos contextos e lógica fora do Editor; não valida renderização. |
| Inicialização gráfica do pacote | FrameworkDialogue_Demo iniciou em D3D SM6, registrou XInputController conectado e completou boot/fluxo Playing. A captura/ativação nativa expirou aguardando aprovação do aplicativo; a apresentação não foi confirmada visualmente pelo agente. Teste físico solicitado ao usuário. |
| Saves existentes | Backup prévio em HorrorConsolidationBackup-20261017-122417, na pasta temporária. SaveSlot_0.sav manteve SHA256 993620832BA65AE506C1FA11AB8E7552859E606C5FE7C2344F00C6A335A20134. Execuções do pacote usam UserDir exclusivo; a pasta Saved foi criada nesse diretório. |

Artefatos na pasta temporária do usuário: `HorrorConsolidationFinalTests` (relatório/log), `HorrorConsolidationRuntimeTests` (relatório/log), `HorrorConsolidationPackage.log`, `HorrorConsolidationPackage/Windows` (jogo) e `HorrorConsolidationManual.log`. Os UserDirs de validação são `HorrorConsolidationRuntimeUser` e `HorrorConsolidationManualUser`.

O log gráfico registra preloads iniciais de shaders e os avisos existentes de áudio 48 kHz/44,1 kHz; também apareceu RAW_MODE_UNSUPPORTED na reinicialização do dispositivo de áudio. O NullRHI registrou mensagens de ModelComponent com vértices ausentes; não houve essas mensagens no log gráfico verificado. Não se atribuiu uma falha visual a partir do teste sem renderização. Um leitor de diff Git mantinha a documentação mapeada; somente os leitores correspondentes foram encerrados para permitir a escrita, sem alteração/exclusão de arquivos do usuário.

Continuam pendentes reprodução visual em PIE das interrupções combinadas, save/load real com cada tela aberta, game over sobre conversa, perda de foco e baixa taxa de frames durante gameplay. A automação de posse/restore/input não substitui essas verificações. Remapeamento persistente, áudio centralizado, migração de conteúdo/saves e load entre mapas/streaming continuam fora desta implementação. A extração de plugins para outro projeto foi documentada, mas não executada como teste de portabilidade.

### Correção de navegação após o teste físico — 17/10/2026

O usuário relatou que a navegação dos menus com controle não funcionava. O log não mostrou erro de execução; a falha estava no foco enviado à raiz das telas. A base de UI passou a escolher um controle disponível e a memorizar o controle focado antes de ocultar uma tela. A troca de modo de input não sobrescreve mais esse alvo com o foco raiz. Diálogos continuam usando foco raiz por sua política explícita de gameplay input.

O build completo Editor Development/Win64 passou. A suíte `HorrorFramework` executou 18 entradas com sucesso, zero falhas/avisos de teste: 15 validações efetivas e três entradas informativas ColdRestart. A nova `Consolidation.MenuFocus` utiliza UMG e uma janela virtual do Slate com um controller/local player: ignora um botão desativado e um ramo oculto, seleciona o primeiro botão disponível, recupera o segundo botão previamente focado após ocultar/mostrar, evita restaurar um botão posteriormente desativado e preserva o foco raiz do diálogo. A janela é virtual; esse teste não injeta comandos no aplicativo do usuário nem certifica navegação física.

Build runtime, cook, stage, pak e archive passaram; cook com zero erros e zero avisos. As quatro entradas Consolidation também passaram no executável empacotado em NullRHI, sem falhas/avisos de teste. O pacote corrigido foi aberto para a nova conferência física com `UserDir=HorrorMenuFocusManualUser`, separado dos saves existentes. O SHA256 do slot 0 do projeto continuou igual ao backup.

Relatório e log: `HorrorMenuFocusTests` e `HorrorMenuFocusRuntimeTests`, na pasta temporária do usuário. O novo pacote está separado em `HorrorMenuFocusPackage/Windows`, preservando a versão anterior. Log de empacotamento: `HorrorMenuFocusPackage.log`; log do novo teste gráfico: `HorrorMenuFocusManual.log`.

Após repetir o teste físico na versão corrigida, o usuário confirmou: “Agora funciona”. A confirmação responde ao roteiro de navegação pelo direcional/analógico esquerdo, confirmação com A, retorno com B, inventário, Objetivos/Documentos e pausa. A navegação de menus com controle fica validada pelo usuário nesse roteiro; isso não encerra as demais verificações pendentes de interrupções, foco, save/load e baixa taxa de frames.

## Etapa 5 — áudio e volumes — 18/10/2026

Implementação descrita em [AudioImplementation.md](AudioImplementation.md): plugin HorrorAudio independente, reprodução assíncrona, cancelamento/limpeza, pausa por definição, volumes por grupo, prévia transacional e preferências separadas. A tela nativa foi configurada em WBP_AudioSettings/DA_UIConfig e o botão Áudio de WBP_MainSettings abre a nova tela. FrameworkAudio_Demo inclui música, ambiente e uma fonte interativa 3D; os SoundWaves de exemplo são cópias, preservando os originais.

| Verificação executada | Resultado |
| --- | --- |
| Build completo Editor Development/Win64 | Passou após o fechamento normal das instâncias autorizado pelo usuário. A correção e o teste de ciclo de vida da tela foram compilados em builds completos posteriores. |
| Blueprints consumidores | WBP_MainSettings e WBP_AudioSettings compilados pelo MCP com warnings_as_errors. Assets de áudio, configuração de UI, mapa e atores externos da demonstração salvos. |
| Suíte HorrorFramework no Editor-Cmd/NullRHI | 22 entradas passaram, zero falhas/avisos de teste: 18 validações efetivas e quatro protocolos ColdRestart informativos. Uma execução anterior falhou na preparação do teste Audio.Preferences por instanciar UObject abstrato; a preparação foi corrigida para usar objetos concretos e a suíte completa foi repetida com sucesso. |
| Preferências/ganho | Testes verificaram edição exclusiva, atualização de componentes existentes, ganho definição × geral × categoria, silêncio/retorno do volume geral, rejeição de valores inválidos, prévia sem escrita, Aplicar, cancelamento após Aplicar e rejeição de versão desconhecida sem atualização parcial. |
| Ciclo de vida de reprodução | Testados cancelamento durante carregamento, callback tardio sem reiniciar som, fim do ator de attachment, limpeza do mundo, liberação de handles e roteamento separado das SoundClasses legadas. Estes testes não aferem audibilidade. |
| Tela/integração de assets | ScreenLifecycle carrega o Blueprint configurado, constrói os controles, encontra foco em um controle interno, verifica as cinco definições de teste com ContinueWhilePaused, atualiza a prévia e confirma restauração/liberação da edição ao sair. |
| Reinício real de preferências | Audio.ColdRestart executado em dois processos separados com AudioTestWrite/AudioTestRead e UserDir exclusivo HorrorAudioTest_20261018_0442. Escrita e leitura passaram. |
| Build runtime e pacote Windows Development | Build runtime concluído. O primeiro cook não chegou à conclusão da finalização; o empacotamento foi repetido usando esse build. BuildCookRun terminou com ExitCode=0; cook de 728 pacotes com zero erros e zero avisos; stage/pak/archive passaram. BootScene e seis demonstrações estão em MapsToCook. |
| Testes no executável empacotado/NullRHI | Quatro entradas Audio passaram com zero falhas/avisos de teste: Preferences, PlaybackLifetime e ScreenLifecycle efetivos; ColdRestart informativo nessa execução. UserDir HorrorAudioRuntimeUser separado do progresso existente. |
| PIE gráfico | FrameworkAudio_Demo iniciou e a cena foi conferida por captura do Editor. Depois BootScene iniciou e exibiu o menu principal. No roteiro Settings > Áudio, o usuário confirmou: “Funciona, incluindo som e cancelamento”. Isso confirma a apresentação dos volumes, reprodução dos exemplos e retorno aos valores anteriores ao sair sem Aplicar. A navegação com controle foi opcional no roteiro e não foi confirmada separadamente. |

Permanecem pendentes pausa/retomada dos sons do mundo no dispositivo físico, confirmação específica da navegação com controle na nova tela, Aplicar via botão e interrupções com menus/diálogo. Não foram certificados Shipping, transferência do plugin para outro projeto nem restauração da posição temporal de sons. Na primeira execução PIE do novo mapa, a tentativa existente de restaurar slot 0 de outro mapa foi recusada pelo SaveSystem com diagnóstico de restauração entre mapas não implementada; o slot não foi modificado. O dispositivo registrou o aviso RAW_MODE_UNSUPPORTED já observado em etapas anteriores.

Artefatos na pasta temporária do usuário: HorrorAudioFinalTests (relatório/log), HorrorAudioColdWrite e HorrorAudioColdRead (relatórios/logs), HorrorAudioRuntimeTests (relatório/log), HorrorAudioPackageFinal.log e HorrorAudioPackage/Windows (pacote). Logs de build: InspectionBuild-HorrorGameFramework.log e InspectionBuild-HorrorGameFrameworkEditor.log.

Backup prévio: HorrorAudioSaveBackup-20261018-011153. SaveSlot_0.sav manteve SHA256 `993620832BA65AE506C1FA11AB8E7552859E606C5FE7C2344F00C6A335A20134`. Os testes que escrevem preferências usam caminhos exclusivos; não houve gravação em slots de progresso nesta etapa.

## Etapa 6 — IA e esconderijos — implementação entregue — 19/10/2026

Código e contratos iniciais em ThreatImplementation.md. Adicionados HorrorAI, integração do alvo/jogador/inimigo, ocupação e olhar limitado dos esconderijos, tela de Game Over e seleção assíncrona do save mais recente do mapa atual. As regras de entrada presenciada, busca temporária, passos silenciosos ao agachar e restauração dentro do esconderijo foram confirmadas pelo usuário.

O build runtime completo Development/Win64 passou. As primeiras compilações identificaram conflitos de nomes/reflexão e acesso, corrigidos antes desse resultado. Após o usuário confirmar o fechamento normal do Editor, o build completo Editor Development/Win64 passou. Uma nova compilação completa também passou com a correção da distância de chegada e verificações adicionais de captura.

A suíte HorrorFramework executou 25 entradas com sucesso, zero falhas/avisos de teste, em HorrorThreatFinalTests: 21 validações efetivas e quatro protocolos ColdRestart informativos. Threat.PerceptionAndMemory verifica visão/cone/parede, conhecimento da entrada no esconderijo, busca/retorno, restauração sem alvo inventado, captura bloqueada por parede e emissão única. Threat.HidingWorldSave verifica ocupação, colisão, limites de olhar, snapshot serializado e retorno à posição original após restaurar e sair. Threat.LatestSave foi executado com -ThreatTestSaves e UserDir exclusivo HorrorThreatTest_EditorFinal_20261018: criou candidatos em slots diferentes e confirmou a escolha por data/mapa, ignorando o snapshot mais novo de outro mapa. O teste recusa sobrescrever slots existentes. A execução inicial teve uma falha na preparação do teste de percepção: posse automática anterior à definição e PlayerState ausente. A preparação foi corrigida e a suíte completa repetida.

Em 19/10, a conexão foi retomada em uma instância do Editor aberta sem UserDir exclusivo. Assets em /Game/_Project/Threat e mapa FrameworkThreat_Demo foram criados e salvos. Seis Blueprints compilaram com warnings_as_errors, além do Level Blueprint. A primeira navegação falhou: posição do volume, capacidade do NavMesh e destino coincidente com a porta foram corrigidos. Após isso, o inimigo percorreu a rota em PIE. A configuração do Recast usa geração dinâmica e pool fixo de 256 tiles. O mapa novo foi incluído em MapsToCook.

O PIE revelou também listener de percepção sem identificação válida. Os sentidos passaram a ser configurados antes de OnRegister, com atualização explícita após a posse; foi acrescentada uma asserção de regressão ao teste. Os builds completos Editor/runtime dessa revisão passaram em 19/10. A suíte completa repetida em HorrorThreatTests_20261019 passou novamente nas 25 entradas, sem falhas; Threat.LatestSave usou UserDir novo HorrorThreatTest_Editor_20261019. Os resultados posteriores de PIE, save/load e pacote estão registrados abaixo. O carregamento automático do slot 0 herdado no BeginPlay foi desconectado somente no mapa novo; tentativas anteriores recusaram o save de outro mapa sem alterá-lo. O Editor foi reaberto em HorrorThreatEditor_20261019 para isolar os próximos testes manuais de persistência.

Backup prévio dos saves em HorrorThreatSaveBackup-20261018-130858, na pasta temporária. O slot 0 conservava o hash registrado na etapa anterior. Não houve gravação em slots de progresso.

O pacote Windows Development HorrorThreatPackage_20261019 concluiu BuildCookRun com ExitCode=0 em 19/10. Cook terminou com zero erros e zero avisos; pak, stage e archive passaram. Log: HorrorThreatPackageFinal_20261019.log, na pasta temporária. Uma tentativa imediatamente anterior não iniciou o cook porque a verificação de SDK do Editor ocupava o AutomationTool; foi repetida após seu término. No executável empacotado/NullRHI, os três testes Threat passaram, com zero falhas/avisos, incluindo LatestSave com escrita em UserDir exclusivo HorrorThreatTest_Runtime_20261019. O mapa FrameworkThreat_Demo foi carregado no runtime com BP_ThreatGameMode e fluxo Playing, sem avisos de listener ou falhas de navegação registrados nessa execução. Isso não substitui o teste visual de perseguição/captura. Relatório: HorrorThreatRuntimeTests_20261019. O hash do save original foi novamente confirmado sem alteração.

A conferência visual após a correção ficou temporariamente bloqueada pela necessidade de reativar o MCP na instância de testes, cuja pasta de usuário nova não herdou Auto Start Server. O usuário iniciou o servidor pelo console e os testes continuaram, como registrado abaixo.

Após o usuário iniciar o servidor MCP pelo console, o PIE foi retomado. O jogador foi iniciado em (-3050,-1380,95), no lado da patrulha: percepção, aproximação e captura ocorreram, com fluxo Playing → GameOver e tela “Você foi capturado”. Confirmada visualmente a mensagem “Nenhum save válido foi encontrado para o mapa atual” ao acionar o carregamento na pasta de testes vazia. O teste de Esc teve um reinício de mapa e precisa ser repetido sob controle antes de certificar o bloqueio de saída; não foi atribuído à implementação sem reprodução. Uma nova sessão foi iniciada diante do esconderijo (-2110,-1360,95), com prompt “Esconder-se”. A captura nativa da ferramenta de computador falha com FrameArrived timeout, e o clique por acessibilidade falha com coordinate input geometry is unavailable. Solicitado ao usuário dar foco ao gameplay e entrar com E para continuar o ciclo de persistência.

O usuário confirmou entrada com E. Leitura MCP verificou o personagem em (-2110,-1500,100), escala unitária e MovementMode=MOVE_None. O usuário salvou pelo HUD de teste e saiu com E; o retorno próximo de (-2110,-1360,92) foi conferido e SaveSlot_0.sav (31162 bytes) apareceu somente na pasta HorrorThreatEditor_20261019. O jogador foi reposicionado em runtime junto à patrulha, houve nova captura e a tela de Game Over foi conferida. O usuário acionou o carregamento e confirmou: “carregar o ultimo save voltou para o esconderijo”. O log registra GameOver → Playing. Quando da verificação posterior, a sessão já estava encerrada; movimento/câmera do estado restaurado não foram inspecionados nessa sessão. O retorno ao mesmo esconderijo por save foi confirmado pelo usuário, além do round trip automatizado já aprovado.

No teste final de Game Over, o usuário confirmou “Permaneceu aberto” ao testar Esc. O pedido incluía B caso estivesse com controle, mas não houve confirmação específica desse botão. A ocorrência anterior de reinício não se reproduziu nessa verificação; não foi feita alteração de código para uma causa não demonstrada.

Audição foi verificada em PIE com jogador e inimigo em lados opostos da porta fechada. Para manter um deslocamento controlado sem depender do foco da janela, foram ajustadas somente cópias runtime de velocidade, frenagem/atrito e rota; a distância de passo usada no resultado foi a configuração original de 160. O inimigo passou de Patrol para Investigate com LastKnownLocation=(-2630,-1233.33,92.05), correspondente ao ruído emitido pelo deslocamento, sem visão através da porta. Posteriormente retornou a Patrol. O destino bloqueado gerou o diagnóstico esperado de caminho indisponível; a IA não atravessou a porta. As tentativas iniciais de velocidade curta pararam antes da emissão de um passo ou fora do alcance útil e não demonstraram audição. A sessão temporária foi encerrada, e mapa, Blueprint do jogador/inimigo e definição de passos permaneceram sem alterações não salvas.

Limites restantes: não foi feita aferição separada em PIE de impactos de itens dropados, silêncio ao agachar, captura após entrada presenciada no esconderijo ou navegação física com controle na nova tela. A regra de testemunho/captura e as restrições de câmera/colisão têm cobertura automatizada; a guarda de agachamento foi revisada no código. Não foi certificado reinício de processo com jogador escondido, Shipping, animações ou carregamento entre mapas. O save original do projeto manteve SHA256 993620832BA65AE506C1FA11AB8E7552859E606C5FE7C2344F00C6A335A20134; as gravações desta etapa ficaram nas pastas exclusivas de testes.

### Mudança de repositório — 20/10/2026

O usuário confirmou o novo projeto em `C:/Users/luuki/Documents/GitHub/UE_Framework_HorrorGame`, com projeto Unreal em `HorrorGameFramework/HorrorGameFramework.uproject`. O AGENTS.md dessa cópia foi lido e o Git estava limpo no commit inicial 6bcdfcb. Foram comparados por SHA256 com a pasta anterior: ThreatImplementation.md, Validation.md, DefaultGame.ini, o .uproject, HorrorEnemyController.cpp, HorrorEnemyTests.cpp, SaveSubsystem.cpp, FrameworkThreatTarget.cpp e FrameworkThreat_Demo.umap. Todos os nove arquivos eram idênticos. Essa conferência não equivale a uma auditoria de todos os arquivos migrados.

Os builds, cook, testes automatizados e testes de gameplay descritos nesta etapa foram executados antes da mudança de pasta. Não foi executado um novo build nem uma nova sessão PIE nesta cópia. Nesta retomada foram atualizados somente o roteiro e os registros da documentação. As ferramentas MCP do Unreal não estão disponíveis na sessão de 20/10; isso não impede esta finalização documental, mas uma nova rodada no Editor precisará da conexão. A mudança de pasta não alterou os contratos nem o escopo da etapa 6.

## Etapa 7 — streaming e slots — entregue em 27/10/2026

Implementação e configuração em [StreamingSaveImplementation.md](StreamingSaveImplementation.md). Plugin HorrorWorld, cache/restauração de áreas no SaveSystem, continuidade do perseguidor, seleção dos cinco slots, confirmação de sobrescrita e Continuar/Game Over com o save manual mais recente de qualquer mapa. Demo separada FrameworkStreaming_Demo com duas áreas e corredor persistente. Saves de mundos independentes abrem o mapa correspondente antes da restauração; a travessia contínua aplica-se às áreas do mesmo mundo persistente.

| Verificação | Resultado final |
| --- | --- |
| Build Editor Development/Win64, Unreal 5.8.3 | Build completo passou em 24/10 após a correção de componentes persistentes; não apenas NoLink. Nenhum C++ foi alterado depois desse build. |
| Regressão Editor em 27/10 | 29 entradas com sucesso, zero falhas/avisos: 25 testes efetivos e quatro protocolos informativos de reinício não acionados. Inclui PersistentComponentIdentity. |
| Pacote Windows Development final | BuildCookRun passou em 27/10, incluindo cook, stage/pak/archive. Cook: zero erros e zero avisos. |
| Lifecycle no executável final | Passou com avisos transitórios de navegação e código zero. Exercitou descarregamento/retorno, documento/porta, drop com física e GUID, recriação do inimigo, terminal de save, cinco metadados, confirmação/cancelamento de sobrescrita, rewind com área descarregada, perseguição efetiva através do corredor, save/load entre mundos, Continuar global e Game Over global com liberação de pausa/UI. |
| ColdRestore em processo novo do pacote final | Passou sem falhas/avisos de teste e código zero. Leu slot 0 produzido por Lifecycle, iniciou com jogador em B e A descarregada e confirmou porta/documento/drop ao voltar a A. |
| PIE | Passou no roteiro de descarregar A, visitar B e voltar, preservando a porta destrancada. A navegação foi construída no Editor antes de duplicar o mundo PIE; o corredor foi conferido em gameplay sem rebuild completo runtime. Não certifica conclusão imediata da geração dinâmica de todos os tiles após o retorno. |

A validação revelou dois defeitos corrigidos: a classe derivada de Recast não correspondia ao agente configurado após reabrir o mapa, e componentes transitórios de input eram serializados como se fossem participantes persistentes. O mapa agora usa RecastNavMesh nativo com navegação do corredor construída/salva no Editor completo. A captura de componentes exige propriedades SaveGame ou listener explícito. A leitura aceita somente registros legados comprovadamente vazios quando o componente está ausente; estado persistente ausente continua sendo erro identificável. O novo teste verifica exclusão dos transitórios, aceitação do vazio legado e rejeição do componente persistente ausente sem alteração do participante.

Artefatos finais em `C:/Users/luuki/AppData/Local/Temp/`:

- `HorrorStage7Regression_20261027/index.json`: regressão; log com o mesmo nome-base e extensão .log.
- `HorrorStage7Delivery_20261027/Windows/HorrorGameFramework.exe`: pacote final Development.
- `HorrorStage7DeliveryLifecycle_20261027/index.json` e `HorrorStage7DeliveryLifecycle_20261027.log`: integração do pacote final.
- `HorrorStage7DeliveryColdRestore_20261027/index.json` e `HorrorStage7DeliveryColdRestore_20261027.log`: restauração no processo novo do mesmo pacote.
- `HorrorStage7PIE.json` e `HorrorStage7PIEValidated_20261027.log`: roteiro PIE e preparação de navegação.

Log do cook final: `C:/Users/luuki/AppData/Roaming/Unreal Engine/AutomationTool/Logs/C+Program+Files+Epic+Games+UE_5.8/Cook-2026.10.27-19.51.55.txt`.

As execuções de integração são opt-in: Lifecycle exige `-StreamingDemoAutomation` e UserDir isolado apropriado ao protocolo; ColdRestore exige `-StreamingDemoColdRead` e a mesma UserDir com o arquivo anterior. Consultar as guardas em StreamingDemoLifecycleTests.cpp antes de reproduzir. O diretório final usado foi `HorrorStage7DeliveryLifecycleUser_20261027` na pasta temporária. Nenhum slot de progresso do usuário foi sobrescrito. A instância de Editor do usuário permaneceu aberta na rodada final de 27/10; os processos auxiliares encerraram somente a si próprios.

Avisos e limites: Lifecycle passou com avisos transitórios de CrowdManager/Recast durante trocas de mapa. A engine não encontrou o template HTML do relatório, mas exportou JSON/logs; isso não foi ocultado como log totalmente limpo. Testes via UnrealEditor -game tiveram erros de inicialização de toolsets Python antes da automação e avisos HLOD da demo antiga, distintos do pacote runtime. O cook final foi limpo. Não foram certificados Shipping, controle físico nas novas telas, revisão gráfica completa, desempenho alvo, reinício escondido, migração com arquivos reais v1/v2 ou todos os cenários de interrupção/falha de I/O. As validações automatizadas de UI chamam handlers e não substituem interação física.
