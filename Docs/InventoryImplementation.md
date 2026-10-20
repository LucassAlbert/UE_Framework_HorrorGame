# Inventário — etapa 2

## Comportamento acordado

O inventário começa com cinco slots. Cada Data Asset define o limite de empilhamento. A coleta é integral: se não couber a quantidade solicitada, nada é retirado do cenário. Itens iguais usam a mesma definição e podem ocupar vários stacks.

Combinar consome uma unidade de cada ingrediente e cria uma unidade do resultado. A ordem de seleção não importa; um stack pode fornecer ambos quando tiver duas unidades. A operação calcula o espaço após o consumo antes de confirmar a alteração.

“Usar” valida novamente o alvo à frente da câmera, distância, obstrução e contrato de uso, mesmo com o inventário pausando o jogo. A chave só é consumida após destrancar. Uso e equipamento bem-sucedidos fecham a tela. Falhas mantêm a tela e o item.

Dropar cria uma unidade física à frente do jogador, validando espaço e colisão simples do mesh antes de retirar a unidade do inventário. O item cai por gravidade e usa detecção contínua de colisão para reduzir atravessamentos de superfícies por pickups pequenos. “Descartar” permanece desativado por padrão; um item futuro pode habilitá-lo no Data Asset.

A etapa 2 oferece `OnInspectRequested`; o botão fica indisponível até existir um consumidor conectado. A etapa 3 implementa esse consumidor e a visualização independente com rotação/zoom, documentada em [InspectionImplementation.md](InspectionImplementation.md).

## Divisão do código

| Código | Responsabilidade |
| --- | --- |
| Plugin `HorrorInventory` | Slots, stacks, equipamento lógico, receitas e contratos de uso |
| `UInventoryItemDefinition` | Nome, descrição, limite, mesh, escala e ações permitidas |
| `UInventoryRecipe` | Dois ingredientes e resultado por soft references |
| `UInventoryGameplayComponent` no projeto | Câmera, uso, drop, equipamento visual, UI e conexão com save |
| `AInventoryCharacter` | Capacidades opcionais sobre o personagem existente |
| `AInventoryWorldItem` | Pickup colocado no mapa, preservado oculto para permitir rewind |
| `AInventoryDroppedItem` | Pickup criado em runtime, reconstruível, com física |
| `AInventoryKeyDoor` | Contrato de chave, trava e apresentação de abertura |
| `AInventoryFlashlight` | Equipamento com spotlight preso à câmera |
| `UInventoryScreen` | UI inicial de cinco slots e ações, atualizada por eventos |

O plugin depende apenas de Core, CoreUObject e Engine. Pode ser copiado para outro projeto sem HorrorCore, HorrorUI ou SaveSystem. A integração específica fica fora dele. O personagem reutiliza movimento e controller existentes; `OnInventory` é um contrato opcional de input, sem dependência do núcleo sobre o inventário.

Component separa capacidades; Observer notifica mudanças sem bindings recorrentes ou Tick de UI. Data Assets mantêm configuração compartilhada separada do estado de cada inventário, conforme a ideia de Type Object. Não foram adicionados Singleton ou fábricas para operações simples.

Referências consultadas:

