# Inimigos, ruídos e esconderijos

## Escopo confirmado

Patrulha, investigação, perseguição, busca temporária e retorno à patrulha. Visão e ruídos configuráveis; andar gera ruído e agachar é silencioso. Se o inimigo presenciar a entrada no esconderijo, vai à entrada e tenta capturar. A captura leva a Game Over, com opção de carregar o save mais recente do mapa atual. Saves feitos no esconderijo restauram a ocupação e o olhar limitado. Animações de entrada/saída e carregamento entre mapas ficam para etapas próprias.

Implementação entregue com assets e demonstração em 19/10/2026. Builds completos Editor/runtime, 25 entradas da suíte no Editor, três testes no pacote e cook Windows passaram. Patrulha, percepção/captura e o ciclo de salvar escondido → captura → carregar de volta ao esconderijo foram verificados em PIE, com participação do usuário. Consulte Validation.md para distinguir verificações automatizadas, confirmações manuais e limites ainda não certificados.

## Responsabilidades

`Plugins/HorrorAI` não depende de HorrorCore, HorrorUI, SaveSystem ou áudio. O controller utiliza AI Perception e navegação do Unreal. `UHorrorEnemyDefinition` configura velocidades, visão, audição, intervalos, busca e distância de captura. `AHorrorEnemyCharacter` contém a apresentação e a memória persistível; `AHorrorEnemyController` coordena estados e destinos. Delegates de captura e mudança de estado permitem integrar apresentação e consequências sem dependência do projeto.

`UHorrorAITargetComponent` identifica alvos elegíveis e comunica ocultação. A integração do projeto, `UFrameworkThreatTarget`, consulta o fluxo de gameplay e o SaveSystem, emite passos e conecta esconderijos. `UHorrorNoiseEmitterComponent` reporta ruídos ao sentido de audição, com definição própria e cooldown; pode observar impactos físicos da raiz de um ator.

`HorrorInteraction` contém `UHideSpotComponent`, que configura posição interna, alcance de entrada e limites de olhar, e `UHidingParticipantComponent`, que controla ocupação, posição segura de retorno, movimento e colisão. O controller existente encaminha o olhar e a ação de saída, preservando seu input compartilhado.

No módulo do projeto, `AFrameworkThreatCharacter` compõe as capacidades opcionais sobre o personagem de inventário. `AFrameworkHidingSpot` combina interação e marcador de save. `AFrameworkEnemy` conecta o inimigo ao save, diálogos, fluxo e UI. `UThreatGameOverScreen` oferece carregamento e reinício do mapa.

A organização usa estados explícitos e eventos, conforme [State](https://gameprogrammingpatterns.com/state.html) e [Observer](https://refactoring.guru/design-patterns/observer). A percepção utiliza o sistema já fornecido pela engine: [AI Perception](https://dev.epicgames.com/documentation/en-us/unreal-engine/ai-perception-in-unreal-engine).

## Comportamento e limites

As posições de patrulha são coordenadas do mundo configuradas no inimigo. A percepção seleciona apenas atores com o componente de alvo. Antes de atualizar a última posição conhecida ou capturar, o controller confere distância, cone e bloqueio visual. Ao perder a visão, navega até a última posição conhecida, procura nas proximidades por tempo configurável e retoma a rota. A movimentação usa caminhos navegáveis, sem teleporte como alternativa a uma falha de navegação.

A ocultação é comunicada antes de reposicionar o jogador. Assim, o inimigo verifica a visibilidade da entrada, em vez de descobrir uma posição interna que não viu. Entradas não presenciadas não revelam automaticamente o esconderijo. Ao sair, a posição original é validada para evitar colocar a cápsula em um obstáculo. Fim de posse, destruição e restauração liberam o estado transitório.

Ruído de gameplay independe do volume ouvido pelo usuário. A distância percorrida gera passos enquanto o personagem anda no chão; agachar e esconder-se interrompem essa emissão. Impactos exigem física, impulso mínimo e cooldown. Sons audíveis podem ser ligados aos eventos pelas ferramentas de conteúdo, usando HorrorAudio; reportar um ruído de IA não reproduz automaticamente um SoundWave.

## Conteúdo da demonstração

O mapa `/Game/Maps/FrameworkThreat_Demo` deriva da demonstração de áudio. Os assets específicos ficam em `/Game/_Project/Threat`: personagem, inimigo, esconderijo, pickup dropado com emissor de impactos, GameMode, tela de Game Over e definições de ruído/inimigo. O personagem preserva inventário, inspeção, objetivos, documentos e diálogos. A configuração global da UI referencia a nova tela de Game Over.

A patrulha fica no lado posterior da porta do puzzle, entre X=-3450 e X=-3100, Y=-1380. O esconderijo está em (-2110,-1500,100). O NavMesh usa geração dinâmica e pool fixo de 256 tiles para cobrir o corredor e atualizar obstáculos móveis. As formas geométricas são placeholders. O novo mapa inicia sem tentar carregar automaticamente o slot 0 herdado das demonstrações anteriores; o carregamento continua disponível pela UI.

Os sentidos são configurados no construtor do controller, antes do registro do componente de percepção. Na posse, os valores do Data Asset são aplicados e o listener é atualizado. Isso evita configurar sentidos em um listener ainda sem identificação válida.

## Persistência e Game Over

A memória do inimigo usa propriedades SaveGame: estado, índice da rota, última posição conhecida e tempo restante. Referências temporárias ao alvo não são serializadas. Após restauração, uma perseguição comum retoma a investigação da posição conhecida até que a visão confirme novamente o alvo. O conhecimento de uma entrada presenciada usa a identidade persistente do esconderijo.

O jogador guarda a identidade do esconderijo e a posição de retorno. Os callbacks reconectam a ocupação após a restauração dos participantes. O esconderijo precisa ter um SaveComponent com identidade estável.

`USaveSubsystem::LoadLatestForCurrentLevel` lê os cinco slots assincronamente e compara SavedAt dos snapshots compatíveis com o mapa aberto. Não escolhe por número do slot nem muda de mapa. Arquivos inválidos são diagnosticados; ausência de save ou falha de restauração mantém a tela de Game Over com mensagem. A seleção reutiliza o pipeline de restauração existente e não grava progresso. Esc/B não fecha Game Over para escapar da captura.
