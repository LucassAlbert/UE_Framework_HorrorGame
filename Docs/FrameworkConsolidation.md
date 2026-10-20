# Consolidação do framework

Esta etapa estabiliza a integração já existente, completa os comandos de controle aprovados e verifica a distribuição Windows. Os resultados executados estão em [Validation.md](Validation.md).

## Controle, pausa e ciclo de vida

O `UIManagerSubsystem` guarda uma referência fraca ao controller ao qual aplicou seus bloqueios. Fechar ou transferir a apresentação libera exatamente um incremento de bloqueio de movimento/olhar e a pausa iniciada pela UI. Bloqueios e pausas anteriores de outros sistemas são preservados. Reaplicar a mesma pilha não acumula bloqueios.

`ReleasePlayerPresentation` encerra telas e HUD pertencentes ao controller. `AHorrorPlayerController` chama essa operação ao perder o pawn e ao encerrar. Na nova posse, limpa o timer de esconderijo, a animação transitória, o foco de interação e os estados anteriores da UI; recupera o HUD quando o fluxo está jogando e a configuração está pronta. Uma transição de saída de esconderijo não pode devolver comandos enquanto houver uma tela modal.

`ResetTransientControlState` centraliza esse reset. Movimento, câmera, interação, agachar e abertura do inventário também exigem o fluxo `Playing`; `Loading` e `GameOver` bloqueiam gameplay mesmo sem uma tela aberta. Isso não implementa o fluxo completo de morte/respawn.

O personagem implementa `ISaveStateListener`. Após restauração, descarta a entrada de movimento pendente, zera a velocidade anterior e reinicia os estados transitórios do controller/câmera. A interface pública torna `SaveSystem` uma dependência pública de `HorrorCore`. O formato dos arquivos de save não mudou.

## Entrada

Os mapeamentos foram adicionados a `MainMappingContext` e `IMC_InventoryDemo`. As teclas existentes foram preservadas.

| Ação | Controle |
| --- | --- |
| Mover | Analógico esquerdo |
| Olhar | Analógico direito |
| Interagir / avançar diálogo | A |
| Agachar | B |
| Pausa | Menu/Start |
| Inventário da demonstração | View/Select |
| Confirmar / voltar em menus | A / B |

Os analógicos usam zona morta radial de 0,2. O olhar aplica inversão apenas do eixo Y, escala de 90 por eixo e `ScaleByDeltaTime`, nessa ordem após a zona morta. A escala é configurável no contexto; não representa uma tela de sensibilidade/remapeamento. O mouse continua com sua cadeia original, sem multiplicação pelo tempo.

### Correção após o teste físico

O usuário confirmou uma falha na navegação dos menus. O gerenciador enviava o foco ao widget raiz da tela; sem um controle filho selecionado, a navegação espacial do Slate não tinha um ponto de partida adequado. `UUIScreenBase` agora oferece `GetNavigationFocusTarget`: localiza o primeiro controle focável no ramo visível/habilitado ou recupera o último controle selecionado ainda disponível. `RememberNavigationFocus` é chamado antes de ocultar a tela. Os modos UIOnly/GameAndUI passam esse alvo ao Slate, sem sobrescrevê-lo em seguida com foco no widget raiz.

Essa política serve também aos controles de Blueprints e a widgets compostos; não há lista de botões específica de inventário no gerenciador. A pesquisa ocorre ao aplicar a pilha, sem Tick. Telas de diálogo com `bUseGameplayInput` mantêm o foco raiz para seus comandos. O direcional e o analógico esquerdo usam a navegação padrão do Slate, com A para confirmar e B para voltar. Uma tela sem controles disponíveis continua podendo receber seus comandos na raiz.

Em Unreal 5.8, os dados ativos ficam em `DefaultKeyMappings.Mappings`; o array legado `Mappings` não deve ser utilizado para configurar esses assets. Os modificadores são subobjetos pertencentes ao próprio contexto.

## Reutilização

Os plugins abaixo contêm lógica de runtime e podem ser levados para um projeto C++ compatível. Migrar um plugin não transfere automaticamente os adaptadores, telas e assets específicos que estão em `Source/HorrorGameFramework` e `Content/_Project`.