- [Catálogo de padrões](https://refactoring.guru/design-patterns/catalog)
- [Código-fonte oficial de Game Programming Patterns](https://github.com/munificent/game-programming-patterns)
- [Component](https://raw.githubusercontent.com/munificent/game-programming-patterns/master/book/component.markdown)
- [Observer](https://raw.githubusercontent.com/munificent/game-programming-patterns/master/book/observer.markdown)
- [Type Object](https://raw.githubusercontent.com/munificent/game-programming-patterns/master/book/type-object.markdown)

## Configuração

Crie Data Assets da classe `InventoryItemDefinition`. `ItemId` identifica o tipo; stacks persistem o caminho do asset e um GUID próprio. Use `MaxStackSize >= 1`, `WorldScale > 0` e mesh com colisão simples para drops físicos.

Uma chave configura `bCanUse=true`, `bConsumeOnUse=true`, `bCanEquip=false`. A porta configura `RequiredKey` com o mesmo `ItemId`. O alvo implementa `InventoryItemTarget` e só retorna sucesso após aplicar o efeito. Consumo é consequência do uso bem-sucedido; esta etapa não introduz vida ou sanidade.

A lanterna configura `bCanEquip=true` e `EquipmentActorClass=InventoryFlashlight` ou um Blueprint derivado. `EquipmentTransform` define sua apresentação relativa à câmera, inicialmente com Y negativo, à esquerda. Persiste-se o stack equipado; o ator visual é reconstruído.

Crie `InventoryRecipe` com `FirstIngredient`, `SecondIngredient` e `ResultItem`. Adicione a receita à lista `Recipes` dos dois ingredientes para poder iniciar por qualquer um. Receitas/resultados carregam sob demanda; durante o carregamento, a UI informa que a combinação ainda não foi executada.

As ações retornam `FInventoryResult` com sucesso e mensagem. Blueprints podem consumir `OnFeedback` para HUD/áudio. Para apresentação própria, substitua a classe de tela no componente e conecte suas ações às APIs públicas.

## Save

O formato gravado passou para versão 2, mantendo leitura da versão 1. Registros novos incluem classe de atores runtime autorizados, dependências e, quando habilitado, velocidade linear/angular e estado de repouso da física.

Propriedades persistentes incluem também valores iguais ao padrão da classe. Antes de aplicar os campos do snapshot, somente as propriedades `SaveGame` são reinicializadas a partir do CDO. Isso permite interpretar campos padrão omitidos por snapshots antigos; se os padrões da classe mudarem, saves legados ainda podem exigir migração explícita.

`USaveComponent.bRecreateOnLoad` é opt-in de classe: exige componente construído na classe e GUID atribuído antes de BeginPlay. Não habilite no jogador nem em atores com `PersistentKey`. Drops usam classe dedicada com essa opção e `bSavePhysics` habilitadas.

O load carrega classes e dependências assincronamente antes de restaurar. Atores runtime ausentes são recriados; os criados depois do snapshot são removidos. Atores colocados continuam exigindo presença e identidade estável. Coletar pickups colocados oculta o ator para permitir rewind.

`ISaveStateListener` permite fornecer dependências, validar estado e atualizar apresentação após desserializar todos os participantes. O inventário permanece independente do SaveSystem.

Ainda não há load entre mapas, coordenação de streaming ou rollback integral se uma desserialização falhar depois de alterar participantes existentes. Falhas são reportadas; um load rejeitado pode exigir reconstrução do mundo antes de continuar gameplay. Essas limitações pertencem à etapa de restauração completa.

## Mapa de demonstração

O scanner de interação inicia ativo por padrão. A opção `bAutoActivate=false` continua disponível para desabilitá-lo explicitamente. O componente de integração libera sua tela e cancela carregamentos pendentes em EndPlay.

Abra `/Game/Maps/FrameworkInventory_Demo` e inicie PIE. O mapa usa cópias dos Blueprints de personagem, controller e GameMode existentes; o personagem herda `InventoryCharacter`. Os assets originais do HorrorCore permanecem como base reutilizável.

Use WASD para movimento, mouse para olhar, E para interagir, C para agachar, P para pausa e I para inventário. O botão View/Select do gamepad abre o inventário; a tela usa foco/navegação de UMG e B retorna. Os mapeamentos de movimento do projeto original foram preservados na cópia do contexto de input.

Há pickups de chave, lanterna e duas partes da chave. Os meshes básicos são placeholders. Selecione um slot e escolha uma ação. Para combinar, escolha “Combinar” no primeiro ingrediente e selecione o segundo. Se a receita ainda estiver carregando, aguarde a mensagem e confirme novamente.

Para destrancar, olhe para a porta, aproxime-se e use a chave pelo inventário. A ação consome uma unidade e fecha a tela; E alterna abertura/fechamento depois de destrancar. A lanterna pode ser equipada, desequipada ou dropada para testar apresentação e coleta física.

A gravação continua pela API de save existente, em pontos de save configurados pelo conteúdo. Esta etapa não implementa a tela funcional de escolha dos cinco slots nem o fluxo completo de morte/transição; esses sistemas ainda têm seus placeholders.
