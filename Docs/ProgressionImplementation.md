# Puzzles, portas e objetivos — etapa 4a

## Comportamento

Interagir com um encaixe abre uma seleção dos itens do inventário e pausa o mundo. Um item incompatível mostra uma mensagem, mantém a seleção aberta e não consome nada. Uma inserção correta consome uma unidade, exibe o item no encaixe e fecha a seleção. Nesta etapa não é possível retirar o item inserido.

Todos os encaixes preenchidos destrancam a porta. A porta continua fechada até o jogador interagir com E. Não aceita uma chave para contornar o puzzle.

O inventário tem um botão “Objetivos”. A tela lista objetivos em andamento e concluídos; objetivos ainda não ativados ficam ocultos. Voltar retorna ao inventário, mantendo a pausa e o HUD existente. Não foi criado atalho de teclado/gamepad para abrir objetivos. Avisos breves apresentam ativações e conclusões sem abrir uma tela nem capturar input.

## Responsabilidades

| Código | Responsabilidade |
| --- | --- |
| Plugin `HorrorPuzzles` | Definições, validação e progresso de encaixes |
| `UItemPuzzleDefinition` | Identidade do puzzle, encaixes, IDs dos itens e fato de conclusão |
| `UItemPuzzleComponent` | Estado individual do puzzle e preparação/confirmação/cancelamento de inserções |
| Plugin `HorrorObjectives` | Definições de objetivos e registro de fatos de progresso |
| `UObjectiveDefinition` | Título, descrição, condições de ativação e conclusão |
| `UObjectiveJournalComponent` | Estados Inactive/Active/Completed, eventos e restauração |
| `APuzzleDoor` | Integração do puzzle com a porta existente e o SaveSystem |
| `APuzzleSocket` | Interação, vínculo com um encaixe, apresentação do item e persistência |
| `AProgressionArea` | Evento de entrada de jogador e registro de visita |
| `UProgressionGameplayComponent` | Transação com inventário, validação do alvo, UI, avisos e save |
| `ProgressionScreens` | Seleção de item, tela de objetivos e avisos temporários |

Os dois novos plugins dependem somente de Core, CoreUObject e Engine. Não dependem um do outro, do inventário, de UI, de HorrorCore ou do SaveSystem. Podem ser copiados para outro projeto com um adaptador próprio. O módulo do projeto conecta os contratos específicos; o núcleo do personagem não ganhou dependência desses plugins.

Component é usado para compor capacidades; Observer/delegates comunicam mudanças; Data Assets separam definições compartilhadas do estado de cada sessão. Não há buscas de todos os atores ou polling por frame para atualizar puzzles/objetivos. O aviso usa tempo de UI apenas enquanto está na tela, inclusive durante pausa.

Referências consultadas: [Observer](https://refactoring.guru/design-patterns/observer), [Component](https://github.com/munificent/game-programming-patterns/blob/master/book/component.markdown) e [catálogo de padrões](https://refactoring.guru/design-patterns/catalog).

## Configuração

Crie um Data Asset `ItemPuzzleDefinition` com `PuzzleId` único e pelo menos um encaixe. Cada encaixe recebe `SlotId` único, nome de apresentação e `RequiredItemId` igual ao `ItemId` do item de inventário. `CompletionFact` identifica o evento produzido ao resolver o puzzle. IDs vazios/repetidos são rejeitados.

Coloque uma `PuzzleDoor` no mapa e atribua o asset em seu componente Puzzle. Coloque um `PuzzleSocket` para cada encaixe; configure `Door` e `SlotId`. Configure mesh/transform do Frame e posição relativa do InsertedMesh. Os itens utilizam o mesh e a escala de cenário já definidos no inventário. Porta e encaixes precisam estar carregados juntos; esta etapa não coordena streaming entre subníveis.

Crie Data Assets `ObjectiveDefinition` e configure a lista Definitions do componente Objectives do personagem. A ativação exige todos os `ActivationFacts`; lista vazia ativa desde o início. A conclusão exige todos os `CompletionFacts`, que não pode ser vazia. Use fatos distintos e estáveis, por exemplo `Puzzle.PowerDoor.Completed` e `Area.PowerRoom.Entered`. Um fato pode concluir um objetivo e ativar o próximo sem dependência direta entre assets.

Uma `ProgressionArea` registra seu `EntryFact` na primeira entrada de um jogador. Configure os limites do componente Volume para a área desejada. Inimigos não completam o objetivo. O personagem precisa gerar eventos de overlap. Blueprints de outros eventos podem usar `RecordFact` para integrar narrativa/puzzles adicionais.

`bEnablePuzzles` e `bEnableObjectives`, definidos antes de BeginPlay no adaptador, controlam as capacidades de integração. Sem objetivos habilitados, o botão fica oculto por não haver consumidor do evento do inventário. Para remover fisicamente um plugin de outro jogo, retire primeiro seus consumidores e dependências do módulo do projeto; não basta desmarcar um plugin exigido pelo código.

## Transação e persistência

A seleção revalida o item, o estado do inventário, o encaixe, o alcance, o trace da câmera e a obstrução no momento de confirmar. Abrir uma tela não concede permissão permanente para aplicar um item.

O puzzle prepara a inserção sem emitir eventos; o adaptador consome uma unidade, atualiza o item do encaixe e confirma o progresso. Falha no consumo cancela a preparação. Uma gravação durante essa transação é rejeitada para evitar snapshots intermediários. Equipamento lógico segue as regras do inventário existente.

O save persiste encaixes preenchidos, item inserido, trava/abertura, fatos, estados de objetivos e visitas às áreas. As dependências de assets são informadas ao SaveSystem para carregamento antes da restauração. Toda a desserialização precede validação e atualização visual.

Restaurar fecha as telas/avisos de progressão e atualiza o diário sem repetir avisos de conclusão. Overlaps provocados pelos transforms durante o load não produzem novas visitas/fatos. Valores padrão vazios são restaurados pelo mecanismo existente, permitindo voltar de um puzzle completo para um save parcial.

O formato geral de save permanece na versão existente. Não foi implementada migração automática quando IDs, condições ou a lista de encaixes mudam entre versões do conteúdo. Dados incompatíveis são rejeitados. Mantêm-se as limitações existentes de load entre mapas/streaming e ausência de rollback integral após uma falha tardia de restauração.

## Mapa de demonstração

Abra `/Game/Maps/FrameworkProgression_Demo`. O mapa e `BP_ProgressionPlayer`/`BP_ProgressionGameMode` são cópias próprias; o controller e o contexto de Enhanced Input do inventário são reutilizados. Os assets novos ficam em `/Game/_Project/Progression`.

O corredor usa meshes básicos como placeholders. Comece pelo botão Objetivos no inventário (I), pegue os dois fusíveis com E e avance até os dois encaixes diante da porta. Cada encaixe informa o nome do fusível correspondente. Selecione o item correto na tela aberta por E. Ao inserir os dois, o primeiro objetivo termina e o segundo aparece. Abra a porta com E e entre no fundo do corredor para terminar o segundo.

A porta, encaixes, pickups e área recebem chaves estáveis de save. Os atores da demonstração ficam carregados para testar o ciclo completo sem depender de streaming do World Partition.

## Validação

Consulte [Validation.md](Validation.md) para resultados efetivamente executados. A existência desta documentação não confirma testes ainda pendentes.