| Plugin | Capacidade | Integração necessária no projeto consumidor |
| --- | --- | --- |
| HorrorInventory | Inventário e definições de itens | Componente, capacidade, Data Assets e comandos de coleta/uso/UI; save fica no adaptador |
| HorrorInspection | Preview, rotação e zoom | Apresentação e configuração do item; não depende de inventário/save/UI |
| HorrorPuzzles | Encaixes e transação de inserção | Fornecimento/consumo de itens, atores visuais e regras de progressão |
| HorrorObjectives | Diário baseado em fatos | Definições, emissão de fatos, apresentação e persistência |
| HorrorDocuments | Definições e coleção | Coleta, leitura/UI e persistência |
| HorrorDialogue | Definições e sessão sequencial | NPC/interação, câmera, apresentação, fatos e persistência |
| HorrorInteraction | Contratos e detecção | Scanner, canais de trace e controller/interagente |
| HorrorUI | Pilha de telas, HUD e políticas de pausa | Configuração das classes, controller e telas do jogo |
| SaveSystem | Registro, snapshots e reconstrução opt-in | Identidades estáveis, propriedades SaveGame, dependências e callbacks |
| HorrorCore | Personagem, controller, boot e fluxo | EnhancedInput, HorrorInteraction, HorrorUI e SaveSystem; assets e configuração do novo jogo |

Para reutilizar: copie o diretório do plugin para `Plugins` do destino, habilite-o e declare o módulo consumidor em `Build.cs`; configure os componentes/contratos e migre pelo Editor apenas o conteúdo necessário. Compile com a engine do destino e valide o cenário naquele projeto. A portabilidade para um projeto separado ainda não foi testada nesta etapa.

Para desativar uma capacidade neste framework, use os adaptadores existentes: `bEnableInspection`, `bEnableDocuments`, `bEnableDialogue`, `bEnablePuzzles` e `bEnableObjectives`. Confira também o conteúdo que depende dela: portas, condições de diálogos e objetivos não devem exigir uma funcionalidade desativada. Desativar o adaptador não apaga o histórico persistido. Inventário ainda faz parte da composição de `AInventoryCharacter`; não há um interruptor global único para todos os sistemas. Não desabilite um plugin no descritor enquanto o módulo do projeto ainda o referencia.

Os adaptadores preservam a separação entre regras reutilizáveis e integração. Componentes mantêm capacidades coesas ([Component, Game Programming Patterns](https://gameprogrammingpatterns.com/component.html)); delegates notificam mudanças sem inserir dependências de UI nas regras ([Observer, Refactoring Guru](https://refactoring.guru/design-patterns/observer)). Os enums de controle/fluxo continuam explícitos; esta etapa não introduz uma hierarquia de classes State para operações simples.

## Distribuição e verificações

`DefaultGame.ini` inclui BootScene e as cinco demonstrações em MapsToCook. O fluxo inicial continua sendo BootScene; selecionar um mapa de demonstração na linha de comando permite exercitá-lo isoladamente. As telas iniciais ainda incluem placeholders.

As regressões novas são `HorrorFramework.Consolidation.UIOwnership`, `PlayerLifecycle` e `GamepadMappings`. As duas primeiras verificam propriedade dos bloqueios, ausência/troca de controller, posse real de pawn, interrupção de esconderijo, restauração e bloqueio por fluxo. A terceira carrega os contextos salvos e verifica comandos, drift, orientação e rotação acumulada em 15/30/120 FPS. Testar os modificadores não substitui testar o dispositivo físico e o foco da UI.

O pacote de validação deve ser executado com `-UserDir=<diretório temporário exclusivo>` para separar seus saves dos slots do projeto. Não use os botões antigos Save/Load do HUD para testes sem essa separação. Load entre mapas/streaming permanece indisponível; o SaveSystem recusa um snapshot de outro mapa.

Remapeamento persistente, áudio centralizado, apresentação final de morte e IA continuam sendo etapas futuras. Os testes desta etapa não certificam desempenho nem substituem a reprodução visual de todas as combinações de telas, carregamento e foco.
