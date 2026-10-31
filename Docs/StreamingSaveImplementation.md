# Streaming de áreas, cinco slots e Continuar — etapa 7

Implementado na cópia `UE_Framework_HorrorGame`, Unreal 5.8.3 / Windows. Registro atualizado em 27/10/2026. Os resultados detalhados estão em `Validation.md`.

## Comportamento para o jogador

- O menu apresenta Continuar, Novo jogo, Carregar jogo, Configurações e Sair. Novo jogo abre a demonstração de streaming.
- Carregar jogo lista cinco slots com mapa/data. Slots vazios e arquivos ilegíveis não oferecem carregamento.
- Continuar e Carregar último save no Game Over escolhem o save manual mais recente dos cinco slots, independentemente do mapa.
- Terminais de save abrem a mesma tela em modo de gravação. Sobrescrever exige confirmação. Voltar cancela a confirmação antes de sair da seleção. Sucesso só é anunciado após a gravação terminar.
- Aproximar-se de uma área inicia seu carregamento assíncrono; uma passagem visível permanece bloqueada até geometria e estado estarem prontos. A travessia entre áreas não abre outro mundo nem apresenta tela de loading.
- Afastar-se descarrega áreas dispensáveis. Ao voltar, portas, documentos, itens e participantes do save são restaurados. Um perseguidor ativo mantém as áreas de seu caminho carregadas e atravessa o corredor no mesmo ator.

Áreas descarregadas ficam congeladas; não há simulação do tempo transcorrido. A continuidade se aplica às áreas do mesmo mundo persistente. Abrir um save de outro mundo independente, incluindo demos antigas, ainda exige abrir seu mapa persistente com OpenLevel antes da restauração. Isso não migra as demos anteriores para streaming nem promete ausência de oscilações de frame por I/O.

## Responsabilidades e dependências

### HorrorWorld

Plugin runtime reutilizável, com dependência de SaveSystem, sem depender de HorrorCore, HorrorUI ou HorrorAI.

`UHorrorWorldDefinition` configura IDs, referências soft dos níveis, limites espaciais, conexões recíprocas, distâncias de pré-carregamento/descarregamento e tempos. A configuração exige IDs/mapas únicos, limites válidos e distância de descarregamento maior que a de carregamento, evitando alternância frequente na fronteira. Os padrões são 1800/2400 cm, atualização de 0,25 s e timeout de área de 45 s.

`UHorrorWorldSubsystem` pertence ao mundo. Coordena explicitamente Unloaded → Loading → Restoring → Ready → Unloading e Failed. Usa ticker também durante pausas para não bloquear uma restauração iniciada por menus. Cancela delegates/ticker no encerramento. Um mundo que começa em um corredor sem áreas próximas também pode concluir sua preparação.

Antes de descarregar, captura o estado da área; falhar na captura mantém seu conteúdo residente e registra o erro. Uma falha de área impede novas gravações enquanto o mundo estiver incompleto. Um carregamento posterior pode resetar áreas falhadas para tentar reconstruí-las. As guardas de timeout encerram operações com diagnóstico; não substituem falhas por progresso fictício.

`AStreamingPassageGate` protege a entrada até Ready. `UAreaResidencyComponent` oferece um contrato genérico para manter rotas. O grafo conecta a área atual do ator, sua origem e a área do jogador quando sua atividade exige continuidade. Não há busca de todos os atores por frame: a atualização consulta os participantes registrados do save.

`AWorldResidentSpawnPoint` cria atores móveis no nível persistente. Seu GUID deriva do GUID estável do spawner; o estado do spawner evita duplicatas no retorno. A classe precisa optar por reconstrução runtime e manter PersistentKey vazia. A área de origem é atribuída antes de FinishSpawning.

### SaveSystem

Permanece responsável por identidade, serialização, leitura/gravação, reconstrução de atores e cache dos estados de áreas. O contrato `ISaveWorldProvider` permite preparar áreas e consultar residência sem criar dependência reversa de SaveSystem para HorrorWorld.

O formato 3 acrescenta caminho completo do mapa persistente, ID do mundo, área ativa, posição de referência, áreas visitadas/residentes e associação de cada ator à sua área. O snapshot combina participantes carregados com os registros das áreas descarregadas. Remoção por streaming não vira destruição permanente; destruição real remove registros obsoletos. Drops preservam classe, GUID e estado físico.

O carregamento prepara o mundo e suas áreas, carrega classes/dependências sob demanda, restaura os participantes e só então libera o fluxo. Identificadores de operação ignoram callbacks antigos; o carregamento completo tem timeout de 90 s. Um rewind no mesmo mundo descarrega/recria as áreas a partir dos padrões autorais antes de aplicar o arquivo, inclusive áreas ainda não visitadas naquele snapshot.

A captura grava componentes que possuem propriedades SaveGame ou implementam explicitamente ISaveStateListener. Componentes transitórios sem estado persistente, como os de input criados pela engine, não geram dependências de nomes entre sessões. Essa correção resolveu uma falha encontrada no pacote ao restaurar um mapa independente. Para snapshots antigos, um componente ausente só é dispensado se seus bytes correspondem exatamente à serialização vazia legada; componentes persistentes ausentes continuam causando erro explícito com nome do ator/componente. O teste PersistentComponentIdentity cobre ambos os casos.

