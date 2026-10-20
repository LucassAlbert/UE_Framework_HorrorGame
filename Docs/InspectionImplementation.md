# Inspeção de itens — etapa 3

## Comportamento

A inspeção é aberta por “Inspecionar” em um item coletado. Mostra uma cópia visual, nome e descrição, sem remover, consumir, desequipar nem alterar o objeto original. O mundo permanece pausado. Esc, B ou “Voltar ao inventário” fecha somente a inspeção e devolve a tela anterior, com sua seleção preservada.

Segure o botão esquerdo e arraste na área do item para girar livremente; a roda aproxima/afasta. No gamepad, o analógico direito gira e os gatilhos controlam zoom. “Restaurar posição”, R ou Y restaura a orientação configurada no Data Asset e o enquadramento inicial. Cada nova abertura começa nessa posição.

A área do item tem fundo preto e iluminação própria. A descrição aparece junto à apresentação, com quebra automática de linha. Os meshes do mapa de demonstração continuam sendo placeholders.

## Responsabilidades e dependências

| Código | Responsabilidade |
| --- | --- |
| Plugin runtime `HorrorInspection` | Sessão, carregamento, cena temporária, rotação, zoom e captura |
| `FInspectionView` | Estado matemático da visualização, sem UI/input/inventário |
| `UInspectionComponent` | Estados Inactive/Loading/Ready/Failed e ciclo de vida dos recursos |
| `AInspectionPreviewActor` privado | Mesh, câmera de captura e luzes na cena temporária |
| `UInspectionGameplayComponent` no projeto | Adaptador entre evento do inventário, componente e pilha de UI |
| `UInspectionScreen` no projeto | Apresentação, arraste, zoom, gamepad, reset e retorno |
| `AInventoryCharacter` | Composição das capacidades opcionais no personagem existente |

O plugin depende apenas de Core, CoreUObject e Engine. Não exige HorrorInventory, HorrorUI, HorrorCore, SaveSystem nem módulos de editor. Pode ser copiado para outro projeto e usado com uma interface própria: chame `StartInspection`, conecte `OnStateChanged`, apresente `GetRenderTarget`, aplique `AdjustView`/`ResetView` e chame `CaptureIfDirty` nas atualizações da apresentação. Feche com `CloseInspection`.

O inventário continua sem dependência sobre inspeção. A integração escuta `OnInspectRequested`. `bEnableInspection` no adaptador permite desativar essa conexão na configuração do personagem; desabilitado antes de BeginPlay, o botão de inspeção fica indisponível por não existir consumidor.

São usados Component para composição de capacidades, Observer nos eventos e um adaptador específico do projeto para conectar contratos. Os estados da sessão são explícitos, sem uma hierarquia de classes de estado para quatro condições simples.

Referências:

- [Catálogo de padrões — Refactoring.Guru](https://refactoring.guru/design-patterns/catalog)
- [Component — fonte oficial de Game Programming Patterns](https://github.com/munificent/game-programming-patterns/blob/master/book/component.markdown)
- [Observer — fonte oficial de Game Programming Patterns](https://github.com/munificent/game-programming-patterns/blob/master/book/observer.markdown)

## Configuração dos itens

Os Data Assets existentes mantêm `bCanInspect`, `Mesh`, `InspectionRotation`, `DisplayName` e `Description`. Não é necessário duplicar configurações em outro asset. A escala usada no cenário não determina o tamanho na tela: os bounds do mesh calculam escala e deslocamento para centralizar a apresentação, inclusive quando o pivot é deslocado.

A rotação usa quaternions, permitindo examinar todos os lados. O zoom tem limites para manter o objeto visível e evitar atravessá-lo com a câmera. Não há hotspots, descoberta automática de pistas, leitura de documentos, animação esquelética ou mudança de estado do item nesta etapa.

## Renderização e ciclo de vida

A sessão cria uma cena runtime temporária separada do mapa, com física, navegação, IA, áudio e colisão desativados. O objeto de preview nunca entra no registro de save nem depende da iluminação/streaming do cenário. Um render target de 768×768 alimenta a UI.

Não há Tick no componente ou no ator de preview. A UI recebe eventos de mouse e usa tempo de UI para as velocidades contínuas do gamepad, mesmo com gameplay pausado. A captura ocorre apenas quando o item, a orientação ou o zoom muda; várias alterações de input são reunidas na atualização da tela.

O mesh carrega assincronamente quando necessário. A tela indica carregamento ou falha e continua permitindo voltar. Cancelamento, geração da solicitação e delegates fracos evitam callbacks antigos sobre uma sessão nova ou encerrada. Fechar, destruir o personagem ou restaurar o save encerra o preview e libera a cena/render target; a inspeção não é um estado persistente do jogo.

Na restauração, o adaptador encerra também a tela de inventário associada, para não deixar um modal com estado anterior ao snapshot. Os outros sistemas continuam responsáveis por sua própria restauração.

## Validação

Resultados de build, automação, Blueprints, PIE e cook devem ser consultados em `Validation.md`. A existência desta documentação não confirma uma validação ainda não executada.
