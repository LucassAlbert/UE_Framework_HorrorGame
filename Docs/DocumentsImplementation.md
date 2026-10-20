# Documentos — etapa 4b

## Comportamento acordado

Documentos usam uma coleção própria e não ocupam os cinco slots do inventário. Interagir com E coleta, oculta o pickup e abre a leitura automaticamente. O mundo pausa e o HUD existente continua visível sob a apresentação. Ao fechar essa leitura inicial, o jogador retorna ao gameplay.

O botão Documentos no inventário abre a coleção. Selecionar uma entrada abre o leitor sobre a coleção; Esc, B ou Voltar retorna à tela anterior. Fechar a coleção retorna ao inventário, preservando a pausa. Não foi criado atalho de gameplay para documentos.

Enquanto existir uma tela modal, o HUD fica visível, mas suas ações são desabilitadas. Isso impede a navegação por teclado de alcançar botões de gameplay por trás do inventário, coleção ou leitor.

A primeira abertura marca o documento como lido, mesmo quando há várias páginas. Reaberturas não repetem o evento de leitura. Cada abertura começa na primeira página; página atual e rolagem são estado de apresentação, não progresso persistente.

Cada página tem texto e uma imagem opcional. O texto tem quebra de linha e rolagem; a imagem preserva sua proporção e tem altura limitada. Página anterior/próxima, Page Up/Page Down ou os botões superiores esquerdo/direito do controle navegam entre páginas. As extremidades desabilitam a ação correspondente. Trocar a página retorna sua rolagem ao início.

## Arquitetura

| Código | Responsabilidade |
| --- | --- |
| Plugin runtime `HorrorDocuments` | Conteúdo e coleção de documentos, sem dependência de inventário, UI ou save |
| `UDocumentDefinition` / `FDocumentPage` | ID, título, páginas, imagens opcionais e fatos de coleta/leitura |
| `UDocumentCollectionComponent` | Aquisição, deduplicação, estado lido e eventos |
| `ADocumentPickup` no projeto | Interação, apresentação física, ocultação e persistência do ator |
| `UDocumentGameplayComponent` no projeto | Adaptador de interação, coleção, telas, objetivos e save |
| `UDocumentCollectionScreen` | Lista de documentos coletados, com estado Novo/Lido |
| `UDocumentReaderScreen` | Texto, paginação, carregamento e apresentação da imagem |
| `AInventoryCharacter` | Composição das capacidades opcionais sobre o controlador existente |

O plugin depende somente de Core, CoreUObject e Engine. Para reutilizá-lo em outro projeto, copie HorrorDocuments, adicione DocumentCollectionComponent ao proprietário desejado e conecte sua apresentação aos eventos. `Collect` inclui o documento; `MarkRead` registra a primeira abertura; `Contains`, `IsRead` e `GetDocuments` consultam o estado. Não há Tick no sistema nem busca de todos os atores para atualizar a coleção.

Component separa capacidades, Observer comunica mudanças e Data Assets mantêm conteúdo compartilhado separado do progresso individual. O módulo do projeto conecta os sistemas; nenhum plugin de núcleo ganhou dependência de documentos. Referências: [Observer](https://refactoring.guru/design-patterns/observer), [Component](https://github.com/munificent/game-programming-patterns/blob/master/book/component.markdown) e [catálogo](https://refactoring.guru/design-patterns/catalog).

## Configuração

Crie um Data Asset da classe DocumentDefinition. Configure DocumentId único, Title e uma lista Pages com pelo menos uma página. Cada página exige texto legível e aceita uma soft reference opcional para Texture2D. Definições sem identificação, título ou páginas válidas e IDs repetidos são rejeitados.

CollectionFact e ReadFact são opcionais. Use nomes estáveis, como `Document.PowerMemo.Collected` e `Document.PowerMemo.Read`, nas condições dos ObjectiveDefinition. Coleta e leitura são eventos distintos, embora normalmente ocorram juntos quando a leitura abre automaticamente. A integração respeita a desativação de objetivos no adaptador de progressão.

Coloque DocumentPickup no mapa, atribua Document e configure o mesh/transform físico pelo componente DocumentMesh ou por um Blueprint derivado. A apresentação física pertence à composição do ator; não exige um item do inventário nem uma sessão de inspeção 3D. Configure uma identidade persistente única em seu componente Save.

`bEnableDocuments`, configurado antes de BeginPlay no adaptador do personagem, desativa a integração. O botão fica oculto quando não existe consumidor. As classes das telas são configuráveis no adaptador. Para um jogo que remova fisicamente o plugin, retire também os consumidores/dependências do módulo do projeto; o código específico do framework não é parte do plugin portátil.

## Carregamento e ciclo de vida

O pickup carrega sua definição assincronamente antes de oferecer interação. A execução revalida o alvo, o alcance, o trace da câmera, a visibilidade e o estado atual. Chamadas diretas à função de coleta passam pelas mesmas verificações.

As imagens carregam somente quando sua página é exibida. O texto já está disponível, permitindo ler durante o carregamento. Uma falha de imagem é apresentada explicitamente, mantendo o texto acessível. Trocar de página, fechar o leitor ou restaurar um save cancela o carregamento anterior. Delegates fracos e um contador de geração impedem callbacks antigos de alterar outra página.

Aquisição e leitura só emitem eventos na primeira alteração. A coleção impede mutações reentrantes durante notificações. Os adaptadores removem binds e fecham as telas associadas em EndPlay e na restauração. Se a criação da leitura falhar depois de adquirir, o documento permanece na coleção e a falha é registrada; não se perde o conteúdo.

## Persistência

O componente da coleção persiste referências às definições e o estado lido. O pickup persiste sua definição e bCollected. A coleta bloqueia saves intermediários entre a ocultação do ator e a atualização da coleção/objetivos. As definições são fornecidas ao SaveSystem como dependências; as imagens continuam sendo carregadas pela apresentação sob demanda.

Restaurar atualiza a coleção e a visibilidade do pickup, fecha leitor/coleção/inventário associado e não reemite eventos de aquisição/leitura. Isso evita repetir objetivos ou efeitos narrativos. Um snapshot anterior à coleta devolve o documento ao cenário; um snapshot coletado e ainda não lido recupera exatamente esse estado.

O formato geral do SaveSystem permanece na versão existente. Não há migração automática de IDs/conteúdo alterado nem garantia de migração de snapshots anteriores à introdução desses componentes. Mantêm-se as limitações existentes de load entre mapas/streaming e rollback integral após falha tardia de restauração.

## Demonstração e validação

Abra `/Game/Maps/FrameworkDocuments_Demo`. O mapa e os Blueprints BP_DocumentsPlayer/BP_DocumentsGameMode são cópias próprias da demonstração de progressão. Os assets de documentos ficam em `/Game/_Project/Documents`.

O registro de energia está diante do ponto inicial. E coleta e abre suas duas páginas; a segunda usa uma textura da engine como imagem de placeholder. A primeira abertura conclui o objetivo de leitura e ativa o objetivo dos fusíveis. O restante do corredor mantém o ciclo de puzzle/porta/área da etapa anterior. Os encaixes foram vinculados à porta da cópia do mapa.

Os testes automatizados usam snapshots em memória ou um slot temporário dedicado com prefixo `HorrorFramework_Documents_Test_`. O teste de reinício é executado em dois processos, recusa sobrescrever um arquivo de teste existente e remove somente o arquivo criado por ele após validar a leitura. Os slots do jogador não são utilizados pelo protocolo.

Resultados efetivamente executados e limitações estão em [Validation.md](Validation.md).