O catálogo `USaveSlotCatalog` lê metadados assincronamente, informa arquivos ausentes/ilegíveis e atualiza a lista após uma gravação. A seleção global usa a data UTC gravada. As APIs antigas SaveGame/LoadGame continuam no slot zero, e LoadLatestForCurrentLevel permanece disponível para consumidores antigos. Os leitores aceitam versões 1 e 2; metadados legados têm teste, mas arquivos reais dessas versões não foram importados nesta validação.

### HorrorCore e integração do projeto

BootManager coordena a pausa de preparação e aguarda o save/mundo antes de apresentar o gameplay. Os eventos liberam apenas os bloqueios pertencentes à operação. Em falha, o fluxo e a apresentação recebem o diagnóstico; a UI não anuncia restauração concluída antes do resultado.

O módulo do projeto compõe as telas nativas, o terminal e o inimigo. `UEnemyAreaResidencyComponent` mantém a rota durante Chase, Investigate, Search e o retorno à origem, reutilizando a IA existente. Os plugins de mundo e IA continuam independentes.

As telas derivam de UIScreenBase, preservando pilha, pausa e navegação já existentes. A confirmação de sobrescrita tem uma ação explícita e pode ser cancelada. Os Blueprints novos configuram classes/mapa, enquanto os contratos e as operações permanecem em C++.

## Demonstração e configuração

- Mapa persistente: `/Game/Maps/FrameworkStreaming_Demo`.
- Áreas: `/Game/Maps/Streaming/FrameworkArea_A` e `FrameworkArea_B`.
- Dados: `/Game/_Project/Streaming/DA_StreamingWorld`.
- Inimigo/game mode e telas: `/Game/_Project/Streaming`.
- Ambas as áreas têm terminal, documento, chave, porta e esconderijo; a área A contém o spawner do inimigo. O corredor permanece no nível persistente.
- Os três mapas estão explicitamente em MapsToCook, além da configuração de cook existente.

O NavMesh da demonstração usa a classe nativa RecastNavMesh, compatível com o agente configurado, geração Dynamic, pool fixo de 2048 tiles e reconstrução no carregamento. A navegação da geometria persistente foi construída e salva no Editor completo. Criar um ator Recast e salvar pelo commandlet Python não inicializava o sistema de navegação necessário para construir o corredor: os testes detectaram a lacuna. Foram verificados pontos de -1000 a 1000 cm antes do salvamento. Não foi acrescentado rebuild completo durante o gameplay para esconder esse problema de autoria.

Para outro projeto, copie/ative HorrorWorld e SaveSystem, crie uma definição para seus próprios níveis e coloque o coordenador no mapa persistente. Configure participantes com SaveComponent e IDs estáveis; use spawners persistentes para atores que atravessam áreas. A composição específica do inimigo/telas está no módulo do projeto e pode ser substituída. Não copie caches, DLLs ou pastas geradas como parte do framework.

## Validação e limites

Build completo Editor e runtime Win64/Development passaram. A regressão final de 27/10 terminou com 29 entradas sem falhas ou avisos de teste: 25 testes efetivos e quatro protocolos informativos de reinício não acionados nessa execução. O pacote Windows Development final concluiu build/cook/stage/archive; o cook terminou com zero erros e zero avisos.

No executável final empacotado, Lifecycle passou com avisos transitórios de navegação: streaming, cache, física/identidade do drop, gravação pelo terminal, cinco slots, cancelamento de sobrescrita, rewind, perseguição efetiva pelo corredor, troca de mapa, Continuar e Game Over. ColdRestore passou sem avisos em outro processo, lendo o arquivo criado pelo primeiro processo e restaurando os estados da área descarregada. Resultados e caminhos dos relatórios estão em Validation.md.

PIE verificou descarregamento/retorno e preservação da porta. A execução validada construiu a navegação no mundo do Editor antes de duplicá-lo para PIE e conferiu o corredor durante o jogo, sem rebuild completo em gameplay. Para reproduzir esse roteiro, prepare a navegação com Build Paths no Editor antes de Play. O teste não certifica a navegação de toda a área imediatamente após cada recarregamento; geração dinâmica pode ainda estar em andamento.

Os testes usam pastas UserDir exclusivas; não sobrescrevem os cinco slots do jogador. As invocações de handlers de UI na automação não equivalem a um teste físico de teclado/controle. Não foram certificados nesta etapa Shipping, desempenho em hardware alvo, reinício escondido ou reprodução física de todas as interrupções/erros de I/O. A demonstração usa geometria e telas de protótipo; as execuções NullRHI não substituem revisão visual.

Ocorreram avisos transitórios de CrowdManager/Recast durante trocas de mapa na integração, inclusive no pacote. A engine também registrou falta do template HTML do relatório; JSON e logs foram gerados e a automação terminou com código zero. Nos testes via UnrealEditor -game, toolsets Python da engine registram erros de inicialização antes da automação; esses erros não aparecem no pacote runtime. A demo antiga Threat produz referências HLOD de editor nesse modo. O problema de classe/agente do NavMesh do mapa novo foi corrigido; os avisos remanescentes estão separados dos resultados de testes.

## Referências

- https://dev.epicgames.com/documentation/en-us/unreal-engine/level-streaming-overview-in-unreal-engine
- https://refactoring.guru/design-patterns/observer — eventos de conclusão e mudanças de área.
- https://gameprogrammingpatterns.com/state.html — estados explícitos de carregamento/atividade.

Os padrões orientam os contratos e a comunicação; não foram introduzidas hierarquias adicionais para operações triviais.
